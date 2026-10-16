// kokkos/src/fortran_iface/srce_shim.cpp
// The bind(C) shim of W3SRCE: the C ABI of ww_kokkos_c.hpp ("W3SRCE") over the
// team kernel of ww_kokkos/srce_kernel.hpp. Marshalling only.
//
// The rules are snl1_shim.cpp's and st4_shim.cpp's: one context per WW3 grid;
// nothing throws across the boundary; Views die inside Kokkos::finalize; inside
// an OpenMP parallel region (W3WAVE's source-term loop under W3_OMPG) the kernel
// runs on Kokkos::Serial. Phase 1 copies one point in, runs, copies it out, and
// the caller serialises the calls (!$OMP CRITICAL (WW_KOKKOS_SRCE)). The kernel
// reads the W3SNL1 and ST4 tables of the same grid (shim_state.hpp).

#include "fortran_iface/ww_kokkos_c.hpp"
#include "fortran_iface/shim_state.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <stdexcept>
#include <string>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/snl1_team.hpp"
#include "ww_kokkos/srce_kernel.hpp"
#include "ww_kokkos/srce_params.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::srce {
namespace {

using DeviceSpace = st4::DeviceSpace;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevInt1D = Kokkos::View<int*, DeviceSpace>;
using DevDouble1D = Kokkos::View<double*, DeviceSpace>;

struct GridCtx {
  Params p{};
  DevReal1D sig, dden, ecos, esin;
  DevInt1D mapwn;
  DevReal1D spec, wn1, cg1, whitecap, taubbl, tauice, rw;
  DevInt1D iw;
  DevDouble1D dw;
  Kokkos::View<PointScalars*, DeviceSpace> scal;
  Kokkos::View<State*, DeviceSpace> state;
  int work_reals = -1, work_ints = -1;
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
std::atomic<long long> g_count{0};
bool g_hook_pushed = false;

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }
void release() { grids().clear(); }

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}
struct Flags {
  bool srce, count;
};
const Flags& flags() {
  static const Flags f{env_is_one("WW_KOKKOS_SRCE"), env_is_one("WW_KOKKOS_SRCE_COUNT")};
  return f;
}
void print_count() {
  std::fprintf(stderr, "ww_kokkos: W3SRCE spectra computed: %lld\n", g_count.load());
}

#if defined(WW_SRCE_SERIAL_NESTED)
bool nested_in_openmp() { return omp_get_level() > 0; }
#endif

template <class V, class T>
V to_device(const char* name, const T* data, int n) {
  V d(name, n);
  Kokkos::View<const T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(data, n);
  Kokkos::deep_copy(d, h);
  return d;
}

template <class T>
using HostUnmanaged =
    Kokkos::View<T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// One point on ExecSpace: stage in, the team kernel, stage out.
template <class ExecSpace>
void run(GridCtx& g, const st4::Params& q, const st4::Fields& f, const snl1::Config& nc,
         const snl1::Tables& nt, const snl1::RealView1D& nsig, const PointIn& in, float* spec,
         const float* wn1, const float* cg1, PointScalars& o, float* whitecap, float* taubbl,
         float* tauice) {
  const ExecSpace ex;
  const Params p = g.p;
  Kokkos::deep_copy(ex, g.spec, HostUnmanaged<float>(spec, p.nspec));
  Kokkos::deep_copy(ex, g.wn1, HostUnmanaged<const float>(wn1, p.nk));
  Kokkos::deep_copy(ex, g.cg1, HostUnmanaged<const float>(cg1, p.nk));
  Kokkos::deep_copy(ex, g.whitecap, HostUnmanaged<float>(whitecap, 4));
  Kokkos::deep_copy(ex, g.taubbl, HostUnmanaged<float>(taubbl, 2));
  Kokkos::deep_copy(ex, g.tauice, HostUnmanaged<float>(tauice, 2));
  Kokkos::deep_copy(ex, g.scal, HostUnmanaged<PointScalars>(&o, 1));

  using Policy = Kokkos::TeamPolicy<ExecSpace>;
  using Member = typename Policy::member_type;
  const size_t scratch = snl1::snl1_team_scratch_bytes<ExecSpace>(nc, nt);
  Policy policy(1, Kokkos::AUTO);
  const int level = scratch <= static_cast<size_t>(policy.scratch_size_max(0)) ? 0 : 1;
  if (scratch > static_cast<size_t>(policy.scratch_size_max(level)))
    throw std::runtime_error("ww_srce: W3SNL1 needs " + std::to_string(scratch) +
                             " B of team scratch, more than this backend offers");
  policy.set_scratch_size(level, Kokkos::PerTeam(scratch));

  const Grid grid{g.sig.data(), g.dden.data(), g.ecos.data(), g.esin.data(), g.mapwn.data()};
  const auto SP = g.spec, W1 = g.wn1, C1 = g.cg1, WC = g.whitecap, TB = g.taubbl, TI = g.tauice,
             RW = g.rw;
  const auto IW = g.iw;
  const auto DW = g.dw;
  const auto SC = g.scal;
  const auto ST = g.state;
  const Real* ns = nsig.data();
  Kokkos::parallel_for(
      "ww_srce", policy, KOKKOS_LAMBDA(const Member& team) {
        const snl1::TeamScratch w = snl1::snl1_team_scratch(team, level, nc, nt);
        w3srce_team(team, p, grid, q, f, nc, nt, ns, w, in, SP.data(), W1.data(), C1.data(),
                    SC(0), WC.data(), TB.data(), TI.data(), ST(0), RW.data(), IW.data(),
                    DW.data());
      });
  ex.fence();

  Kokkos::deep_copy(ex, HostUnmanaged<float>(spec, p.nspec), g.spec);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(whitecap, 4), g.whitecap);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(taubbl, 2), g.taubbl);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(tauice, 2), g.tauice);
  Kokkos::deep_copy(ex, HostUnmanaged<PointScalars>(&o, 1), g.scal);
  ex.fence();
}

}  // namespace
}  // namespace ww::srce

using namespace ww::srce;
using ww::Real;
namespace snl1 = ww::snl1;
namespace st4 = ww::st4;

extern "C" {

int ww_srce_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                 const float* sig, const float* dden, const float* ecos, const float* esin,
                 const int* mapwn) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (nrpar != NRPAR || nipar != NIPAR || rpar == nullptr || ipar == nullptr ||
        sig == nullptr || dden == nullptr || ecos == nullptr || esin == nullptr ||
        mapwn == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: RPAR/IPAR length differs from srce_params.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    if (ipar[I_LAB_SWITCH] != 1) {
      set_error(WW_KOKKOS_ERR_UNSUPPORTED,
                "ww_srce_init: the Kokkos W3SRCE translates switch_lab_shrd's source terms only");
      return WW_KOKKOS_ERR_UNSUPPORTED;
    }
    const Params p = unpack(rpar, ipar);
    if (p.nk <= 0 || p.nth <= 0 || p.nspec != p.nk * p.nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: nspec != nk*nth");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
                "ww_srce_init before ww_snl1_init and ww_st4_commit for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (nc.nk != p.nk || nc.nth != p.nth || q.nk != p.nk || q.nth != p.nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: NK/NTH differ from the DIA or ST4 context");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    GridCtx g;
    g.p = p;
    g.sig = to_device<DevReal1D>("ww_srce.sig", sig, p.nk);
    g.dden = to_device<DevReal1D>("ww_srce.dden", dden, p.nk);
    g.ecos = to_device<DevReal1D>("ww_srce.ecos", ecos, p.nspec);
    g.esin = to_device<DevReal1D>("ww_srce.esin", esin, p.nspec);
    g.mapwn = to_device<DevInt1D>("ww_srce.mapwn", mapwn, p.nspec);
    g.spec = DevReal1D("ww_srce.spec", p.nspec);
    g.wn1 = DevReal1D("ww_srce.wn1", p.nk);
    g.cg1 = DevReal1D("ww_srce.cg1", p.nk);
    g.whitecap = DevReal1D("ww_srce.whitecap", 4);
    g.taubbl = DevReal1D("ww_srce.taubbl", 2);
    g.tauice = DevReal1D("ww_srce.tauice", 2);
    g.work_reals = srce_work_reals(p, q);
    g.work_ints = srce_work_ints(p, q);
    g.rw = DevReal1D("ww_srce.rw", g.work_reals);
    g.iw = DevInt1D("ww_srce.iw", g.work_ints);
    g.dw = DevDouble1D("ww_srce.dw", srce_work_doubles(p));
    g.scal = Kokkos::View<PointScalars*, DeviceSpace>("ww_srce.scal", 1);
    g.state = Kokkos::View<State*, DeviceSpace>("ww_srce.state", 1);
    grids()[igrid] = std::move(g);
    if (!g_hook_pushed) {
      Kokkos::push_finalize_hook(release);
      g_hook_pushed = true;
      if (flags().count) std::atexit(print_count);
    }
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::bad_alloc& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
    return WW_KOKKOS_ERR_KERNEL;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce_init: unknown exception");
    return WW_KOKKOS_ERR_KERNEL;
  }
}

void ww_srce(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float* spec,
             const float* wn1, const float* cg1, float d_inp, float u10abs, float u10dir,
             float as, float ice, float dtg, float dair, float coef, float* ustar,
             float* ustdir, float* tauwx, float* tauwy, float* dtdyn, float* fcut,
             float* tauox, float* tauoy, float* tauwix, float* tauwiy, float* tauwnx,
             float* tauwny, float* phiaw, float* charn, float* tws, float* phioc,
             float* whitecap, float* phibbl, float* taubbl, float* tauice, float* phice,
             float* tauocx, float* tauocy, float* wnmean, int* shave) {
  try {
    const auto it_g = grids().find(igrid);
    if (it_g == grids().end() || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce before ww_srce_init for this grid");
      return;
    }
    GridCtx& g = it_g->second;
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    // The module state of ST4 (the DEL* scalars, TAUHFT...) is read at every
    // call, as ww_sds4() does: the last grid to register it wins, as in WW3.
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce: the DIA or ST4 context of this grid is gone");
      return;
    }
    const PointIn in{it, inflags24, flogrd57, flogrd58, d_inp, u10abs, u10dir, as,
                     ice, dtg, dair, coef};
    PointScalars o{*ustar,  *ustdir, *tauwx,  *tauwy, *dtdyn,  *fcut,  *tauox, *tauoy,
                   *tauwix, *tauwiy, *tauwnx, *tauwny, *phiaw, *charn, *tws,  *phioc,
                   *phibbl, *phice,  *tauocx, *tauocy, *wnmean, *shave};
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested_in_openmp())
      run<Kokkos::Serial>(g, q, f, nc, nt, nsig, in, spec, wn1, cg1, o, whitecap, taubbl, tauice);
    else
#endif
      run<Kokkos::DefaultExecutionSpace>(g, q, f, nc, nt, nsig, in, spec, wn1, cg1, o, whitecap,
                                         taubbl, tauice);
    *ustar = o.ustar;
    *ustdir = o.ustdir;
    *tauwx = o.tauwx;
    *tauwy = o.tauwy;
    *dtdyn = o.dtdyn;
    *fcut = o.fcut;
    *tauox = o.tauox;
    *tauoy = o.tauoy;
    *tauwix = o.tauwix;
    *tauwiy = o.tauwiy;
    *tauwnx = o.tauwnx;
    *tauwny = o.tauwny;
    *phiaw = o.phiaw;
    *charn = o.charn;
    *tws = o.tws;
    *phioc = o.phioc;
    *phibbl = o.phibbl;
    *phice = o.phice;
    *tauocx = o.tauocx;
    *tauocy = o.tauocy;
    *wnmean = o.wnmean;
    *shave = o.shave;
    g_count.fetch_add(1, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce: unknown exception");
  }
}

int ww_srce_enabled(void) { return flags().srce ? 1 : 0; }

int ww_srce_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
