// kokkos/tools/libm_bits/device_check.cu
// The device half of libm_bits: computes expf, logf and sincosf for every float input, and
// powf for random pairs, on a CUDA device, twice: with glibc_mathf.hpp's fma build and with
// CUDA's own functions. Both are compared on the host with the toolchain's glibc, the bits
// WW3's Fortran gets (libm_bits.cpp shows the transcription equals glibc on the host).
//
// It runs on the GPU, so it runs only under the owner's supervision (AGENTS.md, "Running on
// the GPU"): scripts/libm_bits.sh --device asks for a typed "yes". --nvcc only compiles it.
// Compiled with --fmad=false: glibc_mathf.hpp fuses only where it calls ::fma.
//
// Output, one TSV line per function and implementation: device, function, impl, inputs,
// differing, nan_payload, first differing input.
//
// SPDX-License-Identifier: MIT
#include <cuda_runtime.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "glibc_mathf.hpp"

namespace {

constexpr uint64_t kChunk = 1ULL << 26;

enum Fn { kExpf, kLogf, kSincosf, kPowf };

__host__ __device__ inline uint64_t splitmix(uint64_t s) {
  uint64_t z = s + 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

// The input of index i: the float with bit pattern i for the unary functions; for powf a
// random x in [2^-20, 2^20) and y in [-8, 8), the range libm_bits.cpp's random pairs use.
__host__ __device__ inline void input(Fn fn, uint64_t i, float& x, float& y) {
  y = 0;
  if (fn != kPowf) {
    const uint32_t u = static_cast<uint32_t>(i);
    memcpy(&x, &u, 4);
    return;
  }
  const uint64_t r = splitmix(i);
  x = ldexpf(1.0f + (r & 0xffffff) * 0x1p-24f, static_cast<int>((r >> 24) % 40) - 20);
  y = ((r >> 32) & 0xffffff) * 0x1p-20f - 8.0f;
}

__global__ void kernel(Fn fn, bool cuda_own, uint64_t base, uint64_t n, float* out) {
  const uint64_t k = blockIdx.x * static_cast<uint64_t>(blockDim.x) + threadIdx.x;
  if (k >= n) return;
  float x, y, a = 0, b = 0;
  input(fn, base + k, x, y);
  switch (fn) {
    case kExpf: a = cuda_own ? ::expf(x) : ww::libm::expf<true>(x); break;
    case kLogf: a = cuda_own ? ::logf(x) : ww::libm::logf<true>(x); break;
    case kPowf: a = cuda_own ? ::powf(x, y) : ww::libm::powf<true>(x, y); break;
    case kSincosf:
      if (cuda_own) ::sincosf(x, &a, &b);
      else ww::libm::sincosf<true>(x, &a, &b);
      break;
  }
  out[2 * k] = a;
  out[2 * k + 1] = b;
}

uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

int check(Fn fn, const char* name, bool cuda_own, uint64_t total) {
  float* dev = nullptr;
  if (cudaMalloc(&dev, 2 * kChunk * sizeof(float)) != cudaSuccess) return 2;
  std::vector<float> host(2 * kChunk);
  uint64_t differ = 0, nan_payload = 0;
  std::string first = "-";
  for (uint64_t base = 0; base < total; base += kChunk) {
    const uint64_t n = std::min(kChunk, total - base);
    kernel<<<(n + 255) / 256, 256>>>(fn, cuda_own, base, n, dev);
    if (cudaMemcpy(host.data(), dev, 2 * n * sizeof(float), cudaMemcpyDeviceToHost) !=
        cudaSuccess) return 2;
    for (uint64_t k = 0; k < n; ++k) {
      float x, y, wa = 0, wb = 0;
      input(fn, base + k, x, y);
      switch (fn) {
        case kExpf: wa = ::expf(x); break;
        case kLogf: wa = ::logf(x); break;
        case kPowf: wa = ::powf(x, y); break;
        case kSincosf: ::sincosf(x, &wa, &wb); break;
      }
      const float got[2] = {host[2 * k], host[2 * k + 1]}, want[2] = {wa, wb};
      for (int l = 0; l < (fn == kSincosf ? 2 : 1); ++l) {
        if (bits(got[l]) == bits(want[l])) continue;
        if (got[l] != got[l] && want[l] != want[l]) { ++nan_payload; continue; }
        if (differ++ == 0) {
          char buf[128];
          std::snprintf(buf, sizeof buf, "x=%08x y=%08x got=%08x want=%08x", bits(x), bits(y),
                        bits(got[l]), bits(want[l]));
          first = buf;
        }
      }
    }
  }
  cudaFree(dev);
  cudaDeviceProp prop{};
  cudaGetDeviceProperties(&prop, 0);
  std::printf("%s\t%s\t%s\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%s\n", prop.name, name,
              cuda_own ? "cuda" : "ww_glibc_fma", total, differ, nan_payload, first.c_str());
  std::fflush(stdout);
  return differ ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string only = argc > 1 ? argv[1] : "";
  int rc = 0;
  const struct { Fn fn; const char* name; uint64_t total; } fns[] = {
      {kExpf, "expf", 1ULL << 32}, {kLogf, "logf", 1ULL << 32},
      {kSincosf, "sincosf", 1ULL << 32}, {kPowf, "powf", 1ULL << 32}};
  for (const auto& f : fns) {
    if (!only.empty() && only != f.name) continue;
    for (bool cuda_own : {false, true}) {
      const int r = check(f.fn, f.name, cuda_own, f.total);
      if (r == 2) { std::fprintf(stderr, "device_check: CUDA error\n"); return 2; }
      if (!cuda_own) rc |= r;
    }
  }
  // The verdict is the transcription's; CUDA's own functions are printed for comparison.
  return rc;
}
