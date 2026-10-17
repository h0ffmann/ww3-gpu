// kokkos/tools/libm_bits/glibc_mathf.hpp
// glibc 2.44's scalar expf, logf, powf, sincosf and double exp, and one lane of libmvec's SSE4
// _ZGVbN4v_expf and _ZGVbN4vv_powf, written so a CUDA device computes the same bits as the libm
// WW3's Fortran calls on the host. These are the per-point functions of the W3SRCE unit that
// glibc does not round correctly (docs/GPU_LIBM_BITS_202610.md; the correctly rounded ones are
// in coremath_f.hpp and coremath_d.hpp); CUDA's own functions use other algorithms and are not
// expected to match.
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

// exp_data.tab, N = 128 (math/exp_data.c); equal to glibc 2.44's __exp_data.tab (v).
#define WW_LIBM_EXP_TAB {\
   0x0, 0x3ff0000000000000, 0x3c9b3b4f1a88bf6e, 0x3feff63da9fb3335, \
   0xbc7160139cd8dc5d, 0x3fefec9a3e778061, 0xbc905e7a108766d1, 0x3fefe315e86e7f85, \
   0x3c8cd2523567f613, 0x3fefd9b0d3158574, 0xbc8bce8023f98efa, 0x3fefd06b29ddf6de, \
   0x3c60f74e61e6c861, 0x3fefc74518759bc8, 0x3c90a3e45b33d399, 0x3fefbe3ecac6f383, \
   0x3c979aa65d837b6d, 0x3fefb5586cf9890f, 0x3c8eb51a92fdeffc, 0x3fefac922b7247f7, \
   0x3c3ebe3d702f9cd1, 0x3fefa3ec32d3d1a2, 0xbc6a033489906e0b, 0x3fef9b66affed31b, \
   0xbc9556522a2fbd0e, 0x3fef9301d0125b51, 0xbc5080ef8c4eea55, 0x3fef8abdc06c31cc, \
   0xbc91c923b9d5f416, 0x3fef829aaea92de0, 0x3c80d3e3e95c55af, 0x3fef7a98c8a58e51, \
   0xbc801b15eaa59348, 0x3fef72b83c7d517b, 0xbc8f1ff055de323d, 0x3fef6af9388c8dea, \
   0x3c8b898c3f1353bf, 0x3fef635beb6fcb75, 0xbc96d99c7611eb26, 0x3fef5be084045cd4, \
   0x3c9aecf73e3a2f60, 0x3fef54873168b9aa, 0xbc8fe782cb86389d, 0x3fef4d5022fcd91d, \
   0x3c8a6f4144a6c38d, 0x3fef463b88628cd6, 0x3c807a05b0e4047d, 0x3fef3f49917ddc96, \
   0x3c968efde3a8a894, 0x3fef387a6e756238, 0x3c875e18f274487d, 0x3fef31ce4fb2a63f, \
   0x3c80472b981fe7f2, 0x3fef2b4565e27cdd, 0xbc96b87b3f71085e, 0x3fef24dfe1f56381, \
   0x3c82f7e16d09ab31, 0x3fef1e9df51fdee1, 0xbc3d219b1a6fbffa, 0x3fef187fd0dad990, \
   0x3c8b3782720c0ab4, 0x3fef1285a6e4030b, 0x3c6e149289cecb8f, 0x3fef0cafa93e2f56, \
   0x3c834d754db0abb6, 0x3fef06fe0a31b715, 0x3c864201e2ac744c, 0x3fef0170fc4cd831, \
   0x3c8fdd395dd3f84a, 0x3feefc08b26416ff, 0xbc86a3803b8e5b04, 0x3feef6c55f929ff1, \
   0xbc924aedcc4b5068, 0x3feef1a7373aa9cb, 0xbc9907f81b512d8e, 0x3feeecae6d05d866, \
   0xbc71d1e83e9436d2, 0x3feee7db34e59ff7, 0xbc991919b3ce1b15, 0x3feee32dc313a8e5, \
   0x3c859f48a72a4c6d, 0x3feedea64c123422, 0xbc9312607a28698a, 0x3feeda4504ac801c, \
   0xbc58a78f4817895b, 0x3feed60a21f72e2a, 0xbc7c2c9b67499a1b, 0x3feed1f5d950a897, \
   0x3c4363ed60c2ac11, 0x3feece086061892d, 0x3c9666093b0664ef, 0x3feeca41ed1d0057, \
   0x3c6ecce1daa10379, 0x3feec6a2b5c13cd0, 0x3c93ff8e3f0f1230, 0x3feec32af0d7d3de, \
   0x3c7690cebb7aafb0, 0x3feebfdad5362a27, 0x3c931dbdeb54e077, 0x3feebcb299fddd0d, \
   0xbc8f94340071a38e, 0x3feeb9b2769d2ca7, 0xbc87deccdc93a349, 0x3feeb6daa2cf6642, \
   0xbc78dec6bd0f385f, 0x3feeb42b569d4f82, 0xbc861246ec7b5cf6, 0x3feeb1a4ca5d920f, \
   0x3c93350518fdd78e, 0x3feeaf4736b527da, 0x3c7b98b72f8a9b05, 0x3feead12d497c7fd, \
   0x3c9063e1e21c5409, 0x3feeab07dd485429, 0x3c34c7855019c6ea, 0x3feea9268a5946b7, \
   0x3c9432e62b64c035, 0x3feea76f15ad2148, 0xbc8ce44a6199769f, 0x3feea5e1b976dc09, \
   0xbc8c33c53bef4da8, 0x3feea47eb03a5585, 0xbc845378892be9ae, 0x3feea34634ccc320, \
   0xbc93cedd78565858, 0x3feea23882552225, 0x3c5710aa807e1964, 0x3feea155d44ca973, \
   0xbc93b3efbf5e2228, 0x3feea09e667f3bcd, 0xbc6a12ad8734b982, 0x3feea012750bdabf, \
   0xbc6367efb86da9ee, 0x3fee9fb23c651a2f, 0xbc80dc3d54e08851, 0x3fee9f7df9519484, \
   0xbc781f647e5a3ecf, 0x3fee9f75e8ec5f74, 0xbc86ee4ac08b7db0, 0x3fee9f9a48a58174, \
   0xbc8619321e55e68a, 0x3fee9feb564267c9, 0x3c909ccb5e09d4d3, 0x3feea0694fde5d3f, \
   0xbc7b32dcb94da51d, 0x3feea11473eb0187, 0x3c94ecfd5467c06b, 0x3feea1ed0130c132, \
   0x3c65ebe1abd66c55, 0x3feea2f336cf4e62, 0xbc88a1c52fb3cf42, 0x3feea427543e1a12, \
   0xbc9369b6f13b3734, 0x3feea589994cce13, 0xbc805e843a19ff1e, 0x3feea71a4623c7ad, \
   0xbc94d450d872576e, 0x3feea8d99b4492ed, 0x3c90ad675b0e8a00, 0x3feeaac7d98a6699, \
   0x3c8db72fc1f0eab4, 0x3feeace5422aa0db, 0xbc65b6609cc5e7ff, 0x3feeaf3216b5448c, \
   0x3c7bf68359f35f44, 0x3feeb1ae99157736, 0xbc93091fa71e3d83, 0x3feeb45b0b91ffc6, \
   0xbc5da9b88b6c1e29, 0x3feeb737b0cdc5e5, 0xbc6c23f97c90b959, 0x3feeba44cbc8520f, \
   0xbc92434322f4f9aa, 0x3feebd829fde4e50, 0xbc85ca6cd7668e4b, 0x3feec0f170ca07ba, \
   0x3c71affc2b91ce27, 0x3feec49182a3f090, 0x3c6dd235e10a73bb, 0x3feec86319e32323, \
   0xbc87c50422622263, 0x3feecc667b5de565, 0x3c8b1c86e3e231d5, 0x3feed09bec4a2d33, \
   0xbc91bbd1d3bcbb15, 0x3feed503b23e255d, 0x3c90cc319cee31d2, 0x3feed99e1330b358, \
   0x3c8469846e735ab3, 0x3feede6b5579fdbf, 0xbc82dfcd978e9db4, 0x3feee36bbfd3f37a, \
   0x3c8c1a7792cb3387, 0x3feee89f995ad3ad, 0xbc907b8f4ad1d9fa, 0x3feeee07298db666, \
   0xbc55c3d956dcaeba, 0x3feef3a2b84f15fb, 0xbc90a40e3da6f640, 0x3feef9728de5593a, \
   0xbc68d6f438ad9334, 0x3feeff76f2fb5e47, 0xbc91eee26b588a35, 0x3fef05b030a1064a, \
   0x3c74ffd70a5fddcd, 0x3fef0c1e904bc1d2, 0xbc91bdfbfa9298ac, 0x3fef12c25bd71e09, \
   0x3c736eae30af0cb3, 0x3fef199bdd85529c, 0x3c8ee3325c9ffd94, 0x3fef20ab5fffd07a, \
   0x3c84e08fd10959ac, 0x3fef27f12e57d14b, 0x3c63cdaf384e1a67, 0x3fef2f6d9406e7b5, \
   0x3c676b2c6c921968, 0x3fef3720dcef9069, 0xbc808a1883ccb5d2, 0x3fef3f0b555dc3fa, \
   0xbc8fad5d3ffffa6f, 0x3fef472d4a07897c, 0xbc900dae3875a949, 0x3fef4f87080d89f2, \
   0x3c74a385a63d07a7, 0x3fef5818dcfba487, 0xbc82919e2040220f, 0x3fef60e316c98398, \
   0x3c8e5a50d5c192ac, 0x3fef69e603db3285, 0x3c843a59ac016b4b, 0x3fef7321f301b460, \
   0xbc82d52107b43e1f, 0x3fef7c97337b9b5f, 0xbc892ab93b470dc9, 0x3fef864614f5a129, \
   0x3c74b604603a88d3, 0x3fef902ee78b3ff6, 0x3c83c5ec519d7271, 0x3fef9a51fbc74c83, \
   0xbc8ff7128fd391f0, 0x3fefa4afa2a490da, 0xbc8dae98e223747d, 0x3fefaf482d8e67f1, \
   0x3c8ec3bc41aa2008, 0x3fefba1bee615a27, 0x3c842b94c3a9eb32, 0x3fefc52b376bba97, \
   0x3c8a64a931d185ee, 0x3fefd0765b6e4540, 0xbc8e37bae43be3ed, 0x3fefdbfdad9cbe14, \
   0x3c77893b4d91cd9d, 0x3fefe7c1819e90d8, 0x3c5305c14160cc89, 0x3feff3c22b8f71f1, \
}
inline constexpr uint64_t exp_tab_h[256] = WW_LIBM_EXP_TAB;
inline constexpr uint64_t exp2f_tab_h[32] = WW_LIBM_EXP2F_TAB;
inline constexpr double logf_tab_h[32] = WW_LIBM_LOGF_TAB;
inline constexpr double powf_tab_h[32] = WW_LIBM_POWF_TAB;
inline constexpr double sincosf_tab_h[28] = WW_LIBM_SINCOSF_TAB;
inline constexpr uint32_t inv_pio4_h[24] = WW_LIBM_INV_PIO4_TAB;
#if defined(__CUDACC__)
WW_LIBM_DEVTAB uint64_t exp_tab_d[256] = WW_LIBM_EXP_TAB;
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

/// One lane of glibc 2.44's libmvec _ZGVbN4v_expf, its SSE4 build
/// (sysdeps/x86_64/fpu/multiarch/svml_s_expf4_core_sse4.S, constants svml_s_expf_data.S):
/// single-precision arithmetic, every product rounded; a lane with |x| above the domain bound
/// (or NaN) is recomputed by scalar expf, the fma or sse2 build as the host got it. Written
/// from the algorithm and its constants, not copied (the assembly is glibc's, LGPL).
template <bool Fma>
WW_LIBM_FN float mvec_expf(float x) {
  using namespace detail;
  if (static_cast<int32_t>(asuint(x) & 0x7fffffff) > static_cast<int32_t>(0x42aeac4f))
    return expf<Fma>(x);
  const float inv_ln2 = asfloat(0x3fb8aa3b), shifter = asfloat(0x4b400000);
  const float ln2hi = asfloat(0x3f317200), ln2lo = asfloat(0x35bfbe8e);
  const float m = x * inv_ln2 + shifter;
  const float n = m - shifter;
  float r = x - n * ln2hi;
  r = r - n * ln2lo;
  float p = asfloat(0x3c07d9fe) * r + asfloat(0x3d2b8392);
  p = p * r + asfloat(0x3e2aacac);
  p = p * r + asfloat(0x3effff34);
  p = p * r + asfloat(0x3f7ffffe);
  p = r * p + asfloat(0x3f800000);
  return asfloat((asuint(m) + 0x7f) << 23) * p;
}

/// glibc 2.44 exp, double (dbl-64/e_exp.c, ARM optimized-routines math/exp.c, N = 128), as
/// __ieee754_exp_fma (Fma) or its sse2/avx builds; the fused sites are read from the
/// disassembly of __ieee754_exp_fma. W3SDB1 calls it per sea point.
template <bool Fma>
WW_LIBM_FN double exp(double x) {
  using namespace detail;
  constexpr double kInvLn2N = 0x1.71547652b82fep0 * 128, kNegLn2hiN = -0x1.62e42fefa0000p-8,
                   kNegLn2loN = -0x1.cf79abc9e3b3ap-47, kShift = 0x1.8p52;
  constexpr double C2 = 0x1.ffffffffffdbdp-2, C3 = 0x1.555555555543cp-3,
                   C4 = 0x1.55555cf172b91p-5, C5 = 0x1.1111167a4d017p-7;
  uint32_t abstop = static_cast<uint32_t>(asuint64(x) >> 52) & 0x7ff;
  if (abstop - 0x3c9u >= 0x408u - 0x3c9u) {
    if (abstop - 0x3c9u >= 0x80000000u) return 1.0 + x;
    if (abstop >= 0x409u) {
      if (asuint64(x) == asuint64(-INFINITY)) return 0.0;
      if (abstop >= 0x7ffu) return 1.0 + x;
      // glibc's __math_uflow / __math_oflow: 0x1p-767 squared and 0x1p769 squared.
      return (asuint64(x) >> 63) ? 0.0 : INFINITY;
    }
    abstop = 0;
  }
  double kd = madd<Fma>(kInvLn2N, x, kShift);
  const uint64_t ki = asuint64(kd);
  kd -= kShift;
  double r;
  if constexpr (Fma) {
    r = ::fma(kd, kNegLn2hiN, x);
    r = ::fma(kd, kNegLn2loN, r);
  } else {
    r = x + kd * kNegLn2hiN + kd * kNegLn2loN;
  }
  const uint64_t idx = 2 * (ki % 128);
  const uint64_t top = ki << (52 - 7);
  const double tail = asdouble(WW_LIBM_TAB(exp_tab)[idx]);
  uint64_t sbits = WW_LIBM_TAB(exp_tab)[idx + 1] + top;
  const double r2 = r * r;
  const double p1 = madd<Fma>(r, C3, C2);
  const double p2 = madd<Fma>(r, C5, C4);
  const double tmp = madd<Fma>(r2 * r2, p2, madd<Fma>(r2, p1, tail + r));
  if (abstop == 0) {  // specialcase
    if ((ki & 0x80000000) == 0) {
      sbits -= 1009ull << 52;
      const double scale = asdouble(sbits);
      return 0x1p1009 * madd<Fma>(scale, tmp, scale);
    }
    sbits += 1022ull << 52;
    const double scale = asdouble(sbits);
    const double st = scale * tmp;
    double y = scale + st;
    if (y < 1.0) {
      double lo = scale - y + st;
      const double hi = 1.0 + y;
      lo = 1.0 - hi + y + lo;
      y = (hi + lo) - 1.0;
      if (y == 0.0) y = 0.0;
    }
    return 0x1p-1022 * y;
  }
  const double scale = asdouble(sbits);
  return madd<Fma>(scale, tmp, scale);
}

#if defined(WW_LIBM_RCP_RUNS)
// The tables of mvec_powf, from build/libm_bits/svml_tables.hpp (gen_tables.cpp, passed with
// -include): libmvec's own log and exp2 tables, and this CPU's rcpps as runs of equal R.
namespace detail {
inline constexpr uint64_t powf4_la_h[520] = WW_LIBM_POWF4_LA;
inline constexpr uint64_t powf4_t_h[2048] = WW_LIBM_POWF4_T;
inline constexpr uint32_t rcp_start_h[WW_LIBM_RCP_RUNS] = WW_LIBM_RCP_START;
inline constexpr uint32_t rcp_r_h[WW_LIBM_RCP_RUNS] = WW_LIBM_RCP_R;
#if defined(__CUDACC__)
WW_LIBM_DEVTAB uint64_t powf4_la_d[520] = WW_LIBM_POWF4_LA;
WW_LIBM_DEVTAB uint64_t powf4_t_d[2048] = WW_LIBM_POWF4_T;
WW_LIBM_DEVTAB uint32_t rcp_start_d[WW_LIBM_RCP_RUNS] = WW_LIBM_RCP_START;
WW_LIBM_DEVTAB uint32_t rcp_r_d[WW_LIBM_RCP_RUNS] = WW_LIBM_RCP_R;
#endif

// round-to-nearest-even of the host's rcpps(1.m x 2^-10), m a 23-bit mantissa.
WW_LIBM_FN double rcp_rounded(uint32_t m) {
  const uint32_t* st = WW_LIBM_TAB(rcp_start);
  int lo = 0, hi = WW_LIBM_RCP_RUNS - 1;
  while (lo < hi) {
    const int mid = (lo + hi + 1) / 2;
    if (st[mid] <= m) lo = mid; else hi = mid - 1;
  }
  return static_cast<double>(WW_LIBM_TAB(rcp_r)[lo]);
}
}  // namespace detail

/// One lane of glibc 2.44's libmvec _ZGVbN4vv_powf, its SSE4 build
/// (sysdeps/x86_64/fpu/multiarch/svml_s_powf4_core_sse4.S): log(x) from a 10-bit reciprocal
/// R and a table, times y, then 2^(.) from a 2^11-entry table, in double precision with every
/// product rounded. A lane the library sends to scalar powf (x not a positive normal float
/// below FLT_MAX, y infinite or NaN, |y log x| beyond the domain) is computed by powf here
/// too. Written from the algorithm and its constants; the tables are read from the host's
/// libmvec at build time, R from the host's rcpps, so a device computes what this host does.
template <bool Fma>
WW_LIBM_FN float mvec_powf(float x, float y) {
  using namespace detail;
  const uint32_t xi = asuint(x), yi = asuint(y);
  if (static_cast<int32_t>(xi - 0x80800000u) >= static_cast<int32_t>(0xfeffffffu) ||
      static_cast<int32_t>(yi & 0x7fffffff) >= 0x7f800000)
    return powf<Fma>(x, y);
  const double xd = x, yd = y;
  const uint64_t xb = asuint64(xd);
  const double x1 = asdouble((xb & 0x000fffffffffffffULL) | 0x3f50000000000000ULL);
  const double ed = static_cast<double>(static_cast<int32_t>(xb >> 52));
  const double R = rcp_rounded(xi & 0x7fffff);
  const double q = x1 * R - 1.0;
  const double bias = R > 724.0 ? 1023.0 : 1022.0;
  const double q2 = q * q;
  double t = asdouble(0x3fd55555555543c5ULL) * q + asdouble(0xbfdffffffffff81fULL);
  t = t * q2;
  const double qq = q + t;
  const double s = asdouble(WW_LIBM_TAB(powf4_la)[static_cast<int>(R) - 512]) + qq;
  double v = (ed - bias) * asdouble(0x3fe62e42fefa39efULL) + s;
  v = v * yd;
  if (static_cast<int32_t>(static_cast<uint32_t>(asuint64(v) >> 32) & 0x7fffffff) >
      static_cast<int32_t>(0x4059fe36))
    return powf<Fma>(x, y);
  const double shifter = asdouble(0x4338000000000000ULL);
  const double w = asdouble(0x40a71547652b82feULL) * v;
  const double tt = (w - 0.5) + shifter;
  const double k = tt - shifter;
  const double f = (w - k) * asdouble(0x3f362f3904051fa1ULL);
  const uint64_t tb = asuint64(tt);
  const double T = asdouble(WW_LIBM_TAB(powf4_t)[tb & 0x7ff]);
  const double res = T + f * T;
  return static_cast<float>(asdouble(asuint64(res) + ((tb >> 11) << 52)));
}
#endif

}  // namespace ww::libm
