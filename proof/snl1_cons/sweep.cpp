// proof/snl1_cons/sweep.cpp
// Exhaustive bit-for-bit check of W3SNL1 section 1: Fortran (cons_ref.F90) against the
// C++ port (cons_port.cpp) on every one of the 2^32 float32 values of KDMEAN.
//
// CONS depends on one runtime input, KDMEAN; the other six arguments are the &SNL1
// parameters, fixed per run and read from the committed fixture header so both sides get
// the exact bits the L1 tests use. With one float32 input, running all 2^32 cases is a
// proof for this build, not a sample: if no input differs, the two compiled functions are
// equal everywhere under these flags, this libm and this CPU.
//
// usage: sweep [fixture.bin]     (default: kokkos/tests/fixtures/snl1_nk25_nth24.bin)
// exit:  0 if no physical input (finite KDMEAN >= 0) differs, 1 otherwise, 2 on bad input.
//
// SPDX-License-Identifier: MIT

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" float cons_ref(float kdmean, float kdcon, float kdmn, float snlc1, float snls1,
                          float snls2, float snls3);
extern "C" float cons_port(float kdmean, float kdcon, float kdmn, float snlc1, float snls1,
                           float snls2, float snls3);

namespace {

float from_bits(std::uint32_t u) {
  float f;
  std::memcpy(&f, &u, sizeof f);
  return f;
}

std::uint32_t to_bits(float f) {
  std::uint32_t u;
  std::memcpy(&u, &f, sizeof u);
  return u;
}

struct Params {
  float snlc1, kdcon, kdmn, snls1, snls2, snls3;
};

// Header layout: kokkos/tests/fixtures/README.md ("Binary layout").
bool read_params(const char* path, Params& p) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::int32_t head[4];
  float r[10];  // xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, snls3, fachfe
  const bool ok = std::fread(head, sizeof head, 1, f) == 1 && std::fread(r, sizeof r, 1, f) == 1 &&
                  head[0] == 0x534E4C31;
  std::fclose(f);
  if (ok) p = Params{r[3], r[4], r[5], r[6], r[7], r[8]};
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "kokkos/tests/fixtures/snl1_nk25_nth24.bin";
  Params p{};
  if (!read_params(path, p)) {
    std::fprintf(stderr, "sweep: cannot read the SNL1 fixture header from %s\n", path);
    return 2;
  }

  // Classes: 0 physical (finite, >= 0), 1 finite negative, 2 inf or NaN.
  unsigned long long diff0 = 0, diff1 = 0, diff2 = 0, nan_payload = 0;
  std::uint64_t first0 = UINT64_MAX, first1 = UINT64_MAX, first2 = UINT64_MAX;

#pragma omp parallel for schedule(static) reduction(+ : diff0, diff1, diff2, nan_payload) \
    reduction(min : first0, first1, first2)
  for (std::int64_t i = 0; i <= static_cast<std::int64_t>(UINT32_MAX); ++i) {
    const std::uint32_t u = static_cast<std::uint32_t>(i);
    const float kd = from_bits(u);
    const float r = cons_ref(kd, p.kdcon, p.kdmn, p.snlc1, p.snls1, p.snls2, p.snls3);
    const float c = cons_port(kd, p.kdcon, p.kdmn, p.snlc1, p.snls1, p.snls2, p.snls3);
    if (to_bits(r) == to_bits(c)) continue;
    if (std::isnan(r) && std::isnan(c)) {
      ++nan_payload;
      continue;
    }
    if (!std::isfinite(kd)) {
      ++diff2;
      if (u < first2) first2 = u;
    } else if (std::signbit(kd)) {
      ++diff1;
      if (u < first1) first1 = u;
    } else {
      ++diff0;
      if (u < first0) first0 = u;
    }
  }

  std::printf("params  snlc1=%a kdcon=%a kdmn=%a snls1=%a snls2=%a snls3=%a\n", p.snlc1,
              p.kdcon, p.kdmn, p.snls1, p.snls2, p.snls3);
  std::printf("inputs  4294967296 (every float32 KDMEAN)\n");
  const unsigned long long n[3] = {diff0, diff1, diff2};
  const std::uint64_t first[3] = {first0, first1, first2};
  const char* name[3] = {"finite, >= 0", "finite, < 0", "inf or NaN"};
  for (int k = 0; k < 3; ++k) {
    std::printf("differ  %-13s %llu", name[k], n[k]);
    if (n[k] != 0) {
      const float kd = from_bits(static_cast<std::uint32_t>(first[k]));
      std::printf("   e.g. KDMEAN=%a: ref=%a port=%a", kd,
                  cons_ref(kd, p.kdcon, p.kdmn, p.snlc1, p.snls1, p.snls2, p.snls3),
                  cons_port(kd, p.kdcon, p.kdmn, p.snlc1, p.snls1, p.snls2, p.snls3));
    }
    std::printf("\n");
  }
  std::printf("nan     both NaN, payload differs: %llu\n", nan_payload);
  std::printf("verdict %s\n", diff0 == 0 ? "BIT-IDENTICAL on every physical input"
                                         : "DIFFERENT on physical inputs");
  return diff0 == 0 ? 0 : 1;
}
