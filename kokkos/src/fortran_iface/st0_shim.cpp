// kokkos/src/fortran_iface/st0_shim.cpp
// The bind(C) shim of ST0: the C ABI of ww_kokkos_c.hpp ("ST0") over the per-point
// device function of ww_kokkos/srce_st0_spr0.hpp (W3SPR0). Marshalling only.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT, IGRID at
// W3SRCE); nothing throws across the boundary; Views die inside Kokkos::finalize (a
// finalize hook); inside an OpenMP parallel region (W3SRCE under W3_OMPG) the
// kernel runs on Kokkos::Serial. Phase 1 copies in, runs, copies out on every call,
// and the caller serialises the calls (!$OMP CRITICAL in the caller patch).
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
#include "ww_kokkos/srce_st0_spr0.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::st0shim {
namespace {

using ww::Real;
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using Borrowed2D = Kokkos::View<const Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// A registered REAL array as the Fortran has it: its LBOUND and its values.
struct Registered {
  int lbound = 0;
  std::vector<Real> v;
};

struct GridCtx {
  srce::Params p{};       ///< nk, nth, nspec; the rest unused by W3SPR0
  Real fte = 0, ftf = 0, ftwn = 0;
  Registered h[WW_ST0_NARRAYS];
  DevReal1D sig, dden;    ///< SIG(1:NK), DDEN(1:NK) on the device
  std::map<std::string, DevReal2D> buf;
  bool configured = false;
  bool ready = false;
};

// Never destroyed: ww_kokkos_finalize runs from atexit, after the static destructors of
// objects built later, so a static map would be gone when its finalize hook clears it.
std::map<int, GridCtx>& grids() {
  static auto* m = new std::map<int, GridCtx>();
  return *m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_spr0{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST0: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() { grids().clear(); }

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool spr0, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SPR0") != 0), env_is_one("WW_KOKKOS_ST0_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SPR0 spectra computed: %lld\n", g_count_spr0.load());
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
    why = std::string(name) + (r.v.empty() ? " not registered" : " does not cover 1:NK");
    return false;
  }
  DevReal1D d(std::string("ww_st0.") + name, static_cast<size_t>(n));
  auto hm = Kokkos::create_mirror_view(d);
  std::memcpy(hm.data(), r.v.data() + first, sizeof(Real) * static_cast<size_t>(n));
  Kokkos::deep_copy(d, hm);
  out = d;
  return true;
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
        v = DevReal2D(Kokkos::view_alloc(Kokkos::Serial(), "ww_st0." + name), rows, npts);
        return v;
      }
#endif
      v = DevReal2D(Kokkos::view_alloc("ww_st0." + name), rows, npts);
    }
    return v;
  }
};

auto cols(const DevReal2D& v, int npts) {
  return Kokkos::subview(v, Kokkos::ALL, Kokkos::make_pair(0, npts));
}

KOKKOS_INLINE_FUNCTION Real* col(const DevReal2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

}  // namespace
}  // namespace ww::st0shim

using namespace ww::st0shim;
using ww::Real;

extern "C" {

int ww_st0_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st0_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_ST0_NRPAR || nipar != WW_ST0_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st0_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const int nk = ipar[WW_ST0_I_NK], nth = ipar[WW_ST0_I_NTH], nspec = ipar[WW_ST0_I_NSPEC];
    // nk >= 1 is the kernel's precondition (it reads EB(NK)).
    if (nk <= 0 || nth <= 0 || nspec != nk * nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st0_init: NK, NTH, NSPEC inconsistent");
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
    g.fte = rpar[WW_ST0_R_FTE];
    g.ftf = rpar[WW_ST0_R_FTF];
    g.ftwn = rpar[WW_ST0_R_FTWN];
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st0_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st0_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st0_set_real before ww_st0_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_ST0_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1 || extent[0] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_st0_set_real: bad array id, rank, extent or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    Registered& r = it->second.h[id];
    r.lbound = lbound[0];
    r.v.assign(data, data + extent[0]);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st0_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st0_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st0_commit before ww_st0_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    std::string why;
    if (!slice_to_device(g.h[WW_ST0_A_SIG], 1, g.p.nk, "SIG", g.sig, why) ||
        !slice_to_device(g.h[WW_ST0_A_DDEN], 1, g.p.nk, "DDEN", g.dden, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_st0_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st0_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_spr0(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.ready || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_spr0 before ww_st0_commit for this grid");
      return;
    }
    if (npts < 0) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_spr0: npts is negative");
      return;
    }
    if (npts == 0) return ok();
    GridCtx& g = it->second;
    const Exec ex{nested_in_openmp()};
    const int nk = g.p.nk, ns = g.p.nspec;
    const DevReal2D A = ex.buf(g, "spr0.a", ns, npts);
    const DevReal2D CG = ex.buf(g, "spr0.cg", nk, npts);
    const DevReal2D WN = ex.buf(g, "spr0.wn", nk, npts);
    const DevReal2D EB = ex.buf(g, "spr0.eb", nk, npts);
    // EMEAN, FMEAN, WNMEAN, AMAX are INTENT(OUT) and always assigned: copied out only.
    const DevReal2D O = ex.buf(g, "spr0.out", 4, npts);
    ex.copy(cols(A, npts), Borrowed2D(a, ns, npts));
    ex.copy(cols(CG, npts), Borrowed2D(cg, nk, npts));
    ex.copy(cols(WN, npts), Borrowed2D(wn, nk, npts));
    const ww::srce::Params p = g.p;
    const Real fte = g.fte, ftf = g.ftf, ftwn = g.ftwn;
    const DevReal1D sig = g.sig, dden = g.dden;
    ex.for_points("ww_st0.spr0", npts, KOKKOS_LAMBDA(const int ip) {
      Real* o = col(O, ip);
      ww::srce::st0::w3spr0_point(col(A, ip), col(CG, ip), col(WN, ip), o[0], o[1], o[2], o[3], p,
                                  sig.data(), dden.data(), fte, ftf, ftwn, col(EB, ip));
    });
    auto ho = Kokkos::create_mirror_view(Kokkos::HostSpace(), cols(O, npts));
    ex.copy(ho, cols(O, npts));
    for (int ip = 0; ip < npts; ++ip) {
      emean[ip] = ho(0, ip);
      fmean[ip] = ho(1, ip);
      wnmean[ip] = ho(2, ip);
      amax[ip] = ho(3, ip);
    }
    g_count_spr0.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr0: unknown exception");
  }
}

int ww_st0_enabled(int which) {
  switch (which) {
    case 1: return flags().spr0 ? 1 : 0;
    default: return 0;
  }
}

int ww_st0_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
