// kokkos/tools/libm_bits/libm_bits.cpp
// Compares, bit for bit, the libm functions the W3SRCE unit calls per sea point with what a
// device would compute (docs/GPU_LIBM_BITS_202610.md). Host only; it tests glibc and the
// transcription in glibc_mathf.hpp, not a ported routine (ADR-0010 is about the port).
//
//   libm_bits ww <expf|logf|sincosf|powf|mvec_expf|mvec_powf|tanhf|log10f|atan2f|exp|tanh> <fma|plain> [threads]
//       glibc_mathf.hpp against this process's glibc: every float input for the unary
//       functions; for powf every positive x with fixed y, every y with fixed x, and
//       random pairs. mvec_expf and mvec_powf are checked against libmvec's
//       _ZGVbN4v_expf and _ZGVbN4vv_powf, the same inputs four lanes at a time. tanhf, log10f
//       and atan2f are coremath_f.hpp's (one build: the variant argument is ignored); atan2f
//       takes every y at fixed x and random pairs. exp (double) takes every float as a
//       double, random bit patterns and random values in [-750, 710].
//       tanh (double, coremath_d.hpp, one build) takes every float as a double, random bit
//       patterns and random values in [-25, 25].
//   libm_bits cr <tanhf|log10f|atan2f> [threads]
//       glibc against the correctly rounded result (double, then __float128 near a
//       midpoint): every input for the unary ones, random pairs for atan2f.
//   libm_bits mvec <expf|powf> [threads]
//       libmvec's _ZGVbN4v_expf / _ZGVbN4vv_powf against scalar expf / powf, the
//       difference gfortran -O3 builds into W3SDS4 and W3SPR4 (st4_libmvec.hpp).
//   libm_bits probe
//       Which glibc build of expf, logf, powf and sincosf this process got (fma or sse2).
//   libm_bits fingerprint
//       A hash of what this machine's libm returns, per function, over fixed inputs: every
//       per-step function of the W3SRCE unit, libmvec's two included. Two machines with the
//       same hashes give WW3's Fortran the same libm bits there.
//
// One TSV line per check: mode, function, variant, inputs, differing, nan_payload, first
// differing input. Build and run: scripts/libm_bits.sh.
//
// SPDX-License-Identifier: MIT
#include <quadmath.h>
#include <xmmintrin.h>

#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "coremath_d.hpp"
#include "coremath_f.hpp"
#include "glibc_mathf.hpp"

extern "C" __m128 _ZGVbN4vv_powf(__m128, __m128);
extern "C" __m128 _ZGVbN4v_expf(__m128);

namespace {

using ww::libm::detail::asfloat;
using ww::libm::detail::asuint;

struct Tally {
  std::atomic<uint64_t> inputs{0}, differ{0}, nan_payload{0};
  std::mutex m;
  std::string first;
  void note(const char* what) {
    std::lock_guard<std::mutex> g(m);
    if (first.empty()) first = what;
  }
};

// Same bits, or both NaN (counted apart: a NaN's payload is not part of the bit-for-bit
// verdict the regtests give, and x86 and CUDA make different default NaNs).
void compare(Tally& t, float got, float want, const char* label, float a) {
  if (asuint(got) == asuint(want)) return;
  if (std::isnan(got) && std::isnan(want)) { t.nan_payload++; return; }
  t.differ++;
  char buf[160];
  std::snprintf(buf, sizeof buf, "%sx=%08x got=%08x want=%08x", label, asuint(a), asuint(got),
                asuint(want));
  t.note(buf);
}

void compare2(Tally& t, float got, float want, float a, float b) {
  if (asuint(got) == asuint(want)) return;
  if (std::isnan(got) && std::isnan(want)) { t.nan_payload++; return; }
  t.differ++;
  char buf[160];
  std::snprintf(buf, sizeof buf, "a=%08x b=%08x got=%08x want=%08x", asuint(a), asuint(b),
                asuint(got), asuint(want));
  t.note(buf);
}

void parallel(int threads, uint64_t n, const std::function<void(uint64_t, uint64_t)>& f) {
  std::vector<std::thread> pool;
  const uint64_t chunk = (n + threads - 1) / threads;
  for (int i = 0; i < threads; ++i) {
    const uint64_t lo = i * chunk, hi = std::min<uint64_t>(n, lo + chunk);
    if (lo < hi) pool.emplace_back(f, lo, hi);
  }
  for (auto& th : pool) th.join();
}

uint64_t splitmix(uint64_t& s) {
  uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

void print(const char* mode, const char* fn, const char* var, Tally& t) {
  std::printf("%s\t%s\t%s\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%s\n", mode, fn, var,
              t.inputs.load(), t.differ.load(), t.nan_payload.load(),
              t.first.empty() ? "-" : t.first.c_str());
  std::fflush(stdout);
}

template <bool Fma>
void ww_unary(const std::string& fn, int threads, Tally& t) {
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    uint64_t n = 0;
    for (uint64_t u = lo; u < hi; ++u) {
      const float x = asfloat(static_cast<uint32_t>(u));
      if (fn == "expf") {
        compare(t, ww::libm::expf<Fma>(x), ::expf(x), "", x);
      } else if (fn == "mvec_expf") {
        // One input per call, in lane 0, beside three neighbours: lanes are independent.
        alignas(16) float a[4] = {x, -x, x * 0.5f, 1.0f}, r[4];
        _mm_store_ps(r, _ZGVbN4v_expf(_mm_load_ps(a)));
        for (int l = 0; l < 4; ++l) compare(t, ww::libm::mvec_expf<Fma>(a[l]), r[l], "", a[l]);
      } else if (fn == "tanhf") {
        compare(t, ww::libm::tanhf(x), ::tanhf(x), "", x);
      } else if (fn == "log10f") {
        compare(t, ww::libm::log10f(x), ::log10f(x), "", x);
      } else if (fn == "logf") {
        compare(t, ww::libm::logf<Fma>(x), ::logf(x), "", x);
      } else {
        float s, c, gs, gc;
        ww::libm::sincosf<Fma>(x, &s, &c);
        ::sincosf(x, &gs, &gc);
        compare(t, s, gs, "sin ", x);
        compare(t, c, gc, "cos ", x);
      }
      ++n;
    }
    t.inputs += n;
  });
}

float sys_mvec_powf(float x, float y) {
  alignas(16) float a[4] = {x, x, x, x}, b[4] = {y, y, y, y}, r[4];
  _mm_store_ps(r, _ZGVbN4vv_powf(_mm_load_ps(a), _mm_load_ps(b)));
  return r[0];
}

template <bool Fma, bool Mvec>
float ww_pow(float x, float y) {
  if constexpr (Mvec) return ww::libm::mvec_powf<Fma>(x, y);
  else return ww::libm::powf<Fma>(x, y);
}

template <bool Fma, bool Mvec = false>
void ww_powf(int threads, Tally& t) {
  float (*sys)(float, float) = Mvec ? sys_mvec_powf : static_cast<float (*)(float, float)>(::powf);
  // Every positive x (and every negative one for an integer y) with fixed exponents.
  const float ys[] = {0.5f, 1.5f, 2.0f, -0.5f, -1.0f, 0.25f, 1.0f / 3.0f, 2.5f, 3.0f, 4.0f};
  for (float y : ys) {
    parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
      for (uint64_t u = lo; u < hi; ++u) {
        const float x = asfloat(static_cast<uint32_t>(u));
        compare2(t, ww_pow<Fma, Mvec>(x, y), sys(x, y), x, y);
      }
      t.inputs += hi - lo;
    });
  }
  // Every y with fixed bases.
  const float xs[] = {0.5f, 2.0f, 10.0f, 0.1f, 1.1f, 0.9f, 3.0f, 1e-3f};
  for (float x : xs) {
    parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
      for (uint64_t u = lo; u < hi; ++u) {
        const float y = asfloat(static_cast<uint32_t>(u));
        compare2(t, ww_pow<Fma, Mvec>(x, y), sys(x, y), x, y);
      }
      t.inputs += hi - lo;
    });
  }
  // Random bit patterns, and random pairs in the range a source term sees.
  parallel(threads, 1ULL << 31, [&](uint64_t lo, uint64_t hi) {
    uint64_t s = 0x5eed0000ULL + lo;
    for (uint64_t i = lo; i < hi; ++i) {
      const uint64_t r = splitmix(s);
      float x = asfloat(static_cast<uint32_t>(r)), y = asfloat(static_cast<uint32_t>(r >> 32));
      if (i & 1) {
        x = std::ldexp(1.0f + (r & 0xffffff) * 0x1p-24f, static_cast<int>((r >> 24) % 40) - 20);
        y = ((r >> 32) & 0xffffff) * 0x1p-20f - 8.0f;
      }
      compare2(t, ww_pow<Fma, Mvec>(x, y), sys(x, y), x, y);
    }
    t.inputs += hi - lo;
  });
}

void compare_d(Tally& t, double got, double want, double a) {
  uint64_t g, w, x;
  std::memcpy(&g, &got, 8); std::memcpy(&w, &want, 8); std::memcpy(&x, &a, 8);
  if (g == w) return;
  if (std::isnan(got) && std::isnan(want)) { t.nan_payload++; return; }
  t.differ++;
  char buf[160];
  std::snprintf(buf, sizeof buf, "x=%016" PRIx64 " got=%016" PRIx64 " want=%016" PRIx64, x, g, w);
  t.note(buf);
}

template <bool Fma>
void ww_exp(int threads, Tally& t) {
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    for (uint64_t u = lo; u < hi; ++u) {
      const double x = asfloat(static_cast<uint32_t>(u));
      compare_d(t, ww::libm::exp<Fma>(x), std::exp(x), x);
    }
    t.inputs += hi - lo;
  });
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    uint64_t s = 0xe4b0000ULL + lo;
    for (uint64_t i = lo; i < hi; ++i) {
      const uint64_t r = splitmix(s);
      double x;
      if (i & 1) x = -750.0 + 1460.0 * static_cast<double>(r >> 11) * 0x1p-53;
      else std::memcpy(&x, &r, 8);
      compare_d(t, ww::libm::exp<Fma>(x), std::exp(x), x);
    }
    t.inputs += hi - lo;
  });
}

void ww_tanh(int threads, Tally& t) {
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    for (uint64_t u = lo; u < hi; ++u) {
      const double x = asfloat(static_cast<uint32_t>(u));
      compare_d(t, ww::libm::tanh(x), std::tanh(x), x);
    }
    t.inputs += hi - lo;
  });
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    uint64_t s = 0x7a40000ULL + lo;
    for (uint64_t i = lo; i < hi; ++i) {
      const uint64_t r = splitmix(s);
      double x;
      if (i & 1) x = -25.0 + 50.0 * static_cast<double>(r >> 11) * 0x1p-53;
      else std::memcpy(&x, &r, 8);
      compare_d(t, ww::libm::tanh(x), std::tanh(x), x);
    }
    t.inputs += hi - lo;
  });
}

void ww_atan2f(int threads, Tally& t) {
  const float xs[] = {1.0f, -1.0f, 0.5f, -3.0f, 1e-30f, 1e30f, 7.0f, -0.0f};
  for (float x : xs) {
    parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
      for (uint64_t u = lo; u < hi; ++u) {
        const float y = asfloat(static_cast<uint32_t>(u));
        compare2(t, ww::libm::atan2f(y, x), ::atan2f(y, x), y, x);
      }
      t.inputs += hi - lo;
    });
  }
  parallel(threads, 1ULL << 31, [&](uint64_t lo, uint64_t hi) {
    uint64_t s = 0xa7a2f00ULL + lo;
    for (uint64_t i = lo; i < hi; ++i) {
      const uint64_t r = splitmix(s);
      const float y = asfloat(static_cast<uint32_t>(r)), x = asfloat(static_cast<uint32_t>(r >> 32));
      compare2(t, ww::libm::atan2f(y, x), ::atan2f(y, x), y, x);
    }
    t.inputs += hi - lo;
  });
}

// The correctly rounded float of a function, from its double value d and, when d is within
// two double ulps of a float midpoint, its __float128 value q. Sets hard when q is still
// too close to call.
float cr_round(double d, const std::function<__float128()>& quad, bool& hard) {
  hard = false;
  const float f = static_cast<float>(d);
  if (!std::isfinite(d) || !std::isfinite(f) || d == 0) return f;
  const double ulp = std::nextafter(std::fabs(d), INFINITY) - std::fabs(d);
  const double m1 = (static_cast<double>(f) + std::nextafterf(f, -INFINITY)) / 2;
  const double m2 = (static_cast<double>(f) + std::nextafterf(f, INFINITY)) / 2;
  if (std::fabs(d - m1) > 2 * ulp && std::fabs(d - m2) > 2 * ulp) return f;
  const __float128 q = quad();
  const float fq = static_cast<float>(q);
  const __float128 mq1 = (static_cast<__float128>(fq) + std::nextafterf(fq, -INFINITY)) / 2;
  const __float128 mq2 = (static_cast<__float128>(fq) + std::nextafterf(fq, INFINITY)) / 2;
  const __float128 tol = fabsq(q) * static_cast<__float128>(0x1p-100);
  if (fabsq(q - mq1) <= tol || fabsq(q - mq2) <= tol) hard = true;
  return fq;
}

void cr(const std::string& fn, int threads, Tally& t, std::atomic<uint64_t>& unresolved) {
  if (fn == "atan2f") {
    parallel(threads, 1ULL << 30, [&](uint64_t lo, uint64_t hi) {
      uint64_t s = 0xa7a2000ULL + lo;
      for (uint64_t i = lo; i < hi; ++i) {
        const uint64_t r = splitmix(s);
        const float y = asfloat(static_cast<uint32_t>(r)), x = asfloat(static_cast<uint32_t>(r >> 32));
        bool hard;
        const float want = cr_round(std::atan2(static_cast<double>(y), static_cast<double>(x)),
                                    [&] { return atan2q(y, x); }, hard);
        if (hard) { unresolved++; continue; }
        compare2(t, ::atan2f(y, x), want, y, x);
      }
      t.inputs += hi - lo;
    });
    return;
  }
  parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
    for (uint64_t u = lo; u < hi; ++u) {
      const float x = asfloat(static_cast<uint32_t>(u));
      bool hard;
      float want, got;
      if (fn == "tanhf") {
        want = cr_round(std::tanh(static_cast<double>(x)), [&] { return tanhq(x); }, hard);
        got = ::tanhf(x);
      } else {
        want = cr_round(std::log10(static_cast<double>(x)), [&] { return log10q(x); }, hard);
        got = ::log10f(x);
      }
      if (hard) { unresolved++; continue; }
      compare(t, got, want, "", x);
    }
    t.inputs += hi - lo;
  });
}

void mvec(const std::string& fn, int threads, Tally& t) {
  if (fn == "expf") {
    parallel(threads, 1ULL << 32, [&](uint64_t lo, uint64_t hi) {
      alignas(16) float a[4], r[4];
      for (uint64_t u = lo; u < hi; u += 4) {
        for (int l = 0; l < 4; ++l) a[l] = asfloat(static_cast<uint32_t>(u + l));
        _mm_store_ps(r, _ZGVbN4v_expf(_mm_load_ps(a)));
        for (int l = 0; l < 4; ++l)
          compare(t, r[l], ::expf(a[l]), "", a[l]);
      }
      t.inputs += hi - lo;
    });
    return;
  }
  parallel(threads, 1ULL << 30, [&](uint64_t lo, uint64_t hi) {
    uint64_t s = 0x3e3c000ULL + lo;
    alignas(16) float a[4], b[4], r[4];
    for (uint64_t i = lo; i < hi; i += 4) {
      for (int l = 0; l < 4; ++l) {
        const uint64_t q = splitmix(s);
        a[l] = std::ldexp(1.0f + (q & 0xffffff) * 0x1p-24f, static_cast<int>((q >> 24) % 40) - 20);
        b[l] = ((q >> 32) & 0xffffff) * 0x1p-20f - 8.0f;
      }
      _mm_store_ps(r, _ZGVbN4vv_powf(_mm_load_ps(a), _mm_load_ps(b)));
      for (int l = 0; l < 4; ++l)
        compare2(t, r[l], ::powf(a[l], b[l]), a[l], b[l]);
    }
    t.inputs += hi - lo;
  });
}

// The first input where the two glibc builds differ, then which one glibc gave.
template <class F>
const char* which(const F& fn) {
  for (uint64_t u = 0x3f000000; u < (1ULL << 32); ++u) {
    const float x = asfloat(static_cast<uint32_t>(u));
    float fma, sse2, sys;
    fn(x, fma, sse2, sys);
    if (asuint(fma) != asuint(sse2)) {
      if (asuint(sys) == asuint(fma)) return "fma";
      if (asuint(sys) == asuint(sse2)) return "sse2";
      return "neither";
    }
  }
  return "indistinguishable";
}

void probe() {
  using namespace ww::libm;
  std::printf("probe\texpf\t%s\n", which([](float x, float& a, float& b, float& c) {
                a = expf<true>(x); b = expf<false>(x); c = ::expf(x); }));
  std::printf("probe\tlogf\t%s\n", which([](float x, float& a, float& b, float& c) {
                a = logf<true>(x); b = logf<false>(x); c = ::logf(x); }));
  std::printf("probe\tpowf\t%s\n", which([](float x, float& a, float& b, float& c) {
                a = powf<true>(x, 2.5f); b = powf<false>(x, 2.5f); c = ::powf(x, 2.5f); }));
  std::printf("probe\tsincosf\t%s\n", which([](float x, float& a, float& b, float& c) {
                float s; sincosf<true>(x, &s, &a); sincosf<false>(x, &s, &b); ::sincosf(x, &s, &c); }));
}

uint64_t fnv(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 0xff; h *= 0x100000001b3ULL; }
  return h;
}

// Every 16th float bit pattern for a unary function; 2^26 seeded pairs for a binary one.
template <class F>
uint64_t hash_unary(const F& f) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (uint64_t u = 0; u < (1ULL << 32); u += 16) h = fnv(h, f(asfloat(static_cast<uint32_t>(u))));
  return h;
}

template <class F>
uint64_t hash_pairs(uint64_t seed, const F& f) {
  uint64_t h = 0xcbf29ce484222325ULL, s = seed;
  for (uint64_t i = 0; i < (1ULL << 26); i += 4) {
    float a[4], b[4];
    for (int l = 0; l < 4; ++l) {
      const uint64_t r = splitmix(s);
      a[l] = std::ldexp(1.0f + (r & 0xffffff) * 0x1p-24f, static_cast<int>((r >> 24) % 40) - 20);
      b[l] = ((r >> 32) & 0xffffff) * 0x1p-20f - 8.0f;
    }
    h = fnv(h, f(a, b));
  }
  return h;
}

uint64_t bits4(__m128 v) {
  alignas(16) float r[4];
  _mm_store_ps(r, v);
  return (static_cast<uint64_t>(asuint(r[0])) ^ (static_cast<uint64_t>(asuint(r[1])) << 32)) ^
         (static_cast<uint64_t>(asuint(r[2])) * 0x9e3779b97f4a7c15ULL) ^ asuint(r[3]);
}

uint64_t dbits(double d) { uint64_t u; std::memcpy(&u, &d, 8); return u; }

void fingerprint() {
  const auto row = [](const char* fn, uint64_t h) {
    std::printf("fingerprint\t%s\t%016" PRIx64 "\n", fn, h);
    std::fflush(stdout);
  };
  row("expf", hash_unary([](float x) { return asuint(::expf(x)); }));
  row("logf", hash_unary([](float x) { return asuint(::logf(x)); }));
  row("sincosf", hash_unary([](float x) {
        float s, c; ::sincosf(x, &s, &c);
        return (static_cast<uint64_t>(asuint(s)) << 32) | asuint(c); }));
  row("tanhf", hash_unary([](float x) { return asuint(::tanhf(x)); }));
  row("log10f", hash_unary([](float x) { return asuint(::log10f(x)); }));
  row("powf", hash_pairs(1, [](float* a, float* b) {
        uint64_t h = 0;
        for (int l = 0; l < 4; ++l) h = h * 31 + asuint(::powf(a[l], b[l]));
        return h; }));
  row("atan2f", hash_pairs(2, [](float* a, float* b) {
        uint64_t h = 0;
        for (int l = 0; l < 4; ++l) h = h * 31 + asuint(::atan2f(b[l], a[l]));
        return h; }));
  row("_ZGVbN4v_expf", hash_pairs(3, [](float* a, float* b) {
        float x[4];
        for (int l = 0; l < 4; ++l) x[l] = b[l] * 11.0f;
        return bits4(_ZGVbN4v_expf(_mm_loadu_ps(x))) ^ asuint(a[0]); }));
  row("_ZGVbN4vv_powf", hash_pairs(4, [](float* a, float* b) {
        return bits4(_ZGVbN4vv_powf(_mm_loadu_ps(a), _mm_loadu_ps(b))); }));
  row("exp", hash_pairs(5, [](float* a, float* b) {
        return dbits(std::exp(static_cast<double>(b[0]) * 80.0 + a[1])); }));
  row("tanh", hash_pairs(6, [](float* a, float* b) {
        return dbits(std::tanh(static_cast<double>(b[0]) * 3.0 + a[1] * 1e-3)); }));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: libm_bits ww|cr|mvec|probe ...\n");
    return 64;
  }
  const std::string mode = argv[1];
  if (mode == "probe") { probe(); return 0; }
  if (mode == "fingerprint") { fingerprint(); return 0; }
  const std::string fn = argc > 2 ? argv[2] : "";
  // A thread count below 1 would run nothing and report no difference.
  const int targ = mode == "ww" ? 4 : 3;
  if (argc > targ && std::atoi(argv[targ]) < 1) {
    std::fprintf(stderr, "libm_bits: threads must be at least 1\n");
    return 64;
  }
  Tally t;
  if (mode == "ww") {
    const std::string var = argc > 3 ? argv[3] : "fma";
    const int threads = argc > 4 ? std::atoi(argv[4]) : 4;
    const bool fma = var == "fma";
    if (fn == "powf") fma ? ww_powf<true>(threads, t) : ww_powf<false>(threads, t);
    else if (fn == "mvec_powf")
      fma ? ww_powf<true, true>(threads, t) : ww_powf<false, true>(threads, t);
    else if (fn == "atan2f") ww_atan2f(threads, t);
    else if (fn == "exp") fma ? ww_exp<true>(threads, t) : ww_exp<false>(threads, t);
    else if (fn == "tanh") ww_tanh(threads, t);
    else if (fn == "expf" || fn == "logf" || fn == "sincosf" || fn == "mvec_expf" ||
             fn == "tanhf" || fn == "log10f")
      fma ? ww_unary<true>(fn, threads, t) : ww_unary<false>(fn, threads, t);
    else return 64;
    print("ww", fn.c_str(), var.c_str(), t);
  } else if (mode == "cr") {
    const int threads = argc > 3 ? std::atoi(argv[3]) : 4;
    std::atomic<uint64_t> unresolved{0};
    if (fn != "tanhf" && fn != "log10f" && fn != "atan2f") return 64;
    cr(fn, threads, t, unresolved);
    const std::string var = "unresolved=" + std::to_string(unresolved.load());
    print("cr", fn.c_str(), var.c_str(), t);
  } else if (mode == "mvec") {
    const int threads = argc > 3 ? std::atoi(argv[3]) : 4;
    if (fn != "expf" && fn != "powf") return 64;
    mvec(fn, threads, t);
    print("mvec", fn.c_str(), "libmvec", t);
  } else {
    return 64;
  }
  return t.differ.load() ? 1 : 0;
}
