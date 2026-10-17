// kokkos/tools/libm_bits/gen_tables.cpp
// Writes the data glibc_mathf.hpp's mvec_powf needs, from the machine itself rather than from
// glibc's sources: the two tables of libmvec's __svml_spow_data, read from the libmvec.so.1
// WW3 links, what this CPU's rcpps returns for every mantissa libmvec's powf gives it, and
// which of glibc's builds (fma or sse2) this CPU gets (WW_LIBM_HOST_FMA).
//
//   gen_tables <out.hpp> [libmvec.so.1]
//
// Without a path it reads the libmvec.so.1 this process loaded, which is the one the toolchain
// links WW3 and the port against. It finds __svml_spow_data in the library's symbol table.
//
// The header is generated in the build directory and not committed: the tables are glibc's
// (LGPL), and the rcpps runs are a fact about one CPU. It refuses when a constant glibc_mathf.hpp
// hard-codes differs from the library's, which would mean another glibc.
//
// SPDX-License-Identifier: MIT
#include <elf.h>
#include <link.h>
#include <xmmintrin.h>

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "glibc_mathf.hpp"

extern "C" __m128 _ZGVbN4v_expf(__m128);

namespace {

// Offsets in __svml_spow_data (glibc 2.44 sysdeps/x86_64/fpu/svml_s_powf_data.h).
constexpr long kLogLA = 8576, kLogLAEnd = 12736, kT = 13952, kSize = 30848;

struct Expect { long off; uint64_t bits; int width; const char* name; };
// Every constant mvec_powf writes as a literal, as the library must hold it.
constexpr Expect kExpect[] = {
    {0, 0x80800000, 4, "_NMINNORM"},          {64, 0xfeffffff, 4, "_NMAXVAL"},
    {128, 0x7f800000, 4, "_INF"},             {12864, 0x3fd55555555543c5, 8, "_poly_coeff_3"},
    {12928, 0xbfdffffffffff81f, 8, "_poly_coeff_4"}, {12992, 0x000fffffffffffff, 8, "_ExpMask"},
    {13056, 0x3f50000000000000, 8, "_Two10"}, {13312, 0x3ff0000000000000, 8, "_One"},
    {13504, 0x4086a00000000000, 8, "_Threshold"}, {13568, 0x408ff80000000000, 8, "_Bias"},
    {13632, 0x408ff00000000000, 8, "_Bias1"}, {13696, 0x3fe62e42fefa39ef, 8, "_L2"},
    {30400, 0x40a71547652b82fe, 8, "__dbInvLn2"}, {30464, 0x4338000000000000, 8, "__dbShifter"},
    {30528, 0x3fe0000000000000, 8, "__dbHALF"}, {30592, 0x3f362f3904051fa1, 8, "__dbC1"},
    {30656, 0x00000000000007ff, 8, "__lbLOWKBITS"}, {30720, 0x7fffffff, 4, "__iAbsMask"},
    {30784, 0x4059fe36, 4, "__iDomainRange"},
};

// The file offset of a symbol in an ELF64 shared object: its value in .symtab (or .dynsym)
// moved from the address of the section that holds it to that section's file offset.
long symbol_offset(const std::string& path, const char* name) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return -1;
  std::vector<unsigned char> b;
  unsigned char buf[65536];
  for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) b.insert(b.end(), buf, buf + n);
  std::fclose(f);
  // Every range check in subtracted form, so a crafted size cannot wrap around 2^64.
  const auto fits = [&](uint64_t off, uint64_t size) { return off <= b.size() && size <= b.size() - off; };
  if (b.size() < sizeof(Elf64_Ehdr) || std::memcmp(b.data(), ELFMAG, SELFMAG) != 0) return -1;
  Elf64_Ehdr eh;
  std::memcpy(&eh, b.data(), sizeof eh);
  if (eh.e_ident[EI_CLASS] != ELFCLASS64 || eh.e_shentsize != sizeof(Elf64_Shdr) ||
      !fits(eh.e_shoff, static_cast<uint64_t>(eh.e_shnum) * sizeof(Elf64_Shdr)))
    return -1;
  std::vector<Elf64_Shdr> sh(eh.e_shnum);
  std::memcpy(sh.data(), b.data() + eh.e_shoff, eh.e_shnum * sizeof(Elf64_Shdr));
  for (const auto& tab : sh) {
    if ((tab.sh_type != SHT_SYMTAB && tab.sh_type != SHT_DYNSYM) || tab.sh_link >= sh.size()) continue;
    const auto& str = sh[tab.sh_link];
    if (!fits(tab.sh_offset, tab.sh_size) || !fits(str.sh_offset, str.sh_size)) return -1;
    for (uint64_t o = 0; o + sizeof(Elf64_Sym) <= tab.sh_size; o += sizeof(Elf64_Sym)) {
      Elf64_Sym sym;
      std::memcpy(&sym, b.data() + tab.sh_offset + o, sizeof sym);
      if (sym.st_name >= str.sh_size || sym.st_shndx >= sh.size()) continue;
      const char* nm = reinterpret_cast<const char*>(b.data() + str.sh_offset + sym.st_name);
      const size_t len = strnlen(nm, str.sh_size - sym.st_name);
      if (len != std::strlen(name) || std::memcmp(nm, name, len) != 0) continue;
      const auto& sec = sh[sym.st_shndx];
      const uint64_t rel = sym.st_value - sec.sh_addr;
      if (sym.st_value < sec.sh_addr || rel > sec.sh_size || sym.st_size > sec.sh_size - rel ||
          !fits(sec.sh_offset, sec.sh_size))
        return -1;
      return static_cast<long>(sec.sh_offset + rel);
    }
  }
  return -1;
}

int find_libmvec(dl_phdr_info* info, size_t, void* out) {
  if (info->dlpi_name && std::strstr(info->dlpi_name, "/libmvec.so")) {
    *static_cast<std::string*>(out) = info->dlpi_name;
    return 1;
  }
  return 0;
}

// glibc picks its fma or sse2 build of expf, logf, powf, sincosf and exp from the CPU (and
// GLIBC_TUNABLES). Double exp tells them apart most often (1 485 995 of 2^33 inputs,
// docs/GPU_LIBM_BITS_202610.md); 1 if this process gets the fma build, 0 sse2, -1 neither.
// exp<false> is glibc's sse2 build only when this file is compiled with -ffp-contract=off.
int host_fma() {
  uint64_t u = 0x3ff0000000000000ULL;
  for (int i = 0; i < 100000000; ++i) {
    u = u * 6364136223846793005ULL + 1442695040888963407ULL;
    const double x = (ww::libm::detail::asdouble(0x3ff0000000000000ULL | (u >> 12)) - 1.5) * 1400.0;
    const double a = ww::libm::exp<true>(x), b = ww::libm::exp<false>(x), g = std::exp(x);
    if (a == b) continue;
    return g == a ? 1 : g == b ? 0 : -1;
  }
  return -1;
}

// The constants of __svml_s_trig_data that mvec_sinf and mvec_cosf write as literals
// (svml_s_trig_data.h offsets, 64 bytes per vector constant).
constexpr Expect kExpectTrig[] = {
    {0, 0x7fffffff, 4, "__sAbsMask"},       {64, 0x461c4000, 4, "__sRangeReductionVal"},
    {448, 0x40490000, 4, "__sPI1"},         {512, 0x3a7da000, 4, "__sPI2"},
    {576, 0x34222000, 4, "__sPI3"},         {640, 0x2cb4611a, 4, "__sPI4"},
    {896, 0xbe2aaaa6, 4, "__sA3"},          {960, 0x3c08876a, 4, "__sA5"},
    {1024, 0xb94fb7ff, 4, "__sA7"},         {1088, 0x362edef8, 4, "__sA9"},
    {1344, 0x3ea2f983, 4, "__sInvPI"},      {1408, 0x4b400000, 4, "__sRShifter"},
    {1472, 0x3fc90fdb, 4, "__sHalfPI"},     {1536, 0x3f000000, 4, "__sOneHalf"},
};

// 0 when every constant of `want` holds in `lib` at the file offset of `sym`.
int check_constants(const std::string& lib, const char* sym, const Expect* want, size_t n, long size) {
  const long off = symbol_offset(lib, sym);
  if (off < 0) { std::fprintf(stderr, "gen_tables: no %s in %s\n", sym, lib.c_str()); return 3; }
  FILE* f = std::fopen(lib.c_str(), "rb");
  std::vector<unsigned char> d(static_cast<size_t>(size));
  const bool ok = f && std::fseek(f, off, SEEK_SET) == 0 && std::fread(d.data(), 1, d.size(), f) == d.size();
  if (f) std::fclose(f);
  if (!ok) { std::perror(lib.c_str()); return 2; }
  for (size_t i = 0; i < n; ++i) {
    uint64_t v = 0;
    std::memcpy(&v, d.data() + want[i].off, want[i].width);
    if (v != want[i].bits) {
      std::fprintf(stderr, "gen_tables: %s is %" PRIx64 " in %s, %" PRIx64 " expected\n", want[i].name, v,
                   lib.c_str(), want[i].bits);
      return 3;
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) {
    std::fprintf(stderr, "usage: gen_tables <out.hpp> [libmvec.so.1]\n");
    return 64;
  }
  const char* outp = argv[1];
  std::string lib = argc == 3 ? argv[2] : "";
  if (lib.empty()) {
    // A call keeps the -lmvec dependency, so the loader has mapped it.
    alignas(16) float one[4] = {0, 0, 0, 0};
    _mm_store_ps(one, _ZGVbN4v_expf(_mm_load_ps(one)));
    dl_iterate_phdr(find_libmvec, &lib);
    if (lib.empty() || one[0] != 1.0f) { std::fprintf(stderr, "gen_tables: no libmvec loaded\n"); return 2; }
  }
  const long off = symbol_offset(lib, "__svml_spow_data");
  if (off < 0) { std::fprintf(stderr, "gen_tables: no __svml_spow_data in %s\n", lib.c_str()); return 3; }
  const int fma = host_fma();
  if (fma < 0) { std::fprintf(stderr, "gen_tables: this glibc's exp is neither its fma nor its sse2 build\n"); return 3; }
  FILE* f = std::fopen(lib.c_str(), "rb");
  if (!f || std::fseek(f, off, SEEK_SET) != 0) { std::perror(lib.c_str()); return 2; }
  std::vector<unsigned char> d(kSize);
  if (std::fread(d.data(), 1, kSize, f) != static_cast<size_t>(kSize)) { std::perror("read"); return 2; }
  std::fclose(f);
  for (const auto& e : kExpect) {
    uint64_t v = 0;
    std::memcpy(&v, d.data() + e.off, e.width);  // the first lane of the vector constant
    if (v != e.bits) {
      std::fprintf(stderr, "gen_tables: %s is %" PRIx64 " in %s, %" PRIx64 " expected\n", e.name, v,
                   lib.c_str(), e.bits);
      return 3;
    }
  }
  if (const int rc = check_constants(lib, "__svml_s_trig_data", kExpectTrig,
                                     sizeof kExpectTrig / sizeof kExpectTrig[0], 1600))
    return rc;
  FILE* o = std::fopen(outp, "w");
  if (!o) { std::perror(outp); return 2; }
  const char* base = std::strrchr(lib.c_str(), '/');
  std::fprintf(o, "// Generated by kokkos/tools/libm_bits/gen_tables.cpp from %s; do not edit.\n",
               base ? base + 1 : lib.c_str());
  std::fprintf(o, "#pragma once\n#define WW_LIBM_HOST_FMA %d\n#define WW_LIBM_POWF4_LA {", fma);
  for (long p = kLogLA; p < kLogLAEnd; p += 8) {
    uint64_t v; std::memcpy(&v, d.data() + p, 8);
    std::fprintf(o, "%s0x%016" PRIx64 "ULL", p == kLogLA ? "" : ",", v);
  }
  std::fprintf(o, "}\n#define WW_LIBM_POWF4_T {");
  for (int j = 0; j < 2048; ++j) {
    uint64_t v; std::memcpy(&v, d.data() + kT + 8 * j, 8);
    std::fprintf(o, "%s0x%016" PRIx64 "ULL", j ? "," : "", v);
  }
  // R(m) = round-to-nearest-even of rcpps(1.m x 2^-10), m the 23-bit mantissa; stored as the
  // first mantissa of each run of equal R.
  std::vector<uint32_t> start;
  std::vector<uint32_t> r;
  // How many mantissas get another R than an exact reciprocal would give, and how close the
  // nearest one comes to a rounding boundary: the room another vendor's rcpps has.
  uint32_t not_exact = 0;
  double margin = 1.0;
  for (uint32_t m = 0; m < (1u << 23); ++m) {
    float in;
    const uint32_t u = 0x3a800000u | m;
    std::memcpy(&in, &u, 4);
    const float rc = _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(in)));
    const auto R = static_cast<uint32_t>(std::nearbyint(static_cast<double>(rc)));
    const double exact = 1.0 / static_cast<double>(in);
    if (static_cast<uint32_t>(std::nearbyint(exact)) != R) ++not_exact;
    margin = std::fmin(margin, std::fabs(exact - std::floor(exact) - 0.5));
    // mvec_powf reads its log table at R - 512, 520 entries: another CPU's rcpps must stay in it.
    if (R < 512 || R > 1031) {
      std::fprintf(stderr, "gen_tables: rcpps gives R = %" PRIu32 " for mantissa %" PRIu32
                   ", outside the log table\n", R, m);
      std::fclose(o);
      std::remove(outp);
      return 3;
    }
    if (r.empty() || R != r.back()) { start.push_back(m); r.push_back(R); }
  }
  std::fprintf(o, "}\n#define WW_LIBM_RCP_RUNS %zu\n#define WW_LIBM_RCP_START {", start.size());
  for (size_t i = 0; i < start.size(); ++i) std::fprintf(o, "%s%" PRIu32, i ? "," : "", start[i]);
  std::fprintf(o, "}\n#define WW_LIBM_RCP_R {");
  for (size_t i = 0; i < r.size(); ++i) std::fprintf(o, "%s%" PRIu32, i ? "," : "", r[i]);
  std::fprintf(o, "}\n");
  std::fclose(o);
  bool monotone = true;
  for (size_t i = 1; i < r.size(); ++i) monotone &= r[i] < r[i - 1];
  std::printf("gen_tables: %zu rcpps runs (%s), 520 log and 2048 exp2 entries, constants match; "
              "glibc's %s build\n",
              start.size(), monotone ? "strictly decreasing" : "not monotone", fma ? "fma" : "sse2");
  std::printf("gen_tables: rcpps rounds to another R than the exact reciprocal for %" PRIu32
              " of 8388608 mantissas; the exact reciprocal comes within %.3g of a .5 boundary\n",
              not_exact, margin);
  return 0;
}
