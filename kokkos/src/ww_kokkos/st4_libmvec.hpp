// kokkos/src/ww_kokkos/st4_libmvec.hpp
// glibc's vector math (libmvec) where gfortran -O3 uses it in ST4, so the port
// computes the same bits. WW3's build vectorises five loops (w3src4md.F90:316,
// :317, :2315, :2500 with _ZGVbN4vv_powf; :2509 with _ZGVbN4v_expf) in groups
// of four from the first element, and calls scalar powf/expf for the last
// N mod 4 (gimple inventory of WW3@bb08623a, -O3, x86-64). libmvec and scalar
// libm differ by up to 3 ULP, so the grouping is part of the arithmetic. The
// owner chose to emulate it on the host (2026-10-15).
//
// ⚠ On a device there is no libmvec: the lanes fall back to scalar pow/exp,
// and such a build cannot be bit for bit at these sites (AGENTS.md, "Bit for
// bit", GPU). The libmvec variant is chosen by glibc at run time from the CPU,
// for the Fortran and for this code alike.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
extern "C" __m128 _ZGVbN4vv_powf(__m128, __m128);
extern "C" __m128 _ZGVbN4v_expf(__m128);
#define WW_ST4_HAVE_LIBMVEC 1
#endif

namespace ww::st4 {

/// out[i] = x[i]**y[i] for i < n, as gfortran -O3 computes the loop: lanes of
/// four through _ZGVbN4vv_powf, then scalar powf. Each array is read through
/// its accessor, so strided Fortran sections work.
template <class X, class Y, class Out>
KOKKOS_INLINE_FUNCTION void mvec_powf(int n, const X& x, const Y& y, const Out& out) {
  int i = 0;
#if defined(WW_ST4_HAVE_LIBMVEC)
  KOKKOS_IF_ON_HOST((for (; i + 4 <= n; i += 4) {
    alignas(16) float a[4], b[4], r[4];
    for (int l = 0; l < 4; ++l) { a[l] = x(i + l); b[l] = y(i + l); }
    _mm_store_ps(r, _ZGVbN4vv_powf(_mm_load_ps(a), _mm_load_ps(b)));
    for (int l = 0; l < 4; ++l) out(i + l, r[l]);
  }))
#endif
  for (; i < n; ++i) out(i, Kokkos::pow(x(i), y(i)));
}

/// out[i] = exp(x[i]) for i < n, as -O3 computes it: _ZGVbN4v_expf by four, then expf.
template <class X, class Out>
KOKKOS_INLINE_FUNCTION void mvec_expf(int n, const X& x, const Out& out) {
  int i = 0;
#if defined(WW_ST4_HAVE_LIBMVEC)
  KOKKOS_IF_ON_HOST((for (; i + 4 <= n; i += 4) {
    alignas(16) float a[4], r[4];
    for (int l = 0; l < 4; ++l) a[l] = x(i + l);
    _mm_store_ps(r, _ZGVbN4v_expf(_mm_load_ps(a)));
    for (int l = 0; l < 4; ++l) out(i + l, r[l]);
  }))
#endif
  for (; i < n; ++i) out(i, Kokkos::exp(x(i)));
}

}  // namespace ww::st4
