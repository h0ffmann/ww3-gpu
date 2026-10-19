// kokkos/src/ww_kokkos/libm_device.hpp
// The libm functions the kernels WW_KOKKOS_ALL and --kokkos resident run call, with glibc's bits on a device
// (WFIP-0006). On the host each wrapper is the Kokkos call the kernel made before, so the CPU
// build calls glibc as before. On a CUDA build configured with WW_KOKKOS_LIBM (the default
// there), the device calls ww::libm, the copies of glibc 2.44 in kokkos/tools/libm_bits that
// equal glibc on the host on every input checked (docs/GPU_LIBM_BITS_202610.md). Their tables,
// libmvec's, the build host's rcpps and glibc's fma or sse2 choice (WW_LIBM_HOST_FMA) are
// generated at build time by gen_tables. Without WW_KOKKOS_LIBM the device calls libdevice and
// ADR-0014's verdict by tolerance holds.
//
// ⚠ The device bits are argued from the host checks and the SASS operation counts; no device
// run has compared them (scripts/libm_bits.sh --device, the owner at the terminal).
//
// The overloads for another argument type are deleted: a double argument where the Fortran
// calls the float function, or the reverse, is a compile error and not a silent promotion.
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#if defined(WW_KOKKOS_LIBM)
#include "ww_libm_tables.hpp"  // generated: WW_LIBM_HOST_FMA, libmvec's tables, the rcpps runs
#include "glibc_mathf.hpp"
#include "coremath_f.hpp"
#include "coremath_d.hpp"
#endif

namespace ww::dm {

// The device implementations, also called by kernels that keep their own host branch
// (srce_kernel.hpp, srce_ln1.hpp, updt_uwnd.hpp, disp_distab.hpp, st4_libmvec.hpp,
// iogo_w3outg.hpp).
namespace dev {
#if defined(WW_KOKKOS_LIBM)
inline constexpr bool kFma = WW_LIBM_HOST_FMA != 0;
KOKKOS_INLINE_FUNCTION float expf(float x) { return ww::libm::expf<kFma>(x); }
KOKKOS_INLINE_FUNCTION float logf(float x) { return ww::libm::logf<kFma>(x); }
KOKKOS_INLINE_FUNCTION float powf(float x, float y) { return ww::libm::powf<kFma>(x, y); }
KOKKOS_INLINE_FUNCTION void sincosf(float x, float* s, float* c) { ww::libm::sincosf<kFma>(x, s, c); }
KOKKOS_INLINE_FUNCTION float tanhf(float x) { return ww::libm::tanhf(x); }
KOKKOS_INLINE_FUNCTION float log10f(float x) { return ww::libm::log10f(x); }
KOKKOS_INLINE_FUNCTION float atan2f(float y, float x) { return ww::libm::atan2f(y, x); }
KOKKOS_INLINE_FUNCTION float coshf(float x) { return ww::libm::coshf(x); }
KOKKOS_INLINE_FUNCTION float sinhf(float x) { return ww::libm::sinhf(x); }
KOKKOS_INLINE_FUNCTION double exp(double x) { return ww::libm::exp<kFma>(x); }
KOKKOS_INLINE_FUNCTION double tanh(double x) { return ww::libm::tanh(x); }
KOKKOS_INLINE_FUNCTION float mvec_expf(float x) { return ww::libm::mvec_expf<kFma>(x); }
KOKKOS_INLINE_FUNCTION float mvec_powf(float x, float y) { return ww::libm::mvec_powf<kFma>(x, y); }
KOKKOS_INLINE_FUNCTION float mvec_sinf(float x) { return ww::libm::mvec_sinf<kFma>(x); }
KOKKOS_INLINE_FUNCTION float mvec_cosf(float x) { return ww::libm::mvec_cosf<kFma>(x); }
#else
KOKKOS_INLINE_FUNCTION float expf(float x) { return Kokkos::exp(x); }
KOKKOS_INLINE_FUNCTION float logf(float x) { return Kokkos::log(x); }
KOKKOS_INLINE_FUNCTION float powf(float x, float y) { return Kokkos::pow(x, y); }
KOKKOS_INLINE_FUNCTION void sincosf(float x, float* s, float* c) { *c = Kokkos::cos(x); *s = Kokkos::sin(x); }
KOKKOS_INLINE_FUNCTION float tanhf(float x) { return Kokkos::tanh(x); }
KOKKOS_INLINE_FUNCTION float log10f(float x) { return Kokkos::log10(x); }
KOKKOS_INLINE_FUNCTION float atan2f(float y, float x) { return Kokkos::atan2(y, x); }
KOKKOS_INLINE_FUNCTION float coshf(float x) { return Kokkos::cosh(x); }
KOKKOS_INLINE_FUNCTION float sinhf(float x) { return Kokkos::sinh(x); }
KOKKOS_INLINE_FUNCTION double exp(double x) { return Kokkos::exp(x); }
KOKKOS_INLINE_FUNCTION double tanh(double x) { return Kokkos::tanh(x); }
KOKKOS_INLINE_FUNCTION float mvec_expf(float x) { return Kokkos::exp(x); }
KOKKOS_INLINE_FUNCTION float mvec_powf(float x, float y) { return Kokkos::pow(x, y); }
KOKKOS_INLINE_FUNCTION float mvec_sinf(float x) { return Kokkos::sin(x); }
KOKKOS_INLINE_FUNCTION float mvec_cosf(float x) { return Kokkos::cos(x); }
#endif
}  // namespace dev

// One wrapper per entry point: the host branch is the kernel's call as it was written.
KOKKOS_INLINE_FUNCTION float expf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::exp(x);))
  KOKKOS_IF_ON_DEVICE((return dev::expf(x);))
}
KOKKOS_INLINE_FUNCTION float logf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::log(x);))
  KOKKOS_IF_ON_DEVICE((return dev::logf(x);))
}
KOKKOS_INLINE_FUNCTION float powf(float x, float y) {
  KOKKOS_IF_ON_HOST((return Kokkos::pow(x, y);))
  KOKKOS_IF_ON_DEVICE((return dev::powf(x, y);))
}
/// cos and sin of one argument: gfortran -O3 and g++ -O3 both make them one sincosf call on
/// the host, so the device makes one too (WFIP-0006, "the sincos trap").
KOKKOS_INLINE_FUNCTION void sincosf(float x, float& s, float& c) {
  KOKKOS_IF_ON_HOST((c = Kokkos::cos(x); s = Kokkos::sin(x);))
  KOKKOS_IF_ON_DEVICE((dev::sincosf(x, &s, &c);))
}
KOKKOS_INLINE_FUNCTION float tanhf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::tanh(x);))
  KOKKOS_IF_ON_DEVICE((return dev::tanhf(x);))
}
KOKKOS_INLINE_FUNCTION float log10f(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::log10(x);))
  KOKKOS_IF_ON_DEVICE((return dev::log10f(x);))
}
KOKKOS_INLINE_FUNCTION float atan2f(float y, float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::atan2(y, x);))
  KOKKOS_IF_ON_DEVICE((return dev::atan2f(y, x);))
}
/// A lone sin or cos: glibc's sinf and cosf equal the two results of its sincosf on every input
/// (v, libm_bits ww sinf and ww cosf, every input, 2026-10-17), so the device computes sincosf.
KOKKOS_INLINE_FUNCTION float sinf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::sin(x);))
  KOKKOS_IF_ON_DEVICE((float s, c; dev::sincosf(x, &s, &c); return s;))
}
KOKKOS_INLINE_FUNCTION float cosf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::cos(x);))
  KOKKOS_IF_ON_DEVICE((float s, c; dev::sincosf(x, &s, &c); return c;))
}
KOKKOS_INLINE_FUNCTION float coshf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::cosh(x);))
  KOKKOS_IF_ON_DEVICE((return dev::coshf(x);))
}
KOKKOS_INLINE_FUNCTION float sinhf(float x) {
  KOKKOS_IF_ON_HOST((return Kokkos::sinh(x);))
  KOKKOS_IF_ON_DEVICE((return dev::sinhf(x);))
}
KOKKOS_INLINE_FUNCTION double exp(double x) {
  KOKKOS_IF_ON_HOST((return Kokkos::exp(x);))
  KOKKOS_IF_ON_DEVICE((return dev::exp(x);))
}
KOKKOS_INLINE_FUNCTION double tanh(double x) {
  KOKKOS_IF_ON_HOST((return Kokkos::tanh(x);))
  KOKKOS_IF_ON_DEVICE((return dev::tanh(x);))
}

template <class T> float expf(T) = delete;
template <class T> float logf(T) = delete;
template <class T, class U> float powf(T, U) = delete;
template <class T> void sincosf(T, float&, float&) = delete;
template <class T> float tanhf(T) = delete;
template <class T> float log10f(T) = delete;
template <class T, class U> float atan2f(T, U) = delete;
template <class T> float sinf(T) = delete;
template <class T> float cosf(T) = delete;
template <class T> float coshf(T) = delete;
template <class T> float sinhf(T) = delete;
template <class T> double exp(T) = delete;
template <class T> double tanh(T) = delete;

}  // namespace ww::dm
