// kokkos/src/fortran_iface/st3_shim.cpp
// The bind(C) shim of ST3: the C ABI of ww_kokkos_c.hpp ("ST3") over the per-point
// device functions of ww_kokkos/srce_st3_spr3.hpp, srce_st3_sin3.hpp and
// srce_st3_sds3.hpp (W3SPR3, W3SIN3, W3SDS3), with the tables of
// srce_st3_insin3.hpp (INSIN3). Marshalling only.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT, IGRID at
// W3SRCE); nothing throws across the boundary; Views die inside Kokkos::finalize (a
// finalize hook); inside an OpenMP parallel region (W3SRCE under W3_OMPG) the
// kernels run on Kokkos::Serial. Phase 1 copies in, runs, copies out on every call,
// and the caller serialises the calls (!$OMP CRITICAL in the caller patch).
//
// TAUT, DELTAUW, DELU, TAUHFT, DELUST, DELALP are W3SRC3MD module state
// (w3src3md.F90:97-99), filled once per process by INSIN3 from W3IOGR
// (w3iogrmd.F90:1531-1534, SAVE FLINP) with the parameters of the first grid read.
// The shim keeps one copy per process and builds it with the C++ INSIN3 at the
// first ww_st3_commit(), from that grid; a per-grid copy would differ from WW3 in
// ww3_multi.
//
// W3_STAB3 changes W3SIN3 only. The shim is built once for every switch file, so
// the Fortran setup passes the switch in IPAR and ww_sin3 picks the kernel
// instantiation at run time.
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
#include "ww_kokkos/srce_st3_insin3.hpp"
#include "ww_kokkos/srce_st3_sds3.hpp"
#include "ww_kokkos/srce_st3_sin3.hpp"
#include "ww_kokkos/srce_st3_spr3.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::st3shim {
namespace {

using ww::Real;
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using DevInt2D = Kokkos::View<int**, Kokkos::LayoutLeft, DeviceSpace>;
template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// A registered REAL array as the Fortran has it: its LBOUND and its values.
struct Registered {
  int lbound = 0;
  std::vector<Real> v;
};

/// The W3GDATMD scalars the routines and INSIN3 read, under their names in lower case.
struct Scalars {
  Real dth, zzwnd, aalpha, bbeta, zz0max, zzalp, ssinthp, sswellf1, ssdsc1, ddelta1,
      ddelta2, wwnmeanp, wwnmeanptail, fte, sstxftf, sstxftwn, sstxftftail;
};

struct GridCtx {
  srce::Params p{};  ///< nk, nth, nspec; the rest unused by ST3
  Scalars c{};
  bool stab3 = false;
  Registered h[WW_ST3_NARRAYS];
  DevReal1D sig;    ///< SIG(1:NK) on the device
  DevReal1D sig2;   ///< SIG2(1:NSPEC)
  DevReal1D dden;   ///< DDEN(1:NK)
  DevReal1D dden2;  ///< DDEN2(1:NSPEC)
  DevReal1D ecos, esin;  ///< ECOS, ESIN(1:NSPEC)
  std::map<std::string, DevReal2D> buf;
  std::map<std::string, DevInt2D> ibuf;
  bool configured = false;
  bool ready = false;
};

/// W3SRC3MD's module state: one per process.
struct ModuleCtx {
  DevReal1D taut;    ///< TAUT(0:ITAUMAX,0:JUMAX), srce_st3_tabu_stress.hpp's layout
  DevReal1D tauhft;  ///< TAUHFT(0:IUSTAR,0:IALPHA), srce_st3_tabu_tauhf.hpp's layout
  Real delu = 0, deltauw = 0, delust = 0, delalp = 0;
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
std::atomic<long long> g_count_spr3{0}, g_count_sin3{0}, g_count_sds3{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST3: %s (code %d)\n", what, code);
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
  bool spr3, sin3, sds3, count;
};
const Flags& flags() {
  static const Flags f{env_is_one("WW_KOKKOS_SPR3"), env_is_one("WW_KOKKOS_SIN3"),
                       env_is_one("WW_KOKKOS_SDS3"), env_is_one("WW_KOKKOS_ST3_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SPR3 spectra computed: %lld\n", g_count_spr3.load());
  std::fprintf(stderr, "ww_kokkos: W3SIN3 spectra computed: %lld\n", g_count_sin3.load());
  std::fprintf(stderr, "ww_kokkos: W3SDS3 spectra computed: %lld\n", g_count_sds3.load());
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
  DevReal1D d(std::string("ww_st3.") + name, static_cast<size_t>(n));
  auto hm = Kokkos::create_mirror_view(d);
  std::memcpy(hm.data(), r.v.data() + first, sizeof(Real) * static_cast<size_t>(n));
  Kokkos::deep_copy(d, hm);
  out = d;
  return true;
}

/// INSIN3 (w3src3md.F90:726-813), once per process, from grid g (the first committed).
void build_module_tables(ModuleCtx& m, const GridCtx& g) {
  if (m.built) return;
  constexpr int kTauhfSize =
      (ww::srce::st3::kTauhfIustar + 1) * (ww::srce::st3::kTauhfIalpha + 1);
  DevReal1D t("ww_st3.taut", static_cast<size_t>(ww::srce::st3::kTautSize));
  DevReal1D h("ww_st3.tauhft", static_cast<size_t>(kTauhfSize));
  auto ht = Kokkos::create_mirror_view(t);
  auto hh = Kokkos::create_mirror_view(h);
  // SIG(1:NK) of the grid, checked by the caller.
  const Registered& sig = g.h[WW_ST3_A_SIG];
  const Real* sig1 = sig.v.data() + (1 - sig.lbound);
  const ww::srce::st3::Insin3Steps s = ww::srce::st3::insin3(
      g.p, sig1, g.c.zzwnd, g.c.aalpha, g.c.zz0max, g.c.bbeta, g.c.zzalp, ht.data(), hh.data());
  Kokkos::deep_copy(t, ht);
  Kokkos::deep_copy(h, hh);
  m.taut = t;
  m.tauhft = h;
  m.delu = s.delu;
  m.deltauw = s.deltauw;
  m.delust = s.delust;
  m.delalp = s.delalp;
  m.built = true;
}

auto cols(const DevReal2D& v, int npts) {
  return Kokkos::subview(v, Kokkos::ALL, Kokkos::make_pair(0, npts));
}
auto cols(const DevInt2D& v, int npts) {
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
  template <class V>
  V& grow(std::map<std::string, V>& m, const std::string& name, int rows, int npts) const {
    V& v = m[name];
    if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
      v = V();
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested) {
        v = V(Kokkos::view_alloc(Kokkos::Serial(), "ww_st3." + name), rows, npts);
        return v;
      }
#endif
      v = V(Kokkos::view_alloc("ww_st3." + name), rows, npts);
    }
    return v;
  }
  DevReal2D& buf(GridCtx& g, const std::string& name, int rows, int npts) const {
    return grow(g.buf, name, rows, npts);
  }
  DevInt2D& ibuf(GridCtx& g, const std::string& name, int rows, int npts) const {
    return grow(g.ibuf, name, rows, npts);
  }
  /// The device buffer `name`, filled from the caller's (rows, npts) array.
  DevReal2D in(GridCtx& g, const std::string& name, const Real* p, int rows, int npts) const {
    DevReal2D v = buf(g, name, rows, npts);
    copy(cols(v, npts), Borrowed2D<const Real>(p, rows, npts));
    return v;
  }
  DevInt2D iin(GridCtx& g, const std::string& name, const int* p, int rows, int npts) const {
    DevInt2D v = ibuf(g, name, rows, npts);
    copy(cols(v, npts), Borrowed2D<const int>(p, rows, npts));
    return v;
  }
  /// The first npts columns of a device buffer, back to the caller's array.
  void out(Real* p, const DevReal2D& v, int rows, int npts) const {
    copy(Borrowed2D<Real>(p, rows, npts), cols(v, npts));
  }
  void iout(int* p, const DevInt2D& v, int rows, int npts) const {
    copy(Borrowed2D<int>(p, rows, npts), cols(v, npts));
  }
};

KOKKOS_INLINE_FUNCTION Real* col(const DevReal2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}
KOKKOS_INLINE_FUNCTION int* col(const DevInt2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

/// W3SIN3 over the points, as a functor so that the STAB3 instantiation is a
/// template argument of the kernel (no generic lambda around KOKKOS_LAMBDA).
template <bool kStab3>
struct Sin3Points {
  DevReal2D A, CG, K, U, US, DR, AS, UD, Z0, TX, TY, TNX, TNY, S, D, W;
  DevInt2D LW;
  ww::srce::Params p;
  DevReal1D sig, sig2, dden2, ecos, esin, tauhft;
  Real dth, bbeta, zzalp, sswellf1, ssinthp, aalpha, delust, delalp;
  KOKKOS_INLINE_FUNCTION void operator()(const int ip) const {
    // CD and ICE are never read by W3SIN3 (srce_st3_sin3.hpp); IX, IY neither.
    ww::srce::st3::w3sin3_point<kStab3>(
        col(A, ip), col(CG, ip), col(K, ip), U(0, ip), US(0, ip), DR(0, ip), AS(0, ip),
        UD(0, ip), Z0(0, ip), static_cast<Real>(0.), TX(0, ip), TY(0, ip), TNX(0, ip),
        TNY(0, ip), static_cast<Real>(0.), col(S, ip), col(D, ip), col(LW, ip), 0, 0, p,
        sig.data(), sig2.data(), dden2.data(), ecos.data(), esin.data(), dth, bbeta, zzalp,
        sswellf1, ssinthp, aalpha, delust, delalp, tauhft.data(), col(W, ip));
  }
};

/// The common checks of a compute entry point. Returns the grid, or nullptr
/// after recording the error.
GridCtx* compute_ctx(int igrid, int npts, const char* who) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready || !module_ctx().built ||
      !Kokkos::is_initialized()) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
              (std::string(who) + " before ww_st3_commit for this grid").c_str());
    return nullptr;
  }
  if (npts < 0) {
    set_error(WW_KOKKOS_ERR_BAD_SHAPE, (std::string(who) + ": npts is negative").c_str());
    return nullptr;
  }
  return &it->second;
}

}  // namespace
}  // namespace ww::st3shim

using namespace ww::st3shim;
using ww::Real;

extern "C" {

int ww_st3_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st3_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_ST3_NRPAR || nipar != WW_ST3_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st3_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const int nk = ipar[WW_ST3_I_NK], nth = ipar[WW_ST3_I_NTH], nspec = ipar[WW_ST3_I_NSPEC];
    const int stab3 = ipar[WW_ST3_I_STAB3];
    // nk >= 1 is the kernels' precondition (they read SIG(NK), CG(NK), EB(NK)).
    if (nk <= 0 || nth <= 0 || nspec != nk * nth || (stab3 != 0 && stab3 != 1)) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st3_init: NK, NTH, NSPEC or STAB3 inconsistent");
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
    g.stab3 = stab3 == 1;
    Scalars& c = g.c;
    c.dth = rpar[WW_ST3_R_DTH];
    c.zzwnd = rpar[WW_ST3_R_ZZWND];
    c.aalpha = rpar[WW_ST3_R_AALPHA];
    c.bbeta = rpar[WW_ST3_R_BBETA];
    c.zz0max = rpar[WW_ST3_R_ZZ0MAX];
    c.zzalp = rpar[WW_ST3_R_ZZALP];
    c.ssinthp = rpar[WW_ST3_R_SSINTHP];
    c.sswellf1 = rpar[WW_ST3_R_SSWELLF1];
    c.ssdsc1 = rpar[WW_ST3_R_SSDSC1];
    c.ddelta1 = rpar[WW_ST3_R_DDELTA1];
    c.ddelta2 = rpar[WW_ST3_R_DDELTA2];
    c.wwnmeanp = rpar[WW_ST3_R_WWNMEANP];
    c.wwnmeanptail = rpar[WW_ST3_R_WWNMEANPTAIL];
    c.fte = rpar[WW_ST3_R_FTE];
    c.sstxftf = rpar[WW_ST3_R_SSTXFTF];
    c.sstxftwn = rpar[WW_ST3_R_SSTXFTWN];
    c.sstxftftail = rpar[WW_ST3_R_SSTXFTFTAIL];
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st3_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st3_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st3_set_real before ww_st3_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_ST3_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1 || extent[0] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_st3_set_real: bad array id, rank, extent or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    Registered& r = it->second.h[id];
    r.lbound = lbound[0];
    r.v.assign(data, data + extent[0]);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st3_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st3_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st3_commit before ww_st3_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    const int nk = g.p.nk, ns = g.p.nspec;
    std::string why;
    if (!slice_to_device(g.h[WW_ST3_A_SIG], 1, nk, "SIG", g.sig, why) ||
        !slice_to_device(g.h[WW_ST3_A_SIG2], 1, ns, "SIG2", g.sig2, why) ||
        !slice_to_device(g.h[WW_ST3_A_DDEN], 1, nk, "DDEN", g.dden, why) ||
        !slice_to_device(g.h[WW_ST3_A_DDEN2], 1, ns, "DDEN2", g.dden2, why) ||
        !slice_to_device(g.h[WW_ST3_A_ECOS], 1, ns, "ECOS", g.ecos, why) ||
        !slice_to_device(g.h[WW_ST3_A_ESIN], 1, ns, "ESIN", g.esin, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_st3_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    build_module_tables(module_ctx(), g);
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st3_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_spr3(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* fmeans, float* wnmean, float* amax,
             const float* u, const float* udir, float* ustar, float* usdir,
             const float* tauwx, const float* tauwy, float* cd, float* z0, float* charn,
             const int* llws, float* fmeanws) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_spr3");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const DevReal2D A = ex.in(*g, "spr3.a", a, ns, npts);
    const DevReal2D CG = ex.in(*g, "spr3.cg", cg, nk, npts);
    const DevReal2D WN = ex.in(*g, "spr3.wn", wn, nk, npts);
    const DevReal2D U = ex.in(*g, "spr3.u", u, 1, npts);
    const DevReal2D UD = ex.in(*g, "spr3.udir", udir, 1, npts);
    const DevReal2D TX = ex.in(*g, "spr3.tauwx", tauwx, 1, npts);
    const DevReal2D TY = ex.in(*g, "spr3.tauwy", tauwy, 1, npts);
    const DevInt2D LW = ex.iin(*g, "spr3.llws", llws, ns, npts);
    // USTAR, USDIR are INTENT(INOUT): copied in and out.
    const DevReal2D US = ex.in(*g, "spr3.ustar", ustar, 1, npts);
    const DevReal2D USD = ex.in(*g, "spr3.usdir", usdir, 1, npts);
    // The nine INTENT(OUT) scalars, assigned on every path, as rows of one buffer.
    const DevReal2D O = ex.buf(*g, "spr3.out", 9, npts);
    const DevReal2D W = ex.buf(*g, "spr3.work", ww::srce::st3::spr3_work_reals(g->p), npts);
    const ww::srce::Params p = g->p;
    const Scalars c = g->c;
    const ModuleCtx& m = module_ctx();
    const Real deltauw = m.deltauw, delu = m.delu;
    const DevReal1D sig = g->sig, dden = g->dden, taut = m.taut;
    ex.for_points("ww_st3.spr3", npts, KOKKOS_LAMBDA(const int ip) {
      Real* o = col(O, ip);
      ww::srce::st3::w3spr3_point(p, col(A, ip), col(CG, ip), col(WN, ip), o[0], o[1], o[2],
                                  o[3], o[4], U(0, ip), UD(0, ip), US(0, ip), USD(0, ip),
                                  TX(0, ip), TY(0, ip), o[5], o[6], o[7], col(LW, ip), o[8],
                                  sig.data(), dden.data(), c.wwnmeanp, c.wwnmeanptail, c.fte,
                                  c.sstxftf, c.sstxftwn, c.sstxftftail, taut.data(), deltauw,
                                  delu, c.zzwnd, c.aalpha, col(W, ip));
    });
    ex.out(ustar, US, 1, npts);
    ex.out(usdir, USD, 1, npts);
    auto ho = Kokkos::create_mirror_view(Kokkos::HostSpace(), cols(O, npts));
    ex.copy(ho, cols(O, npts));
    for (int ip = 0; ip < npts; ++ip) {
      emean[ip] = ho(0, ip);
      fmean[ip] = ho(1, ip);
      fmeans[ip] = ho(2, ip);
      wnmean[ip] = ho(3, ip);
      amax[ip] = ho(4, ip);
      cd[ip] = ho(5, ip);
      z0[ip] = ho(6, ip);
      charn[ip] = ho(7, ip);
      fmeanws[ip] = ho(8, ip);
    }
    g_count_spr3.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr3: unknown exception");
  }
}

void ww_sin3(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* ustar, const float* drat, const float* as,
             const float* usdir, const float* z0, float* tauwx, float* tauwy, float* tauwnx,
             float* tauwny, float* s, float* d, int* llws) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sin3");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const ModuleCtx& m = module_ctx();
    const Scalars c = g->c;
    auto fill = [&](auto& f) {
      f.A = ex.in(*g, "sin3.a", a, ns, npts);
      f.CG = ex.in(*g, "sin3.cg", cg, nk, npts);
      f.K = ex.in(*g, "sin3.k", k, ns, npts);
      f.U = ex.in(*g, "sin3.u", u, 1, npts);
      f.US = ex.in(*g, "sin3.ustar", ustar, 1, npts);
      f.DR = ex.in(*g, "sin3.drat", drat, 1, npts);
      f.AS = ex.in(*g, "sin3.as", as, 1, npts);
      f.UD = ex.in(*g, "sin3.usdir", usdir, 1, npts);
      f.Z0 = ex.in(*g, "sin3.z0", z0, 1, npts);
      // TAUWX, TAUWY, TAUWNX, TAUWNY, S, D, LLWS are INTENT(OUT), assigned on every path.
      f.TX = ex.buf(*g, "sin3.tauwx", 1, npts);
      f.TY = ex.buf(*g, "sin3.tauwy", 1, npts);
      f.TNX = ex.buf(*g, "sin3.tauwnx", 1, npts);
      f.TNY = ex.buf(*g, "sin3.tauwny", 1, npts);
      f.S = ex.buf(*g, "sin3.s", ns, npts);
      f.D = ex.buf(*g, "sin3.d", ns, npts);
      f.W = ex.buf(*g, "sin3.dstab", ww::srce::st3::sin3_work_reals(g->p), npts);
      f.LW = ex.ibuf(*g, "sin3.llws", ns, npts);
      f.p = g->p;
      f.sig = g->sig;
      f.sig2 = g->sig2;
      f.dden2 = g->dden2;
      f.ecos = g->ecos;
      f.esin = g->esin;
      f.tauhft = m.tauhft;
      f.dth = c.dth;
      f.bbeta = c.bbeta;
      f.zzalp = c.zzalp;
      f.sswellf1 = c.sswellf1;
      f.ssinthp = c.ssinthp;
      f.aalpha = c.aalpha;
      f.delust = m.delust;
      f.delalp = m.delalp;
      ex.for_points("ww_st3.sin3", npts, f);
      ex.out(tauwx, f.TX, 1, npts);
      ex.out(tauwy, f.TY, 1, npts);
      ex.out(tauwnx, f.TNX, 1, npts);
      ex.out(tauwny, f.TNY, 1, npts);
      ex.out(s, f.S, ns, npts);
      ex.out(d, f.D, ns, npts);
      ex.iout(llws, f.LW, ns, npts);
    };
    if (g->stab3) {
      Sin3Points<true> f;
      fill(f);
    } else {
      Sin3Points<false> f;
      fill(f);
    }
    g_count_sin3.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sin3: unknown exception");
  }
}

void ww_sds3(int igrid, int npts, const float* a, const float* k, const float* emean,
             const float* fmean, const float* wnmean, float* s, float* d) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sds3");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const DevReal2D A = ex.in(*g, "sds3.a", a, ns, npts);
    const DevReal2D K = ex.in(*g, "sds3.k", k, nk, npts);
    const DevReal2D EM = ex.in(*g, "sds3.emean", emean, 1, npts);
    const DevReal2D FM = ex.in(*g, "sds3.fmean", fmean, 1, npts);
    const DevReal2D WM = ex.in(*g, "sds3.wnmean", wnmean, 1, npts);
    // S, D are INTENT(OUT), assigned on every path.
    const DevReal2D S = ex.buf(*g, "sds3.s", ns, npts);
    const DevReal2D D = ex.buf(*g, "sds3.d", ns, npts);
    const ww::srce::Params p = g->p;
    const Scalars c = g->c;
    ex.for_points("ww_st3.sds3", npts, KOKKOS_LAMBDA(const int ip) {
      // CG, USTAR, USDIR, DEPTH, IX, IY are never read by W3SDS3 (srce_st3_sds3.hpp).
      const Real zero = static_cast<Real>(0.);
      ww::srce::st3::w3sds3_point(col(A, ip), col(K, ip), nullptr, EM(0, ip), FM(0, ip),
                                  WM(0, ip), zero, zero, zero, col(S, ip), col(D, ip), 0, 0, p,
                                  c.ssdsc1, c.ddelta1, c.ddelta2);
    });
    ex.out(s, S, ns, npts);
    ex.out(d, D, ns, npts);
    g_count_sds3.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sds3: unknown exception");
  }
}

int ww_st3_enabled(int which) {
  const Flags& f = flags();
  switch (which) {
    case 1: return f.spr3 ? 1 : 0;
    case 2: return f.sin3 ? 1 : 0;
    case 3: return f.sds3 ? 1 : 0;
    default: return 0;
  }
}

int ww_st3_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
