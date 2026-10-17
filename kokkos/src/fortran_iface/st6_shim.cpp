// kokkos/src/fortran_iface/st6_shim.cpp
// The bind(C) shim of ST6: the C ABI of ww_kokkos_c.hpp ("ST6") over the per-point
// device functions of ww_kokkos/srce_st6_spr6.hpp, srce_st6_sin6.hpp (with LFACTOR
// and TAU_WAVE_ATMOS) and srce_st6_sds6.hpp (W3SPR6, W3SIN6, W3SDS6). Marshalling only.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT, IGRID at
// W3SRCE); nothing throws across the boundary; Views die inside Kokkos::finalize (a
// finalize hook); inside an OpenMP parallel region (W3SRCE under W3_OMPG) the
// kernels run on Kokkos::Serial. Phase 1 copies in, runs, copies out on every call,
// and the caller serialises the calls (!$OMP CRITICAL in the caller patch).
//
// IKN, MK10Hz, IK10Hz, SIG10Hz and DSII10Hz are W3SRC6MD's ALLOCATABLE, SAVE module
// arrays (w3src6md.F90:103-105), allocated and filled on the first call that needs
// them and never again: IKN by the first W3SIN6 or W3SDS6 (:449-456, :696-704) from
// that call's NSPEC and NTH, the 10 Hz tables by the first LFACTOR, inside the first
// W3SIN6 (:955-1013), from that call's SIG2(IKN), DSII2(IKN) and XFR. The shim keeps
// one copy of each per process and builds it at the same first call, from the grid
// of that call; a per-grid copy would differ from WW3 in ww3_multi. The Fortran
// declares them THREADPRIVATE under W3_OMPG (the `#ifdef W3_OMPG || W3_OMPH` at :106
// tests W3_OMPG alone), one copy per thread, all built from the first grid a thread
// meets; the shim keeps one, which is the same tables whenever every thread's first
// ST6 call is on the same grid.
//
// DSII10Hz is also rewritten after the build: TAU_WAVE_ATMOS, on every call where
// NK < NK10Hz, sets it again from SIG10Hz and the calling grid's XFR (:1294-1297),
// and the next LFACTOR of any grid reads it. The kernel computes that rewrite into its
// own work (tau_wave_atmos(), dsii10hz_w); rewrite_dsii10hz() applies it to the one
// table after each ww_sin6, so a ww3_multi grid with another XFR leaves the table as
// the Fortran does.
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
#include "ww_kokkos/srce_st6_lfactor.hpp"
#include "ww_kokkos/srce_st6_sds6.hpp"
#include "ww_kokkos/srce_st6_sin6.hpp"
#include "ww_kokkos/srce_st6_spr6.hpp"
#include "ww_kokkos/srce_st6_util.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::st6shim {
namespace {

using ww::Real;
namespace st6 = ww::srce::st6;
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevInt1D = Kokkos::View<int*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using Borrowed2D = Kokkos::View<const Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
using Host2D = Kokkos::View<Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                            Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// A registered REAL array as the Fortran has it: its LBOUND and its values.
struct Registered {
  int lbound = 0;
  std::vector<Real> v;
};

struct GridCtx {
  srce::Params p{};  ///< nk, nth, nspec; the rest unused by ST6
  Real dth = 0, xfr = 0, fte = 0, ftf = 0, ftwn = 0;
  Real sin6a0 = 0, sin6ws = 0, sin6chkmin = 0, sin6chkcap = 0, sin6chkinf = 0,
       sin6chksig = 0, sds6a1 = 0, sds6a2 = 0;
  bool sin6flcap = false, sds6et = false;
  int sds6p1 = 0, sds6p2 = 0;
  Registered h[WW_ST6_NARRAYS];
  std::vector<Real> sig2_h, dden2_h;  ///< SIG2(1:NSPEC), DDEN2(1:NSPEC) for the tables
  DevReal1D sig, dden, dsii;          ///< (1:NK)
  DevReal1D sig2, dden2, ecos, esin;  ///< (1:NSPEC)
  std::map<std::string, DevReal2D> buf;
  DevInt1D rc;  ///< the kernel's return code per point
  bool configured = false;
  bool ready = false;
};

/// IKN of W3SRC6MD (:104). built is ALLOCATED(IKN).
struct IknCtx {
  bool built = false;
  std::vector<int> h;  ///< IRANGE(1,NSPEC,NTH) of the first calling grid, 1-based values
  DevInt1D d;
};

/// MK10Hz, SIG10Hz, DSII10Hz of W3SRC6MD (:103, :105). built is ALLOCATED(IK10Hz).
/// IK10Hz itself is read only by the build.
struct TenHzCtx {
  bool built = false;
  int mk10hz = 0;
  std::vector<Real> sig10hz_h;  ///< SIG10Hz on the host, for rewrite_dsii10hz()
  DevReal1D sig10hz, dsii10hz;
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}
IknCtx& ikn_ctx() {
  static IknCtx m;
  return m;
}
TenHzCtx& tenhz_ctx() {
  static TenHzCtx m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_spr6{0}, g_count_sin6{0}, g_count_sds6{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST6: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() {
  grids().clear();
  ikn_ctx() = IknCtx{};
  tenhz_ctx() = TenHzCtx{};
}

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool spr6, sin6, sds6, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SPR6") != 0), (ww_kokkos_flag("WW_KOKKOS_SIN6") != 0),
                       (ww_kokkos_flag("WW_KOKKOS_SDS6") != 0), env_is_one("WW_KOKKOS_ST6_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SPR6 spectra computed: %lld\n", g_count_spr6.load());
  std::fprintf(stderr, "ww_kokkos: W3SIN6 spectra computed: %lld\n", g_count_sin6.load());
  std::fprintf(stderr, "ww_kokkos: W3SDS6 spectra computed: %lld\n", g_count_sds6.load());
}

bool nested_in_openmp() {
#if defined(WW_SRCE_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// Elements lb..lb+n-1 (Fortran indices) of a registered array; false when the
/// array does not cover them.
bool slice(const Registered& r, int lb, int n, const char* name, const char* range,
           std::vector<Real>& out, std::string& why) {
  const long first = static_cast<long>(lb) - r.lbound;
  if (r.v.empty() || first < 0 || first + n > static_cast<long>(r.v.size())) {
    why = std::string(name) + (r.v.empty() ? " not registered" : " does not cover ") +
          (r.v.empty() ? "" : range);
    return false;
  }
  out.assign(r.v.begin() + first, r.v.begin() + first + n);
  return true;
}

DevReal1D to_device(const std::vector<Real>& h, const char* name) {
  DevReal1D d(std::string("ww_st6.") + name, h.size());
  auto hm = Kokkos::create_mirror_view(d);
  if (!h.empty()) std::memcpy(hm.data(), h.data(), sizeof(Real) * h.size());
  Kokkos::deep_copy(d, hm);
  return d;
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
  template <class V>
  V alloc(const std::string& label, size_t n0) const {
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested) return V(Kokkos::view_alloc(Kokkos::Serial(), label), n0);
#endif
    return V(Kokkos::view_alloc(label), n0);
  }
  /// A (rows, >= npts) device buffer of this grid, grown, never shrunk.
  DevReal2D& buf(GridCtx& g, const std::string& name, int rows, int npts) const {
    DevReal2D& v = g.buf[name];
    if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
      v = DevReal2D();
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested) {
        v = DevReal2D(Kokkos::view_alloc(Kokkos::Serial(), "ww_st6." + name), rows, npts);
        return v;
      }
#endif
      v = DevReal2D(Kokkos::view_alloc("ww_st6." + name), rows, npts);
    }
    return v;
  }
  DevInt1D& rc(GridCtx& g, int npts) const {
    if (g.rc.extent(0) < static_cast<size_t>(npts)) {
      g.rc = DevInt1D();
      g.rc = alloc<DevInt1D>("ww_st6.rc", static_cast<size_t>(npts));
    }
    return g.rc;
  }
};

auto cols(const DevReal2D& v, int npts) {
  return Kokkos::subview(v, Kokkos::ALL, Kokkos::make_pair(0, npts));
}

KOKKOS_INLINE_FUNCTION Real* col(const DevReal2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

/// The per-point return codes, on the host.
std::vector<int> codes(const GridCtx& g, int npts, const Exec& ex) {
  std::vector<int> out(static_cast<size_t>(npts), 0);
  Kokkos::View<int*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(out.data(),
                                                                                    out.size());
  ex.copy(h, Kokkos::subview(g.rc, Kokkos::make_pair(0, npts)));
  return out;
}

/// IF (.NOT.ALLOCATED(IKN)) IKN = IRANGE(1,NSPEC,NTH), then the SIZE check
/// (:449-456, :696-704), with the calling grid. Returns 0 or WW_ST6_IKN with info set.
/// The table counts as built either way: the Fortran has allocated it before it stops.
int build_ikn(const srce::Params& p, const Exec& ex, int* info) {
  IknCtx& t = ikn_ctx();
  if (t.built) return 0;
  const int n = st6::range_size(1, p.nspec, p.nth);
  t.h.assign(static_cast<size_t>(n > 0 ? n : 1), 0);
  t.built = true;
  if (st6::w3sds6_ikn(p, t.h.data()) != 0) {
    t.h.resize(static_cast<size_t>(n));
    info[0] = n;
    info[1] = p.nk;
    return WW_ST6_IKN;
  }
  t.d = ex.alloc<DevInt1D>("ww_st6.ikn", t.h.size());
  Kokkos::View<const int*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> hv(
      t.h.data(), t.h.size());
  ex.copy(t.d, hv);
  return 0;
}

/// A later grid reads IKN of the grid that built it. Where SIZE(IKN) /= NK, or
/// IKN+ITH-1 runs past this grid's NSPEC, the Fortran's array statements are
/// non-conforming or out of bounds (:457-465, :706, :749-751): refused, not guessed.
bool ikn_fits(const srce::Params& p) {
  const IknCtx& t = ikn_ctx();
  const int n = static_cast<int>(t.h.size());
  return t.d.extent(0) > 0 && n == p.nk && t.h[static_cast<size_t>(n - 1)] + p.nth - 1 <= p.nspec;
}

/// The 10 Hz tables of the first LFACTOR call: lfactor_tables() with the SIG and
/// DSII W3SIN6 passes it (:457-459). Returns 0 or WW_ST6_LF_IK10HZ with info set.
int build_tenhz(const GridCtx& g, const Exec& ex, int* info) {
  TenHzCtx& t = tenhz_ctx();
  if (t.built) return 0;
  const int nk = g.p.nk;
  std::vector<Real> sig(static_cast<size_t>(nk)), dsii(static_cast<size_t>(nk));
  st6::w3sin6_sig_dsii(nk, ikn_ctx().h.data(), g.sig2_h.data(), g.dden2_h.data(), g.dth,
                       sig.data(), dsii.data());
  const int nk10hz = st6::lfactor_nk10hz(nk, sig[0], g.xfr);
  const int nik = st6::range_size(1, nk10hz, 1);
  std::vector<Real> ik10hz(static_cast<size_t>(nik > 0 ? nik : 1));
  std::vector<Real> sig10hz(static_cast<size_t>(nk10hz)), dsii10hz(static_cast<size_t>(nk10hz));
  int mk10hz = 0;
  t.built = true;
  if (st6::lfactor_tables(nk, g.xfr, sig.data(), dsii.data(), mk10hz, ik10hz.data(),
                          sig10hz.data(), dsii10hz.data()) != 0) {
    info[0] = nik;
    info[1] = nk10hz;
    return WW_ST6_LF_IK10HZ;
  }
  t.mk10hz = mk10hz;
  t.sig10hz_h = sig10hz;
  t.sig10hz = ex.alloc<DevReal1D>("ww_st6.sig10hz", sig10hz.size());
  t.dsii10hz = ex.alloc<DevReal1D>("ww_st6.dsii10hz", dsii10hz.size());
  using HostR = Kokkos::View<const Real*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  ex.copy(t.sig10hz, HostR(sig10hz.data(), sig10hz.size()));
  ex.copy(t.dsii10hz, HostR(dsii10hz.data(), dsii10hz.size()));
  return 0;
}

/// TAU_WAVE_ATMOS's rewrite of the SAVE DSII10Hz (:1294-1297), which runs when
/// SIN6A0 > 0 (W3SIN6 :522) and NK < NK10Hz, after the MK10Hz check: the expressions
/// of tau_wave_atmos() for dsii10hz_w, applied to the table of the process. Exact for
/// npts == 1, which is how W3SRCE calls; with npts > 1 the Fortran's points after the
/// first read the rewritten table, the kernel's read the one before the call.
void rewrite_dsii10hz(const GridCtx& g, int nk10hz, const Exec& ex) {
  TenHzCtx& t = tenhz_ctx();
  if (!(g.sin6a0 > static_cast<Real>(0)) || g.p.nk >= nk10hz || t.mk10hz != nk10hz) return;
  const Real xfr = g.xfr;
  const std::vector<Real>& sig10hz = t.sig10hz_h;
  std::vector<Real> dsii10hz(static_cast<size_t>(nk10hz));
  const Real c = xfr - static_cast<Real>(1.) / xfr;
  for (int ik = 0; ik < nk10hz; ++ik) {
    const size_t k = static_cast<size_t>(ik);
    dsii10hz[k] = (sig10hz[k] * static_cast<Real>(0.5)) * c;
  }
  const size_t last = static_cast<size_t>(nk10hz - 1);
  dsii10hz[0] = (sig10hz[0] * static_cast<Real>(0.5)) * (xfr - static_cast<Real>(1.));
  dsii10hz[last] = ((sig10hz[last] * static_cast<Real>(0.5)) * (xfr - static_cast<Real>(1.))) / xfr;
  using HostR = Kokkos::View<const Real*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  ex.copy(t.dsii10hz, HostR(dsii10hz.data(), dsii10hz.size()));
}

GridCtx* ready_grid(int igrid, const char* what) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready || !Kokkos::is_initialized()) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED, (std::string(what) + " before ww_st6_commit for this grid").c_str());
    return nullptr;
  }
  return &it->second;
}

}  // namespace
}  // namespace ww::st6shim

using namespace ww::st6shim;
using ww::Real;

extern "C" {

int ww_st6_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st6_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_ST6_NRPAR || nipar != WW_ST6_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st6_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const int nk = ipar[WW_ST6_I_NK], nth = ipar[WW_ST6_I_NTH], nspec = ipar[WW_ST6_I_NSPEC];
    if (nk <= 0 || nth <= 0 || nspec != nk * nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st6_init: NK, NTH, NSPEC inconsistent");
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
    g.dth = rpar[WW_ST6_R_DTH];
    g.xfr = rpar[WW_ST6_R_XFR];
    g.fte = rpar[WW_ST6_R_FTE];
    g.ftf = rpar[WW_ST6_R_FTF];
    g.ftwn = rpar[WW_ST6_R_FTWN];
    g.sin6a0 = rpar[WW_ST6_R_SIN6A0];
    g.sin6ws = rpar[WW_ST6_R_SIN6WS];
    g.sin6chkmin = rpar[WW_ST6_R_SIN6CHKMIN];
    g.sin6chkcap = rpar[WW_ST6_R_SIN6CHKCAP];
    g.sin6chkinf = rpar[WW_ST6_R_SIN6CHKINF];
    g.sin6chksig = rpar[WW_ST6_R_SIN6CHKSIG];
    g.sds6a1 = rpar[WW_ST6_R_SDS6A1];
    g.sds6a2 = rpar[WW_ST6_R_SDS6A2];
    g.sin6flcap = ipar[WW_ST6_I_SIN6FLCAP] != 0;
    g.sds6p1 = ipar[WW_ST6_I_SDS6P1];
    g.sds6p2 = ipar[WW_ST6_I_SDS6P2];
    g.sds6et = ipar[WW_ST6_I_SDS6ET] != 0;
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st6_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st6_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st6_set_real before ww_st6_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_ST6_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1 || extent[0] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_st6_set_real: bad array id, rank, extent or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    Registered& r = it->second.h[id];
    r.lbound = lbound[0];
    r.v.assign(data, data + extent[0]);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st6_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st6_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st6_commit before ww_st6_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    const int nk = g.p.nk, ns = g.p.nspec;
    std::string why;
    std::vector<Real> sig, dden, dsii, ecos, esin;
    if (!slice(g.h[WW_ST6_A_SIG], 1, nk, "SIG", "1:NK", sig, why) ||
        !slice(g.h[WW_ST6_A_DDEN], 1, nk, "DDEN", "1:NK", dden, why) ||
        !slice(g.h[WW_ST6_A_DSII], 1, nk, "DSII", "1:NK", dsii, why) ||
        !slice(g.h[WW_ST6_A_SIG2], 1, ns, "SIG2", "1:NSPEC", g.sig2_h, why) ||
        !slice(g.h[WW_ST6_A_DDEN2], 1, ns, "DDEN2", "1:NSPEC", g.dden2_h, why) ||
        !slice(g.h[WW_ST6_A_ECOS], 1, ns, "ECOS", "1:NSPEC", ecos, why) ||
        !slice(g.h[WW_ST6_A_ESIN], 1, ns, "ESIN", "1:NSPEC", esin, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_st6_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    g.sig = to_device(sig, "SIG");
    g.dden = to_device(dden, "DDEN");
    g.dsii = to_device(dsii, "DSII");
    g.sig2 = to_device(g.sig2_h, "SIG2");
    g.dden2 = to_device(g.dden2_h, "DDEN2");
    g.ecos = to_device(ecos, "ECOS");
    g.esin = to_device(esin, "ESIN");
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st6_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_spr6(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax, float* fp) {
  try {
    GridCtx* gp = ready_grid(igrid, "ww_spr6");
    if (gp == nullptr) return;
    if (npts < 0) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_spr6: npts is negative");
      return;
    }
    if (npts == 0) {
      ok();
      return;
    }
    GridCtx& g = *gp;
    const Exec ex{nested_in_openmp()};
    const int nk = g.p.nk, ns = g.p.nspec;
    const DevReal2D A = ex.buf(g, "spr6.a", ns, npts);
    const DevReal2D CG = ex.buf(g, "spr6.cg", nk, npts);
    const DevReal2D WN = ex.buf(g, "spr6.wn", nk, npts);
    const DevReal2D OUT = ex.buf(g, "spr6.out", 5, npts);
    const DevReal2D EB = ex.buf(g, "spr6.eb", nk, npts);
    ex.copy(cols(A, npts), Borrowed2D(a, ns, npts));
    ex.copy(cols(CG, npts), Borrowed2D(cg, nk, npts));
    ex.copy(cols(WN, npts), Borrowed2D(wn, nk, npts));
    const ww::srce::Params p = g.p;
    const Real dth = g.dth, fte = g.fte, ftf = g.ftf, ftwn = g.ftwn;
    const DevReal1D sig = g.sig, dden = g.dden, dsii = g.dsii;
    ex.for_points("ww_st6.spr6", npts, KOKKOS_LAMBDA(const int ip) {
      Real em, fm, wm, am, f;
      ww::srce::st6::w3spr6_point(col(A, ip), col(CG, ip), col(WN, ip), em, fm, wm, am, f, p,
                                  sig.data(), dden.data(), dsii.data(), dth, fte, ftf, ftwn,
                                  col(EB, ip));
      OUT(0, ip) = em;
      OUT(1, ip) = fm;
      OUT(2, ip) = wm;
      OUT(3, ip) = am;
      OUT(4, ip) = f;
    });
    std::vector<Real> out(static_cast<size_t>(5) * static_cast<size_t>(npts));
    ex.copy(Host2D(out.data(), 5, npts), cols(OUT, npts));
    for (int ip = 0; ip < npts; ++ip) {
      const Real* o = out.data() + 5 * ip;
      emean[ip] = o[0];
      fmean[ip] = o[1];
      wnmean[ip] = o[2];
      amax[ip] = o[3];
      fp[ip] = o[4];
    }
    g_count_spr6.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr6: unknown exception");
  }
}

int ww_sin6(int igrid, int npts, const float* a, const float* cg, const float* wn2,
            const float* uabs, const float* ustar, const float* usdir, const float* cd,
            const float* dair, float* tauwx, float* tauwy, float* taunwx, float* taunwy,
            float* charn, float* s, float* d, int* info) {
  try {
    if (info != nullptr) info[0] = info[1] = 0;
    GridCtx* gp = ready_grid(igrid, "ww_sin6");
    if (gp == nullptr) return 0;
    if (npts < 0 || info == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_sin6: npts is negative or info is null");
      return 0;
    }
    if (npts == 0) {
      ok();
      return 0;
    }
    GridCtx& g = *gp;
    const Exec ex{nested_in_openmp()};
    const int nk = g.p.nk, ns = g.p.nspec;
    if (const int rc = build_ikn(g.p, ex, info); rc != 0) {
      ok();
      return rc;
    }
    if (!ikn_fits(g.p)) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_sin6: IKN of the first ST6 grid does not fit this grid");
      return 0;
    }
    if (const int rc = build_tenhz(g, ex, info); rc != 0) {
      ok();
      return rc;
    }
    const IknCtx& ti = ikn_ctx();
    const TenHzCtx& tt = tenhz_ctx();
    // This grid's NK10Hz sizes the work; the kernel stops on MK10Hz /= NK10Hz before
    // it reads the tables.
    const int nk10hz =
        st6::lfactor_nk10hz(nk, g.sig2_h[static_cast<size_t>(ti.h[0] - 1)], g.xfr);
    const int nwork = st6::w3sin6_work_size(g.p, nk10hz);

    // The per-point scalars in (UABS, USTAR, USDIR, CD, DAIR), packed by column.
    std::vector<Real> scal(static_cast<size_t>(5) * static_cast<size_t>(npts));
    for (int ip = 0; ip < npts; ++ip) {
      Real* o = scal.data() + 5 * ip;
      o[0] = uabs[ip];
      o[1] = ustar[ip];
      o[2] = usdir[ip];
      o[3] = cd[ip];
      o[4] = dair[ip];
    }
    const DevReal2D A = ex.buf(g, "sin6.a", ns, npts);
    const DevReal2D CG = ex.buf(g, "sin6.cg", nk, npts);
    const DevReal2D WN2 = ex.buf(g, "sin6.wn2", ns, npts);
    const DevReal2D IN = ex.buf(g, "sin6.in", 5, npts);
    const DevReal2D OUT = ex.buf(g, "sin6.out", 5, npts);
    // S and D are INTENT(OUT) and assigned whole by every path that returns.
    const DevReal2D S = ex.buf(g, "sin6.s", ns, npts);
    const DevReal2D D = ex.buf(g, "sin6.d", ns, npts);
    const DevReal2D W = ex.buf(g, "sin6.work", nwork, npts);
    const DevInt1D RC = ex.rc(g, npts);
    ex.copy(cols(A, npts), Borrowed2D(a, ns, npts));
    ex.copy(cols(CG, npts), Borrowed2D(cg, nk, npts));
    ex.copy(cols(WN2, npts), Borrowed2D(wn2, ns, npts));
    ex.copy(cols(IN, npts), Borrowed2D(scal.data(), 5, npts));
    const ww::srce::Params p = g.p;
    const Real dth = g.dth, xfr = g.xfr, sin6a0 = g.sin6a0, sin6ws = g.sin6ws,
               sin6chkmin = g.sin6chkmin, sin6chkcap = g.sin6chkcap,
               sin6chkinf = g.sin6chkinf, sin6chksig = g.sin6chksig;
    const bool sin6flcap = g.sin6flcap;
    const DevReal1D sig2 = g.sig2, dden2 = g.dden2, ecos = g.ecos, esin = g.esin;
    const DevInt1D ikn = ti.d;
    const int mk10hz = tt.mk10hz;
    const DevReal1D sig10hz = tt.sig10hz, dsii10hz = tt.dsii10hz;
    ex.for_points("ww_st6.sin6", npts, KOKKOS_LAMBDA(const int ip) {
      Real twx = 0, twy = 0, tnx = 0, tny = 0, ch = 0;
      RC(ip) = ww::srce::st6::w3sin6_point(
          col(A, ip), col(CG, ip), col(WN2, ip), IN(0, ip), IN(1, ip), IN(2, ip), IN(3, ip),
          IN(4, ip), twx, twy, tnx, tny, ch, col(S, ip), col(D, ip), p, dth, xfr, sig2.data(),
          dden2.data(), ecos.data(), esin.data(), sin6a0, sin6ws, sin6chkmin, sin6flcap,
          sin6chkcap, sin6chkinf, sin6chksig, ikn.data(), mk10hz, sig10hz.data(),
          dsii10hz.data(), col(W, ip));
      OUT(0, ip) = twx;
      OUT(1, ip) = twy;
      OUT(2, ip) = tnx;
      OUT(3, ip) = tny;
      OUT(4, ip) = ch;
    });
    const std::vector<int> rc = codes(g, npts, ex);
    // A stop (LFACTOR's or TAU_WAVE_ATMOS's EXTCDE(3)) at the first point that has one.
    for (int ip = 0; ip < npts; ++ip) {
      const int lf = rc[static_cast<size_t>(ip)] & 0xF;
      const int tw = rc[static_cast<size_t>(ip)] >> st6::kSin6TauShift;
      if (lf == st6::kLfactorExtcde3 || tw == 3) {
        info[0] = mk10hz;
        info[1] = nk10hz;
        ok();
        return lf == st6::kLfactorExtcde3 ? WW_ST6_LF_MK10HZ : WW_ST6_TWA_MK10HZ;
      }
    }
    rewrite_dsii10hz(g, st6::tau_wave_atmos_nk10hz(g.sig2_h[static_cast<size_t>(ti.h[0] - 1)],
                                                   g.xfr, nk),
                     ex);
    std::vector<Real> out(static_cast<size_t>(5) * static_cast<size_t>(npts));
    ex.copy(Host2D(out.data(), 5, npts), cols(OUT, npts));
    for (int ip = 0; ip < npts; ++ip) {
      const Real* o = out.data() + 5 * ip;
      tauwx[ip] = o[0];
      tauwy[ip] = o[1];
      taunwx[ip] = o[2];
      taunwy[ip] = o[3];
      charn[ip] = o[4];
    }
    ex.copy(Host2D(s, ns, npts), cols(S, npts));
    ex.copy(Host2D(d, ns, npts), cols(D, npts));
    int ret = 0;
    long long clean = 0;
    for (int ip = 0; ip < npts; ++ip) {
      if ((rc[static_cast<size_t>(ip)] & 0xF) == st6::kLfactorItermax) {
        if (ret == 0) info[0] = ip;
        ret = WW_ST6_LF_ITERMAX;
      } else {
        ++clean;
      }
    }
    // A point with LFACTOR's warning is computed again by the Fortran W3SIN6 in the
    // caller, so it is not counted here.
    g_count_sin6.fetch_add(clean, std::memory_order_relaxed);
    ok();
    return ret;
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sin6: unknown exception");
  }
  return 0;
}

int ww_sds6(int igrid, int npts, const float* a, const float* cg, const float* wn, float* s,
            float* d, int* info) {
  try {
    if (info != nullptr) info[0] = info[1] = 0;
    GridCtx* gp = ready_grid(igrid, "ww_sds6");
    if (gp == nullptr) return 0;
    if (npts < 0 || info == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_sds6: npts is negative or info is null");
      return 0;
    }
    if (npts == 0) {
      ok();
      return 0;
    }
    GridCtx& g = *gp;
    const Exec ex{nested_in_openmp()};
    const int nk = g.p.nk, ns = g.p.nspec;
    if (const int rc = build_ikn(g.p, ex, info); rc != 0) {
      ok();
      return rc;
    }
    if (!ikn_fits(g.p)) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_sds6: IKN of the first ST6 grid does not fit this grid");
      return 0;
    }
    const DevReal2D A = ex.buf(g, "sds6.a", ns, npts);
    const DevReal2D CG = ex.buf(g, "sds6.cg", nk, npts);
    const DevReal2D WN = ex.buf(g, "sds6.wn", nk, npts);
    const DevReal2D S = ex.buf(g, "sds6.s", ns, npts);
    const DevReal2D D = ex.buf(g, "sds6.d", ns, npts);
    const DevReal2D W = ex.buf(g, "sds6.work", 10 * nk, npts);
    ex.copy(cols(A, npts), Borrowed2D(a, ns, npts));
    ex.copy(cols(CG, npts), Borrowed2D(cg, nk, npts));
    ex.copy(cols(WN, npts), Borrowed2D(wn, nk, npts));
    const ww::srce::Params p = g.p;
    const Real dth = g.dth, xfr = g.xfr, sds6a1 = g.sds6a1, sds6a2 = g.sds6a2;
    const int sds6p1 = g.sds6p1, sds6p2 = g.sds6p2;
    const bool sds6et = g.sds6et;
    const DevReal1D sig2 = g.sig2, dsii = g.dsii;
    const DevInt1D ikn = ikn_ctx().d;
    ex.for_points("ww_st6.sds6", npts, KOKKOS_LAMBDA(const int ip) {
      ww::srce::st6::w3sds6_point(col(A, ip), col(CG, ip), col(WN, ip), col(S, ip), col(D, ip), p,
                                  ikn.data(), sig2.data(), dsii.data(), dth, xfr, sds6a1, sds6a2,
                                  sds6p1, sds6p2, sds6et, col(W, ip));
    });
    ex.copy(Host2D(s, ns, npts), cols(S, npts));
    ex.copy(Host2D(d, ns, npts), cols(D, npts));
    g_count_sds6.fetch_add(npts, std::memory_order_relaxed);
    ok();
    return 0;
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sds6: unknown exception");
  }
  return 0;
}

int ww_st6_enabled(int which) {
  switch (which) {
    case 1: return flags().spr6 ? 1 : 0;
    case 2: return flags().sin6 ? 1 : 0;
    case 3: return flags().sds6 ? 1 : 0;
    default: return 0;
  }
}

int ww_st6_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
