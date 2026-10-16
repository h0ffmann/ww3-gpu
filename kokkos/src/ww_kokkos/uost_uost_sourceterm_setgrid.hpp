// kokkos/src/ww_kokkos/uost_uost_sourceterm_setgrid.hpp
// UOST_SOURCETERM_SETGRID: the SETGRID method of the unresolved-obstacles source term object;
// points it at a spatial and a spectral grid and caches COS, SIN of the directions.
// Host setup. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:75-93 (the type), :606-667 (the method) at
// NOAA-EMC/WW3 03adda91; author L. Mentaschi.
// Evidence: kokkos/port_report/UOST_SOURCETERM_SETGRID_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>
#include <vector>

#include "real.hpp"

#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
extern "C" __m128 _ZGVbN4v_cosf(__m128);
extern "C" __m128 _ZGVbN4v_sinf(__m128);
#define WW_UOST_HAVE_LIBMVEC 1
#endif

namespace ww::uost {

/// TYPE UOST_SOURCETERM (:75-93), the state of the module object SRCTRM. costh, sinth
/// (Fortran base 1) are empty while unallocated; grd, sgd are the POINTER components.
/// The type-bound procedures are free functions taking the object.
template <class Grid, class Sgrd>
struct UostSourceterm {
  std::vector<Real> costh;
  std::vector<Real> sinth;
  Real gammaup = static_cast<Real>(10);    // :77
  Real gammadown = static_cast<Real>(20);  // :78
  const Grid* grd = nullptr;
  const Sgrd* sgd = nullptr;
};

/// UOST_SOURCETERM_SETGRID on the host; allocates, so it never runs in a kernel.
/// nth, th are SGD%NTH and SGD%TH (NTH values, 0-based, Fortran base 1; w3gdatmd.F90:800).
/// For uost_setgrid, pass [](auto& st, auto& g, auto& s) { uost_sourceterm_setgrid(st, g, s,
/// <s's NTH>, <s's TH>); }.
template <class Grid, class Sgrd>
inline void uost_sourceterm_setgrid(UostSourceterm<Grid, Sgrd>& self, const Grid& grd,
                                    const Sgrd& sgd, int nth, const Real* th) {
  // :652-653.
  self.grd = &grd;
  self.sgd = &sgd;

  // :655-662. The Fortran frees and re-allocates; the values are all overwritten below.
  const std::size_t n = nth > 0 ? static_cast<std::size_t>(nth) : 0;
  self.costh.assign(n, static_cast<Real>(0));
  self.sinth.assign(n, static_cast<Real>(0));
  if (nth <= 0) return;
  Real* costh = self.costh.data();
  Real* sinth = self.sinth.data();

  // :663-666. -O3 vectorises this loop when NTH-1 > 4: lanes of four from ITH = 1 through
  // _ZGVbN4v_cosf / _ZGVbN4v_sinf, then scalar cosf / sinf for the last NTH mod 4; for
  // NTH <= 5 every element is scalar. libmvec and libm may differ by ULPs, so the split is
  // part of the arithmetic. The object's alias check (COSTH, SINTH against TH) always passes:
  // both arrays were just allocated. The object also keeps NTH = HUGE(0) scalar, which here
  // keeps i + 4 from overflowing. th must be contiguous (SGD%TH is allocated contiguous).
  int i = 0;
#if defined(WW_UOST_HAVE_LIBMVEC)
  if (static_cast<unsigned>(nth - 1) > 4u && nth != 2147483647) {
    for (; i + 4 <= nth; i += 4) {
      alignas(16) float a[4], c[4], s[4];
      for (int l = 0; l < 4; ++l) a[l] = th[i + l];
      _mm_store_ps(c, _ZGVbN4v_cosf(_mm_load_ps(a)));
      _mm_store_ps(s, _ZGVbN4v_sinf(_mm_load_ps(a)));
      for (int l = 0; l < 4; ++l) {
        costh[i + l] = c[l];
        sinth[i + l] = s[l];
      }
    }
  }
#endif
  for (; i < nth; ++i) {
    costh[i] = std::cos(th[i]);
    sinth[i] = std::sin(th[i]);
  }
}

}  // namespace ww::uost
