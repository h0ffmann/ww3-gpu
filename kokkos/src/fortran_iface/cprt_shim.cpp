// kokkos/src/fortran_iface/cprt_shim.cpp
// The bind(C) shim of the cprt wiring unit (PATCH.md section 27): W3CPRT's spectral partitioning
// on the resident VA. Marshalling and the order of the launches only; the arithmetic is the
// translated headers iosf_w3cprt.hpp and part_w3part.hpp, called unchanged.
//
// The caller is W3WAVE (w3wavemd.F90) through W3KOKKOS_CPRT of w3kokkos_cprtmd.F90 on the fork,
// at an output time with FLPART, outside any OpenMP region, in place of CALL W3CPRT ( IMOD ) and
// of the host read of VA before it.
//
// Order. The local sea points go in chunks of `chunk` points (WW_KOKKOS_CPRT_CHUNK, read once;
// by default four per thread of the execution space on a host backend, 1024 on a device), each
// point with its own work slot. Per chunk: one launch runs sections 2-4 (W3PART) for every point;
// a reduction counts XP-full messages and the points PTMEAN cannot take; an integer prefix sum
// gives each point its first DTPRT column and ICPRT(JSEA+1,2); a last launch copies XP(:,0:NP) to
// those columns. Integer sums are exact in any order, so DTPRT and ICPRT are the Fortran's JSEA
// order by construction, and each point's floating-point arithmetic is W3PART's serial code.
//
// Memory contract (phase 2). VA is read in d_va (ww::resident::device_va), so it crosses only
// when the host holds the newer bits, and W3WAVE makes no host read of VA for this call. On a
// backend whose execution space reads host memory (Serial, OpenMP) the inputs are WW3's own
// arrays, read in place. On a device the inputs (MAPSF, MAPSTA, U10, U10D, ASF, DW, CG, WN, the
// frequency and direction tables, DISTAB's EWN1 and ECG1) are copied in per call, which is per
// output time. ICPRT (NSEALM+1 by 2 ints) and the FINSIZ columns of DTPRT (15 Reals each) are
// built in the shim's buffers and copied back by ww_cprt_fetch; nothing else comes back. The work
// slots are device buffers of `chunk` slots that live for the run. No Unified Memory.
//
// WW_KOKKOS_CPRT_COUNT=1 prints, at exit, the calls, the sea points, the fallbacks to the Fortran
// and the bytes copied in and back (on a host backend the copies a device would make, labelled as
// such: none is made). WW_KOKKOS_TIMERS=1 times cprt.call, cprt.h2d, cprt.kernel and cprt.d2h.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/cprt_c.hpp"
#include "fortran_iface/timers.hpp"
#include "fortran_iface/ww_kokkos_c.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <new>
#include <string>
#include <type_traits>

#include "ww_kokkos/iosf_w3cprt.hpp"
#include "ww_kokkos/part_ptnghb.hpp"
#include "ww_kokkos/part_w3part.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/resident_state.hpp"

#if defined(_OPENMP)
#include <omp.h>
#endif

namespace ww::cprtshim {
namespace {

using ww::Real;
using Exec = Kokkos::DefaultExecutionSpace;
using Mem = Exec::memory_space;
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

std::atomic<long long> g_calls{0}, g_points{0}, g_fallbacks{0}, g_h2d_bytes{0}, g_d2h_bytes{0};

void print_counts() {
  const char* proj = kHostReadable ? " (projected for a device, none made)" : "";
  std::fprintf(stderr, "ww_kokkos: W3CPRT Kokkos calls: %lld\n", g_calls.load());
  std::fprintf(stderr, "ww_kokkos: W3CPRT sea points: %lld\n", g_points.load());
  std::fprintf(stderr, "ww_kokkos: W3CPRT fallbacks to the Fortran (no peak): %lld\n", g_fallbacks.load());
  std::fprintf(stderr, "ww_kokkos: W3CPRT input bytes copied H2D%s: %lld\n", proj, g_h2d_bytes.load());
  std::fprintf(stderr, "ww_kokkos: W3CPRT table bytes copied back D2H%s: %lld\n", proj, g_d2h_bytes.load());
}

/// WW_KOKKOS_CPRT_COUNT=1 registers the exit print when the library loads, so a run in which
/// the entry point was never called still says 0.
[[maybe_unused]] const bool g_count_on = [] {
  const char* v = std::getenv("WW_KOKKOS_CPRT_COUNT");
  const bool c = v != nullptr && std::strcmp(v, "1") == 0;
  if (c) std::atexit(print_counts);
  return c;
}();

int fail(int code, const char* what) {
  std::fprintf(stderr, "ww_kokkos: cprt: %s (code %d)\n", what, code);
  return code;
}

/// Points per chunk: WW_KOKKOS_CPRT_CHUNK when a positive integer, else the default above.
int chunk_size() {
  static const int n = [] {
    if (const char* v = std::getenv("WW_KOKKOS_CPRT_CHUNK")) {
      char* end = nullptr;
      const long c = std::strtol(v, &end, 10);
      if (end != v && *end == '\0' && c > 0 && c <= (1L << 20)) return static_cast<int>(c);
      std::fprintf(stderr, "ww_kokkos: cprt: WW_KOKKOS_CPRT_CHUNK='%s' ignored\n", v);
    }
    return kHostReadable ? 4 * Exec().concurrency() : 1024;
  }();
  return n;
}

/// The shim's buffers, kept between calls.
struct Bufs {
  // Inputs on a device (unused on a host backend).
  D2<int> mapsf, mapsta;
  D1<Real> u10, u10d, asf, dw, sig, dsip, dsii, th, ecos, esin, ewn1, ecg1;
  D2<Real> cg, wn;
  D1<int> neigh;
  ww::part::PtnghbTable table;
  // Work slots and per-point results of a chunk.
  D1<Real> wr;
  D1<int> wi, np, nxp, rc, first;
  // The table, until ww_cprt_fetch.
  D2<int> icprt;
  D2<Real> dtprt;
  int igrid = -1, nsealm = -1, finsiz = -1;
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
  if (kHostReadable) return U1<const T>(p, n);
  ensure(buf, name, n);
  Kokkos::deep_copy(buf, H1<const T>(p, n));
  g_h2d_bytes.fetch_add(static_cast<long long>(n * sizeof(T)), std::memory_order_relaxed);
  return U1<const T>(buf.data(), n);
}
template <class T>
U2<const T> in2(D2<T>& buf, const char* name, const T* p, size_t n0, size_t n1) {
  if (kHostReadable) return U2<const T>(p, n0, n1);
  ensure(buf, name, n0, n1);
  Kokkos::deep_copy(buf, H2<const T>(p, n0, n1));
  g_h2d_bytes.fetch_add(static_cast<long long>(n0 * n1 * sizeof(T)), std::memory_order_relaxed);
  return U2<const T>(buf.data(), n0, n1);
}
/// What a device would copy in, counted on a host backend too.
void count_in(size_t bytes) {
  if (kHostReadable) g_h2d_bytes.fetch_add(static_cast<long long>(bytes), std::memory_order_relaxed);
}

bool nested_in_openmp() {
#if defined(_OPENMP)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

int run(int igrid, const int* ipar, const float* rpar, float* va, int nspec, int nsealm, const int* mapsf,
        const int* mapsta, const float* u10, const float* u10d, const float* asf, const float* dw,
        const float* cg, const float* wn, const float* sig, const float* dsip, const float* dsii,
        const float* th, const float* ecos, const float* esin, const float* ewn1, const float* ecg1,
        int* finsiz_out, int* nxpfull_out, int* fallback_out) {
  const int nk = ipar[WW_CPRT_NK], nth = ipar[WW_CPRT_NTH], nseal = ipar[WW_CPRT_NSEAL];
  const int nsea = ipar[WW_CPRT_NSEA], nx = ipar[WW_CPRT_NX], ny = ipar[WW_CPRT_NY];
  const int nasf = ipar[WW_CPRT_NASF], ptmeth = ipar[WW_CPRT_PTMETH], ihmax = ipar[WW_CPRT_IHMAX];
  const int nar1d = ipar[WW_CPRT_NAR1D], n1max = ipar[WW_CPRT_N1MAX];
  // Under W3_SHRD NSEAL = NSEA = NSEALM; JSEA = ISEA is what the kernel assumes.
  if (nk < 1 || nth < 1 || nspec != nk * nth || nseal < 0 || nseal != nsea || nsealm < nseal || nx < 1 ||
      ny < 1 || nasf < nsea || ihmax < 1 || ipar[WW_CPRT_DIMP] != ww::part::kDimp || nar1d < 2 || n1max < 0 ||
      n1max > nar1d - 1)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_va: bad sizes");
  const int dimxp = ww::iosf::w3cprt_dimxp(nk, nth, ptmeth);
  if (dimxp < 1) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_va: DIMXP < 1");

  ww::timers::Region tcall("cprt.call");
  Bufs& b = bufs();
  b.finsiz = -1;
  ww::resident::DevVa dva;
  const int st = ww::resident::device_va(igrid, va, nspec, nsealm, dva);
  if (st != WW_KOKKOS_OK) return fail(st, "the resident VA refused this call");
  if (dva.stride(1) != static_cast<size_t>(nspec)) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "the resident VA is padded");

  ww::iosf::W3cprtArgs<Mem> a;
  ww::part::PtmeanGrid g{};
  {
    ww::timers::Region t("cprt.h2d");
    const size_t nk2 = static_cast<size_t>(nk) + 2, ns1 = static_cast<size_t>(nsea) + 1;
    const size_t ns = static_cast<size_t>(nsea), nt = static_cast<size_t>(nth);
    a.va = U2<const Real>(dva.data(), static_cast<size_t>(nspec), static_cast<size_t>(nsealm) + 1);
    a.mapsf = in2(b.mapsf, "ww_cprt.mapsf", mapsf, ns, 3);
    a.mapsta = in2(b.mapsta, "ww_cprt.mapsta", mapsta, static_cast<size_t>(ny), static_cast<size_t>(nx));
    a.u10 = in1(b.u10, "ww_cprt.u10", u10, ns);
    a.u10d = in1(b.u10d, "ww_cprt.u10d", u10d, ns);
    a.asf = in1(b.asf, "ww_cprt.asf", asf, ns);
    a.dw = in1(b.dw, "ww_cprt.dw", dw, ns1);
    a.cg = in2(b.cg, "ww_cprt.cg", cg, nk2, ns1);
    a.wn = in2(b.wn, "ww_cprt.wn", wn, nk2, ns1);
    count_in((ns * 3 + static_cast<size_t>(nx) * static_cast<size_t>(ny)) * sizeof(int) +
             (3 * ns + ns1 + 2 * nk2 * ns1) * sizeof(Real));
    const auto s_sig = in1(b.sig, "ww_cprt.sig", sig, nk2);
    const auto s_dsip = in1(b.dsip, "ww_cprt.dsip", dsip, nk2);
    const auto s_dsii = in1(b.dsii, "ww_cprt.dsii", dsii, static_cast<size_t>(nk));
    const auto s_th = in1(b.th, "ww_cprt.th", th, nt);
    const auto s_ecos = in1(b.ecos, "ww_cprt.ecos", ecos, nt);
    const auto s_esin = in1(b.esin, "ww_cprt.esin", esin, nt);
    const size_t na = static_cast<size_t>(nar1d) + 1;
    const auto s_ewn1 = in1(b.ewn1, "ww_cprt.ewn1", ewn1, na);
    const auto s_ecg1 = in1(b.ecg1, "ww_cprt.ecg1", ecg1, na);
    count_in((2 * nk2 + static_cast<size_t>(nk) + 3 * nt + 2 * na) * sizeof(Real));
    // PTNGHB's table (w3partmd.F90:630-821): a function of NK and NTH, rebuilt when they change.
    if (b.table.update(nk, nth) || b.neigh.extent(0) != b.table.neigh.size()) {
      ensure(b.neigh, "ww_cprt.neigh", b.table.neigh.size());
      Kokkos::deep_copy(b.neigh, H1<const int>(b.table.neigh.data(), b.table.neigh.size()));
    }
    g.nk = nk;
    g.nth = nth;
    g.dth = rpar[WW_CPRT_DTH];
    g.xfr = rpar[WW_CPRT_XFR];
    g.fachfe = rpar[WW_CPRT_FACHFE];
    g.fte = rpar[WW_CPRT_FTE];
    g.sig = s_sig.data();
    g.dsip = s_dsip.data();
    g.dsii = s_dsii.data();
    g.th = s_th.data();
    g.ecos = s_ecos.data();
    g.esin = s_esin.data();
    g.disp.ewn1 = s_ewn1.data();
    g.disp.ecg1 = s_ecg1.data();
    g.disp.dsie = rpar[WW_CPRT_DSIE];
    g.disp.n1max = n1max;
  }
  const ww::part::PtmeanOpts o{rpar[WW_CPRT_HSPMIN], rpar[WW_CPRT_WSMULT], ptmeth, rpar[WW_CPRT_UNDEF]};
  const ww::part::W3partOpts po{ihmax, rpar[WW_CPRT_WSCUT], rpar[WW_CPRT_PTFCUT], ipar[WW_CPRT_FLCOMB] != 0};

  const int chunk = nseal < chunk_size() ? (nseal > 0 ? nseal : 1) : chunk_size();
  const size_t nreal = static_cast<size_t>(ww::iosf::w3cprt_nreal(nk, nth, dimxp));
  const size_t nint = static_cast<size_t>(ww::iosf::w3cprt_nint(nk, nth, dimxp, ihmax));
  const size_t nxp = static_cast<size_t>(ww::part::kDimp) * (static_cast<size_t>(dimxp) + 1);
  const size_t nspec_s = static_cast<size_t>(nspec);
  if (b.wr.extent(0) < nreal * chunk) b.wr = D1<Real>(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ww_cprt.wr"), nreal * chunk);
  if (b.wi.extent(0) < nint * chunk) b.wi = D1<int>(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ww_cprt.wi"), nint * chunk);
  for (D1<int>* v : {&b.np, &b.nxp, &b.rc, &b.first})
    if (v->extent(0) < static_cast<size_t>(chunk)) *v = D1<int>(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ww_cprt.pt"), chunk);
  // ICPRT = 0, ICPRT(1,2) = 1 (:230-231).
  b.icprt = D2<int>("ww_cprt.icprt", static_cast<size_t>(nsealm) + 1, 2);
  Kokkos::deep_copy(Kokkos::subview(b.icprt, 0, 1), 1);
  if (b.dtprt.extent(0) != static_cast<size_t>(ww::part::kDimp) || b.dtprt.extent(1) == 0)
    b.dtprt = D2<Real>(Kokkos::view_alloc(Kokkos::WithoutInitializing, "ww_cprt.dtprt"), ww::part::kDimp,
                       static_cast<size_t>(nseal > 0 ? nseal : 1));

  const auto wr = b.wr;
  const auto wi = b.wi;
  const auto vnp = b.np;
  const auto vnxp = b.nxp;
  const auto vrc = b.rc;
  const auto vfirst = b.first;
  const auto icprt = b.icprt;
  const int* neigh = b.neigh.data();
  int base = 0, nxpfull = 0;
  {
    ww::timers::Region t("cprt.kernel");
    for (int c0 = 0; c0 < nseal; c0 += chunk) {
      const int n = (nseal - c0 < chunk) ? nseal - c0 : chunk;
      // Sections 2-4, one point per index.
      Kokkos::parallel_for(
          "ww::iosf::w3cprt_point", Kokkos::RangePolicy<Exec>(0, n), KOKKOS_LAMBDA(const int i) {
            Real* r = wr.data() + nreal * static_cast<size_t>(i);
            int* ib = wi.data() + nint * static_cast<size_t>(i);
            Real* e2 = r;
            Real* xp = e2 + nspec_s;
            const ww::part::W3partScratch w = ww::part::w3part_carve(xp + nxp, ib, nk, nth, dimxp, ihmax);
            int np = 0, nfull = 0;
            vrc(i) = ww::iosf::w3cprt_point(a, g, o, po, neigh, dimxp, c0 + i + 1, e2, xp, w, np, nfull);
            vnp(i) = np;
            vnxp(i) = nfull;
          });
      int nopeak = 0, nfull = 0;
      Kokkos::parallel_reduce(
          "ww::iosf::w3cprt_status", Kokkos::RangePolicy<Exec>(0, n),
          KOKKOS_LAMBDA(const int i, int& sx, int& sp) {
            sx += vnxp(i);
            if (vrc(i) == ww::part::kPtmeanNoPeak) sp += 1;
          },
          nfull, nopeak);
      if (nopeak > 0) {
        *finsiz_out = 0;
        *nxpfull_out = 0;
        *fallback_out = 1;
        g_fallbacks.fetch_add(1, std::memory_order_relaxed);
        return WW_KOKKOS_OK;
      }
      nxpfull += nfull;
      // ICPRT(JSEA,2) = 1 + the columns of the points before JSEA (:251, :286).
      int total = 0;
      const int b0 = base;
      Kokkos::parallel_scan(
          "ww::iosf::w3cprt_scan", Kokkos::RangePolicy<Exec>(0, n),
          KOKKOS_LAMBDA(const int i, int& acc, const bool final) {
            const int np = vnp(i);
            const int cols = np >= 0 ? np + 1 : 0;
            if (final) {
              vfirst(i) = b0 + 1 + acc;
              icprt(c0 + i + 1, 1) = b0 + 1 + acc + cols;  // ICPRT(JSEA+1,2)
            }
            acc += cols;
          },
          total);
      if (static_cast<size_t>(base) + static_cast<size_t>(total) > b.dtprt.extent(1)) {
        size_t cap = 2 * b.dtprt.extent(1);
        if (cap < static_cast<size_t>(base) + static_cast<size_t>(total))
          cap = static_cast<size_t>(base) + static_cast<size_t>(total);
        Kokkos::resize(b.dtprt, static_cast<size_t>(ww::part::kDimp), cap);
      }
      const auto dtprt = b.dtprt;
      Kokkos::parallel_for(
          "ww::iosf::w3cprt_store", Kokkos::RangePolicy<Exec>(0, n), KOKKOS_LAMBDA(const int i) {
            const Real* xp = wr.data() + nreal * static_cast<size_t>(i) + nspec_s;
            ww::iosf::w3cprt_store(c0 + i + 1, vnp(i), vfirst(i), xp, icprt, dtprt);
          });
      base += total;
    }
    Exec().fence("ww_cprt_va");
  }
  b.igrid = igrid;
  b.nsealm = nsealm;
  b.finsiz = base;  // FINSIZ = ICPRT(NSEAL+1,2) - 1 (:312)
  *finsiz_out = base;
  *nxpfull_out = nxpfull;
  *fallback_out = 0;
  g_calls.fetch_add(1, std::memory_order_relaxed);
  g_points.fetch_add(nseal, std::memory_order_relaxed);
  return WW_KOKKOS_OK;
}

}  // namespace
}  // namespace ww::cprtshim

extern "C" int ww_cprt_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar, float* va,
                          int nspec, int nsealm, const int* mapsf, const int* mapsta, const float* u10,
                          const float* u10d, const float* asf, const float* dw, const float* cg,
                          const float* wn, const float* sig, const float* dsip, const float* dsii,
                          const float* th, const float* ecos, const float* esin, const float* ewn1,
                          const float* ecg1, int* finsiz, int* nxpfull, int* fallback) {
  using namespace ww::cprtshim;
  if (ipar == nullptr || rpar == nullptr || nipar != WW_CPRT_NIPAR || nrpar != WW_CPRT_NRPAR)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_va: bad packet");
  if (va == nullptr || mapsf == nullptr || mapsta == nullptr || u10 == nullptr || u10d == nullptr ||
      asf == nullptr || dw == nullptr || cg == nullptr || wn == nullptr || sig == nullptr || dsip == nullptr ||
      dsii == nullptr || th == nullptr || ecos == nullptr || esin == nullptr || ewn1 == nullptr ||
      ecg1 == nullptr || finsiz == nullptr || nxpfull == nullptr || fallback == nullptr)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_va: a null pointer");
  try {
    if (!Kokkos::is_initialized()) return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    if (nested_in_openmp()) return fail(WW_KOKKOS_ERR_UNSUPPORTED, "inside an OpenMP region");
    // W3CPRT reads VA and never writes it: d_va keeps the state device_va left (BOTH or DEVICE).
    return run(igrid, ipar, rpar, va, nspec, nsealm, mapsf, mapsta, u10, u10d, asf, dw, cg, wn, sig, dsip,
               dsii, th, ecos, esin, ewn1, ecg1, finsiz, nxpfull, fallback);
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}

extern "C" int ww_cprt_fetch(int igrid, int nsealm, int* icprt, int finsiz, float* dtprt) {
  using namespace ww::cprtshim;
  if (icprt == nullptr || dtprt == nullptr) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_fetch: a null pointer");
  try {
    if (!Kokkos::is_initialized()) return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    Bufs& b = bufs();
    if (b.finsiz < 0 || igrid != b.igrid || nsealm != b.nsealm || finsiz != b.finsiz)
      return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_cprt_fetch: no table of this grid and size");
    ww::timers::Region t("cprt.d2h");
    const size_t nrow = static_cast<size_t>(nsealm) + 1;
    Kokkos::deep_copy(H2<int>(icprt, nrow, 2), b.icprt);
    long long bytes = static_cast<long long>(nrow * 2 * sizeof(int));
    if (finsiz > 0) {
      const size_t nc = static_cast<size_t>(finsiz);
      Kokkos::deep_copy(H2<Real>(dtprt, static_cast<size_t>(ww::part::kDimp), nc),
                        Kokkos::subview(b.dtprt, Kokkos::ALL, std::make_pair(size_t{0}, nc)));
      bytes += static_cast<long long>(nc * ww::part::kDimp * sizeof(Real));
    }
    Exec().fence("ww_cprt_fetch");
    g_d2h_bytes.fetch_add(bytes, std::memory_order_relaxed);
    b.finsiz = -1;
    return WW_KOKKOS_OK;
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}
