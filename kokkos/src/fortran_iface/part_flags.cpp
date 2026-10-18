// kokkos/src/fortran_iface/part_flags.cpp
// The one place a WW_KOKKOS_<PART> run-time flag is resolved: every shim's
// ww_*_enabled() and W3KOKKOS_SETUP in w3kokkosmd.F90 ask ww_kokkos_flag(), so the
// Fortran LOGICALs and the shims cannot disagree about which parts run on Kokkos.
//
// WW_KOKKOS_<PART> set: "1" is on, any other value is off (the rule of every flag
// since 2026-10-14). WW_KOKKOS_<PART> unset: the master WW_KOKKOS_ALL decides, "1"
// on, anything else or unset off. So an unset master keeps today's defaults (all
// off), WW_KOKKOS_ALL=1 turns every part on, WW_KOKKOS_ALL=0 every part off, and a
// part set explicitly wins over the master. The *_COUNT diagnostics are not parts:
// they are read by each shim directly and ignore the master.
//
// The parts of kOptInOnly ignore the master too: the owner, 2026-10-16, asked that the PR1, PR2
// and unstructured explicit propagation (W3XYP1, W3KTP1, W3XYP2, W3KTP2, W3XYPUG) stay on the
// Fortran unless their own flag is "1", so WW_KOKKOS_ALL=1 cannot route them to Kokkos. The PR3
// propagation of the prop3 unit (W3XYP3, W3KTP3, PATCH.md section 11), W3OUTG of the outg unit
// (PATCH.md section 15), the partial read of VA before W3IOPE of the pnt unit (section 16) and
// W3UICE's zeroing of VA repeated on the device by the updv unit (section 17) are opt-in the same
// way until their rows are regtested. ST4_TABLES is opt-in as well (the owner, 2026-10-17): the
// C++ rebuild of the ST4 tables gives the Fortran's values ("0 values differ", the
// ST4_TABLES row of PATCH.md) but cost about 8 s serial per run (coast, SRCE 18.3-18.6 s against 26.6-26.7 s with ST4_TABLES,
// commit 4fbffbe; under a second since the TABU_* host parallel_for, TABU_TAUHF2_KOKKOS.md),
// while without it the shim reads the tables the Fortran built, the same bits by
// construction.
//
// WW_KOKKOS_PRESET=<name> (PATCH.md section 25) turns on the parts of one line of presets.txt as if
// each WW_KOKKOS_<PART> were "1", so a part the switch file does not cover stops the model as it
// does under --kokkos resident; a WW_KOKKOS_<PART> that is set still wins, and the master decides
// the parts the preset does not name. WW_KOKKOS_STRICT=1 (section 24) is read here once as well,
// and the per-part count of calls left on the Fortran that it would have stopped is kept here and
// printed once at exit. Every variable is read once per process (a function-local static): no
// getenv on a per-call path.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/ww_kokkos_c.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
struct Preset {
  const char* name;
  const char* parts;  // PART,PART,... without the WW_KOKKOS_ prefix
};
constexpr Preset kPresets[] = {
#include "ww_kokkos_presets.inc"
};
constexpr Preset kUnknown{"", ""};

// WW_KOKKOS_PRESET as read once: nullptr unset or empty, &kUnknown for a name presets.txt lacks.
const Preset* preset() {
  static const Preset* const p = []() -> const Preset* {
    const char* v = std::getenv("WW_KOKKOS_PRESET");
    if (v == nullptr || *v == '\0') return nullptr;
    for (const Preset& q : kPresets)
      if (std::strcmp(v, q.name) == 0) return &q;
    return &kUnknown;
  }();
  return p;
}

// Whether `part` is one item of the comma list `list`.
bool in_list(const char* part, const char* list) {
  const std::size_t n = std::strlen(part);
  for (const char* s = list; *s != '\0';) {
    const char* e = std::strchr(s, ',');
    const std::size_t m = e ? static_cast<std::size_t>(e - s) : std::strlen(s);
    if (m == n && std::strncmp(s, part, n) == 0) return true;
    if (e == nullptr) break;
    s = e + 1;
  }
  return false;
}

// The parts that can fall back to the Fortran, each with its count; filled at set-up, serially.
constexpr int kMaxSlots = 64;
char g_slot_name[kMaxSlots][25] = {};
long long g_slot_count[kMaxSlots] = {};
int g_nslots = 0;

void print_fallbacks() {
  std::fprintf(stderr, "ww_kokkos: calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead):");
  for (int i = 0; i < g_nslots; ++i) std::fprintf(stderr, " %s=%lld", g_slot_name[i], g_slot_count[i]);
  std::fprintf(stderr, "\n");
}

constexpr const char* kOptInOnly[] = {"WW_KOKKOS_XYP1", "WW_KOKKOS_KTP1", "WW_KOKKOS_XYP2",
                                      "WW_KOKKOS_KTP2", "WW_KOKKOS_XYPUG", "WW_KOKKOS_XYP3",
                                      "WW_KOKKOS_KTP3", "WW_KOKKOS_OUTG", "WW_KOKKOS_IOPE",
                                      "WW_KOKKOS_UPDV", "WW_KOKKOS_ST4_TABLES"};
}  // namespace

extern "C" int ww_kokkos_flag(const char* name) {
  if (name == nullptr) return 0;
  if (const char* v = std::getenv(name)) return std::strcmp(v, "1") == 0 ? 1 : 0;
  constexpr const char kPrefix[] = "WW_KOKKOS_";
  if (const Preset* p = preset(); p != nullptr && std::strncmp(name, kPrefix, sizeof kPrefix - 1) == 0 &&
                                  in_list(name + sizeof kPrefix - 1, p->parts))
    return 1;
  for (const char* p : kOptInOnly)
    if (std::strcmp(name, p) == 0) return 0;
  const char* all = std::getenv("WW_KOKKOS_ALL");
  return (all != nullptr && std::strcmp(all, "1") == 0) ? 2 : 0;
}

extern "C" int ww_kokkos_preset_status() {
  const Preset* p = preset();
  return p == nullptr ? 0 : (p == &kUnknown ? -1 : 1);
}

extern "C" int ww_kokkos_strict() {
  static const int strict = [] {
    const char* v = std::getenv("WW_KOKKOS_STRICT");
    return (v != nullptr && std::strcmp(v, "1") == 0) ? 1 : 0;
  }();
  return strict;
}

extern "C" int ww_kokkos_fallback_slot(const char* part) {
  if (part == nullptr || std::strlen(part) >= sizeof g_slot_name[0]) return -1;
  for (int i = 0; i < g_nslots; ++i)
    if (std::strcmp(g_slot_name[i], part) == 0) return i;
  if (g_nslots == kMaxSlots) return -1;
  if (g_nslots == 0) std::atexit(print_fallbacks);
  std::strcpy(g_slot_name[g_nslots], part);
  return g_nslots++;
}

extern "C" void ww_kokkos_fallback_add(int slot) {
  if (slot >= 0 && slot < g_nslots) ++g_slot_count[slot];
}
