// kokkos/tools/libm_bits/device_check.cu
// The device half of libm_bits: computes on a CUDA device every per-step libm function that
// glibc_mathf.hpp and coremath_f.hpp transcribe, and compares each result on the host with what
// WW3's Fortran gets there: the toolchain's glibc, or libmvec for the vector functions
// (libm_bits.cpp shows the transcriptions equal those on the host). Where CUDA has a function
// of its own (expf, logf, powf, sincosf, tanhf, log10f, atan2f, exp, tanh) it is computed too and
// printed beside the transcription for comparison; the exit code is the transcriptions' only.
//
// It runs on the GPU, so it runs only under the owner's supervision (AGENTS.md, "Running on
// the GPU"): scripts/libm_bits.sh --device asks for a typed "yes". --nvcc only compiles it.
// Compiled with --fmad=false and with build/libm_bits/svml_tables.hpp pre-included (mvec_powf's
// tables): the headers fuse only where they call ::fma or ::fmaf.
//
// Output, one TSV line per function and implementation: device, function, impl, inputs,
// differing, nan_payload, first differing input.
//
// SPDX-License-Identifier: MIT
#include <cuda_runtime.h>
#include <xmmintrin.h>

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "coremath_d.hpp"
#include "coremath_f.hpp"
#include "glibc_mathf.hpp"

extern "C" __m128 _ZGVbN4v_expf(__m128);
extern "C" __m128 _ZGVbN4vv_powf(__m128, __m128);

namespace {

constexpr uint64_t kChunk = 1ULL << 26;

enum Fn { kExpf, kLogf, kSincosf, kPowf, kMvecExpf, kMvecPowf, kTanhf, kLog10f, kAtan2f, kExp, kTanh };

__host__ __device__ inline uint64_t splitmix(uint64_t s) {
  uint64_t z = s + 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

// The input of index i: the float with bit pattern i for the unary functions (exp and tanh widen it
// to double); for powf and mvec_powf a random x in [2^-20, 2^20) and y in [-8, 8), the range
// libm_bits.cpp's random pairs use; for atan2f a random pair of bit patterns.
__host__ __device__ inline void input(Fn fn, uint64_t i, float& x, float& y) {
  y = 0;
  if (fn == kPowf || fn == kMvecPowf) {
    const uint64_t r = splitmix(i);
    x = ldexpf(1.0f + (r & 0xffffff) * 0x1p-24f, static_cast<int>((r >> 24) % 40) - 20);
    y = ((r >> 32) & 0xffffff) * 0x1p-20f - 8.0f;
    return;
  }
  if (fn == kAtan2f) {
    const uint64_t r = splitmix(i ^ 0xa7a2f00ULL);
    const uint32_t a = static_cast<uint32_t>(r), b = static_cast<uint32_t>(r >> 32);
    memcpy(&y, &a, 4);
    memcpy(&x, &b, 4);
    return;
  }
  const uint32_t u = static_cast<uint32_t>(i);
  memcpy(&x, &u, 4);
}

bool has_cuda(Fn fn) { return fn != kMvecExpf && fn != kMvecPowf; }

// Two doubles per input: a float result is widened (exactly), sincosf uses both.
__global__ void kernel(Fn fn, bool cuda_own, uint64_t base, uint64_t n, double* out) {
  const uint64_t k = blockIdx.x * static_cast<uint64_t>(blockDim.x) + threadIdx.x;
  if (k >= n) return;
  float x, y, a = 0, b = 0;
  double d = 0;
  input(fn, base + k, x, y);
  switch (fn) {
    case kExpf: a = cuda_own ? ::expf(x) : ww::libm::expf<true>(x); break;
    case kLogf: a = cuda_own ? ::logf(x) : ww::libm::logf<true>(x); break;
    case kPowf: a = cuda_own ? ::powf(x, y) : ww::libm::powf<true>(x, y); break;
    case kSincosf:
      if (cuda_own) ::sincosf(x, &a, &b);
      else ww::libm::sincosf<true>(x, &a, &b);
      break;
    case kMvecExpf: a = ww::libm::mvec_expf<true>(x); break;
    case kMvecPowf: a = ww::libm::mvec_powf<true>(x, y); break;
    case kTanhf: a = cuda_own ? ::tanhf(x) : ww::libm::tanhf(x); break;
    case kLog10f: a = cuda_own ? ::log10f(x) : ww::libm::log10f(x); break;
    case kAtan2f: a = cuda_own ? ::atan2f(y, x) : ww::libm::atan2f(y, x); break;
    case kExp: d = cuda_own ? ::exp(static_cast<double>(x)) : ww::libm::exp<true>(x); break;
    case kTanh: d = cuda_own ? ::tanh(static_cast<double>(x)) : ww::libm::tanh(x); break;
  }
  out[2 * k] = (fn == kExp || fn == kTanh) ? d : static_cast<double>(a);
  out[2 * k + 1] = static_cast<double>(b);
}

uint64_t bits(double v) { uint64_t u; std::memcpy(&u, &v, 8); return u; }
uint32_t fbits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }

// What the host's Fortran gets for input i: glibc, or libmvec lane 0 for the vector functions.
void reference(Fn fn, float x, float y, double& wa, double& wb) {
  float a = 0, b = 0;
  alignas(16) float va[4] = {x, x, x, x}, vb[4] = {y, y, y, y}, vr[4];
  wb = 0;
  switch (fn) {
    case kExpf: a = ::expf(x); break;
    case kLogf: a = ::logf(x); break;
    case kPowf: a = ::powf(x, y); break;
    case kSincosf: ::sincosf(x, &a, &b); break;
    case kMvecExpf: _mm_store_ps(vr, _ZGVbN4v_expf(_mm_load_ps(va))); a = vr[0]; break;
    case kMvecPowf:
      _mm_store_ps(vr, _ZGVbN4vv_powf(_mm_load_ps(va), _mm_load_ps(vb)));
      a = vr[0];
      break;
    case kTanhf: a = ::tanhf(x); break;
    case kLog10f: a = ::log10f(x); break;
    case kAtan2f: a = ::atan2f(y, x); break;
    case kExp: wa = std::exp(static_cast<double>(x)); return;
    case kTanh: wa = std::tanh(static_cast<double>(x)); return;
  }
  wa = a;
  wb = b;
}

int check(Fn fn, const char* name, bool cuda_own, uint64_t total) {
  double* dev = nullptr;
  if (cudaMalloc(&dev, 2 * kChunk * sizeof(double)) != cudaSuccess) return 2;
  std::vector<double> host(2 * kChunk);
  uint64_t differ = 0, nan_payload = 0;
  std::string first = "-";
  for (uint64_t base = 0; base < total; base += kChunk) {
    const uint64_t n = std::min(kChunk, total - base);
    kernel<<<(n + 255) / 256, 256>>>(fn, cuda_own, base, n, dev);
    if (cudaMemcpy(host.data(), dev, 2 * n * sizeof(double), cudaMemcpyDeviceToHost) !=
        cudaSuccess) return 2;
    for (uint64_t k = 0; k < n; ++k) {
      float x, y;
      double wa, wb;
      input(fn, base + k, x, y);
      reference(fn, x, y, wa, wb);
      const double got[2] = {host[2 * k], host[2 * k + 1]}, want[2] = {wa, wb};
      for (int l = 0; l < (fn == kSincosf ? 2 : 1); ++l) {
        if (bits(got[l]) == bits(want[l])) continue;
        if (got[l] != got[l] && want[l] != want[l]) { ++nan_payload; continue; }
        if (differ++ == 0) {
          char buf[160];
          std::snprintf(buf, sizeof buf, "x=%08x y=%08x got=%016" PRIx64 " want=%016" PRIx64,
                        fbits(x), fbits(y), bits(got[l]), bits(want[l]));
          first = buf;
        }
      }
    }
  }
  cudaFree(dev);
  cudaDeviceProp prop{};
  cudaGetDeviceProperties(&prop, 0);
  std::printf("%s\t%s\t%s\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%s\n", prop.name, name,
              cuda_own ? "cuda" : "ww", total, differ, nan_payload, first.c_str());
  std::fflush(stdout);
  return differ ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string only = argc > 1 ? argv[1] : "";
  int rc = 0;
  const struct { Fn fn; const char* name; uint64_t total; } fns[] = {
      {kExpf, "expf", 1ULL << 32},       {kLogf, "logf", 1ULL << 32},
      {kSincosf, "sincosf", 1ULL << 32}, {kPowf, "powf", 1ULL << 32},
      {kMvecExpf, "mvec_expf", 1ULL << 32}, {kMvecPowf, "mvec_powf", 1ULL << 32},
      {kTanhf, "tanhf", 1ULL << 32},     {kLog10f, "log10f", 1ULL << 32},
      {kAtan2f, "atan2f", 1ULL << 32},   {kExp, "exp", 1ULL << 32},
      {kTanh, "tanh", 1ULL << 32}};
  for (const auto& f : fns) {
    if (!only.empty() && only != f.name) continue;
    for (bool cuda_own : {false, true}) {
      if (cuda_own && !has_cuda(f.fn)) continue;
      const int r = check(f.fn, f.name, cuda_own, f.total);
      if (r == 2) { std::fprintf(stderr, "device_check: CUDA error\n"); return 2; }
      if (!cuda_own) rc |= r;
    }
  }
  // The verdict is the transcriptions'; CUDA's own functions are printed for comparison.
  return rc;
}
