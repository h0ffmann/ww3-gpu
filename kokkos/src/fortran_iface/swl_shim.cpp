// kokkos/src/fortran_iface/swl_shim.cpp
// The bind(C) shim of W3SWLDMD's W3SWL6: the C ABI of ww_kokkos_c.hpp ("SWL") over the
// per-point device function of ww_kokkos/srce_swl6.hpp. Marshalling only. W3SWL4 has
// no caller in WW3 and is not wired.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT, IGRID at
// W3SRCE); nothing throws across the boundary; Views die inside Kokkos::finalize (a
// finalize hook); inside an OpenMP parallel region (W3SRCE under W3_OMPG) the
// kernel runs on Kokkos::Serial. Phase 1 copies in, runs, copies out on every call,
// and the caller serialises the calls (!$OMP CRITICAL in the caller patch).
//
// IKN is W3SWL6's ALLOCATABLE, SAVE table (w3swldmd.F90:359, :370-372): one per
// process, built on the first call from the NSPEC and NTH of the grid that makes it,
// and checked against each later grid's NK (:374-377). The shim keeps it the same way.
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
#include "ww_kokkos/srce_swl6.hpp"
#include "ww_kokkos/srce_swl_util.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::swlshim {
namespace {

using ww::Real;
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevInt1D = Kokkos::View<int*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using Borrowed2D = Kokkos::View<const Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// A registered REAL array as the Fortran has it: its LBOUND and its values.
struct Registered {
  int lbound = 0;
  std::vector<Real> v;
};

struct GridCtx {
  srce::Params p{};       ///< nk, nth, nspec; the rest unused by W3SWL6
  Real dth = 0, swl6b1 = 0;
  bool swl6cstb1 = false;
  Registered h[WW_SWL_NARRAYS];
  DevReal1D sig, dden;    ///< SIG(1:NK), DDEN(1:NK) on the device
  std::map<std::string, DevReal2D> buf;
  DevInt1D rc;            ///< the kernel's return code per point
  bool configured = false;
  bool ready = false;
};

/// W3SWL6's SAVE IKN: one per process. built is .NOT.ALLOCATED(IKN) negated.
struct IknCtx {
  bool built = false;
  std::vector<int> h;  ///< IRANGE(1,NSPEC,NTH) of the first calling grid
  DevInt1D d;
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}
IknCtx& ikn_ctx() {
  static IknCtx m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_swl6{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: SWL: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() {
  grids().clear();
  ikn_ctx() = IknCtx{};
}

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool swl6, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SWL6") != 0), env_is_one("WW_KOKKOS_SWL_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SWL6 spectra computed: %lld\n", g_count_swl6.load());
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
  DevReal1D d(std::string("ww_swl.") + name, static_cast<size_t>(n));
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
        v = DevReal2D(Kokkos::view_alloc(Kokkos::Serial(), "ww_swl." + name), rows, npts);
        return v;
      }
#endif
      v = DevReal2D(Kokkos::view_alloc("ww_swl." + name), rows, npts);
    }
    return v;
  }
  DevInt1D& rc(GridCtx& g, int npts) const {
    if (g.rc.extent(0) < static_cast<size_t>(npts)) {
      g.rc = DevInt1D();
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested) {
        g.rc = DevInt1D(Kokkos::view_alloc(Kokkos::Serial(), "ww_swl.rc"), npts);
        return g.rc;
      }
#endif
      g.rc = DevInt1D(Kokkos::view_alloc("ww_swl.rc"), npts);
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

/// :370-372, IF (.NOT.ALLOCATED(IKN)) IKN = IRANGE(1,NSPEC,NTH), with the calling
/// grid's NSPEC and NTH.
void build_ikn(const srce::Params& p, const Exec& ex) {
  IknCtx& t = ikn_ctx();
  if (t.built) return;
  const int n = ww::srce::swl::w3swl6_ikn(p, nullptr);
  t.h.assign(static_cast<size_t>(n), 0);
  if (n > 0) ww::srce::swl::w3swl6_ikn(p, t.h.data());
  // Size 1 when IKN is empty, so the device pointer is never null; nikn says 0.
  const size_t ext = n > 0 ? static_cast<size_t>(n) : 1;
#if defined(WW_SRCE_SERIAL_NESTED)
  if (ex.nested) {
    t.d = DevInt1D(Kokkos::view_alloc(Kokkos::Serial(), "ww_swl.ikn"), ext);
  } else {
    t.d = DevInt1D(Kokkos::view_alloc("ww_swl.ikn"), ext);
  }
#else
  t.d = DevInt1D(Kokkos::view_alloc("ww_swl.ikn"), ext);
#endif
  if (n > 0) {
    Kokkos::View<const int*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> hv(
        t.h.data(), static_cast<size_t>(n));
    ex.copy(Kokkos::subview(t.d, Kokkos::make_pair(0, n)), hv);
  }
  t.built = true;
}

}  // namespace
}  // namespace ww::swlshim

using namespace ww::swlshim;
using ww::Real;

extern "C" {

int ww_swl_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_swl_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_SWL_NRPAR || nipar != WW_SWL_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_swl_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const int nk = ipar[WW_SWL_I_NK], nth = ipar[WW_SWL_I_NTH], nspec = ipar[WW_SWL_I_NSPEC];
    if (nk <= 0 || nth <= 0 || nspec != nk * nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_swl_init: NK, NTH, NSPEC inconsistent");
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
    g.dth = rpar[WW_SWL_R_DTH];
    g.swl6b1 = rpar[WW_SWL_R_SWL6B1];
    g.swl6cstb1 = ipar[WW_SWL_I_SWL6CSTB1] != 0;
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_swl_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_swl_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_swl_set_real before ww_swl_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_SWL_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1 || extent[0] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_swl_set_real: bad array id, rank, extent or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    Registered& r = it->second.h[id];
    r.lbound = lbound[0];
    r.v.assign(data, data + extent[0]);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_swl_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_swl_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_swl_commit before ww_swl_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    std::string why;
    if (!slice_to_device(g.h[WW_SWL_A_SIG], 1, g.p.nk, "SIG", g.sig, why) ||
        !slice_to_device(g.h[WW_SWL_A_DDEN], 1, g.p.nk, "DDEN", g.dden, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_swl_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_swl_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_swl6(int igrid, int npts, const float* a, const float* cg, const float* wn, float* s,
            float* d, int* nikn) {
  try {
    if (nikn != nullptr) *nikn = 0;
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.ready || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_swl6 before ww_swl_commit for this grid");
      return 0;
    }
    if (npts < 0 || nikn == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_swl6: npts is negative or nikn is null");
      return 0;
    }
    if (npts == 0) {
      ok();
      return 0;
    }
    GridCtx& g = it->second;
    const Exec ex{nested_in_openmp()};
    const int nk = g.p.nk, nth = g.p.nth, ns = g.p.nspec;
    build_ikn(g.p, ex);
    const IknCtx& t = ikn_ctx();
    const int n_ikn = static_cast<int>(t.h.size());
    *nikn = n_ikn;
    // With NK equal and NTH larger on the grid that built IKN, D(IKN+(ITH-1)) reaches
    // past NSPEC (:420-422): out of bounds in the Fortran, refused here.
    if (n_ikn == nk && n_ikn > 0 && t.h[static_cast<size_t>(n_ikn - 1)] + nth - 1 > ns) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE,
                "ww_swl6: IKN of the first grid indexes past this grid's NSPEC");
      return 0;
    }
    const DevReal2D A = ex.buf(g, "swl6.a", ns, npts);
    const DevReal2D CG = ex.buf(g, "swl6.cg", nk, npts);
    const DevReal2D WN = ex.buf(g, "swl6.wn", nk, npts);
    // S and D are INTENT(OUT) and assigned whole when the kernel returns 0.
    const DevReal2D S = ex.buf(g, "swl6.s", ns, npts);
    const DevReal2D D = ex.buf(g, "swl6.d", ns, npts);
    const DevReal2D W = ex.buf(g, "swl6.work", 5 * nk, npts);
    const DevReal2D K = ex.buf(g, "swl6.k", ns, npts);
    const DevInt1D RC = ex.rc(g, npts);
    ex.copy(cols(A, npts), Borrowed2D(a, ns, npts));
    ex.copy(cols(CG, npts), Borrowed2D(cg, nk, npts));
    ex.copy(cols(WN, npts), Borrowed2D(wn, nk, npts));
    const ww::srce::Params p = g.p;
    const Real dth = g.dth, swl6b1 = g.swl6b1;
    const bool swl6cstb1 = g.swl6cstb1;
    const DevReal1D sig = g.sig, dden = g.dden;
    const DevInt1D ikn = t.d;
    ex.for_points("ww_swl.swl6", npts, KOKKOS_LAMBDA(const int ip) {
      Real* w = col(W, ip);
      RC(ip) = ww::srce::swl::w3swl6_point(col(A, ip), col(CG, ip), col(WN, ip), col(S, ip),
                                           col(D, ip), p, dth, sig.data(), dden.data(),
                                           swl6cstb1, swl6b1, ikn.data(), n_ikn, w, w + nk,
                                           w + 2 * nk, w + 3 * nk, w + 4 * nk, col(K, ip));
    });
    auto hrc = Kokkos::create_mirror_view(Kokkos::HostSpace(),
                                          Kokkos::subview(RC, Kokkos::make_pair(0, npts)));
    ex.copy(hrc, Kokkos::subview(RC, Kokkos::make_pair(0, npts)));
    int rc = 0;
    for (int ip = 0; ip < npts; ++ip)
      if (hrc(ip) != 0) rc = hrc(ip);
    if (rc != 0) {
      ok();
      return rc;
    }
    Kokkos::View<Real**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>>
        hs(s, ns, npts), hd(d, ns, npts);
    ex.copy(hs, cols(S, npts));
    ex.copy(hd, cols(D, npts));
    g_count_swl6.fetch_add(npts, std::memory_order_relaxed);
    ok();
    return 0;
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_swl6: unknown exception");
  }
  return 0;
}

int ww_swl_enabled(int which) {
  switch (which) {
    case 1: return flags().swl6 ? 1 : 0;
    default: return 0;
  }
}

int ww_swl_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
