// kokkos/src/fortran_iface/outg_shim.cpp
// The bind(C) shim of the outg wiring unit (WFIP-0005 unit 5): W3OUTG's integrals on the resident
// VA. Marshalling only; the arithmetic is the translated header iogo_w3outg.hpp, called unchanged.
//
// The caller is W3WAVE (w3wavemd.F90) through W3KOKKOS_OUTG of w3kokkos_outgmd.F90 on the fork,
// at a field-output time, outside any OpenMP region, in place of CALL W3OUTG (VA, FLPFLD, FLOUTG,
// FLOUTG2). W3KOKKOS_OUTG calls the Fortran W3OUTG instead when a field this unit does not compute
// is requested: the partitions of section 6 (FLPFLD; they read W3CPRT's table, built on the host
// from VA), CALC_U3STOKES (FLOLOC(6,8), (6,12)), SKEWNESS (FLOLOC(8,7..9)) and CALC_WBT
// (FLOLOC(2,17)). So ww::iogo::w3outg runs here with FLPART false and no hook requested.
//
// Memory contract (phase 2). A reads d_va of the grid (ww::resident::device_va), so VA crosses
// only when the host holds the newer bits, and W3WAVE makes no host read of VA for this call. On a
// backend whose execution space reads host memory (Serial, OpenMP) every input and output is
// WW3's own array, read and written in place, with no copy. On a device the inputs (CG, WN, DW,
// the frequency and direction tables, MAPSF, MAPSTA) are copied in per call, which is per output
// time, and the forty-six fields are device buffers that live for the run: only those a host
// reader needs come back, the FLOGRD (FLOUTG) and FLOGR2 (FLOUTG2) entries W3IOGO ('WRITE') writes
// and, when OARST, the FLOGRR entries W3IORS ('HOT') writes (w3iorsmd.F90:1044-1120 at the pin).
// No other reader of these fields runs in ww3_shel under the lab switches (PATCH.md section 15
// has the audit). No Unified Memory.
//
// WW_KOKKOS_OUTG_COUNT=1 prints, at exit, the calls, the sea points and the fields and bytes that
// came back (on a host backend, the copies a device would make, labelled as such: none is made).
// WW_KOKKOS_TIMERS=1 times outg.call, outg.h2d, outg.kernel and outg.d2h (timers.hpp).
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/outg_c.hpp"
#include "fortran_iface/timers.hpp"
#include "fortran_iface/ww_kokkos_c.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <type_traits>

#include "ww_kokkos/iogo_w3outg.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/resident_state.hpp"

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace ww::outgshim {
namespace {

using ww::Real;
using Exec = Kokkos::DefaultExecutionSpace;
using Mem = Exec::memory_space;
using Args = ww::iogo::W3OutgArgs<Exec>;
static_assert(std::is_same_v<Real, float>, "the C ABI passes REAL as float");
constexpr bool kHostReadable = Kokkos::SpaceAccessibility<Exec, Kokkos::HostSpace>::accessible;

template <class T>
using H1 = Kokkos::View<T*, Kokkos::LayoutLeft, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
template <class T>
using H2 = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
template <class T>
using D1 = Kokkos::View<T*, Kokkos::LayoutLeft, Mem>;
template <class T>
using D2 = Kokkos::View<T**, Kokkos::LayoutLeft, Mem>;
template <class T>
using U1 = Kokkos::View<T*, Kokkos::LayoutLeft, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
template <class T>
using U2 = Kokkos::View<T**, Kokkos::LayoutLeft, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
using U3 = Kokkos::View<const Real***, Kokkos::LayoutLeft, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

// The (group, index) entries of FLOGRD that make W3IOGO ('WRITE') write each field
// (w3iogomd.F90:3158-3683 at the pin), and of FLOGRR that make W3IORS ('HOT') write it
// (w3iorsmd.F90:1044-1120); {0, 0} ends a list. W3IOGO writes FP0 for (2,6) and for TP, (2,18).
struct Key {
  int i, j;
};
struct FieldKeys {
  Key grd[3];
  Key grr[2];
};
constexpr FieldKeys kKeys[WW_OUTG_NFIELD] = {
    {{{2, 1}}, {{2, 1}}},             // HS
    {{{2, 2}}, {{2, 2}}},             // WLM
    {{{2, 3}}, {}},                   // T02
    {{{2, 4}}, {{2, 4}}},             // T0M1
    {{{2, 5}}, {{2, 5}}},             // T01
    {{{2, 6}, {2, 18}}, {{2, 6}}},    // FP0
    {{{2, 7}}, {{2, 7}}},             // THM
    {{{2, 8}}, {}},                   // THS
    {{{2, 9}}, {}},                   // THP0
    {{{2, 10}}, {}},                  // HSIG
    {{{8, 6}}, {}},                   // QKK
    {{{8, 5}}, {}},                   // QP
    {{{2, 17}}, {}},                  // WBT
    {{{2, 11}}, {}},                  // STMAXE
    {{{2, 12}}, {}},                  // STMAXD
    {{{2, 14}}, {}},                  // HCMAXE
    {{{2, 13}}, {}},                  // HMAXE
    {{{2, 16}}, {}},                  // HCMAXD
    {{{2, 15}}, {}},                  // HMAXD
    {{{7, 1}}, {}},                   // ABA
    {{{7, 1}}, {}},                   // ABD
    {{{7, 2}}, {{7, 2}}},             // UBA
    {{{7, 2}}, {{7, 2}}},             // UBD
    {{{6, 1}}, {}},                   // SXX
    {{{6, 1}}, {}},                   // SYY
    {{{6, 1}}, {}},                   // SXY
    {{{6, 6}}, {{6, 6}}},             // USSX
    {{{6, 6}}, {{6, 6}}},             // USSY
    {{{6, 5}}, {{6, 5}}},             // TUSX
    {{{6, 5}}, {{6, 5}}},             // TUSY
    {{{8, 1}}, {}},                   // MSSX
    {{{8, 1}}, {}},                   // MSSY
    {{{8, 3}}, {}},                   // MSSD
    {{{8, 2}}, {}},                   // MSCX
    {{{8, 2}}, {}},                   // MSCY
    {{{8, 4}}, {}},                   // MSCD
    {{{6, 7}}, {}},                   // PRMS
    {{{6, 7}}, {}},                   // TPMS
    {{{6, 3}}, {{6, 3}}},             // BHD
    {{{5, 3}}, {}},                   // CGE
    {{{3, 1}}, {}},                   // EF
    {{{3, 2}}, {}},                   // TH1M
    {{{3, 3}}, {}},                   // STH1M
    {{{3, 4}}, {}},                   // TH2M
    {{{3, 5}}, {}},                   // STH2M
    {{{6, 9}}, {}},                   // P2SMS
};

std::atomic<long long> g_calls{0}, g_points{0}, g_back{0};
std::atomic<long long> g_back_bytes{0};

void print_counts() {
  const char* proj = kHostReadable ? " (projected for a device, none made)" : "";
  std::fprintf(stderr, "ww_kokkos: W3OUTG Kokkos calls: %lld\n", g_calls.load());
  std::fprintf(stderr, "ww_kokkos: W3OUTG sea points: %lld\n", g_points.load());
  std::fprintf(stderr, "ww_kokkos: W3OUTG fields copied back D2H%s: %lld\n", proj, g_back.load());
  std::fprintf(stderr, "ww_kokkos: W3OUTG bytes copied back D2H%s: %lld\n", proj, g_back_bytes.load());
}

/// WW_KOKKOS_OUTG_COUNT=1 registers the exit print when the library loads, so a run in which
/// the entry point was never called still says 0.
[[maybe_unused]] const bool g_count_on = [] {
  const char* v = std::getenv("WW_KOKKOS_OUTG_COUNT");
  const bool c = v != nullptr && std::strcmp(v, "1") == 0;
  if (c) std::atexit(print_counts);
  return c;
}();

int fail(int code, const char* what) {
  std::fprintf(stderr, "ww_kokkos: outg: %s (code %d)\n", what, code);
  return code;
}

/// The device buffers of one grid, kept between calls (unused on a host backend).
struct Bufs {
  D2<Real> cg, wn, ebd;
  D1<Real> dw, sig, dden, dsii, ecos, esin, ec2, es2, esc;
  D2<int> mapsf, mapsta;
  D1<Real> f1[WW_OUTG_N1D];
  D2<Real> f2[WW_OUTG_NFIELD - WW_OUTG_N1D];
};
Bufs& bufs() {
  static auto* b = new Bufs();  // never destroyed, as every shim's state
  static bool hooked = false;
  if (!hooked && Kokkos::is_initialized()) {
    hooked = true;
    Kokkos::push_finalize_hook([] { *b = Bufs(); });
  }
  return *b;
}

template <class V>
void ensure(V& v, const char* name, size_t n0, size_t n1 = 0) {
  if constexpr (V::rank == 1) {
    if (v.extent(0) != n0) v = V(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(name)), n0);
  } else {
    if (v.extent(0) != n0 || v.extent(1) != n1)
      v = V(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(name)), n0, n1);
  }
}

/// A read-only input: WW3's array itself on a host backend; else its device buffer, refreshed.
template <class T>
U1<const T> in1(D1<T>& buf, const char* name, const T* p, size_t n) {
  if constexpr (kHostReadable) {
    (void)buf;
    (void)name;
    return U1<const T>(p, n);
  } else {
    ensure(buf, name, n);
    Kokkos::deep_copy(buf, H1<const T>(p, n));
    return U1<const T>(buf.data(), n);
  }
}
template <class T>
U2<const T> in2(D2<T>& buf, const char* name, const T* p, size_t n0, size_t n1) {
  if constexpr (kHostReadable) {
    (void)buf;
    (void)name;
    return U2<const T>(p, n0, n1);
  } else {
    ensure(buf, name, n0, n1);
    Kokkos::deep_copy(buf, H2<const T>(p, n0, n1));
    return U2<const T>(buf.data(), n0, n1);
  }
}

/// An output field: WW3's array on a host backend; else its device buffer.
U1<Real> out1(D1<Real>& buf, float* p, size_t n) {
  if constexpr (kHostReadable) {
    (void)buf;
    return U1<Real>(p, n);
  } else {
    ensure(buf, "ww_outg.field", n);
    return U1<Real>(buf.data(), n);
  }
}
U2<Real> out2(D2<Real>& buf, float* p, size_t n0, size_t n1) {
  if (p == nullptr) return U2<Real>();
  if constexpr (kHostReadable) {
    (void)buf;
    return U2<Real>(p, n0, n1);
  } else {
    ensure(buf, "ww_outg.field2", n0, n1);
    return U2<Real>(buf.data(), n0, n1);
  }
}

bool nested_in_openmp() {
#if defined(_OPENMP)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

int run(int igrid, const int* ipar, const float* rpar, const int* flogrd, const int* flogr2,
        const int* flogrr, float* va, int nspec, int nsealm, const float* cg, const float* wn,
        const float* dw, const float* sig, const float* dden, const float* dsii, const float* ecos,
        const float* esin, const float* ec2, const float* es2, const float* esc, const int* mapsf,
        const int* mapsta, float* const* fields) {
  const int nk = ipar[WW_OUTG_NK], nth = ipar[WW_OUTG_NTH], nseal = ipar[WW_OUTG_NSEAL];
  const int nsea = ipar[WW_OUTG_NSEA], nx = ipar[WW_OUTG_NX], ny = ipar[WW_OUTG_NY];
  const int nfld = ipar[WW_OUTG_NFLD], nogrp = ipar[WW_OUTG_NOGRP], ngrpp = ipar[WW_OUTG_NGRPP];
  if (nk < 1 || nth < 1 || nspec != nk * nth || nseal < 0 || nseal > nsealm || nsea < nseal ||
      nx < 1 || ny < 1 || nfld < nseal || nogrp < 10 || ngrpp < 1)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: bad sizes");
  for (int k = 0; k < WW_OUTG_N1D; ++k)
    if (fields[k] == nullptr) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: a null 1-D field");

  ww::iogo::W3OutgScalars s{};
  s.nk = nk;
  s.nth = nth;
  s.nseal = nseal;
  s.noswll = 0;
  s.dth = rpar[WW_OUTG_DTH];
  s.xfr = rpar[WW_OUTG_XFR];
  s.fte = rpar[WW_OUTG_FTE];
  s.ftwl = rpar[WW_OUTG_FTWL];
  s.fttr = rpar[WW_OUTG_FTTR];
  s.undef = rpar[WW_OUTG_UNDEF];
  s.stexu = rpar[WW_OUTG_STEXU];
  s.steyu = rpar[WW_OUTG_STEYU];
  s.stedu = rpar[WW_OUTG_STEDU];
  s.wscut = rpar[WW_OUTG_WSCUT];
  s.p2msf2 = ipar[WW_OUTG_P2MSF2];
  s.p2msf3 = ipar[WW_OUTG_P2MSF3];
  for (int k = 0; k < 5; ++k) {
    s.e3df2[k] = ipar[WW_OUTG_E3DF2 + k];
    s.e3df3[k] = ipar[WW_OUTG_E3DF3 + k];
  }
  s.ompg = ipar[WW_OUTG_OMPG] != 0;

  const bool floutg = ipar[WW_OUTG_FLOUTG] != 0, floutg2 = ipar[WW_OUTG_FLOUTG2] != 0;
  const bool oarst = ipar[WW_OUTG_OARST] != 0;
  const auto lgrd = [&](const int* f, int i, int j) { return f[(i - 1) + (j - 1) * nogrp] != 0; };
  const auto floloc = [&](int i, int j) {
    return (floutg && lgrd(flogrd, i, j)) || (floutg2 && lgrd(flogr2, i, j));
  };
  // The fields this unit leaves to the Fortran (W3KOKKOS_OUTG falls back for them).
  if (floloc(6, 8) || floloc(6, 12) || floloc(8, 7) || floloc(8, 8) || floloc(8, 9) || floloc(2, 17))
    return fail(WW_KOKKOS_ERR_UNSUPPORTED, "ww_outg_va: a field left to the Fortran W3OUTG is requested");
  // The 2-D fields: the extent of each, and a requested one must exist.
  size_t n2[WW_OUTG_NFIELD - WW_OUTG_N1D];
  for (int k = 0; k < 5; ++k) {
    n2[k] = s.e3df3[k] >= s.e3df2[k] ? static_cast<size_t>(s.e3df3[k] - s.e3df2[k] + 1) : 0;
    if (floloc(3, k + 1) && (fields[WW_OUTG_EF + k] == nullptr || n2[k] == 0))
      return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: a requested 3-D field is not allocated");
  }
  n2[5] = s.p2msf3 >= s.p2msf2 ? static_cast<size_t>(s.p2msf3 - s.p2msf2 + 1) : 0;
  if (floloc(6, 9) && (fields[WW_OUTG_P2SMS] == nullptr || n2[5] == 0))
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: P2SMS is requested and not allocated");

  ww::timers::Region tcall("outg.call");
  ww::resident::DevVa dva;
  const int rc = ww::resident::device_va(igrid, va, nspec, nsealm, dva);
  if (rc != WW_KOKKOS_OK) return fail(rc, "the resident VA refused this call");
  if (dva.stride(1) != static_cast<size_t>(nspec))
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "the resident VA is padded");

  Bufs& b = bufs();
  Args g;
  const size_t nf = static_cast<size_t>(nfld);
  {
    ww::timers::Region t("outg.h2d");
    g.a = U3(dva.data(), static_cast<size_t>(nth), static_cast<size_t>(nk), static_cast<size_t>(nsealm) + 1);
    const size_t nk2 = static_cast<size_t>(nk) + 2, ns1 = static_cast<size_t>(nsea) + 1;
    g.cg = in2(b.cg, "ww_outg.cg", cg, nk2, ns1);
    g.wn = in2(b.wn, "ww_outg.wn", wn, nk2, ns1);
    g.dw = in1(b.dw, "ww_outg.dw", dw, ns1);
    g.sig = in1(b.sig, "ww_outg.sig", sig, nk2);
    g.dden = in1(b.dden, "ww_outg.dden", dden, static_cast<size_t>(nk));
    g.dsii = in1(b.dsii, "ww_outg.dsii", dsii, static_cast<size_t>(nk));
    const size_t nt = static_cast<size_t>(nth);
    g.ecos = in1(b.ecos, "ww_outg.ecos", ecos, nt);
    g.esin = in1(b.esin, "ww_outg.esin", esin, nt);
    g.ec2 = in1(b.ec2, "ww_outg.ec2", ec2, nt);
    g.es2 = in1(b.es2, "ww_outg.es2", es2, nt);
    g.esc = in1(b.esc, "ww_outg.esc", esc, nt);
    g.mapsf = in2(b.mapsf, "ww_outg.mapsf", mapsf, static_cast<size_t>(nsea), 3);
    g.mapsta = in2(b.mapsta, "ww_outg.mapsta", mapsta, static_cast<size_t>(ny), static_cast<size_t>(nx));
    ensure(b.ebd, "ww_outg.ebd", static_cast<size_t>(nk), static_cast<size_t>(nseal));
    g.ebd = b.ebd;
    Args::V1<Real>* o1[WW_OUTG_N1D] = {&g.hs,     &g.wlm,    &g.t02,    &g.t0m1, &g.t01,  &g.fp0,  &g.thm,
                                 &g.ths,    &g.thp0,   &g.hsig,   &g.qkk,  &g.qp,   &g.wbt,  &g.stmaxe,
                                 &g.stmaxd, &g.hcmaxe, &g.hmaxe,  &g.hcmaxd, &g.hmaxd, &g.aba, &g.abd,
                                 &g.uba,    &g.ubd,    &g.sxx,    &g.syy,  &g.sxy,  &g.ussx, &g.ussy,
                                 &g.tusx,   &g.tusy,   &g.mssx,   &g.mssy, &g.mssd, &g.mscx, &g.mscy,
                                 &g.mscd,   &g.prms,   &g.tpms,   &g.bhd,  &g.cge};
    for (int k = 0; k < WW_OUTG_N1D; ++k) *o1[k] = out1(b.f1[k], fields[k], nf);
    Args::V2<Real>* o2[WW_OUTG_NFIELD - WW_OUTG_N1D] = {&g.ef, &g.th1m, &g.sth1m, &g.th2m, &g.sth2m, &g.p2sms};
    for (int k = 0; k < WW_OUTG_NFIELD - WW_OUTG_N1D; ++k)
      *o2[k] = out2(b.f2[k], fields[WW_OUTG_N1D + k], nf, n2[k]);
    // CGE is not reset by W3OUTG (it writes sea points only): its device buffer holds the host bits.
    if constexpr (!kHostReadable) Kokkos::deep_copy(g.cge, H1<const Real>(fields[WW_OUTG_CGE], nf));
  }

  ww::iogo::W3OutgFlags f{false, floutg, floutg2, flogrd, flogr2};
  {
    ww::timers::Region t("outg.kernel");
    ww::iogo::w3outg(Exec(), s, f, g, ww::iogo::W3OutgHooks{});
    Exec().fence("ww_outg_va");
  }

  // The fields a host reader needs, back to WW3's arrays (on a host backend they are there).
  {
    ww::timers::Region t("outg.d2h");
    for (int k = 0; k < WW_OUTG_NFIELD; ++k) {
      bool want = false;
      for (const Key& key : kKeys[k].grd)
        if (key.i != 0) want = want || floloc(key.i, key.j);
      if (oarst)
        for (const Key& key : kKeys[k].grr)
          if (key.i != 0) want = want || lgrd(flogrr, key.i, key.j);
      if (!want) continue;
      const size_t n = k < WW_OUTG_N1D ? nf : nf * n2[k - WW_OUTG_N1D];
      if (n == 0) continue;
      if constexpr (!kHostReadable) {
        if (k < WW_OUTG_N1D)
          Kokkos::deep_copy(H1<Real>(fields[k], nf), b.f1[k]);
        else
          Kokkos::deep_copy(H2<Real>(fields[k], nf, n2[k - WW_OUTG_N1D]), b.f2[k - WW_OUTG_N1D]);
      }
      g_back.fetch_add(1, std::memory_order_relaxed);
      g_back_bytes.fetch_add(static_cast<long long>(n * sizeof(float)), std::memory_order_relaxed);
    }
    if constexpr (!kHostReadable) Exec().fence("ww_outg_va d2h");
  }
  g_calls.fetch_add(1, std::memory_order_relaxed);
  g_points.fetch_add(nseal, std::memory_order_relaxed);
  return WW_KOKKOS_OK;
}

}  // namespace
}  // namespace ww::outgshim

extern "C" int ww_outg_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar,
                          const int* flogrd, const int* flogr2, const int* flogrr, float* va, int nspec,
                          int nsealm, const float* cg, const float* wn, const float* dw, const float* sig,
                          const float* dden, const float* dsii, const float* ecos, const float* esin,
                          const float* ec2, const float* es2, const float* esc, const int* mapsf,
                          const int* mapsta, float* const* fields, int nfields) {
  using namespace ww::outgshim;
  if (ipar == nullptr || rpar == nullptr || nipar != WW_OUTG_NIPAR || nrpar != WW_OUTG_NRPAR ||
      nfields != WW_OUTG_NFIELD || fields == nullptr)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: bad packet");
  if (flogrd == nullptr || flogr2 == nullptr || flogrr == nullptr || va == nullptr || cg == nullptr ||
      wn == nullptr || dw == nullptr || sig == nullptr || dden == nullptr || dsii == nullptr ||
      ecos == nullptr || esin == nullptr || ec2 == nullptr || es2 == nullptr || esc == nullptr ||
      mapsf == nullptr || mapsta == nullptr)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_outg_va: a null pointer");
  try {
    if (!Kokkos::is_initialized()) return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    if (nested_in_openmp()) return fail(WW_KOKKOS_ERR_UNSUPPORTED, "inside an OpenMP region");
    const int st = run(igrid, ipar, rpar, flogrd, flogr2, flogrr, va, nspec, nsealm, cg, wn, dw, sig,
                       dden, dsii, ecos, esin, ec2, es2, esc, mapsf, mapsta, fields);
    // W3OUTG reads A and never writes it: d_va keeps the state device_va left (BOTH or DEVICE).
    return st;
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}
