// bench/dia/v1_cpu.cpp
// ww3-dia-gpu (v1, the GPU DIA kernel proposed upstream) on the CPU, so it can be timed in
// Serial and OpenMP next to WW3's W3SNL1 and the Kokkos port. The arithmetic is kernel_dia_sequential of
// ww3_dia_benchmark.cu (consigcody94/ww3-dia-gpu@38919d2, lines 92-126) line for line, and the
// inputs are its own main()'s (lines 128-164): synthetic energies, four index arrays
// (isp +- NTH [+1]) mod n, its eight weights, CONS/DAL constants. Its one-thread-per-point loop is
// the OpenMP loop here. It computes SA1 and SA2 of section 3 only; it is not the DIA, and its
// numbers are not comparable bin for bin with W3SNL1's (docs/W3SNL1_GPU_COMPARISON_202610.md).
// Built with GCC's default contraction, as nvcc's default would fuse it on the GPU.
//
// Derived from ww3_dia_benchmark.cu, which carries this licence:
//
// BSD 3-Clause License
//
// Copyright (c) 2026, Project Eagle NOAA Optimization
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <cstdlib>
#include <vector>

namespace {

struct V1 {
  int ngrid = 0, nspec = 0, nth = 0, nspec_ext = 0;
  std::vector<float> ue, af11, sa1, sa2;
  std::vector<int> ip1, ip2, im1, im2;
  float awg[8] = {0.25f, 0.25f, 0.25f, 0.25f, 0.15f, 0.15f, 0.35f, 0.35f};
  float cons = 3.0e7f, dal1 = 0.5f, dal2 = 0.25f, dal3 = 0.125f;
};

V1& v1() {
  static V1 s;
  return s;
}

}  // namespace

extern "C" {

// main() of ww3_dia_benchmark.cu, lines 128-164, at ngrid points and nspec = nk*nth bins.
void v1_init(int ngrid, int nk, int nth) {
  V1& s = v1();
  s.ngrid = ngrid;
  s.nth = nth;
  s.nspec = nk * nth;
  s.nspec_ext = s.nspec + nth * 8;
  std::srand(42);
  s.ue.assign(static_cast<size_t>(ngrid) * s.nspec_ext, 0.f);
  for (int ig = 0; ig < ngrid; ig++)
    for (int isp = 0; isp < s.nspec_ext; isp++)
      s.ue[static_cast<size_t>(ig) * s.nspec_ext + isp] =
          0.01f + 0.5f * (std::rand() / static_cast<float>(RAND_MAX));
  s.ip1.resize(s.nspec);
  s.ip2.resize(s.nspec);
  s.im1.resize(s.nspec);
  s.im2.resize(s.nspec);
  s.af11.resize(s.nspec);
  for (int isp = 0; isp < s.nspec; isp++) {
    s.ip1[isp] = (isp + nth) % s.nspec_ext;
    s.ip2[isp] = (isp + nth + 1) % s.nspec_ext;
    s.im1[isp] = (isp + s.nspec_ext - nth) % s.nspec_ext;
    s.im2[isp] = (isp + s.nspec_ext - nth - 1) % s.nspec_ext;
    s.af11[isp] = 1.0f + 0.1f * (std::rand() / static_cast<float>(RAND_MAX));
  }
  s.sa1.assign(static_cast<size_t>(ngrid) * s.nspec, 0.f);
  s.sa2.assign(static_cast<size_t>(ngrid) * s.nspec, 0.f);
}

// kernel_dia_sequential, lines 92-126: one point per iteration of the OpenMP loop.
void v1_run(void) {
  V1& s = v1();
  const float* ue = s.ue.data();
  const int* ip1 = s.ip1.data();
  const int* ip2 = s.ip2.data();
  const int* im1 = s.im1.data();
  const int* im2 = s.im2.data();
  const float* awg = s.awg;
  const float* af11 = s.af11.data();
  float* sa1 = s.sa1.data();
  float* sa2 = s.sa2.data();
  const int nspec = s.nspec, nspec_ext = s.nspec_ext, ngrid = s.ngrid;
  const float cons = s.cons, dal1 = s.dal1, dal2 = s.dal2, dal3 = s.dal3;
#pragma omp parallel for schedule(static)
  for (int igrid = 0; igrid < ngrid; igrid++) {
    const size_t base = static_cast<size_t>(igrid) * nspec_ext;
    for (int isp = 0; isp < nspec; isp++) {
      float E00 = ue[base + isp];
      float EP1 = awg[0] * ue[base + ip1[isp]] + awg[1] * ue[base + ip2[isp]];
      float EM1 = awg[2] * ue[base + im1[isp]] + awg[3] * ue[base + im2[isp]];
      float EP2 = awg[4] * ue[base + ip1[isp]] + awg[5] * ue[base + ip2[isp]];
      float EM2 = awg[6] * ue[base + im1[isp]] + awg[7] * ue[base + im2[isp]];

      float FACTOR = cons * af11[isp] * E00;
      float SA1A = E00 * (EP1 * dal1 + EM1 * dal2);
      float SA1B = SA1A - EP1 * EM1 * dal3;
      float SA2A = E00 * (EP2 * dal1 + EM2 * dal2);
      float SA2B = SA2A - EP2 * EM2 * dal3;

      sa1[static_cast<size_t>(igrid) * nspec + isp] = FACTOR * SA1B;
      sa2[static_cast<size_t>(igrid) * nspec + isp] = FACTOR * SA2B;
    }
  }
}

// A value of the output, so the compiler cannot drop the loop.
float v1_checksum(void) {
  const V1& s = v1();
  double sum = 0.0;
  for (size_t i = 0; i < s.sa1.size(); i += 97) sum += s.sa1[i] + s.sa2[i];
  return static_cast<float>(sum);
}

}  // extern "C"
