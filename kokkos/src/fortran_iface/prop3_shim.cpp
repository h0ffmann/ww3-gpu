// kokkos/src/fortran_iface/prop3_shim.cpp
// The bind(C) shim of the prop3 wiring unit: the propagation of the lab switch (PR3 with UQ) on
// Kokkos, batched (PATCH.md section 11). Marshalling only; the arithmetic is the translated
// headers prop_xyp3.hpp (W3XYP3 with W3QCK3) and prop_ktp3.hpp (W3KTP3 with W3QCK1, W3QCK2),
// called unchanged.
//
// The caller is W3WAVE (w3wavemd.F90) through W3KOKKOS_XYP3 and W3KOKKOS_KTP3 of
// w3kokkos_prop3md.F90 on the fork, outside any OpenMP region: each call replaces a whole loop of
// the Fortran.
//
//  * ww_xyp3_batch replaces section 3.6.3's DO ISPEC loop, W3GATH + W3XYP3 + W3SCAT (W3_SHRD) per
//    component. The components are independent: W3GATH sets all of FIELD (FIELD = 0., then the
//    sea points of VA(ISPEC,:)), W3XYP3 writes only FIELD and its own automatic arrays (its USE
//    list, w3pro3md.F90:793-805 at the pin, has no module array it writes), and W3SCAT writes
//    only VA(ISPEC,:). So one launch runs every component, one per work item, each with its own
//    FIELD and work arrays, the serial body of W3XYP3 inside it unchanged; the result is the
//    Fortran loop's for any order of the components. The scatter back into VA is a second
//    launch over sea points, so that threads write contiguous VA columns instead of one float
//    each of a shared cache line.
//  * ww_ktp3_batch replaces section 3.6.2's (or 3.6.4's) DO ITLOC loop around the !$OMP DO over
//    JSEA. W3KTP3 reads and writes only VA(:,JSEA), CFLTHMAX(JSEA), CFLKMAX(JSEA) of its point
//    (w3pro3md.F90:1519-1961), and every other argument is the same for each ITLOC. So the point's
//    NTIMES calls run back to back in one work item, the same sequence of calls on the same
//    VA(:,JSEA) as the Fortran's sub-step loop, and the CFL maxima are the last call's.
//
// Memory contract. ww_xyp3_batch and ww_ktp3_batch are phase 1: on a backend whose execution
// space can read host memory (Serial, OpenMP) the kernels read and write WW3's arrays in place,
// with no copy; on a device (CUDA) VA and the CFL arrays are copied in and out whole per call.
// ww_xyp3_batch_va and ww_ktp3_batch_va (WFIP-0005 unit 3b) read and write the resident VA of the
// grid instead (ww::resident::device_va, resident_state.hpp), so VA does not cross for the
// propagation on any backend. On a device every read-only input keeps its buffer between calls
// and is copied again only when its bits differ from those of the last copy (a host shadow,
// compared with memcmp): the grid tables, which W3WAVE never writes, cross once per run, and the
// maps, CG, WN, CX, CY, MAPSTA and the packed scalars only after W3WAVE rewrote them (PATCH.md
// section 13 lists the writers). A copy, or its skip, moves bits and computes nothing. No Unified
// Memory. Work buffers are kept between calls and grown on demand, at most
// WW_KOKKOS_PROP3_BATCH_MIB MiB each (default 1024); a step with more components or points than
// fit runs in several launches.
//
// WW_KOKKOS_PROP3_COUNT=1 prints, at exit, the calls and the components or point calls each entry
// point ran (the _va entries counted with their routine, and apart as resident calls), and on a
// device the input copies made and skipped: the evidence that a run exercised the port.
// WW_KOKKOS_TIMERS=1 times the regions xyp3.call/.h2d/.kernel/.d2h and ktp3.call/.h2d/.kernel/.d2h
// (timers.hpp).
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/prop3_c.hpp"
#include "fortran_iface/resident_c.hpp"
#include "fortran_iface/timers.hpp"

#include <Kokkos_Core.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

#include "ww_kokkos/prop_ktp3.hpp"
#include "ww_kokkos/prop_params.hpp"
#include "ww_kokkos/prop_xyp3.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/resident_state.hpp"

#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_PROP3_SERIAL_NESTED 1
#include <omp.h>
#endif

namespace ww::prop3shim {
namespace {

using ww::Real;
static_assert(std::is_same_v<Real, float>, "the C ABI passes REAL as float");
static_assert(static_cast<int>(WW_XYP3_NRPAR) == static_cast<int>(ww::prop::NRPAR) &&
                  static_cast<int>(WW_XYP3_NIPAR) == static_cast<int>(ww::prop::NIPAR),
              "prop3_c.hpp and prop_params.hpp disagree on the XYP3 packets");

template <class T>
using HostUnmanaged =
    Kokkos::View<T*, Kokkos::LayoutLeft, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

enum Routine { XYP3, KTP3, NROUTINES };
const char* const kName[NROUTINES] = {"W3XYP3", "W3KTP3"};
const char* const kItem[NROUTINES] = {"spectra", "point calls"};
std::atomic<long long> g_calls[NROUTINES], g_items[NROUTINES], g_launches[NROUTINES];
std::atomic<long long> g_resident[NROUTINES], g_in_copies[NROUTINES], g_in_same[NROUTINES];

void print_counts() {
  for (int r = 0; r < NROUTINES; ++r) {
    std::fprintf(stderr, "ww_kokkos: %s batched calls: %lld\n", kName[r], g_calls[r].load());
    std::fprintf(stderr, "ww_kokkos: %s batched calls on the resident VA: %lld\n", kName[r],
                 g_resident[r].load());
    std::fprintf(stderr, "ww_kokkos: %s batched launches: %lld\n", kName[r], g_launches[r].load());
    std::fprintf(stderr, "ww_kokkos: %s %s: %lld\n", kName[r], kItem[r], g_items[r].load());
    std::fprintf(stderr, "ww_kokkos: %s device input copies: %lld\n", kName[r],
                 g_in_copies[r].load());
    std::fprintf(stderr, "ww_kokkos: %s device inputs unchanged, not copied: %lld\n", kName[r],
                 g_in_same[r].load());
  }
}

/// WW_KOKKOS_PROP3_COUNT=1 registers the exit print when the library loads, so a run in which
/// neither entry point was called still says 0.
[[maybe_unused]] const bool g_count_on = [] {
  const char* v = std::getenv("WW_KOKKOS_PROP3_COUNT");
  const bool c = v != nullptr && std::strcmp(v, "1") == 0;
  if (c) std::atexit(print_counts);
  return c;
}();

void count(Routine r, long long items, long long launches) {
  g_calls[r].fetch_add(1, std::memory_order_relaxed);
  g_items[r].fetch_add(items, std::memory_order_relaxed);
  g_launches[r].fetch_add(launches, std::memory_order_relaxed);
}

int fail(int code, const char* what) {
  std::fprintf(stderr, "ww_kokkos: prop3: %s (code %d)\n", what, code);
  return code;
}

constexpr long long kBatchMibDefault = 1024;

/// The work buffers' cap in bytes: WW_KOKKOS_PROP3_BATCH_MIB, a positive integer, else the
/// default (a value that does not parse is reported once).
long long cap_bytes() {
  static const long long cap = [] {
    long long mib = kBatchMibDefault;
    if (const char* v = std::getenv("WW_KOKKOS_PROP3_BATCH_MIB")) {
      char* end = nullptr;
      const long long x = std::strtoll(v, &end, 10);
      if (end != v && *end == '\0' && x > 0 && x <= (1LL << 30))
        mib = x;
      else
        std::fprintf(stderr, "ww_kokkos: WW_KOKKOS_PROP3_BATCH_MIB='%s' ignored, using %lld\n", v,
                     kBatchMibDefault);
    }
    return mib * 1024 * 1024;
  }();
  return cap;
}

bool nested_in_openmp() {
#if defined(WW_PROP3_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// The bits of a read-only input as last copied to the device: its length and a host copy.
struct Shadow {
  bool valid = false;
  size_t n = 0;
  std::vector<unsigned char> bytes;
};

/// Buffers kept between calls on one memory space: the work arrays, and on a device the copies
/// of the inputs (one per argument slot) with the shadow of each read-only one. Emptied by a
/// Kokkos finalize hook, before Kokkos::finalize; never destroyed (ww_kokkos_finalize runs from
/// atexit, after static destructors of objects built later).
template <class Mem>
struct Buffers {
  Kokkos::View<Real*, Mem> work_r;
  Kokkos::View<int*, Mem> work_i;
  std::vector<Kokkos::View<Real*, Mem>> in_r;
  std::vector<Kokkos::View<int*, Mem>> in_i;
  std::vector<Shadow> shadow_r, shadow_i;
};

/// Argument slots: W3XYP3's from 0, W3KTP3's from kKtp3Slot, so the two never share a buffer
/// or a shadow.
constexpr int kKtp3Slot = 32;

template <class Mem>
Buffers<Mem>& buffers() {
  static Buffers<Mem>* b = [] {
    auto* p = new Buffers<Mem>();
    Kokkos::push_finalize_hook([p] { *p = Buffers<Mem>(); });
    return p;
  }();
  return *b;
}

std::mutex g_mutex;  // one caller at a time: the buffers are shared

template <class V>
void grow(V& v, size_t n, const char* name) {
  if (v.extent(0) < n) {
    v = V();
    v = V(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(name)), n);
  }
}

/// Where the kernels of execution space E find caller array p of n elements: p itself when E
/// can read host memory, else a device buffer holding p's bits. `slot` numbers the argument, so
/// each keeps its own buffer. A read-only p (const T) is copied only when its n or its bits
/// differ from the last copy into that buffer; an inout p is copied at every call.
template <class E, class T>
T* stage(const E& e, int slot, T* p, size_t n) {
  using Mem = typename E::memory_space;
  if constexpr (Kokkos::SpaceAccessibility<E, Kokkos::HostSpace>::accessible) {
    (void)e;
    (void)slot;
    (void)n;
    return p;
  } else {
    using U = std::remove_const_t<T>;
    auto& b = buffers<Mem>();
    auto& pool = [&]() -> auto& {
      if constexpr (std::is_same_v<U, Real>)
        return b.in_r;
      else
        return b.in_i;
    }();
    const size_t k = static_cast<size_t>(slot);
    if (pool.size() <= k) pool.resize(k + 1);
    auto& v = pool[k];
    const Routine r = slot >= kKtp3Slot ? KTP3 : XYP3;
    if constexpr (std::is_const_v<T>) {
      auto& shadows = [&]() -> auto& {
        if constexpr (std::is_same_v<U, Real>)
          return b.shadow_r;
        else
          return b.shadow_i;
      }();
      if (shadows.size() <= k) shadows.resize(k + 1);
      Shadow& sh = shadows[k];
      const size_t nbytes = n * sizeof(U);
      if (sh.valid && sh.n == n &&
          (nbytes == 0 || std::memcmp(sh.bytes.data(), p, nbytes) == 0)) {
        g_in_same[r].fetch_add(1, std::memory_order_relaxed);
        return v.data();
      }
      sh.valid = false;
      grow(v, std::max<size_t>(n, 1), "ww_prop3.in");
      if (n > 0)
        Kokkos::deep_copy(e, Kokkos::subview(v, std::make_pair(size_t{0}, n)),
                          HostUnmanaged<const U>(p, n));
      const auto* src = reinterpret_cast<const unsigned char*>(p);
      sh.bytes.assign(src, src + nbytes);
      sh.n = n;
      sh.valid = true;
    } else {
      grow(v, std::max<size_t>(n, 1), "ww_prop3.in");
      if (n > 0)
        Kokkos::deep_copy(e, Kokkos::subview(v, std::make_pair(size_t{0}, n)),
                          HostUnmanaged<const U>(p, n));
    }
    g_in_copies[r].fetch_add(1, std::memory_order_relaxed);
    return v.data();
  }
}

/// Copies device array d of n elements back to caller array p, when stage() copied it.
template <class E, class T>
void unstage(const E& e, T* p, const T* d, size_t n) {
  if constexpr (!Kokkos::SpaceAccessibility<E, Kokkos::HostSpace>::accessible) {
    using Mem = typename E::memory_space;
    if (n > 0)
      Kokkos::deep_copy(e, HostUnmanaged<T>(p, n),
                        Kokkos::View<const T*, Mem, Kokkos::MemoryTraits<Kokkos::Unmanaged>>(d, n));
  } else {
    (void)e;
    (void)p;
    (void)d;
    (void)n;
  }
}

/// Runs f(E()) on Kokkos::Serial inside an OpenMP region, else on the default space, under the
/// lock, timing the whole call as region `name`, and turns an exception into a status.
template <class F>
int run(const char* name, const F& f) {
  try {
    if (!Kokkos::is_initialized())
      return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    std::lock_guard<std::mutex> lock(g_mutex);
    const bool nested = nested_in_openmp();
    ww::timers::Region t(name, !nested);
#if defined(WW_PROP3_SERIAL_NESTED)
    if (nested) return f(Kokkos::Serial(), false);
#endif
    return f(Kokkos::DefaultExecutionSpace(), !nested);
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}

/// run() on the resident VA of grid igrid (WFIP-0005 unit 3b): syncs d_va (one host-to-device
/// copy when the host holds the newer bits), runs f(E(), fence, d_va.data()) on the default
/// space, and leaves the state DEVICE. Refused inside an OpenMP region: the resident context is
/// not locked, and every caller in W3WAVE is outside one.
template <class F>
int run_va(const char* name, Routine r, int igrid, float* va, int nspec, int nsealm,
           const F& f) {
  try {
    if (!Kokkos::is_initialized())
      return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    if (nested_in_openmp())
      return fail(WW_KOKKOS_ERR_UNSUPPORTED, "the resident VA inside an OpenMP region");
    std::lock_guard<std::mutex> lock(g_mutex);
    ww::timers::Region t(name);
    ww::resident::DevVa dva;
    const int rc = ww::resident::device_va(igrid, va, nspec, nsealm, dva);
    if (rc != WW_KOKKOS_OK) return fail(rc, "the resident VA refused this call");
    if (dva.stride(1) != static_cast<size_t>(nspec))
      return fail(WW_KOKKOS_ERR_BAD_SHAPE, "the resident VA is padded");
    const int st = f(Kokkos::DefaultExecutionSpace(), true, dva.data());
    // The launch wrote d_va; on a failure the model stops (PROP3_CHECK), so the state is moot.
    ww::resident::device_written(igrid);
    if (st == WW_KOKKOS_OK) g_resident[r].fetch_add(1, std::memory_order_relaxed);
    return st;
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}

/// How many work items of `per_item` bytes fit the cap; at least one.
long long chunk_items(long long n, long long per_item) {
  const long long fit = std::max(1LL, cap_bytes() / std::max(1LL, per_item));
  return std::max(1LL, std::min(n, fit));
}

/// The arguments of ww_xyp3_batch, as received.
struct Xyp3Args {
  ww::prop::Params p;
  const int* imaps;
  const float* tscal;
  float dtg;
  const int *mapsf, *mapx2, *mapy2, *mapaxy, *mapcxy, *maptrn, *mapsta, *isbpi, *on;
  const float *clats, *sig, *ecos, *esin, *dpdx, *dpdy, *dqdx, *dqdy, *gsqrt, *atrnx, *atrny;
  const float *cg, *cx, *cy, *bbpi0, *bbpin;
  int flbpi, nbi, ncol;
  float* va;
  float* dva;  // the resident VA of the grid in E's memory space, or nullptr: va, staged
};

template <class E>
int xyp3_batch(const E& e, bool fence, const Xyp3Args& a) {
  using namespace ww::prop;
  using Mem = typename E::memory_space;
  const Params& p = a.p;
  const int* const imaps = a.imaps;
  const float* const tscal = a.tscal;
  const float dtg = a.dtg;
  const int flbpi = a.flbpi, nbi = a.nbi, ncol = a.ncol;
  const int* const on = a.on;
  float* const va = a.va;
  const size_t nx = static_cast<size_t>(p.nx), ny = static_cast<size_t>(p.ny);
  const size_t nsea = static_cast<size_t>(p.nsea), nspec = static_cast<size_t>(p.nspec);
  const size_t nxy = nx * ny, nk2 = static_cast<size_t>(p.nk) + 2;
  const size_t nbis = flbpi != 0 ? static_cast<size_t>(nbi) : 0;
  const size_t nbb = flbpi != 0 ? nspec * (static_cast<size_t>(nbi) + 1) : 0;

  std::vector<int> list;
  for (int isp = 1; isp <= p.nspec; ++isp)
    if (on[isp - 1] != 0) list.push_back(isp);
  const long long nlist = static_cast<long long>(list.size());

  ww::timers::Region t_in("xyp3.h2d", fence);
  int slot = 0;
  Grid g{};
  g.mapsf = stage(e, slot++, a.mapsf, 3 * nsea);
  g.clats = stage(e, slot++, a.clats, nsea + 1);
  g.sig = stage(e, slot++, a.sig, nk2);
  g.ecos = stage(e, slot++, a.ecos, nspec + static_cast<size_t>(p.nth));
  g.esin = stage(e, slot++, a.esin, nspec + static_cast<size_t>(p.nth));
  g.dpdx = stage(e, slot++, a.dpdx, nxy);
  g.dpdy = stage(e, slot++, a.dpdy, nxy);
  g.dqdx = stage(e, slot++, a.dqdx, nxy);
  g.dqdy = stage(e, slot++, a.dqdy, nxy);
  g.gsqrt = stage(e, slot++, a.gsqrt, nxy);
  g.atrnx = stage(e, slot++, a.atrnx, 3 * nxy);
  g.atrny = stage(e, slot++, a.atrny, 3 * nxy);
  g.mapx2 = stage(e, slot++, a.mapx2, nxy);
  g.mapy2 = stage(e, slot++, a.mapy2, nxy);
  g.mapaxy = stage(e, slot++, a.mapaxy, nxy);
  g.mapcxy = stage(e, slot++, a.mapcxy, nsea);
  g.maptrn = stage(e, slot++, a.maptrn, nxy);
  g.nmx0 = imaps[0];
  g.nmx1 = imaps[1];
  g.nmx2 = imaps[2];
  g.nmy0 = imaps[3];
  g.nmy1 = imaps[4];
  g.nmy2 = imaps[5];
  g.nact = imaps[6];
  g.ncent = imaps[7];
  Step s{};
  s.cg = stage(e, slot++, a.cg, nk2 * (nsea + 1));
  s.cx = stage(e, slot++, a.cx, nsea + 1);
  s.cy = stage(e, slot++, a.cy, nsea + 1);
  s.dsec_day = tscal[0];
  s.flbpi = flbpi != 0 ? 1 : 0;
  s.nbi = flbpi != 0 ? nbi : 0;
  s.isbpi = stage(e, slot++, a.isbpi, nbis);
  s.bbpi0 = stage(e, slot++, a.bbpi0, nbb);
  s.bbpin = stage(e, slot++, a.bbpin, nbb);
  s.dsec_bpi0 = flbpi != 0 ? tscal[1] : Real(0);
  s.dsec_bpin = flbpi != 0 ? tscal[2] : Real(0);
  const int* const msta = stage(e, slot++, a.mapsta, nxy);
  const int* const lst = stage(e, slot++, static_cast<const int*>(list.data()), list.size());
  float* const vad =
      a.dva != nullptr ? a.dva : stage(e, slot++, va, nspec * static_cast<size_t>(ncol));
  t_in.stop();

  // Per component: FIELD(1-NY:NY*(NX+2)), then W3XYP3's reals; its ints apart.
  const size_t nf = ny * (nx + 3);
  const size_t nwr = nf + static_cast<size_t>(xyp3_work_reals(p.nx, p.ny, p.nsea));
  const size_t nwi = static_cast<size_t>(xyp3_work_ints(p.nx, p.ny, p.nsea));
  const long long chunk = chunk_items(
      nlist, static_cast<long long>((nwr + nwi) * sizeof(Real)));
  auto& b = buffers<Mem>();
  grow(b.work_r, nwr * static_cast<size_t>(chunk), "ww_prop3.xyp3_work_r");
  grow(b.work_i, nwi * static_cast<size_t>(chunk), "ww_prop3.xyp3_work_i");
  Real* const wr0 = b.work_r.data();
  int* const wi0 = b.work_i.data();
  const int qoff = p.ny - 1;  // FIELD(i) -> [i + NY - 1]
  const int insea = p.nsea, inspec = p.nspec;

  long long launches = 0;
  for (long long n0 = 0; n0 < nlist; n0 += chunk) {
    const int m = static_cast<int>(std::min(chunk, nlist - n0));
    const int k0 = static_cast<int>(n0);
    ww::timers::Region t_k("xyp3.kernel", fence);
    Kokkos::parallel_for(
        "ww_xyp3_batch", Kokkos::RangePolicy<E, Kokkos::Schedule<Kokkos::Dynamic>>(e, 0, m),
        KOKKOS_LAMBDA(const int i) {
          const int isp = lst[k0 + i];
          Real* const field = wr0 + static_cast<size_t>(i) * nwr;
          // W3GATH, W3_SHRD: FIELD = 0., then FIELD(MAPSF(ISEA,3)) = VA(ISPEC,ISEA).
          for (size_t k = 0; k < nf; ++k) field[k] = Real(0);
          for (int isea = 1; isea <= insea; ++isea) {
            const int ixy = g.mapsf[2 * insea + isea - 1];
            field[ixy + qoff] = vad[static_cast<size_t>(isea) * inspec + isp - 1];
          }
          w3xyp3(p, g, s, isp, dtg, msta, field, field + nf, wi0 + static_cast<size_t>(i) * nwi);
        });
    // W3SCAT, W3_SHRD: VA(ISPEC,ISEA) = FIELD(IXY) where MAPSTA(IXY) /= 0, per sea point.
    Kokkos::parallel_for(
        "ww_xyp3_scatter", Kokkos::RangePolicy<E>(e, 1, insea + 1), KOKKOS_LAMBDA(const int isea) {
          const int ixy = g.mapsf[2 * insea + isea - 1];
          if (msta[ixy - 1] == 0) return;
          for (int i = 0; i < m; ++i)
            vad[static_cast<size_t>(isea) * inspec + lst[k0 + i] - 1] =
                wr0[static_cast<size_t>(i) * nwr + static_cast<size_t>(ixy + qoff)];
        });
    t_k.stop();
    ++launches;
  }

  ww::timers::Region t_out("xyp3.d2h", fence);
  if (a.dva == nullptr) unstage(e, va, vad, nspec * static_cast<size_t>(ncol));
  e.fence("ww_xyp3_batch");
  t_out.stop();
  count(XYP3, nlist, launches);
  return int{WW_KOKKOS_OK};
}

/// The arguments of ww_ktp3_batch, as received.
struct Ktp3Args {
  ww::prop::Ktp3Params p;
  const float *sig, *dsip, *ecos, *esin, *ec2, *esc, *es2, *cg, *wn, *pscal;
  const int *mapwn, *mapth2, *mapwn2, *jsea, *isea;
  int itstep, ntimes, nsea, npts, ncol;
  float facth, fack;
  float *va, *cflthmax, *cflkmax;
  float* dva;  // the resident VA of the grid in E's memory space, or nullptr: va, staged
};

template <class E>
int ktp3_batch(const E& e, bool fence, const Ktp3Args& a) {
  using namespace ww::prop;
  using Mem = typename E::memory_space;
  const Ktp3Params& p = a.p;
  const int itstep = a.itstep, ntimes = a.ntimes, nsea = a.nsea, npts = a.npts, ncol = a.ncol;
  const float facth = a.facth, fack = a.fack;
  const size_t nspec = static_cast<size_t>(p.nspec), nk2 = static_cast<size_t>(p.nk2);
  const size_t ntab = nspec + static_cast<size_t>(p.nth);
  const size_t ncell = nk2 * (static_cast<size_t>(nsea) + 1);
  const size_t ncfl = static_cast<size_t>(ncol) - 1;

  ww::timers::Region t_in("ktp3.h2d", fence);
  int slot = kKtp3Slot;
  Ktp3Grid g{};
  g.sig = stage(e, slot++, a.sig, nk2);
  g.dsip = stage(e, slot++, a.dsip, nk2);
  g.ecos = stage(e, slot++, a.ecos, ntab);
  g.esin = stage(e, slot++, a.esin, ntab);
  g.ec2 = stage(e, slot++, a.ec2, ntab);
  g.esc = stage(e, slot++, a.esc, ntab);
  g.es2 = stage(e, slot++, a.es2, ntab);
  g.mapwn = stage(e, slot++, a.mapwn, ntab);
  g.mapth2 = stage(e, slot++, a.mapth2, nk2 * static_cast<size_t>(p.nth));
  g.mapwn2 = stage(e, slot++, a.mapwn2, ntab);
  const float* const cgd = stage(e, slot++, a.cg, ncell);
  const float* const wnd = stage(e, slot++, a.wn, ncell);
  const int* const jsd = stage(e, slot++, a.jsea, static_cast<size_t>(npts));
  const int* const isd = stage(e, slot++, a.isea, static_cast<size_t>(npts));
  const float* const psd =
      stage(e, slot++, a.pscal, static_cast<size_t>(WW_KTP3_NPSCAL) * static_cast<size_t>(npts));
  float* const vad =
      a.dva != nullptr ? a.dva : stage(e, slot++, a.va, nspec * static_cast<size_t>(ncol));
  float* const cthd = stage(e, slot++, a.cflthmax, ncfl);
  float* const ckd = stage(e, slot++, a.cflkmax, ncfl);
  t_in.stop();

  const size_t nw = static_cast<size_t>(ktp3_work_reals(p.nk, p.nth));
  const long long chunk =
      chunk_items(npts, static_cast<long long>(nw * sizeof(Real)));
  auto& b = buffers<Mem>();
  grow(b.work_r, nw * static_cast<size_t>(chunk), "ww_prop3.ktp3_work");
  Real* const w0 = b.work_r.data();

  long long launches = 0;
  for (long long n0 = 0; n0 < npts; n0 += chunk) {
    const int m = static_cast<int>(std::min<long long>(chunk, npts - n0));
    const int k0 = static_cast<int>(n0);
    ww::timers::Region t_k("ktp3.kernel", fence);
    Kokkos::parallel_for(
        "ww_ktp3_batch", Kokkos::RangePolicy<E, Kokkos::Schedule<Kokkos::Dynamic>>(e, 0, m),
        KOKKOS_LAMBDA(const int i) {
          const int j = k0 + i;
          const int js = jsd[j];
          const int is = isd[j];
          const float* const ps = psd + static_cast<size_t>(WW_KTP3_NPSCAL) * j;
          const Ktp3Work w = ktp3_work(w0 + static_cast<size_t>(i) * nw, p.nk, p.nth);
          float* const vcol = vad + static_cast<size_t>(js) * nspec;  // VA(:,JSEA)
          const float* const cgc = cgd + static_cast<size_t>(is) * nk2;  // CG(:,ISEA)
          const float* const wnc = wnd + static_cast<size_t>(is) * nk2;  // WN(:,ISEA)
          Real cth = Real(0), ck = Real(0);
          for (int t = 0; t < ntimes; ++t)
            w3ktp3(p, g, itstep, is, facth, fack, ps[0], cgc, wnc, ps[1], ps[2], ps[3], ps[4],
                   ps[5], ps[6], ps[7], ps[8], ps[9], nullptr, nullptr, vcol, cth, ck, w);
          cthd[js - 1] = cth;  // CFLTHMAX(JSEA)
          ckd[js - 1] = ck;    // CFLKMAX(JSEA)
        });
    t_k.stop();
    ++launches;
  }

  ww::timers::Region t_out("ktp3.d2h", fence);
  if (a.dva == nullptr) unstage(e, a.va, vad, nspec * static_cast<size_t>(ncol));
  unstage(e, a.cflthmax, cthd, ncfl);
  unstage(e, a.cflkmax, ckd, ncfl);
  e.fence("ww_ktp3_batch");
  t_out.stop();
  count(KTP3, static_cast<long long>(npts) * ntimes, launches);
  return int{WW_KOKKOS_OK};
}

/// The resident VA an entry point works on: grid, SIZE(VA,1), UBOUND(VA,2).
struct ResidentVa {
  int igrid, nspec, nsealm;
};

// The checks and the call of ww_xyp3_batch and ww_xyp3_batch_va: res == nullptr stages va,
// else the kernels work on the resident VA of res->igrid (ncol is then NSEALM+1).
int xyp3_entry(const ResidentVa* res, const float* rpar, int nrpar, const int* ipar, int nipar,
               const int* imaps, int nimaps, const float* tscal, int ntscal, float dtg,
               const int* mapsf, const float* clats, const float* sig, const float* ecos,
               const float* esin, const float* dpdx, const float* dpdy, const float* dqdx,
               const float* dqdy, const float* gsqrt, const float* atrnx, const float* atrny,
               const int* mapx2, const int* mapy2, const int* mapaxy, const int* mapcxy,
               const int* maptrn, const int* mapsta, const float* cg, const float* cx,
               const float* cy, int flbpi, int nbi, const int* isbpi, const float* bbpi0,
               const float* bbpin, const int* on, int ncol, float* va) {
  using namespace ww::prop;
  if (rpar == nullptr || ipar == nullptr || imaps == nullptr || tscal == nullptr ||
      nrpar != WW_XYP3_NRPAR || nipar != WW_XYP3_NIPAR || nimaps != WW_XYP3_NIMAPS ||
      ntscal != WW_XYP3_NTSCAL)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_xyp3_batch: bad packet");
  const Params p = unpack(rpar, ipar);
  if (p.nx < 1 || p.ny < 1 || p.nsea < 1 || p.nk < 1 || p.nth < 1 || p.nspec != p.nk * p.nth ||
      ncol < p.nsea + 1 || nbi < 0 || mapsf == nullptr || clats == nullptr || sig == nullptr ||
      ecos == nullptr || esin == nullptr || dpdx == nullptr || dpdy == nullptr ||
      dqdx == nullptr || dqdy == nullptr || gsqrt == nullptr || atrnx == nullptr ||
      atrny == nullptr || mapx2 == nullptr || mapy2 == nullptr || mapaxy == nullptr ||
      mapcxy == nullptr || maptrn == nullptr || mapsta == nullptr || cg == nullptr ||
      cx == nullptr || cy == nullptr || on == nullptr || va == nullptr ||
      (flbpi != 0 && (isbpi == nullptr || bbpi0 == nullptr || bbpin == nullptr)) ||
      (res != nullptr && res->nspec != p.nspec))
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_xyp3_batch: bad sizes or null pointer");
  if (p.iclose == kIcloseTrpl)
    return fail(WW_KOKKOS_ERR_UNSUPPORTED, "ww_xyp3_batch: tripole grid (W3XYP3 stops there)");

  const Xyp3Args args{p,     imaps, tscal, dtg,   mapsf, mapx2, mapy2,  mapaxy, mapcxy, maptrn,
                      mapsta, isbpi, on,  clats, sig,   ecos,  esin,  dpdx,   dpdy,   dqdx,
                      dqdy,  gsqrt, atrnx, atrny, cg,    cx,    cy,    bbpi0,  bbpin,  flbpi,
                      nbi,   ncol,  va,    nullptr};
  if (res != nullptr)
    return run_va("xyp3.call", XYP3, res->igrid, va, res->nspec, res->nsealm,
                  [&](const auto& e, bool fence, float* dva) {
                    Xyp3Args ar = args;
                    ar.dva = dva;
                    return xyp3_batch(e, fence, ar);
                  });
  return run("xyp3.call", [&](const auto& e, bool fence) { return xyp3_batch(e, fence, args); });
}

// The checks and the call of ww_ktp3_batch and ww_ktp3_batch_va, as xyp3_entry.
int ktp3_entry(const ResidentVa* res, const float* rpar, int nrpar, const int* ipar, int nipar,
               const float* sig, const float* dsip, const float* ecos, const float* esin,
               const float* ec2, const float* esc, const float* es2, const int* mapwn,
               const int* mapth2, const int* mapwn2, int itstep, int ntimes, float facth,
               float fack, int nsea, const float* cg, const float* wn, int npts, const int* jsea,
               const int* isea, const float* pscal, int ncol, float* va, float* cflthmax,
               float* cflkmax) {
  using namespace ww::prop;
  if (rpar == nullptr || ipar == nullptr || nrpar != WW_KTP3_NRPAR || nipar != WW_KTP3_NIPAR)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_ktp3_batch: bad packet");
  Ktp3Params p{};
  p.dmin = rpar[0];
  p.ctmax = rpar[1];
  p.fachfa = rpar[2];
  p.nk = ipar[0];
  p.nth = ipar[1];
  p.nspec = ipar[2];
  p.nk2 = ipar[3];
  p.flcth = ipar[4];
  p.flck = ipar[5];
  p.flcur = ipar[6];
  if (p.nk < 1 || p.nth < 1 || p.nspec != p.nk * p.nth || p.nk2 != p.nk + 2 || nsea < 1 ||
      npts < 0 || npts > nsea || ntimes < 0 || ncol < 2 || sig == nullptr || dsip == nullptr ||
      ecos == nullptr || esin == nullptr || ec2 == nullptr || esc == nullptr || es2 == nullptr ||
      mapwn == nullptr || mapth2 == nullptr || mapwn2 == nullptr || cg == nullptr ||
      wn == nullptr || va == nullptr || cflthmax == nullptr || cflkmax == nullptr ||
      (npts > 0 && (jsea == nullptr || isea == nullptr || pscal == nullptr)) ||
      (res != nullptr && res->nspec != p.nspec))
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_ktp3_batch: bad sizes or null pointer");
  for (int j = 0; j < npts; ++j)
    if (jsea[j] < 1 || jsea[j] >= ncol || isea[j] < 1 || isea[j] > nsea)
      return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_ktp3_batch: a point outside VA or the sea");
  if (ntimes == 0 || npts == 0) {
    count(KTP3, 0, 0);
    return int{WW_KOKKOS_OK};
  }

  const Ktp3Args args{p,     sig,   dsip,   ecos,   esin,   ec2,    esc,    es2,   cg,
                      wn,    pscal, mapwn,  mapth2, mapwn2, jsea,   isea,   itstep, ntimes,
                      nsea,  npts,  ncol,   facth,  fack,   va,     cflthmax, cflkmax,
                      nullptr};
  if (res != nullptr)
    return run_va("ktp3.call", KTP3, res->igrid, va, res->nspec, res->nsealm,
                  [&](const auto& e, bool fence, float* dva) {
                    Ktp3Args ar = args;
                    ar.dva = dva;
                    return ktp3_batch(e, fence, ar);
                  });
  return run("ktp3.call", [&](const auto& e, bool fence) { return ktp3_batch(e, fence, args); });
}

}  // namespace
}  // namespace ww::prop3shim

using namespace ww::prop3shim;

extern "C" {

// ww_xyp3_batch: section 3.6.3 of W3WAVE on a regular or curvilinear grid, W3_SHRD:
//   DO ISPEC = 1, NSPEC; IF (IAPPRO(ISPEC) == IAPROC) W3GATH, W3XYP3, W3SCAT; END DO
// (w3wavemd.F90, the ISPEC loop after FIELD = 0. at the pin), every component in one call.
// | C      | Fortran                                  | Type         | Intent | Length            |
// | rpar   | DTCFL, DTH, XFR, WDCG, WDTH              | const float* | in     | nrpar = 5         |
// | ipar   | NX, NY, NSEA, NK, NTH, NSPEC, ICLOSE,    | const int*   | in     | nipar = 11        |
// |        | FLCX, FLCY, FLAGLL, FLCUR (0/1)          |              |        |                   |
// | imaps  | NMX0, NMX1, NMX2, NMY0, NMY1, NMY2,      | const int*   | in     | nimaps = 8        |
// |        | NACT, NCENT (W3ADATMD)                   |              |        |                   |
// | tscal  | DSEC21((/TIME(1),0/),TIME),              | const float* | in     | ntscal = 3        |
// |        | DSEC21(TBPI0,TIME), DSEC21(TBPI0,TBPIN)  |              |        | (last two FLBPI)  |
// | dtg    | DTG                                      | float        | in     |                   |
// | mapsf  | MAPSF(NSEA,3)                            | const int*   | in     | 3*NSEA            |
// | clats  | CLATS(0:NSEA)                            | const float* | in     | NSEA+1            |
// | sig    | SIG(0:NK+1)                              | const float* | in     | NK+2              |
// | ecos   | ECOS, ESIN(NSPEC+NTH)                    | const float* | in     | NSPEC+NTH each    |
// | dpdx   | DPDX, DPDY, DQDX, DQDY, GSQRT(NY,NX)     | const float* | in     | NX*NY each        |
// | atrnx  | ATRNX, ATRNY(NY*NX,-1:1)                 | const float* | in     | 3*NX*NY each      |
// | mapx2  | MAPX2, MAPY2, MAPAXY(NY*NX)              | const int*   | in     | NX*NY each        |
// | mapcxy | MAPCXY(NSEA)                             | const int*   | in     | NSEA              |
// | maptrn | MERGE(1,0,MAPTRN(NY*NX))                 | const int*   | in     | NX*NY             |
// | mapsta | MAPSTA(NY*NX), the dummy W3XYP3 and      | const int*   | in     | NX*NY             |
// |        | W3SCAT get                               |              |        |                   |
// | cg     | CG(0:NK+1,0:NSEA)                        | const float* | in     | (NK+2)*(NSEA+1)   |
// | cx, cy | CX, CY(0:NSEA)                           | const float* | in     | NSEA+1 each       |
// | flbpi  | FLBPI (0/1)                              | int          | in     |                   |
// | nbi    | NBI                                      | int          | in     |                   |
// | isbpi  | ISBPI(NBI)                               | const int*   | in     | NBI (FLBPI)       |
// | bbpi0  | BBPI0, BBPIN(NSPEC,0:NBI)                | const float* | in     | NSPEC*(NBI+1)     |
// | on     | IAPPRO(ISPEC) == IAPROC, 0/1             | const int*   | in     | NSPEC             |
// | ncol   | SIZE(VA,2) = NSEALM+1                    | int          | in     |                   |
// | va     | VA(NSPEC,0:NSEALM)                       | float*       | inout  | NSPEC*ncol; rows  |
// |        |                                          |              |        | with on, 1:NSEA   |
// ICLOSE_TRPL is refused (W3XYP3 stops there, w3pro3md.F90:868-874). The kernel's MAPSF stride
// is NSEA: the Fortran wrapper stops on a MAPSF with other than NSEA rows.
int ww_xyp3_batch(const float* rpar, int nrpar, const int* ipar, int nipar, const int* imaps,
                  int nimaps, const float* tscal, int ntscal, float dtg, const int* mapsf,
                  const float* clats, const float* sig, const float* ecos, const float* esin,
                  const float* dpdx, const float* dpdy, const float* dqdx, const float* dqdy,
                  const float* gsqrt, const float* atrnx, const float* atrny, const int* mapx2,
                  const int* mapy2, const int* mapaxy, const int* mapcxy, const int* maptrn,
                  const int* mapsta, const float* cg, const float* cx, const float* cy,
                  int flbpi, int nbi, const int* isbpi, const float* bbpi0, const float* bbpin,
                  const int* on, int ncol, float* va) {
  return xyp3_entry(nullptr, rpar, nrpar, ipar, nipar, imaps, nimaps, tscal, ntscal, dtg, mapsf,
                    clats, sig, ecos, esin, dpdx, dpdy, dqdx, dqdy, gsqrt, atrnx, atrny, mapx2,
                    mapy2, mapaxy, mapcxy, maptrn, mapsta, cg, cx, cy, flbpi, nbi, isbpi, bbpi0,
                    bbpin, on, ncol, va);
}

// ww_xyp3_batch_va (WFIP-0005 unit 3b): ww_xyp3_batch on the resident VA of grid igrid. The
// arguments are ww_xyp3_batch's, ncol replaced by:
// | C      | Fortran                    | Type   | Intent | Shape                          |
// | igrid  | IGRID (W3GDATMD)           | int    | in     |                                |
// | va     | VA(1,0) (W3WDATMD)         | float* | inout  | nspec x nsealm+1, never read   |
// |        |                            |        |        | or written here but to bind it |
// | nspec  | SIZE(VA,1)                 | int    | in     | = NSPEC                        |
// | nsealm | UBOUND(VA,2)               | int    | in     | >= NSEA                        |
// The kernels read and write d_va (ww::resident::device_va syncs it first); afterwards the
// state is DEVICE and the host VA is stale until a ww_va_host_read or _write.
int ww_xyp3_batch_va(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                     const int* imaps, int nimaps, const float* tscal, int ntscal, float dtg,
                     const int* mapsf, const float* clats, const float* sig, const float* ecos,
                     const float* esin, const float* dpdx, const float* dpdy, const float* dqdx,
                     const float* dqdy, const float* gsqrt, const float* atrnx,
                     const float* atrny, const int* mapx2, const int* mapy2, const int* mapaxy,
                     const int* mapcxy, const int* maptrn, const int* mapsta, const float* cg,
                     const float* cx, const float* cy, int flbpi, int nbi, const int* isbpi,
                     const float* bbpi0, const float* bbpin, const int* on, float* va, int nspec,
                     int nsealm) {
  const ResidentVa r{igrid, nspec, nsealm};
  return xyp3_entry(&r, rpar, nrpar, ipar, nipar, imaps, nimaps, tscal, ntscal, dtg, mapsf,
                    clats, sig, ecos, esin, dpdx, dpdy, dqdx, dqdy, gsqrt, atrnx, atrny, mapx2,
                    mapy2, mapaxy, mapcxy, maptrn, mapsta, cg, cx, cy, flbpi, nbi, isbpi, bbpi0,
                    bbpin, on, nsealm + 1, va);
}

// ww_ktp3_batch: one intra-spectral part of W3WAVE (sections 3.6.2, 3.6.4), on a regular or
// curvilinear grid: DO ITLOC (NTIMES of them); DO JSEA with MAPSTA(IY,IX) == 1: W3KTP3; END DO.
// The Fortran wrapper lists the points and packs their scalars as W3WAVE passes them.
// | C        | Fortran                                  | Type         | Intent | Length          |
// | rpar     | DMIN, CTMAX, FACHFA                      | const float* | in     | nrpar = 3       |
// | ipar     | NK, NTH, NSPEC, NK2, FLCTH, FLCK, FLCUR  | const int*   | in     | nipar = 7       |
// | sig      | SIG, DSIP(0:NK+1)                        | const float* | in     | NK+2 each       |
// | ecos     | ECOS, ESIN, EC2, ESC, ES2(NSPEC+NTH)     | const float* | in     | NSPEC+NTH each  |
// | mapwn    | MAPWN(NSPEC+NTH)                         | const int*   | in     | NSPEC+NTH       |
// | mapth2   | MAPTH2((NK+2)*NTH)                       | const int*   | in     | (NK+2)*NTH      |
// | mapwn2   | MAPWN2(NSPEC+NTH)                        | const int*   | in     | NSPEC+NTH       |
// | itstep   | ITSTEP (W3ADATMD)                        | int          | in     |                 |
// | ntimes   | the ITLOC count of the part              | int          | in     | >= 0            |
// | facth    | FACTH, FACK                              | float        | in     |                 |
// | nsea     | NSEA                                     | int          | in     |                 |
// | cg, wn   | CG, WN(0:NK+1,0:NSEA)                    | const float* | in     | (NK+2)*(NSEA+1) |
// | npts     | the points listed                        | int          | in     |                 |
// | jsea     | JSEA of each point                       | const int*   | in     | npts            |
// | isea     | ISEA of each point (INIT_GET_ISEA)       | const int*   | in     | npts            |
// | pscal    | (CTHG0S(ISEA), MAX(DMIN,DW(ISEA)),       | const float* | in     | 10*npts         |
// |          | DDDX, DDDY(IY,IX), CX, CY(ISEA), DCXDX,  |              |        |                 |
// |          | DCXDY, DCYDX, DCYDY(IY,IX)) per point    |              |        |                 |
// | ncol     | SIZE(VA,2) = NSEALM+1                    | int          | in     |                 |
// | va       | VA(NSPEC,0:NSEALM)                       | float*       | inout  | NSPEC*ncol      |
// | cflthmax | CFLTHMAX, CFLKMAX(NSEALM)                | float*       | inout  | ncol-1 each     |
// DCDX and DCDY are not passed: W3KTP3 reads them only under W3_REFRX, which the translation
// does not cover (the set-up refuses it).
int ww_ktp3_batch(const float* rpar, int nrpar, const int* ipar, int nipar, const float* sig,
                  const float* dsip, const float* ecos, const float* esin, const float* ec2,
                  const float* esc, const float* es2, const int* mapwn, const int* mapth2,
                  const int* mapwn2, int itstep, int ntimes, float facth, float fack, int nsea,
                  const float* cg, const float* wn, int npts, const int* jsea, const int* isea,
                  const float* pscal, int ncol, float* va, float* cflthmax, float* cflkmax) {
  return ktp3_entry(nullptr, rpar, nrpar, ipar, nipar, sig, dsip, ecos, esin, ec2, esc, es2,
                    mapwn, mapth2, mapwn2, itstep, ntimes, facth, fack, nsea, cg, wn, npts, jsea,
                    isea, pscal, ncol, va, cflthmax, cflkmax);
}

// ww_ktp3_batch_va (WFIP-0005 unit 3b): ww_ktp3_batch on the resident VA of grid igrid. The
// arguments are ww_ktp3_batch's, ncol replaced by igrid (first), and nspec, nsealm after va, as
// in ww_xyp3_batch_va; cflthmax and cflkmax stay host arrays (NSEALM each, as before).
int ww_ktp3_batch_va(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                     const float* sig, const float* dsip, const float* ecos, const float* esin,
                     const float* ec2, const float* esc, const float* es2, const int* mapwn,
                     const int* mapth2, const int* mapwn2, int itstep, int ntimes, float facth,
                     float fack, int nsea, const float* cg, const float* wn, int npts,
                     const int* jsea, const int* isea, const float* pscal, float* va, int nspec,
                     int nsealm, float* cflthmax, float* cflkmax) {
  const ResidentVa r{igrid, nspec, nsealm};
  return ktp3_entry(&r, rpar, nrpar, ipar, nipar, sig, dsip, ecos, esin, ec2, esc, es2, mapwn,
                    mapth2, mapwn2, itstep, ntimes, facth, fack, nsea, cg, wn, npts, jsea, isea,
                    pscal, nsealm + 1, va, cflthmax, cflkmax);
}

}  // extern "C"
