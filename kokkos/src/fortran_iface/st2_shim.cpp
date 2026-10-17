// kokkos/src/fortran_iface/st2_shim.cpp
// The bind(C) shim of ST2: the C ABI of ww_kokkos_c.hpp ("ST2") over the per-point
// device functions of ww_kokkos/srce_st2_spr2.hpp, srce_st2_sin2.hpp and
// srce_st2_sds2.hpp (W3SPR2, W3SIN2, W3SDS2), with the table of
// srce_st2_inptab.hpp (INPTAB). Marshalling only.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT, IGRID at
// W3SRCE); nothing throws across the boundary; Views die inside Kokkos::finalize (a
// finalize hook); inside an OpenMP parallel region (W3SRCE under W3_OMPG) the
// kernels run on Kokkos::Serial. Phase 1 copies in, runs, copies out on every call,
// and the caller serialises the calls (!$OMP CRITICAL in the caller patch).
//
// BETATB, DSIGA and DDRAG are W3SRC2MD module state (w3src2md.F90:97-99), PRIVATE,
// filled once per process by INPTAB from W3IOGR (w3iogrmd.F90:1493): they depend
// on no grid. The shim keeps one copy per process and builds it with the C++
// INPTAB at the first ww_st2_commit().
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/ww_kokkos_c.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/srce_params.hpp"
#include "ww_kokkos/srce_st2_inptab.hpp"
#include "ww_kokkos/srce_st2_sds2.hpp"
#include "ww_kokkos/srce_st2_sin2.hpp"
#include "ww_kokkos/srce_st2_spr2.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::st2shim {
namespace {

using ww::Real;
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// A registered REAL array as the Fortran has it: its LBOUND and its values.
struct Registered {
  int lbound = 0;
  std::vector<Real> v;
};

/// The W3GDATMD scalars the three routines read, under their names in lower case.
struct Scalars {
  Real dth, xfr, fte, ftf, ftwn, fttr, fpimin, zwind, fswell;
  Real xf1, xf2, xfh, sdsaln, cdsa0, cdsa1, cdsa2, cdsb0, cdsb1, cdsb2, cdsb3;
};

struct GridCtx {
  srce::Params p{};  ///< nk, nth, nspec, facti1, facti2; the rest unused by ST2
  Scalars c{};
  Registered h[WW_ST2_NARRAYS];
  DevReal1D sig;   ///< SIG(1:NK+1) on the device
  DevReal1D sig2;  ///< SIG2(1:NSPEC)
  DevReal1D dden;  ///< DDEN(1:NK)
  DevReal1D ecos, esin;  ///< ECOS, ESIN(1:NSPEC)
  std::map<std::string, DevReal2D> buf;
  bool configured = false;
  bool ready = false;
};

/// W3SRC2MD's module state: one per process.
struct ModuleCtx {
  DevReal1D betatb;  ///< BETATB(-NRSIGA:NRSIGA+1, NRDRAG+1), srce_st2_inptab.hpp's layout
  Real dsiga = 0, ddrag = 0;
  bool built = false;
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}
ModuleCtx& module_ctx() {
  static ModuleCtx m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_spr2{0}, g_count_sin2{0}, g_count_sds2{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST2: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() {
  grids().clear();
  module_ctx() = ModuleCtx{};
}

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool spr2, sin2, sds2, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SPR2") != 0), (ww_kokkos_flag("WW_KOKKOS_SIN2") != 0),
                       (ww_kokkos_flag("WW_KOKKOS_SDS2") != 0), env_is_one("WW_KOKKOS_ST2_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SPR2 spectra computed: %lld\n", g_count_spr2.load());
  std::fprintf(stderr, "ww_kokkos: W3SIN2 spectra computed: %lld\n", g_count_sin2.load());
  std::fprintf(stderr, "ww_kokkos: W3SDS2 spectra computed: %lld\n", g_count_sds2.load());
}

bool nested_in_openmp() {
#if defined(WW_SRCE_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// Elements lb..lb+n-1 (Fortran indices) of a registered array, on the device;
/// false when the array does not cover them.
bool slice_to_device(const Registered& r, int lb, int n, const char* name, DevReal1D& out,
                     std::string& why) {
  const long first = static_cast<long>(lb) - r.lbound;
  if (r.v.empty() || first < 0 || first + n > static_cast<long>(r.v.size())) {
    why = std::string(name) + (r.v.empty() ? " not registered" : " is too short");
    return false;
  }
  DevReal1D d(std::string("ww_st2.") + name, static_cast<size_t>(n));
  auto hm = Kokkos::create_mirror_view(d);
  std::memcpy(hm.data(), r.v.data() + first, sizeof(Real) * static_cast<size_t>(n));
  Kokkos::deep_copy(d, hm);
  out = d;
  return true;
}

/// INPTAB (w3src2md.F90:847-1228), once per process.
void build_module_tables(ModuleCtx& m) {
  if (m.built) return;
  DevReal1D b("ww_st2.betatb", static_cast<size_t>(ww::srce::st2::kBetatbSize));
  auto hb = Kokkos::create_mirror_view(b);
  ww::srce::st2::inptab(hb.data(), m.dsiga, m.ddrag);
  Kokkos::deep_copy(b, hb);
  m.betatb = b;
  m.built = true;
}

auto cols(const DevReal2D& v, int npts) {
  return Kokkos::subview(v, Kokkos::ALL, Kokkos::make_pair(0, npts));
}

struct Exec {
  bool nested;
  template <class F>
  void for_points(const char* name, int npts, const F& f) const {
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested) {
      Kokkos::parallel_for(name, Kokkos::RangePolicy<Kokkos::Serial>(0, npts), f);
      Kokkos::Serial().fence();
      return;
    }
#endif
    Kokkos::parallel_for(name, Kokkos::RangePolicy<>(0, npts), f);
    Kokkos::fence();
  }
  template <class Dst, class Src>
  void copy(const Dst& dst, const Src& src) const {
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested) {
      Kokkos::deep_copy(Kokkos::Serial(), dst, src);
      return;
    }
#endif
    Kokkos::deep_copy(dst, src);
  }
  /// A (rows, >= npts) device buffer of this grid, grown, never shrunk.
  DevReal2D& buf(GridCtx& g, const std::string& name, int rows, int npts) const {
    DevReal2D& v = g.buf[name];
    if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
      v = DevReal2D();
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested) {
        v = DevReal2D(Kokkos::view_alloc(Kokkos::Serial(), "ww_st2." + name), rows, npts);
        return v;
      }
#endif
      v = DevReal2D(Kokkos::view_alloc("ww_st2." + name), rows, npts);
    }
    return v;
  }
  /// The device buffer `name`, filled from the caller's (rows, npts) array.
  DevReal2D in(GridCtx& g, const std::string& name, const Real* p, int rows, int npts) const {
    DevReal2D v = buf(g, name, rows, npts);
    copy(cols(v, npts), Borrowed2D<const Real>(p, rows, npts));
    return v;
  }
  /// The first npts columns of a device buffer, back to the caller's array.
  void out(Real* p, const DevReal2D& v, int rows, int npts) const {
    copy(Borrowed2D<Real>(p, rows, npts), cols(v, npts));
  }
};

KOKKOS_INLINE_FUNCTION Real* col(const DevReal2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

/// The common checks of a compute entry point. Returns the grid, or nullptr
/// after recording the error.
GridCtx* compute_ctx(int igrid, int npts, const char* who) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready || !module_ctx().built ||
      !Kokkos::is_initialized()) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
              (std::string(who) + " before ww_st2_commit for this grid").c_str());
    return nullptr;
  }
  if (npts < 0) {
    set_error(WW_KOKKOS_ERR_BAD_SHAPE, (std::string(who) + ": npts is negative").c_str());
    return nullptr;
  }
  return &it->second;
}

}  // namespace
}  // namespace ww::st2shim

using namespace ww::st2shim;
using ww::Real;

extern "C" {

int ww_st2_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st2_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_ST2_NRPAR || nipar != WW_ST2_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st2_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const int nk = ipar[WW_ST2_I_NK], nth = ipar[WW_ST2_I_NTH], nspec = ipar[WW_ST2_I_NSPEC];
    // nk >= 1 is the kernels' precondition (they read EB(NK), SIN1A(NK)).
    if (nk <= 0 || nth <= 0 || nspec != nk * nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st2_init: NK, NTH, NSPEC inconsistent");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    if (!g_hook_pushed) {
      Kokkos::push_finalize_hook(release);
      g_hook_pushed = true;
    }
    if (flags().count) {
      static bool registered = false;
      if (!registered) {
        std::atexit(print_counts);
        registered = true;
      }
    }
    GridCtx& g = grids()[igrid];
    g.p.nk = nk;
    g.p.nth = nth;
    g.p.nspec = nspec;
    g.p.facti1 = rpar[WW_ST2_R_FACTI1];
    g.p.facti2 = rpar[WW_ST2_R_FACTI2];
    Scalars& c = g.c;
    c.dth = rpar[WW_ST2_R_DTH];
    c.xfr = rpar[WW_ST2_R_XFR];
    c.fte = rpar[WW_ST2_R_FTE];
    c.ftf = rpar[WW_ST2_R_FTF];
    c.ftwn = rpar[WW_ST2_R_FTWN];
    c.fttr = rpar[WW_ST2_R_FTTR];
    c.fpimin = rpar[WW_ST2_R_FPIMIN];
    c.zwind = rpar[WW_ST2_R_ZWIND];
    c.fswell = rpar[WW_ST2_R_FSWELL];
    c.xf1 = rpar[WW_ST2_R_XF1];
    c.xf2 = rpar[WW_ST2_R_XF2];
    c.xfh = rpar[WW_ST2_R_XFH];
    c.sdsaln = rpar[WW_ST2_R_SDSALN];
    c.cdsa0 = rpar[WW_ST2_R_CDSA0];
    c.cdsa1 = rpar[WW_ST2_R_CDSA1];
    c.cdsa2 = rpar[WW_ST2_R_CDSA2];
    c.cdsb0 = rpar[WW_ST2_R_CDSB0];
    c.cdsb1 = rpar[WW_ST2_R_CDSB1];
    c.cdsb2 = rpar[WW_ST2_R_CDSB2];
    c.cdsb3 = rpar[WW_ST2_R_CDSB3];
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st2_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st2_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st2_set_real before ww_st2_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_ST2_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1 || extent[0] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_st2_set_real: bad array id, rank, extent or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    Registered& r = it->second.h[id];
    r.lbound = lbound[0];
    r.v.assign(data, data + extent[0]);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st2_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st2_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st2_commit before ww_st2_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    const int nk = g.p.nk, ns = g.p.nspec;
    std::string why;
    // SIG(NK+1) is read by W3SDS2 (w3src2md.F90:721); SIG is (0:MK+1), MK >= NK.
    if (!slice_to_device(g.h[WW_ST2_A_SIG], 1, nk + 1, "SIG", g.sig, why) ||
        !slice_to_device(g.h[WW_ST2_A_SIG2], 1, ns, "SIG2", g.sig2, why) ||
        !slice_to_device(g.h[WW_ST2_A_DDEN], 1, nk, "DDEN", g.dden, why) ||
        !slice_to_device(g.h[WW_ST2_A_ECOS], 1, ns, "ECOS", g.ecos, why) ||
        !slice_to_device(g.h[WW_ST2_A_ESIN], 1, ns, "ESIN", g.esin, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_st2_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    build_module_tables(module_ctx());
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st2_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_spr2(int igrid, int npts, const float* a, const float* cg, const float* wn,
             const float* depth, const float* fpi, const float* u, const float* ustar,
             float* emean, float* fmean, float* wnmean, float* amax, float* alfa, float* fp) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_spr2");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const DevReal2D A = ex.in(*g, "spr2.a", a, ns, npts);
    const DevReal2D CG = ex.in(*g, "spr2.cg", cg, nk, npts);
    const DevReal2D WN = ex.in(*g, "spr2.wn", wn, nk, npts);
    const DevReal2D DP = ex.in(*g, "spr2.depth", depth, 1, npts);
    const DevReal2D FPI = ex.in(*g, "spr2.fpi", fpi, 1, npts);
    const DevReal2D U = ex.in(*g, "spr2.u", u, 1, npts);
    const DevReal2D US = ex.in(*g, "spr2.ustar", ustar, 1, npts);
    // EMEAN, FMEAN, WNMEAN, AMAX, ALFA, FP are INTENT(OUT), assigned on every path.
    const DevReal2D ALFA = ex.buf(*g, "spr2.alfa", nk, npts);
    const DevReal2D O = ex.buf(*g, "spr2.out", 5, npts);
    const DevReal2D EB = ex.buf(*g, "spr2.eb", nk, npts);
    const ww::srce::Params p = g->p;
    const Scalars c = g->c;
    const DevReal1D sig = g->sig, dden = g->dden;
    ex.for_points("ww_st2.spr2", npts, KOKKOS_LAMBDA(const int ip) {
      Real* o = col(O, ip);
      ww::srce::st2::w3spr2_point(col(A, ip), col(CG, ip), col(WN, ip), DP(0, ip), FPI(0, ip),
                                  U(0, ip), US(0, ip), o[0], o[1], o[2], o[3], col(ALFA, ip),
                                  o[4], p, c.dth, sig.data(), dden.data(), c.fte, c.ftf,
                                  c.ftwn, col(EB, ip));
    });
    ex.out(alfa, ALFA, nk, npts);
    auto ho = Kokkos::create_mirror_view(Kokkos::HostSpace(), cols(O, npts));
    ex.copy(ho, cols(O, npts));
    for (int ip = 0; ip < npts; ++ip) {
      emean[ip] = ho(0, ip);
      fmean[ip] = ho(1, ip);
      wnmean[ip] = ho(2, ip);
      amax[ip] = ho(3, ip);
      fp[ip] = ho(4, ip);
    }
    g_count_spr2.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr2: unknown exception");
  }
}

void ww_sin2(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* udir, const float* cd, const float* z0, float* fpi,
             float* s, float* d) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sin2");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const DevReal2D A = ex.in(*g, "sin2.a", a, ns, npts);
    const DevReal2D CG = ex.in(*g, "sin2.cg", cg, nk, npts);
    const DevReal2D K = ex.in(*g, "sin2.k", k, ns, npts);
    const DevReal2D U = ex.in(*g, "sin2.u", u, 1, npts);
    const DevReal2D UD = ex.in(*g, "sin2.udir", udir, 1, npts);
    const DevReal2D CD = ex.in(*g, "sin2.cd", cd, 1, npts);
    const DevReal2D Z0 = ex.in(*g, "sin2.z0", z0, 1, npts);
    // FPI, S, D are INTENT(OUT), assigned on every path.
    const DevReal2D FPI = ex.buf(*g, "sin2.fpi", 1, npts);
    const DevReal2D S = ex.buf(*g, "sin2.s", ns, npts);
    const DevReal2D D = ex.buf(*g, "sin2.d", ns, npts);
    const DevReal2D W = ex.buf(*g, "sin2.sin1a", nk, npts);
    const ww::srce::Params p = g->p;
    const Scalars c = g->c;
    const ModuleCtx& m = module_ctx();
    const Real dsiga = m.dsiga, ddrag = m.ddrag;
    const DevReal1D sig = g->sig, sig2 = g->sig2, dden = g->dden, ecos = g->ecos,
                    esin = g->esin, betatb = m.betatb;
    ex.for_points("ww_st2.sin2", npts, KOKKOS_LAMBDA(const int ip) {
      ww::srce::st2::w3sin2_point(col(A, ip), col(CG, ip), col(K, ip), U(0, ip), UD(0, ip),
                                  CD(0, ip), Z0(0, ip), FPI(0, ip), col(S, ip), col(D, ip), p,
                                  sig.data(), sig2.data(), dden.data(), ecos.data(),
                                  esin.data(), c.xfr, c.fte, c.fttr, c.fpimin, c.zwind,
                                  c.fswell, dsiga, ddrag, betatb.data(), col(W, ip));
    });
    ex.out(fpi, FPI, 1, npts);
    ex.out(s, S, ns, npts);
    ex.out(d, D, ns, npts);
    g_count_sin2.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sin2: unknown exception");
  }
}

void ww_sds2(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* fpi, const float* ustar, const float* alfa, float* s, float* d) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sds2");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const DevReal2D A = ex.in(*g, "sds2.a", a, ns, npts);
    const DevReal2D CG = ex.in(*g, "sds2.cg", cg, nk, npts);
    const DevReal2D K = ex.in(*g, "sds2.k", k, nk, npts);
    const DevReal2D FPI = ex.in(*g, "sds2.fpi", fpi, 1, npts);
    const DevReal2D US = ex.in(*g, "sds2.ustar", ustar, 1, npts);
    const DevReal2D AL = ex.in(*g, "sds2.alfa", alfa, nk, npts);
    // S, D are INTENT(OUT), assigned on every path.
    const DevReal2D S = ex.buf(*g, "sds2.s", ns, npts);
    const DevReal2D D = ex.buf(*g, "sds2.d", ns, npts);
    const DevReal2D W = ex.buf(*g, "sds2.ebd", nk, npts);
    const ww::srce::Params p = g->p;
    const Scalars c = g->c;
    const DevReal1D sig = g->sig, dden = g->dden;
    ex.for_points("ww_st2.sds2", npts, KOKKOS_LAMBDA(const int ip) {
      ww::srce::st2::w3sds2_point(col(A, ip), col(CG, ip), col(K, ip), FPI(0, ip), US(0, ip),
                                  col(AL, ip), col(S, ip), col(D, ip), p, sig.data(),
                                  dden.data(), c.dth, c.fte, c.fpimin, c.xf1, c.xf2, c.xfh,
                                  c.sdsaln, c.cdsa0, c.cdsa1, c.cdsa2, c.cdsb0, c.cdsb1,
                                  c.cdsb2, c.cdsb3, col(W, ip));
    });
    ex.out(s, S, ns, npts);
    ex.out(d, D, ns, npts);
    g_count_sds2.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sds2: unknown exception");
  }
}

int ww_st2_enabled(int which) {
  const Flags& f = flags();
  switch (which) {
    case 1: return f.spr2 ? 1 : 0;
    case 2: return f.sin2 ? 1 : 0;
    case 3: return f.sds2 ? 1 : 0;
    default: return 0;
  }
}

int ww_st2_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
