// kokkos/tools/libm_bits/glibc_mathf.hpp
// glibc 2.44's scalar expf, logf, powf and sincosf, written so a CUDA device computes the
// same bits as the libm WW3's Fortran calls on the host. These four are the ones the W3SRCE
// unit calls per sea point that glibc does not round correctly (see
// docs/GPU_LIBM_BITS_202610.md); CUDA's own expf, logf, powf and sincosf use other
// algorithms and are not expected to match.
//
// Algorithm and tables: ARM optimized-routines at f67e7daa (math/expf.c, logf.c, powf.c,
// sincosf.h, sincosf.c and their *_data.c), "MIT OR Apache-2.0 WITH LLVM-exception",
// Copyright (c) 1999-2022 Arm Limited; glibc 2.44 imports the same code (flt-32/e_expf.c
// and siblings). Configuration: glibc's x86_64 build, TOINT_INTRINSICS 0,
// POWF_SCALE_BITS 0, WANT_ROUNDING 1, WANT_ERRNO_UFLOW 1.
//
// On x86_64, glibc picks one of two builds of each function at load time (ifunc-fma.h):
// the "fma" build when the CPU has FMA and AVX2, otherwise "sse2". The two differ only in
// which a*b+c GCC fused when it compiled glibc with -mfma; the fma build's fused sites are
// read from the disassembly of __expf_fma, __logf_fma, __powf_fma and __sincosf_fma in
// /nix/store/h4wfwic1...-glibc-2.44-25/lib/libm.so.6. madd<Fma> reproduces either build:
// fused, ::fma (correctly rounded on the host and on a device); unfused, a*b then +c. The file must be compiled without contraction
// (-ffp-contract=off, nvcc --fmad=false), or the unfused build and the unfused sites of the
// fused build are fused by the compiler.
//
// Verified on the host by libm_bits.cpp against the toolchain's glibc (exhaustive for the
// unary functions); not run on a device (AGENTS.md, "Running on the GPU") ⚠.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <math.h>
#include <cstdint>

#if defined(__CUDACC__)
#define WW_LIBM_FN __host__ __device__ inline
#define WW_LIBM_DEVTAB static __constant__ const
#else
#define WW_LIBM_FN inline
#endif

namespace ww::libm {

namespace detail {

WW_LIBM_FN uint32_t asuint(float f) { uint32_t u; __builtin_memcpy(&u, &f, 4); return u; }
WW_LIBM_FN float asfloat(uint32_t u) { float f; __builtin_memcpy(&f, &u, 4); return f; }
WW_LIBM_FN uint64_t asuint64(double f) { uint64_t u; __builtin_memcpy(&u, &f, 8); return u; }
WW_LIBM_FN double asdouble(uint64_t u) { double f; __builtin_memcpy(&f, &u, 8); return f; }

template <bool Fma>
WW_LIBM_FN double madd(double a, double b, double c) {
  if constexpr (Fma) return ::fma(a, b, c);
  else return a * b + c;
}

// glibc's math_errf.c: the value each error path returns (errno is not modelled).
WW_LIBM_FN float xflowf(uint32_t sign, float y) { return (sign ? -y : y) * y; }
WW_LIBM_FN float oflowf(uint32_t sign) { return xflowf(sign, 0x1p97f); }
WW_LIBM_FN float uflowf(uint32_t sign) { return xflowf(sign, 0x1p-95f); }
WW_LIBM_FN float may_uflowf(uint32_t sign) { return xflowf(sign, 0x1.4p-75f); }
// glibc divides by zero to raise the flag; the value is the same infinity.
WW_LIBM_FN float divzerof(uint32_t sign) { return sign ? -INFINITY : INFINITY; }
WW_LIBM_FN float invalidf(float x) { return (x - x) / (x - x); }

// The tables, once for the host and once in constant memory for a device.
#define WW_LIBM_EXP2F_TAB                                                             \
  {0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f, 0x3fef9301d0125b51,   \
   0x3fef72b83c7d517b, 0x3fef54873168b9aa, 0x3fef387a6e756238, 0x3fef1e9df51fdee1,   \
   0x3fef06fe0a31b715, 0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,   \
   0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429, 0x3feea47eb03a5585,   \
   0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74, 0x3feea11473eb0187, 0x3feea589994cce13,   \
   0x3feeace5422aa0db, 0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,   \
   0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c, 0x3fef3720dcef9069,   \
   0x3fef5818dcfba487, 0x3fef7c97337b9b5f, 0x3fefa4afa2a490da, 0x3fefd0765b6e4540}
// logf_data.tab and powf_log2_data.tab share invc; logc differs (natural log, log2).
#define WW_LIBM_LOGF_TAB                                                              \
  {0x1.661ec79f8f3bep+0, -0x1.57bf7808caadep-2, 0x1.571ed4aaf883dp+0, -0x1.2bef0a7c06ddbp-2, \
   0x1.49539f0f010bp+0,  -0x1.01eae7f513a67p-2, 0x1.3c995b0b80385p+0, -0x1.b31d8a68224e9p-3, \
   0x1.30d190c8864a5p+0, -0x1.6574f0ac07758p-3, 0x1.25e227b0b8eap+0,  -0x1.1aa2bc79c81p-3,   \
   0x1.1bb4a4a1a343fp+0, -0x1.a4e76ce8c0e5ep-4, 0x1.12358f08ae5bap+0, -0x1.1973c5a611cccp-4, \
   0x1.0953f419900a7p+0, -0x1.252f438e10c1ep-5, 0x1p+0,               0x0p+0,               \
   0x1.e608cfd9a47acp-1, 0x1.aa5aa5df25984p-5,  0x1.ca4b31f026aap-1,  0x1.c5e53aa362eb4p-4,  \
   0x1.b2036576afce6p-1, 0x1.526e57720db08p-3,  0x1.9c2d163a1aa2dp-1, 0x1.bc2860d22477p-3,   \
   0x1.886e6037841edp-1, 0x1.1058bc8a07ee1p-2,  0x1.767dcf5534862p-1, 0x1.4043057b6ee09p-2}
#define WW_LIBM_POWF_TAB                                                              \
  {0x1.661ec79f8f3bep+0, -0x1.efec65b963019p-2, 0x1.571ed4aaf883dp+0, -0x1.b0b6832d4fca4p-2, \
   0x1.49539f0f010bp+0,  -0x1.7418b0a1fb77bp-2, 0x1.3c995b0b80385p+0, -0x1.39de91a6dcf7bp-2, \
   0x1.30d190c8864a5p+0, -0x1.01d9bf3f2b631p-2, 0x1.25e227b0b8eap+0,  -0x1.97c1d1b3b7afp-3,  \
   0x1.1bb4a4a1a343fp+0, -0x1.2f9e393af3c9fp-3, 0x1.12358f08ae5bap+0, -0x1.960cbbf788d5cp-4, \
   0x1.0953f419900a7p+0, -0x1.a6f9db6475fcep-5, 0x1p+0,               0x0p+0,               \
   0x1.e608cfd9a47acp-1, 0x1.338ca9f24f53dp-4,  0x1.ca4b31f026aap-1,  0x1.476a9543891bap-3,  \
   0x1.b2036576afce6p-1, 0x1.e840b4ac4e4d2p-3,  0x1.9c2d163a1aa2dp-1, 0x1.40645f0c6651cp-2,  \
   0x1.886e6037841edp-1, 0x1.88e9c2c1b9ff8p-2,  0x1.767dcf5534862p-1, 0x1.ce0a44eb17bccp-2}
// sincos_t: sign[4], hpi_inv (x 2^24, TOINT_INTRINSICS 0), hpi, c0..c4, s1..s3; the second
// entry negates the cosine polynomial (sincosf_data.c).
#define WW_LIBM_SINCOSF_TAB                                                           \
  {1.0, -1.0, -1.0, 1.0, 0x1.45F306DC9C883p+23, 0x1.921FB54442D18p0,                  \
   0x1p0, -0x1.ffffffd0c621cp-2, 0x1.55553e1068f19p-5, -0x1.6c087e89a359dp-10,        \
   0x1.99343027bf8c3p-16, -0x1.555545995a603p-3, 0x1.1107605230bc4p-7,                \
   -0x1.994eb3774cf24p-13,                                                            \
   1.0, -1.0, -1.0, 1.0, 0x1.45F306DC9C883p+23, 0x1.921FB54442D18p0,                  \
   -0x1p0, 0x1.ffffffd0c621cp-2, -0x1.55553e1068f19p-5, 0x1.6c087e89a359dp-10,        \
   -0x1.99343027bf8c3p-16, -0x1.555545995a603p-3, 0x1.1107605230bc4p-7,               \
   -0x1.994eb3774cf24p-13}
#define WW_LIBM_INV_PIO4_TAB                                                          \
  {0xa2,       0xa2f9,     0xa2f983,   0xa2f9836e, 0xf9836e4e, 0x836e4e44,             \
   0x6e4e4415, 0x4e441529, 0x441529fc, 0x1529fc27, 0x29fc2757, 0xfc2757d1,             \
   0x2757d1f5, 0x57d1f534, 0xd1f534dd, 0xf534ddc0, 0x34ddc0db, 0xddc0db62,             \
   0xc0db6295, 0xdb629599, 0x6295993c, 0x95993c43, 0x993c4390, 0x3c439041}

inline constexpr uint64_t exp2f_tab_h[32] = WW_LIBM_EXP2F_TAB;
inline constexpr double logf_tab_h[32] = WW_LIBM_LOGF_TAB;
inline constexpr double powf_tab_h[32] = WW_LIBM_POWF_TAB;
inline constexpr double sincosf_tab_h[28] = WW_LIBM_SINCOSF_TAB;
inline constexpr uint32_t inv_pio4_h[24] = WW_LIBM_INV_PIO4_TAB;
#if defined(__CUDACC__)
WW_LIBM_DEVTAB uint64_t exp2f_tab_d[32] = WW_LIBM_EXP2F_TAB;
WW_LIBM_DEVTAB double logf_tab_d[32] = WW_LIBM_LOGF_TAB;
WW_LIBM_DEVTAB double powf_tab_d[32] = WW_LIBM_POWF_TAB;
WW_LIBM_DEVTAB double sincosf_tab_d[28] = WW_LIBM_SINCOSF_TAB;
WW_LIBM_DEVTAB uint32_t inv_pio4_d[24] = WW_LIBM_INV_PIO4_TAB;
#endif
#if defined(__CUDA_ARCH__)
#define WW_LIBM_TAB(name) name##_d
#else
#define WW_LIBM_TAB(name) name##_h
#endif

// __exp2f_data: shift, invln2_scaled, poly_scaled (expf); shift_scaled, poly (powf).
inline constexpr double kExp2fShift = 0x1.8p+52;
inline constexpr double kExp2fShiftScaled = 0x1.8p+52 / 32;
inline constexpr double kInvLn2N = 0x1.71547652b82fep+0 * 32;
inline constexpr double kExpfC0 = 0x1.c6af84b912394p-5 / 32 / 32 / 32;
inline constexpr double kExpfC1 = 0x1.ebfce50fac4f3p-3 / 32 / 32;
inline constexpr double kExpfC2 = 0x1.62e42ff0c52d6p-1 / 32;
inline constexpr double kExp2C0 = 0x1.c6af84b912394p-5;
inline constexpr double kExp2C1 = 0x1.ebfce50fac4f3p-3;
inline constexpr double kExp2C2 = 0x1.62e42ff0c52d6p-1;
// __logf_data: ln2, poly; __powf_log2_data: poly.
inline constexpr double kLn2 = 0x1.62e42fefa39efp-1;
inline constexpr double kLogfA0 = -0x1.00ea348b88334p-2;
inline constexpr double kLogfA1 = 0x1.5575b0be00b6ap-2;
inline constexpr double kLogfA2 = -0x1.ffffef20a4123p-2;
inline constexpr double kPowfA0 = 0x1.27616c9496e0bp-2;
inline constexpr double kPowfA1 = -0x1.71969a075c67ap-2;
inline constexpr double kPowfA2 = 0x1.ec70a6ca7baddp-2;
inline constexpr double kPowfA3 = -0x1.7154748bef6c8p-1;
inline constexpr double kPowfA4 = 0x1.71547652ab82bp0;

WW_LIBM_FN uint32_t top12(float x) { return asuint(x) >> 20; }

// powf.c: log2(x) for a positive normalised (or pre-scaled subnormal) bit pattern ix.
template <bool Fma>
WW_LIBM_FN double log2_inline(uint32_t ix) {
  const uint32_t tmp = ix - 0x3f330000;
  const int i = (tmp >> (23 - 4)) % 16;
  const uint32_t top = tmp & 0xff800000;
  const uint32_t iz = ix - top;
  const int k = static_cast<int32_t>(top) >> 23;
  const double invc = WW_LIBM_TAB(powf_tab)[2 * i];
  const double logc = WW_LIBM_TAB(powf_tab)[2 * i + 1];
  const double z = asfloat(iz);
  const double r = madd<Fma>(z, invc, -1.0);
  const double y0 = logc + static_cast<double>(k);
  const double r2 = r * r;
  double y = madd<Fma>(kPowfA0, r, kPowfA1);
  const double p = madd<Fma>(kPowfA2, r, kPowfA3);
  const double r4 = r2 * r2;
  double q = madd<Fma>(kPowfA4, r, y0);
  q = madd<Fma>(p, r2, q);
  y = madd<Fma>(y, r4, q);
  return y;
}

// powf.c: 2^xd, the sign carried in the exponent's bias.
template <bool Fma>
WW_LIBM_FN double exp2_inline(double xd, uint32_t sign_bias) {
  double kd = xd + kExp2fShiftScaled;
  const uint64_t ki = asuint64(kd);
  kd -= kExp2fShiftScaled;
  const double r = xd - kd;
  uint64_t t = WW_LIBM_TAB(exp2f_tab)[ki % 32];
  const uint64_t ski = ki + sign_bias;
  t += ski << (52 - 5);
  const double s = asdouble(t);
  const double z = madd<Fma>(kExp2C0, r, kExp2C1);
  const double r2 = r * r;
  double y = madd<Fma>(kExp2C2, r, 1.0);
  y = madd<Fma>(z, r2, y);
  return y * s;
}

WW_LIBM_FN int checkint(uint32_t iy) {
  const int e = iy >> 23 & 0xff;
  if (e < 0x7f) return 0;
  if (e > 0x7f + 23) return 2;
  if (iy & ((1u << (0x7f + 23 - e)) - 1)) return 0;
  if (iy & (1u << (0x7f + 23 - e))) return 1;
  return 2;
}

WW_LIBM_FN bool zeroinfnan(uint32_t ix) { return 2 * ix - 1 >= 2u * 0x7f800000 - 1; }
WW_LIBM_FN bool issignaling(float x) {
  const uint32_t ix = asuint(x);
  return 2 * (ix ^ 0x00400000) > 2u * 0x7fc00000;
}

// sincosf.h, reduce_large: x mod pi/2 for |x| >= 120 with a 192-bit 4/pi.
WW_LIBM_FN double reduce_large(uint32_t xi, int* np) {
  const uint32_t* arr = &WW_LIBM_TAB(inv_pio4)[(xi >> 26) & 15];
  const int shift = (xi >> 23) & 7;
  xi = (xi & 0xffffff) | 0x800000;
  xi <<= shift;
  uint64_t res0 = xi * arr[0];
  const uint64_t res1 = static_cast<uint64_t>(xi) * arr[4];
  const uint64_t res2 = static_cast<uint64_t>(xi) * arr[8];
  res0 = (res2 >> 32) | (res0 << 32);
  res0 += res1;
  const uint64_t n = (res0 + (1ULL << 61)) >> 62;
  res0 -= n << 62;
  const double x = static_cast<double>(static_cast<int64_t>(res0));
  *np = static_cast<int>(n);
  return x * 0x1.921FB54442D18p-62;
}

// sincosf.h, sincosf_poly; p points at one 14-entry sincos_t.
template <bool Fma>
WW_LIBM_FN void sincosf_poly(double x, double x2, const double* p, int n, float* sinp,
                             float* cosp) {
  const double c0 = p[6], c1 = p[7], c2 = p[8], c3 = p[9], c4 = p[10];
  const double s1 = p[11], s2 = p[12], s3 = p[13];
  const double x4 = x2 * x2;
  const double x3 = x2 * x;
  const double cc2 = madd<Fma>(x2, c4, c3);
  const double ss1 = madd<Fma>(x2, s3, s2);
  if (n & 1) { float* tmp = sinp; sinp = cosp; cosp = tmp; }
  const double cc1 = madd<Fma>(x2, c1, c0);
  const double x5 = x3 * x2;
  const double x6 = x4 * x2;
  const double s = madd<Fma>(x3, s1, x);
  const double c = madd<Fma>(x4, c2, cc1);
  *sinp = static_cast<float>(madd<Fma>(x5, ss1, s));
  *cosp = static_cast<float>(madd<Fma>(x6, cc2, c));
}

}  // namespace detail

/// glibc 2.44 expf (flt-32/e_expf.c), the fma build when Fma is true.
template <bool Fma>
WW_LIBM_FN float expf(float x) {
  using namespace detail;
  const double xd = x;
  const uint32_t abstop = top12(x) & 0x7ff;
  if (abstop >= top12(88.0f)) {
    if (asuint(x) == asuint(-INFINITY)) return 0.0f;
    if (abstop >= top12(INFINITY)) return x + x;
    if (x > 0x1.62e42ep6f) return oflowf(0);
    if (x < -0x1.9fe368p6f) return uflowf(0);
    if (x < -0x1.9d1d9ep6f) return may_uflowf(0);
  }
  // The fma build fuses InvLn2N*xd into both of its uses (__expf_fma: vfmadd, vfmsub).
  double kd, r;
  if constexpr (Fma) {
    kd = ::fma(kInvLn2N, xd, kExp2fShift);
    const uint64_t ki = asuint64(kd);
    kd -= kExp2fShift;
    r = ::fma(kInvLn2N, xd, -kd);
    uint64_t t = WW_LIBM_TAB(exp2f_tab)[ki % 32];
    t += ki << (52 - 5);
    const double s = asdouble(t);
    const double z = ::fma(kExpfC0, r, kExpfC1);
    const double r2 = r * r;
    double y = ::fma(kExpfC2, r, 1.0);
    y = ::fma(z, r2, y);
    return static_cast<float>(y * s);
  } else {
    const double z0 = kInvLn2N * xd;
    kd = z0 + kExp2fShift;
    const uint64_t ki = asuint64(kd);
    kd -= kExp2fShift;
    r = z0 - kd;
    uint64_t t = WW_LIBM_TAB(exp2f_tab)[ki % 32];
    t += ki << (52 - 5);
    const double s = asdouble(t);
    const double z = kExpfC0 * r + kExpfC1;
    const double r2 = r * r;
    double y = kExpfC2 * r + 1.0;
    y = z * r2 + y;
    return static_cast<float>(y * s);
  }
}

/// glibc 2.44 logf (flt-32/e_logf.c).
template <bool Fma>
WW_LIBM_FN float logf(float x) {
  using namespace detail;
  uint32_t ix = asuint(x);
  if (ix == 0x3f800000) return 0;
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000) {
    if (ix * 2 == 0) return divzerof(1);
    if (ix == 0x7f800000) return x;
    if ((ix & 0x80000000) || ix * 2 >= 0xff000000) return invalidf(x);
    ix = asuint(x * 0x1p23f);
    ix -= 23 << 23;
  }
  const uint32_t tmp = ix - 0x3f330000;
  const int i = (tmp >> (23 - 4)) % 16;
  const int k = static_cast<int32_t>(tmp) >> 23;
  const uint32_t iz = ix - (tmp & 0xff800000);
  const double invc = WW_LIBM_TAB(logf_tab)[2 * i];
  const double logc = WW_LIBM_TAB(logf_tab)[2 * i + 1];
  const double z = asfloat(iz);
  const double r = madd<Fma>(z, invc, -1.0);
  const double y0 = madd<Fma>(static_cast<double>(k), kLn2, logc);
  const double r2 = r * r;
  double y = madd<Fma>(kLogfA1, r, kLogfA2);
  y = madd<Fma>(kLogfA0, r2, y);
  y = madd<Fma>(y, r2, y0 + r);
  return static_cast<float>(y);
}

/// glibc 2.44 powf (flt-32/e_powf.c).
template <bool Fma>
WW_LIBM_FN float powf(float x, float y) {
  using namespace detail;
  constexpr uint32_t kSignBias = 1u << (5 + 11);
  uint32_t sign_bias = 0;
  uint32_t ix = asuint(x);
  const uint32_t iy = asuint(y);
  if (ix - 0x00800000 >= 0x7f800000 - 0x00800000 || zeroinfnan(iy)) {
    if (zeroinfnan(iy)) {
      if (2 * iy == 0) return issignaling(x) ? x + y : 1.0f;
      if (ix == 0x3f800000) return issignaling(y) ? x + y : 1.0f;
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000) return x + y;
      if (2 * ix == 2 * 0x3f800000) return 1.0f;
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000)) return 0.0f;
      return y * y;
    }
    if (zeroinfnan(ix)) {
      float x2 = x * x;
      if (ix & 0x80000000 && checkint(iy) == 1) { x2 = -x2; sign_bias = 1; }
      if (2 * ix == 0 && iy & 0x80000000) return divzerof(sign_bias);
      return iy & 0x80000000 ? 1 / x2 : x2;
    }
    if (ix & 0x80000000) {
      const int yint = checkint(iy);
      if (yint == 0) return invalidf(x);
      if (yint == 1) sign_bias = kSignBias;
      ix &= 0x7fffffff;
    }
    if (ix < 0x00800000) {
      ix = asuint(x * 0x1p23f);
      ix &= 0x7fffffff;
      ix -= 23 << 23;
    }
  }
  const double ylogx = static_cast<double>(y) * log2_inline<Fma>(ix);
  if ((asuint64(ylogx) >> 47 & 0xffff) >= asuint64(126.0) >> 47) {
    if (ylogx <= -150.0) return uflowf(sign_bias);
    if (ylogx < -149.0) return may_uflowf(sign_bias);
    if (ylogx > 0x1.fffffffa3aae2p+6) {
      if (ylogx > 0x1.fffffffd1d571p+6) return oflowf(sign_bias);
      // glibc tests 1.0f + 0x1p-25f against 1.0f here, which round-to-nearest makes false.
      return sign_bias ? -0x1.fffffep127f : 0x1.fffffep127f;
    }
  }
  return static_cast<float>(exp2_inline<Fma>(ylogx, sign_bias));
}

/// glibc 2.44 sincosf (flt-32/s_sincosf.c). GCC's fma build pairs the sine and cosine
/// polynomials in one SIMD register; each lane is the scalar arithmetic below.
template <bool Fma>
WW_LIBM_FN void sincosf(float y, float* sinp, float* cosp) {
  using namespace detail;
  const double* tab = WW_LIBM_TAB(sincosf_tab);
  double x = y;
  const uint32_t abstop = top12(y) & 0x7ff;
  if (abstop < (top12(0x1.921FB6p-1f) & 0x7ff)) {
    const double x2 = x * x;
    if (abstop < (top12(0x1p-12f) & 0x7ff)) { *sinp = y; *cosp = 1.0f; return; }
    sincosf_poly<Fma>(x, x2, tab, 0, sinp, cosp);
  } else if (abstop < (top12(120.0f) & 0x7ff)) {
    const double r = x * tab[4];
    const int n = (static_cast<int32_t>(r) + 0x800000) >> 24;
    x = madd<Fma>(-static_cast<double>(n), tab[5], x);
    const double s = tab[n & 3];
    const double* p = (n & 2) ? tab + 14 : tab;
    sincosf_poly<Fma>(x * s, x * x, p, n, sinp, cosp);
  } else if (abstop < (top12(INFINITY) & 0x7ff)) {
    const uint32_t xi = asuint(y);
    const int sign = xi >> 31;
    int n;
    x = reduce_large(xi, &n);
    const double s = tab[(n + sign) & 3];
    const double* p = ((n + sign) & 2) ? tab + 14 : tab;
    sincosf_poly<Fma>(x * s, x * x, p, n, sinp, cosp);
  } else {
    *sinp = *cosp = y - y;
  }
}

}  // namespace ww::libm
