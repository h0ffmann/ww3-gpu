// kokkos/tools/libm_bits/coremath_f.hpp
// glibc 2.44's tanhf, log10f and atan2f, correctly rounded, for host and device code. glibc
// imports them from the CORE-MATH project (sysdeps/ieee754/flt-32/s_tanhf.c, e_log10f.c,
// e_atan2f.c); they are copied here with their notices, under the MIT licence they carry. Since
// glibc's results are correctly rounded (libm_bits cr: every input of tanhf and log10f), a copy
// that equals glibc on every input (libm_bits ww tanhf, log10f, atan2f) is too, and so is a
// device build with the same operations. Changes from glibc's text: C++ for host and device,
// tables as constexpr locals, glibc's error helpers replaced by glibc_mathf.hpp's, errno and the
// SVID wrappers dropped. Compile without contraction (-ffp-contract=off, nvcc --fmad=false):
// the fused operations are the explicit fma calls.
//
// SPDX-License-Identifier: MIT
#pragma once

#include "glibc_mathf.hpp"

namespace ww::libm {

/* Correctly-rounded hyperbolic tangent function for binary32 value.

Copyright (c) 2022-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/tanh/tanhf.c, revision b1ecd83).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
WW_LIBM_FN float tanhf(float x) {
  using namespace detail;
  double z = x;
  uint32_t ux = asuint(x);
  int e = (ux >> 23) & 0xff;
  if (e == 0xff) {
    if (ux << 9) return x + x; /* x = nan */
    return (ux >> 31) ? -1.0f : 1.0f; /* x = +-inf */
  }
  if (e < 115) {
    if (e < 102) {
      if ((ux << 1) == 0) return x;
      return ::fmaf(-x, ::fabsf(x), x);
    }
    float x2 = x * x;
    return ::fmaf(x, -0x1.555556p-2f * x2, x);
  }
  if ((ux << 1) > (0x41102cb3u << 1)) return ::copysignf(1.0f, x) - ::copysignf(0x1p-25f, x);
  double z2 = z * z;
  double z4 = z2 * z2;
  double z8 = z4 * z4;
  constexpr double cn[] = {0x1p+0,                0x1.30877b8b72d33p-3,  0x1.694aa09ae9e5ep-8,
                           0x1.4101377abb729p-14, 0x1.e0392b1db0018p-22, 0x1.2533756e546f7p-30,
                           0x1.d62e5abe6ae8ap-41, 0x1.b06be534182dep-54};
  constexpr double cd[] = {0x1p+0,                0x1.ed99131b0ebeap-2,  0x1.0d27ed6c95a69p-5,
                           0x1.7cbdaca0e9fccp-11, 0x1.b4e60b892578ep-18, 0x1.a6f707c5c71abp-26,
                           0x1.35a8b6e2cd94cp-35, 0x1.ca8230677aa01p-47};
  double n0 = cn[0] + z2 * cn[1];
  double n2 = cn[2] + z2 * cn[3];
  double n4 = cn[4] + z2 * cn[5];
  double n6 = cn[6] + z2 * cn[7];
  n0 += z4 * n2;
  n4 += z4 * n6;
  n0 += z8 * n4;
  double d0 = cd[0] + z2 * cd[1];
  double d2 = cd[2] + z2 * cd[3];
  double d4 = cd[4] + z2 * cd[5];
  double d6 = cd[6] + z2 * cd[7];
  d0 += z4 * d2;
  d4 += z4 * d6;
  d0 += z8 * d4;
  double r = z * n0 / d0;
  return static_cast<float>(r);
}

/* Correctly-rounded radix-10 logarithm function for binary32 value.

Copyright (c) 2022-2026 Alexei Sibidanov.

This file is part of the CORE-MATH project
project (file src/binary32/log10/log10f.c, revision ebff4c43).

(The permission notice is the one above, word for word, in glibc's file.)
*/
WW_LIBM_FN float log10f(float x) {
  using namespace detail;
  // reciprocal of 1+i/64 (i=0..64) rounded to 29 bits
  constexpr double tr[] = {
      0x1p+0,         0x1.f81f82p-1,  0x1.f07c1fp-1,  0x1.e9131acp-1, 0x1.e1e1e1ep-1, 0x1.dae6077p-1,
      0x1.d41d41dp-1, 0x1.cd85689p-1, 0x1.c71c71cp-1, 0x1.c0e0704p-1, 0x1.bacf915p-1, 0x1.b4e81b5p-1,
      0x1.af286bdp-1, 0x1.a98ef6p-1,  0x1.a41a41ap-1, 0x1.9ec8e95p-1, 0x1.999999ap-1, 0x1.948b0fdp-1,
      0x1.8f9c19p-1,  0x1.8acb90fp-1, 0x1.8618618p-1, 0x1.8181818p-1, 0x1.7d05f41p-1, 0x1.78a4c81p-1,
      0x1.745d174p-1, 0x1.702e05cp-1, 0x1.6c16c17p-1, 0x1.6816817p-1, 0x1.642c859p-1, 0x1.605816p-1,
      0x1.5c9882cp-1, 0x1.58ed231p-1, 0x1.5555555p-1, 0x1.51d07ebp-1, 0x1.4e5e0a7p-1, 0x1.4afd6ap-1,
      0x1.47ae148p-1, 0x1.446f865p-1, 0x1.4141414p-1, 0x1.3e22cbdp-1, 0x1.3b13b14p-1, 0x1.3813814p-1,
      0x1.3521cfbp-1, 0x1.323e34ap-1, 0x1.2f684bep-1, 0x1.2c9fb4ep-1, 0x1.29e412ap-1, 0x1.27350b9p-1,
      0x1.2492492p-1, 0x1.21fb781p-1, 0x1.1f7047ep-1, 0x1.1cf06aep-1, 0x1.1a7b961p-1, 0x1.1811812p-1,
      0x1.15b1e5fp-1, 0x1.135c811p-1, 0x1.1111111p-1, 0x1.0ecf56cp-1, 0x1.0c9715p-1,  0x1.0a6810ap-1,
      0x1.0842108p-1, 0x1.0624dd3p-1, 0x1.041041p-1,  0x1.0204081p-1, 0.5};
  // logarithms of the reciprocals with offset
  constexpr double tl[] = {
      -0x1.2p-46,           0x1.b947689310dfap-8, 0x1.b5e909c96d11bp-7, 0x1.45f4f59ed1e08p-6,
      0x1.af5f92cbd8bc1p-6, 0x1.0ba01a606dcdep-5, 0x1.3ed119b9a29cdp-5, 0x1.714834298ed14p-5,
      0x1.a30a9d983564cp-5, 0x1.d41d512670665p-5, 0x1.02428c0f65442p-4, 0x1.1a23444eecb67p-4,
      0x1.31b30543f4bddp-4, 0x1.48f3ed39bfc2dp-4, 0x1.5fe8049a0e34bp-4, 0x1.769140a6a9f3p-4,
      0x1.8cf1836c98bdbp-4, 0x1.a30a9d55540cap-4, 0x1.b8de4d1ee8167p-4, 0x1.ce6e4202ca20fp-4,
      0x1.e3bc1accacd3p-4,  0x1.f8c9683b5aafdp-4, 0x1.06cbd68ca9a03p-3, 0x1.11142f19df6c5p-3,
      0x1.1b3e71fa7a913p-3, 0x1.254b4d37a4677p-3, 0x1.2f3b6912cbe9cp-3, 0x1.390f68311581ap-3,
      0x1.42c7e7fffc53dp-3, 0x1.4c65808c78cd1p-3, 0x1.55e8c50751beap-3, 0x1.5f52445dec36cp-3,
      0x1.68a288c3f1195p-3, 0x1.71da17bdf0cadp-3, 0x1.7af973608af6ep-3, 0x1.84011952a250ep-3,
      0x1.8cf1837a7e9f4p-3, 0x1.95cb2891e436ap-3, 0x1.9e8e7b0f86974p-3, 0x1.a73beaa5db121p-3,
      0x1.afd3e39455867p-3, 0x1.b856cf060d985p-3, 0x1.c0c5134de1f9p-3,  0x1.c91f1371bc934p-3,
      0x1.d1652ffcd3ee8p-3, 0x1.d997c6f635e09p-3, 0x1.e1b733ab90edp-3,  0x1.e9c3ceadac7ebp-3,
      0x1.f1bdeec43a29ap-3, 0x1.f9a5e7a5fa392p-3, 0x1.00be05ac02ef5p-2, 0x1.04a054d81a29ep-2,
      0x1.087a0835957c5p-2, 0x1.0c4b4570994e1p-2, 0x1.101431aa1fe1bp-2, 0x1.13d4f08b98da2p-2,
      0x1.178da53edb85cp-2, 0x1.1b3e71e9f9d22p-2, 0x1.1ee777defdeb7p-2, 0x1.2288d7b48e205p-2,
      0x1.2622b0f52e469p-2, 0x1.29b522a4c62dep-2, 0x1.2d404b0e30f4ap-2, 0x1.30c4478f3fbafp-2,
      0x1.34413509f78dfp-2};
  // 10^n, as bit patterns
  constexpr float stf[] = {0x1.2a05f2p+33f, 0x1.4p+3f,     0x1.9p+6f,      0.0f,
                           0x1.f4p+9f,      0.0f,          0x1.388p+13f,   0x1.86ap+16f,
                           0.0f,            0x1.e848p+19f, 0.0f,           0x1.312dp+23f,
                           0x1.7d784p+26f,  0.0f,          0x1.dcd65p+29f, 0x1p+0f};
  constexpr double b[] = {0x1.bcb7b15d35067p-2, -0x1.bcbb1cd29cbafp-3, 0x1.2870e2624ce4ep-3};
  constexpr double c[] = {0x1.bcb7b1526e50ep-2,  -0x1.bcb7b1526e53dp-3, 0x1.287a7636f3fa2p-3,
                          -0x1.bcb7b146a14b3p-4, 0x1.63c627d5219cbp-4,  -0x1.2880736c8762dp-4,
                          0x1.fc1ecf913961ap-5};
  // ln(2)/ln(10)
  const double ln10 = 0x1.34413509f79ffp-2, ln10h = 0x1.34413509f7ap-2,
               ln10l = -0x1.0cee0ed4ca7e9p-54;
  uint32_t ux = asuint(x);
  if (ux >= 0x7f800000u) {  // <=-0, nan, inf (as_special)
    const uint32_t ax = ux << 1;
    if (ax == 0u) return divzerof(1);
    if (ux == 0x7f800000u) return x;
    if (ax > 0xff000000u) return x + x;
    return invalidf(x);
  }
  if (ux == asuint(stf[(ux >> 24) & 0xf])) {  // x = 10^n
    unsigned je = (static_cast<int>(ux) >> 23) - 126;
    je = (je * 0x4d104d4) >> 28;
    return static_cast<float>(je);
  }
  if (ux < 0x00800000u) {
    if (ux == 0u) return divzerof(1);  // x=+0
    // subnormal
#if defined(__CUDA_ARCH__)
    int n = __clz(static_cast<int>(ux)) - 8;
#else
    int n = __builtin_clz(ux) - 8;
#endif
    ux <<= n;
    ux -= n << 23;
  }
  int e = (static_cast<int>(ux) >> 23) - 127;
  int64_t m = ux & ((1 << 23) - 1), j = (m + (1 << (23 - 7))) >> (23 - 6);
  double tz = asdouble((static_cast<uint64_t>(m) << (52 - 23)) | (UINT64_C(0x3ff) << 52));
  double z = tz * tr[j] - 1, z2 = z * z;
  double r = ((e * ln10 + tl[j]) + z * b[0]) + z2 * (b[1] + z * b[2]);
  float ub = static_cast<float>(r), lb = static_cast<float>(r + 0x1.af23fp-34);
  if (ub != lb) {
    double f = z * ((c[0] + z * c[1]) + z2 * ((c[2] + z * c[3]) + z2 * (c[4] + z * c[5] + z2 * c[6])));
    f += ln10l * e;
    f += tl[j] - tl[0];
    double el = e * ln10h;
    r = el + f;
    ub = static_cast<float>(r);
    tz = r;
    if (!(asuint64(tz) & ((1 << 28) - 1))) {
      double dr = (el - r) + f;
      r += dr * 32;
      ub = static_cast<float>(r);
    }
  }
  return ub;
}

/* Correctly-rounded arctangent function of two binary32 values.

Copyright (c) 2022-2024 Alexei Sibidanov and Paul Zimmermann.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/atan2/atan2f.c, revision 9b28a4a).

(The permission notice is the one above, word for word, in glibc's file.)
*/
namespace detail {
WW_LIBM_FN double muldd(double xh, double xl, double ch, double cl, double* l) {
  double ahlh = ch * xl;
  double alhh = cl * xh;
  double ahhh = ch * xh;
  double ahhl = ::fma(ch, xh, -ahhh);
  ahhl += alhh + ahlh;
  ch = ahhh + ahhl;
  *l = (ahhh - ch) + ahhl;
  return ch;
}

WW_LIBM_FN double polydd(double xh, double xl, int n, const double c[][2], double* l) {
  int i = n - 1;
  double ch = c[i][0];
  double cl = c[i][1];
  while (--i >= 0) {
    ch = muldd(xh, xl, ch, cl, &cl);
    double th = ch + c[i][0];
    double tl = (c[i][0] - th) + ch;
    ch = th;
    cl += tl + c[i][1];
  }
  *l = cl;
  return ch;
}

/* for y/x tiny, use Taylor approximation z - z^3/3 where z=y/x */
WW_LIBM_FN float cr_atan2f_tiny(float y, float x) {
  double dy = y;
  double dx = x;
  double z = dy / dx;
  // C promotes the float x and y to double here; nvcc must be told, or it picks a host-only fma.
  double e = ::fma(-z, static_cast<double>(x), static_cast<double>(y));
  /* z * x + e = y thus y/x = z + e/x */
  const double c = -0x1.5555555555555p-2; /* -1/3 rounded to nearest */
  double zz = z * z;
  double cz = c * z;
  e = e / dx + cz * zz;
  uint64_t t = asuint64(z);
  if ((t & UINT64_C(0xfffffff)) == 0) { /* boundary case */
    if (z * e > 0) t += 1;
    else t -= 1;
  }
  double r = asdouble(t);
  float rf = static_cast<float>(r);
  if (rf == 0.0f) return uflowf(static_cast<uint32_t>(t >> 63));
  return rf;
}
}  // namespace detail

WW_LIBM_FN float atan2f(float y, float x) {
  using namespace detail;
  constexpr double cn[] = {0x1p+0,               0x1.40e0698f94c35p+1, 0x1.248c5da347f0dp+1,
                           0x1.d873386572976p-1, 0x1.46fa40b20f1dp-3,  0x1.33f5e041eed0fp-7,
                           0x1.546bbf28667c5p-14};
  constexpr double cd[] = {0x1p+0,               0x1.6b8b143a3f6dap+1, 0x1.8421201d18ed5p+1,
                           0x1.8221d086914ebp+0, 0x1.670657e3a07bap-2, 0x1.0f4951fd1e72dp-5,
                           0x1.b3874b8798286p-11};
  constexpr double m[] = {0, 1};
  constexpr double pi = 0x1.921fb54442d18p+1, pi2 = 0x1.921fb54442d18p+0,
                   pi2l = 0x1.1a62633145c07p-54;
  constexpr double off[] = {0.0f, pi2, pi, pi2, -0.0f, -pi2, -pi, -pi2};
  constexpr double offl[] = {0.0f, pi2l, 2 * pi2l, pi2l, -0.0f, -pi2l, -2 * pi2l, -pi2l};
  constexpr double sgn[] = {1, -1};
  uint32_t ux = asuint(x);
  uint32_t uy = asuint(y);
  uint32_t ax = ux & (~0u >> 1);
  uint32_t ay = uy & (~0u >> 1);
  if (ay >= (0xffu << 23) || ax >= (0xffu << 23)) {
    if (ay > (0xffu << 23)) return x + y; /* nan */
    if (ax > (0xffu << 23)) return x + y; /* nan */
    bool yinf = ay == (0xffu << 23);
    bool xinf = ax == (0xffu << 23);
    if (yinf & xinf) {
      if (ux >> 31) return static_cast<float>(0x1.2d97c7f3321d2p+1 * sgn[uy >> 31]); /* +/-3pi/4 */
      else return static_cast<float>(0x1.921fb54442d18p-1 * sgn[uy >> 31]); /* +/-pi/4 */
    }
    if (xinf) {
      if (ux >> 31) return static_cast<float>(pi * sgn[uy >> 31]);
      else return static_cast<float>(0.0 * sgn[uy >> 31]);
    }
    if (yinf) return static_cast<float>(pi2 * sgn[uy >> 31]);
  }
  if (ay == 0) {
    if (!ax) {
      uint32_t i = (uy >> 31) * 4 + (ux >> 31) * 2;
      if (ux >> 31) return static_cast<float>(off[i] + offl[i]);
      else return static_cast<float>(off[i]);
    }
    if (!(ux >> 31)) return static_cast<float>(0.0 * sgn[uy >> 31]);
  }
  uint32_t gt = ay > ax;
  uint32_t i = (uy >> 31) * 4 + (ux >> 31) * 2 + gt;

  double zx = x;
  double zy = y;
  double z = (m[gt] * zx + m[1 - gt] * zy) / (m[gt] * zy + m[1 - gt] * zx);
  /* z = x/y if |y| > |x|, and z = y/x otherwise */
  double r;
  int d = static_cast<int>(ax) - static_cast<int>(ay);
  if (d < (27 << 23) && d > (-(27 << 23))) {
    double z2 = z * z, z4 = z2 * z2, z8 = z4 * z4;
    double cn0 = cn[0] + z2 * cn[1];
    double cn2 = cn[2] + z2 * cn[3];
    double cn4 = cn[4] + z2 * cn[5];
    double cn6 = cn[6];
    cn0 += z4 * cn2;
    cn4 += z4 * cn6;
    cn0 += z8 * cn4;
    double cd0 = cd[0] + z2 * cd[1];
    double cd2 = cd[2] + z2 * cd[3];
    double cd4 = cd[4] + z2 * cd[5];
    double cd6 = cd[6];
    cd0 += z4 * cd2;
    cd4 += z4 * cd6;
    cd0 += z8 * cd4;
    r = cn0 / cd0;
  } else {
    r = 1;
  }
  z *= sgn[gt];
  r = z * r + off[i];
  if (((asuint64(r) + 8) & 0xfffffff) <= 16) {
    /* check tiny y/x */
    if (ay < ax && ((ax - ay) >> 23 >= 25)) return cr_atan2f_tiny(y, x);
    double zh;
    double zl;
    if (gt == 0) {
      zh = zy / zx;
      zl = ::fma(zh, -zx, zy) / zx;
    } else {
      zh = zx / zy;
      zl = ::fma(zh, -zy, zx) / zy;
    }
    double z2l;
    double z2h = muldd(zh, zl, zh, zl, &z2l);
    constexpr double c[32][2] = {
        {0x1p+0, -0x1.8c1dac5492248p-87},
        {-0x1.5555555555555p-2, -0x1.55553bf3a2abep-56},
        {0x1.999999999999ap-3, -0x1.99deed1ec9071p-57},
        {-0x1.2492492492492p-3, -0x1.fd99c8d18269ap-58},
        {0x1.c71c71c71c717p-4, -0x1.651eee4c4d9dp-61},
        {-0x1.745d1745d1649p-4, -0x1.632683d6c44a6p-58},
        {0x1.3b13b13b11c63p-4, 0x1.bf69c1f8af41dp-58},
        {-0x1.11111110e6338p-4, 0x1.3c3e431e8bb68p-61},
        {0x1.e1e1e1dc45c4ap-5, -0x1.be2db05c77bbfp-59},
        {-0x1.af286b8164b4fp-5, 0x1.a4673491f0942p-61},
        {0x1.86185e9ad4846p-5, 0x1.e12e32d79fceep-59},
        {-0x1.642c6d5161faep-5, 0x1.3ce76c1ca03fp-59},
        {0x1.47ad6f277e5bfp-5, -0x1.abd8d85bdb714p-60},
        {-0x1.2f64a2ee8896dp-5, 0x1.ef87d4b615323p-61},
        {0x1.1a6a2b31741b5p-5, 0x1.a5d9d973547eep-62},
        {-0x1.07fbdad65e0a6p-5, -0x1.65ac07f5d35f4p-61},
        {0x1.ee9932a9a5f8bp-6, 0x1.f8b9623f6f55ap-61},
        {-0x1.ce8b5b9584dc6p-6, 0x1.fe5af96e8ea2dp-61},
        {0x1.ac9cb288087b7p-6, -0x1.450cdfceaf5cap-60},
        {-0x1.84b025351f3e6p-6, 0x1.579561b0d73dap-61},
        {0x1.52f5b8ecdd52bp-6, 0x1.036bd2c6fba47p-60},
        {-0x1.163a8c44909dcp-6, 0x1.18f735ffb9f16p-60},
        {0x1.a400dce3eea6fp-7, -0x1.c90569c0c1b5cp-61},
        {-0x1.1caa78ae6db3ap-7, -0x1.4c60f8161ea09p-61},
        {0x1.52672453c0731p-8, 0x1.834efb598c338p-62},
        {-0x1.5850c5be137cfp-9, -0x1.445fc150ca7f5p-63},
        {0x1.23eb98d22e1cap-10, -0x1.388fbaf1d783p-64},
        {-0x1.8f4e974a40741p-12, 0x1.271198a97da34p-66},
        {0x1.a5cf2e9cf76e5p-14, -0x1.887eb4a63b665p-68},
        {-0x1.420c270719e32p-16, 0x1.efd595b27888bp-71},
        {0x1.3ba2d69b51677p-19, -0x1.4fb06829cdfc7p-73},
        {-0x1.29b7e6f676385p-23, -0x1.a783b6de718fbp-77}};
    double pl;
    double ph = polydd(z2h, z2l, 32, c, &pl);
    zh *= sgn[gt];
    zl *= sgn[gt];
    ph = muldd(zh, zl, ph, pl, &pl);
    double sh = ph + off[i];
    double sl = ((off[i] - sh) + ph) + pl + offl[i];
    float rf = static_cast<float>(sh);
    double th = rf;
    double dh = sh - th;
    double tm = dh + sl;
    uint64_t tth = asuint64(th);
    if (th + th * 0x1p-60 == th - th * 0x1p-60) {
      tth &= UINT64_C(0x7ff) << 52;
      tth -= UINT64_C(24) << 52;
      if (::fabs(tm) > asdouble(tth)) tm *= 1.25;
      else tm *= 0.75;
    }
    r = th + tm;
  }
  float rf = static_cast<float>(r);
  if (rf == 0.0f) return uflowf(asuint(rf) >> 1);
  return rf;
}

}  // namespace ww::libm
