// kokkos/src/fortran_iface/srce_shim.cpp
// The bind(C) shim of W3SRCE: the C ABI of ww_kokkos_c.hpp ("W3SRCE") over the
// team kernel of ww_kokkos/srce_kernel.hpp. Marshalling only.
//
// The rules are snl1_shim.cpp's and st4_shim.cpp's: one context per WW3 grid;
// nothing throws across the boundary; Views die inside Kokkos::finalize; inside
// an OpenMP parallel region (W3WAVE's source-term loop under W3_OMPG) the kernel
// runs on Kokkos::Serial. Phase 1 copies one point in, runs, copies it out, and
// the caller serialises the calls (!$OMP CRITICAL (WW_KOKKOS_SRCE)). The kernel
// reads the W3SNL1 and ST4 tables of the same grid (shim_state.hpp). The batched
// entry on the resident VA keeps its per-step inputs on the device and copies one
// only when its bits change (ww_srce_batch_va, WFIP-0005 unit 4); with
// WW_KOKKOS_SRCE_COUNT=1 it counts those copies, and on a host backend what a
// device would copy.

#include "fortran_iface/ww_kokkos_c.hpp"
#include "fortran_iface/resident_c.hpp"
#include "fortran_iface/shim_state.hpp"
#include "fortran_iface/timers.hpp"

#include <Kokkos_Core.hpp>

#include <type_traits>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/resident_state.hpp"
#include "ww_kokkos/snl1_team.hpp"
#include "ww_kokkos/srce_kernel.hpp"
#include "ww_kokkos/srce_params.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::srce {
namespace {

using DeviceSpace = st4::DeviceSpace;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevInt1D = Kokkos::View<int*, DeviceSpace>;
using DevDouble1D = Kokkos::View<double*, DeviceSpace>;

/// The per-step inputs of ww_srce_batch_va, in the order run_batch stages them (WFIP-0005
/// unit 4). PSC, WC, TB, TI are inout: the kernel writes them, and they are copied back.
enum BatchIn { kInJsea, kInWn1, kInCg1, kInPin, kInPsc, kInWc, kInTb, kInTi, kNumIn };
constexpr const char* kInName[kNumIn] = {"BJSEA", "BWN1", "BCG1", "BPIN",
                                         "BPSC",  "BWC",  "BTB",  "BTI"};

/// The bits of one input as its device buffer holds them: what was last copied in, or for an
/// inout input what was last copied back.
struct Shadow {
  bool valid = false;
  std::vector<unsigned char> bytes;
};

struct GridCtx {
  Params p{};
  DevReal1D sig, dden, ecos, esin;
  DevInt1D mapwn;
  DevReal1D spec, wn1, cg1, whitecap, taubbl, tauice, rw;
  DevInt1D iw;
  DevDouble1D dw;
  Kokkos::View<PointScalars*, DeviceSpace> scal;
  Kokkos::View<State*, DeviceSpace> state;
  int work_reals = -1, work_ints = -1;
  // ww_srce_batch: the same arrays for batch_cap points, point index slowest; b_spec only
  // for ww_srce_batch (ww_srce_batch_va reads the spectra in the resident VA through b_jsea).
  int batch_cap = 0, spec_cap = 0;
  DevReal1D b_spec, b_wn1, b_cg1, b_pin, b_psc, b_whitecap, b_taubbl, b_tauice, b_rw;
  DevInt1D b_shave, b_iw, b_jsea;
  DevDouble1D b_dw;
  Kokkos::View<PointScalars*, DeviceSpace> b_scal;
  Kokkos::View<State*, DeviceSpace> b_state;
  // ww_srce_batch_va, a step in one launch: the bits each b_* input buffer holds (BatchIn).
  // Invalidated when the buffers are reallocated or another path writes them.
  std::array<Shadow, kNumIn> shadow;
};

// Never destroyed: ww_kokkos_finalize runs from atexit, after the static destructors of
// objects built later, so a static map would be gone when its finalize hook clears it.
std::map<int, GridCtx>& grids() {
  static auto* m = new std::map<int, GridCtx>();
  return *m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
std::atomic<long long> g_count{0};
std::atomic<long long> g_batch_calls{0}, g_batch_launches{0}, g_batch_points{0};
// ww_srce_batch_va's per-step inputs (BatchIn): copies to the device and their bytes, inputs
// whose bits had not changed and were not copied, and the copies back of the inout ones. On a
// backend that reads host memory nothing is copied; with WW_KOKKOS_SRCE_COUNT=1 the same
// comparison runs there and these say what a device would have copied (g_in_projected).
std::array<std::atomic<long long>, kNumIn> g_in_copies{}, g_in_copy_bytes{}, g_in_same{},
    g_in_same_bytes{};
std::atomic<long long> g_out_copies{0}, g_out_bytes{0};
std::atomic<bool> g_in_projected{false};
bool g_hook_pushed = false;

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }
void release() { grids().clear(); }

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}
struct Flags {
  bool srce, count, staged;
};
// WW_KOKKOS_SRCE_STAGED=1, a diagnostic read once: on a host backend ww_srce_batch_va stages its
// inputs into the batch buffers and skips unchanged ones, as on a device, instead of reading the
// caller's arrays in place, so the skip is exercised on the CPU (a wrong skip shows as stale
// inputs in the output).
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SRCE") != 0), env_is_one("WW_KOKKOS_SRCE_COUNT"),
                       env_is_one("WW_KOKKOS_SRCE_STAGED")};
  return f;
}
void print_count() {
  std::fprintf(stderr, "ww_kokkos: W3SRCE spectra computed: %lld\n", g_count.load());
  // One count per line, "<name>: <n>", the form scripts/ww3-gpu.sh prints.
  std::fprintf(stderr, "ww_kokkos: W3SRCE batched calls: %lld\n", g_batch_calls.load());
  std::fprintf(stderr, "ww_kokkos: W3SRCE batched launches: %lld\n", g_batch_launches.load());
  std::fprintf(stderr, "ww_kokkos: W3SRCE batched points: %lld\n", g_batch_points.load());
  long long nc = 0, bc = 0, ns = 0, bs = 0;
  for (int k = 0; k < kNumIn; ++k) {
    nc += g_in_copies[k].load();
    bc += g_in_copy_bytes[k].load();
    ns += g_in_same[k].load();
    bs += g_in_same_bytes[k].load();
  }
  if (nc + ns == 0) return;
  const char* how = g_in_projected.load() ? " (host backend: projected for a device, none made)"
                                           : "";
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch input copies H2D%s: %lld\n", how, nc);
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch input bytes H2D%s: %lld\n", how, bc);
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch inputs unchanged, not copied: %lld\n", ns);
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch input bytes unchanged, not copied: %lld\n", bs);
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch output copies D2H%s: %lld\n", how,
               g_out_copies.load());
  std::fprintf(stderr, "ww_kokkos: W3SRCE batch output bytes D2H%s: %lld\n", how,
               g_out_bytes.load());
  for (int k = 0; k < kNumIn; ++k)
    std::fprintf(stderr, "ww_kokkos: W3SRCE batch input %s copied %lld (%lld B), unchanged %lld\n",
                 kInName[k], g_in_copies[k].load(), g_in_copy_bytes[k].load(), g_in_same[k].load());
}

constexpr long long kBatchMibDefault = 1024;

/// The batch buffers' cap in bytes: WW_KOKKOS_SRCE_BATCH_MIB, a positive
/// integer, else the default (a value that does not parse is reported once).
long long batch_cap_bytes() {
  static const long long cap = [] {
    long long mib = kBatchMibDefault;
    if (const char* v = std::getenv("WW_KOKKOS_SRCE_BATCH_MIB")) {
      char* end = nullptr;
      const long long x = std::strtoll(v, &end, 10);
      if (end != v && *end == '\0' && x > 0 && x <= (1LL << 30))
        mib = x;
      else
        std::fprintf(stderr, "ww_kokkos: WW_KOKKOS_SRCE_BATCH_MIB='%s' ignored, using %lld\n", v,
                     kBatchMibDefault);
    }
    return mib * 1024 * 1024;
  }();
  return cap;
}

/// Device bytes one point takes in the batch buffers: with its spectrum (ww_srce_batch), or
/// with its column index in the resident VA instead (ww_srce_batch_va).
long long batch_bytes_per_point(const GridCtx& g, bool spec) {
  const Params& p = g.p;
  const long long reals = (spec ? static_cast<long long>(p.nspec) : 0LL) + 2LL * p.nk +
                          WW_SRCE_NPIN + WW_SRCE_NPSC + 4 + 2 + 2 + g.work_reals;
  const long long ints = (spec ? 1LL : 2LL) + g.work_ints;
  return reals * static_cast<long long>(sizeof(Real)) + ints * static_cast<long long>(sizeof(int)) +
         static_cast<long long>(srce_work_doubles(p)) * static_cast<long long>(sizeof(double)) +
         static_cast<long long>(sizeof(PointScalars) + sizeof(State));
}

/// Grow the batch buffers of `g` to `n` points (b_spec only when `spec`); they never shrink.
void batch_reserve(GridCtx& g, int n, bool spec) {
  const Params& p = g.p;
  const size_t m = static_cast<size_t>(n);
  if (spec && n > g.spec_cap) {
    g.b_spec = DevReal1D("ww_srce.b_spec", m * p.nspec);
    g.spec_cap = n;
  }
  if (n <= g.batch_cap) return;
  for (Shadow& s : g.shadow) s.valid = false;
  g.b_wn1 = DevReal1D("ww_srce.b_wn1", m * p.nk);
  g.b_cg1 = DevReal1D("ww_srce.b_cg1", m * p.nk);
  g.b_pin = DevReal1D("ww_srce.b_pin", m * WW_SRCE_NPIN);
  g.b_psc = DevReal1D("ww_srce.b_psc", m * WW_SRCE_NPSC);
  g.b_whitecap = DevReal1D("ww_srce.b_whitecap", m * 4);
  g.b_taubbl = DevReal1D("ww_srce.b_taubbl", m * 2);
  g.b_tauice = DevReal1D("ww_srce.b_tauice", m * 2);
  g.b_rw = DevReal1D("ww_srce.b_rw", m * g.work_reals);
  g.b_shave = DevInt1D("ww_srce.b_shave", m);
  g.b_iw = DevInt1D("ww_srce.b_iw", m * g.work_ints);
  g.b_jsea = DevInt1D("ww_srce.b_jsea", m);
  g.b_dw = DevDouble1D("ww_srce.b_dw", m * srce_work_doubles(p));
  g.b_scal = Kokkos::View<PointScalars*, DeviceSpace>("ww_srce.b_scal", m);
  g.b_state = Kokkos::View<State*, DeviceSpace>("ww_srce.b_state", m);
  g.batch_cap = n;
}

#if defined(WW_SRCE_SERIAL_NESTED)
bool nested_in_openmp() { return omp_get_level() > 0; }
#endif

template <class V, class T>
V to_device(const char* name, const T* data, int n) {
  V d(name, n);
  Kokkos::View<const T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(data, n);
  Kokkos::deep_copy(d, h);
  return d;
}

template <class T>
using HostUnmanaged =
    Kokkos::View<T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// One point on ExecSpace: stage in, the team kernel, stage out.
template <class ExecSpace>
void run(GridCtx& g, const st4::Params& q, const st4::Fields& f, const snl1::Config& nc,
         const snl1::Tables& nt, const snl1::RealView1D& nsig, const PointIn& in, float* spec,
         const float* wn1, const float* cg1, PointScalars& o, float* whitecap, float* taubbl,
         float* tauice) {
  const ExecSpace ex;
  const Params p = g.p;
  // On Kokkos::Serial (a caller inside an OpenMP region) the work is synchronous: no fence.
  constexpr bool fence = !std::is_same_v<ExecSpace, Kokkos::Serial>;
  ww::timers::Region t_in("srce.h2d", fence);
  Kokkos::deep_copy(ex, g.spec, HostUnmanaged<float>(spec, p.nspec));
  Kokkos::deep_copy(ex, g.wn1, HostUnmanaged<const float>(wn1, p.nk));
  Kokkos::deep_copy(ex, g.cg1, HostUnmanaged<const float>(cg1, p.nk));
  Kokkos::deep_copy(ex, g.whitecap, HostUnmanaged<float>(whitecap, 4));
  Kokkos::deep_copy(ex, g.taubbl, HostUnmanaged<float>(taubbl, 2));
  Kokkos::deep_copy(ex, g.tauice, HostUnmanaged<float>(tauice, 2));
  Kokkos::deep_copy(ex, g.scal, HostUnmanaged<PointScalars>(&o, 1));
  t_in.stop();

  using Policy = Kokkos::TeamPolicy<ExecSpace>;
  using Member = typename Policy::member_type;
  const size_t scratch = snl1::snl1_team_scratch_bytes<ExecSpace>(nc, nt);
  Policy policy(1, Kokkos::AUTO);
  const int level = scratch <= static_cast<size_t>(policy.scratch_size_max(0)) ? 0 : 1;
  if (scratch > static_cast<size_t>(policy.scratch_size_max(level)))
    throw std::runtime_error("ww_srce: W3SNL1 needs " + std::to_string(scratch) +
                             " B of team scratch, more than this backend offers");
  policy.set_scratch_size(level, Kokkos::PerTeam(scratch));

  const Grid grid{g.sig.data(), g.dden.data(), g.ecos.data(), g.esin.data(), g.mapwn.data()};
  const auto SP = g.spec, W1 = g.wn1, C1 = g.cg1, WC = g.whitecap, TB = g.taubbl, TI = g.tauice,
             RW = g.rw;
  const auto IW = g.iw;
  const auto DW = g.dw;
  const auto SC = g.scal;
  const auto ST = g.state;
  const Real* ns = nsig.data();
  ww::timers::Region t_k("srce.kernel", fence);
  Kokkos::parallel_for(
      "ww_srce", policy, KOKKOS_LAMBDA(const Member& team) {
        const snl1::TeamScratch w = snl1::snl1_team_scratch(team, level, nc, nt);
        w3srce_team(team, p, grid, q, f, nc, nt, ns, w, in, SP.data(), W1.data(), C1.data(),
                    SC(0), WC.data(), TB.data(), TI.data(), ST(0), RW.data(), IW.data(),
                    DW.data());
      });
  ex.fence();
  t_k.stop();

  ww::timers::Region t_out("srce.d2h", fence);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(spec, p.nspec), g.spec);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(whitecap, 4), g.whitecap);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(taubbl, 2), g.taubbl);
  Kokkos::deep_copy(ex, HostUnmanaged<float>(tauice, 2), g.tauice);
  Kokkos::deep_copy(ex, HostUnmanaged<PointScalars>(&o, 1), g.scal);
  ex.fence();
}

template <class T>
using DevUnmanaged = Kokkos::View<T*, DeviceSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// Whether `nbytes` at `p` differ from the bits of shadow `s`.
bool differs(const Shadow& s, const void* p, size_t nbytes) {
  return !s.valid || s.bytes.size() != nbytes ||
         (nbytes > 0 && std::memcmp(s.bytes.data(), p, nbytes) != 0);
}
void remember(Shadow& s, const void* p, size_t nbytes) {
  const auto* b = static_cast<const unsigned char*>(p);
  s.bytes.assign(b, b + nbytes);
  s.valid = true;
}

/// How run_batch stages ww_srce_batch_va's inputs (WFIP-0005 unit 4).
struct Staging {
  bool in_place;  ///< the backend reads host memory: the kernel uses the caller's arrays
  bool keep;      ///< the shadows are kept (a step in one launch; on a host backend only to count)
};

/// Where the kernel reads input `slot` (n elements at `host`, offset to the chunk): `host` itself
/// in place, else device buffer `dev`, copied into only when the bits differ from those it holds
/// (its shadow). The comparison is the guarantee: a writer of the input missed in WW3 cannot
/// leave stale bits on the device. A copy, or its skip, moves bits and computes nothing.
template <class ExecSpace, class T>
T* stage_in(const ExecSpace& ex, GridCtx& g, const Staging& st, int slot, T* host,
            std::remove_const_t<T>* dev, size_t n) {
  using U = std::remove_const_t<T>;
  const size_t nbytes = n * sizeof(U);
  Shadow& sh = g.shadow[static_cast<size_t>(slot)];
  const bool copy = !st.keep || differs(sh, host, nbytes);
  if (st.in_place && !st.keep) {
    // nothing copied, nothing counted
  } else if (copy) {
    g_in_copies[slot].fetch_add(1, std::memory_order_relaxed);
    g_in_copy_bytes[slot].fetch_add(static_cast<long long>(nbytes), std::memory_order_relaxed);
  } else {
    g_in_same[slot].fetch_add(1, std::memory_order_relaxed);
    g_in_same_bytes[slot].fetch_add(static_cast<long long>(nbytes), std::memory_order_relaxed);
  }
  // A read-only input's buffer now holds the host's bits; an inout one's holds them only after
  // the kernel and the copy back (remember_out), so until then its shadow is void.
  if (!st.keep || !std::is_const_v<T>)
    sh.valid = false;
  else if (copy)
    remember(sh, host, nbytes);
  if (st.in_place) return host;
  if (copy && n > 0) Kokkos::deep_copy(ex, DevUnmanaged<U>(dev, n), HostUnmanaged<const U>(host, n));
  return dev;
}

/// After the kernel (and on a device the copy back): an inout input's shadow takes the bits the
/// device buffer now holds, which are the host's.
void remember_out(GridCtx& g, const Staging& st, int slot, const void* host, size_t nbytes) {
  if (st.keep) remember(g.shadow[static_cast<size_t>(slot)], host, nbytes);
}

/// The time-step arguments of ww_srce_batch, shared by every point.
struct BatchStep {
  int it, inflags24, flogrd57, flogrd58;
  Real dtg;
};

/// Points [n0, n0+n) of a ww_srce_batch call on ExecSpace: one copy of each
/// array in, one launch of n teams, team i on point n0+i, one copy out. With `va`
/// (ww_srce_batch_va), team i reads and writes its spectrum in place, column jsea[n0+i]
/// of the resident VA, and the column indices cross instead of the spectra.
template <class ExecSpace>
void run_batch(GridCtx& g, const st4::Params& q, const st4::Fields& f, const snl1::Config& nc,
               const snl1::Tables& nt, const snl1::RealView1D& nsig, const BatchStep& c,
               int n0, int n, float* spec, const float* wn1, const float* cg1,
               const float* pin, float* psc, float* whitecap, float* taubbl, float* tauice,
               int* shave, const ww::resident::DevVa* va = nullptr, const int* jsea = nullptr,
               bool whole = false) {
  const ExecSpace ex;
  const Params p = g.p;
  const size_t o0 = static_cast<size_t>(n0), m = static_cast<size_t>(n);
  const size_t nspec = static_cast<size_t>(p.nspec), nk = static_cast<size_t>(p.nk);
  const auto head = [](const auto& v, size_t len) {
    return Kokkos::subview(v, std::make_pair(static_cast<size_t>(0), len));
  };
  constexpr bool fence = !std::is_same_v<ExecSpace, Kokkos::Serial>;
  const bool resident = va != nullptr;
  // On a host backend the shadows are kept only to count what a device would copy.
  const bool in_place =
      Kokkos::SpaceAccessibility<ExecSpace, Kokkos::HostSpace>::accessible && !flags().staged;
  const Staging st{in_place, whole && (!in_place || flags().count)};
  if (resident && st.in_place && st.keep) g_in_projected.store(true, std::memory_order_relaxed);
  ww::timers::Region t_in("srce.batch.h2d", fence);
  const int* js_p = g.b_jsea.data();
  const Real *w1_p = g.b_wn1.data(), *c1_p = g.b_cg1.data(), *pi_p = g.b_pin.data();
  Real *ps_p = g.b_psc.data(), *wc_p = g.b_whitecap.data(), *tb_p = g.b_taubbl.data(),
       *ti_p = g.b_tauice.data();
  int* sh_p = g.b_shave.data();
  if (resident) {
    js_p = stage_in(ex, g, st, kInJsea, jsea + o0, g.b_jsea.data(), m);
    w1_p = stage_in(ex, g, st, kInWn1, wn1 + o0 * nk, g.b_wn1.data(), m * nk);
    c1_p = stage_in(ex, g, st, kInCg1, cg1 + o0 * nk, g.b_cg1.data(), m * nk);
    pi_p = stage_in(ex, g, st, kInPin, pin + o0 * WW_SRCE_NPIN, g.b_pin.data(), m * WW_SRCE_NPIN);
    ps_p = stage_in(ex, g, st, kInPsc, psc + o0 * WW_SRCE_NPSC, g.b_psc.data(), m * WW_SRCE_NPSC);
    wc_p = stage_in(ex, g, st, kInWc, whitecap + o0 * 4, g.b_whitecap.data(), m * 4);
    tb_p = stage_in(ex, g, st, kInTb, taubbl + o0 * 2, g.b_taubbl.data(), m * 2);
    ti_p = stage_in(ex, g, st, kInTi, tauice + o0 * 2, g.b_tauice.data(), m * 2);
    if (st.in_place) sh_p = shave + o0;
  } else {
    for (Shadow& x : g.shadow) x.valid = false;
    Kokkos::deep_copy(ex, head(g.b_spec, m * nspec),
                      HostUnmanaged<float>(spec + o0 * nspec, m * nspec));
    Kokkos::deep_copy(ex, head(g.b_wn1, m * nk), HostUnmanaged<const float>(wn1 + o0 * nk, m * nk));
    Kokkos::deep_copy(ex, head(g.b_cg1, m * nk), HostUnmanaged<const float>(cg1 + o0 * nk, m * nk));
    Kokkos::deep_copy(ex, head(g.b_pin, m * WW_SRCE_NPIN),
                      HostUnmanaged<const float>(pin + o0 * WW_SRCE_NPIN, m * WW_SRCE_NPIN));
    Kokkos::deep_copy(ex, head(g.b_psc, m * WW_SRCE_NPSC),
                      HostUnmanaged<float>(psc + o0 * WW_SRCE_NPSC, m * WW_SRCE_NPSC));
    Kokkos::deep_copy(ex, head(g.b_whitecap, m * 4), HostUnmanaged<float>(whitecap + o0 * 4, m * 4));
    Kokkos::deep_copy(ex, head(g.b_taubbl, m * 2), HostUnmanaged<float>(taubbl + o0 * 2, m * 2));
    Kokkos::deep_copy(ex, head(g.b_tauice, m * 2), HostUnmanaged<float>(tauice + o0 * 2, m * 2));
  }
  t_in.stop();

  // Dynamic, as W3WAVE's own loop over the sea points (SCHEDULE (DYNAMIC,1)): the cost of a point
  // varies (ice, wind, the DIA's frequency range), and a hybrid CPU's P and E cores run at
  // different speeds, so a static split leaves threads waiting at the end of the launch. Each
  // point owns its slices of every array and its team scratch, so which thread runs it does not
  // change a bit. CUDA ignores the schedule.
  using Policy = Kokkos::TeamPolicy<ExecSpace, Kokkos::Schedule<Kokkos::Dynamic>>;
  using Member = typename Policy::member_type;
  const size_t scratch = snl1::snl1_team_scratch_bytes<ExecSpace>(nc, nt);
  Policy policy(n, Kokkos::AUTO);
  const int level = scratch <= static_cast<size_t>(policy.scratch_size_max(0)) ? 0 : 1;
  if (scratch > static_cast<size_t>(policy.scratch_size_max(level)))
    throw std::runtime_error("ww_srce_batch: W3SNL1 needs " + std::to_string(scratch) +
                             " B of team scratch, more than this backend offers");
  policy.set_scratch_size(level, Kokkos::PerTeam(scratch));
  // One point per steal, W3WAVE's DYNAMIC,1: Kokkos' automatic chunk (8 at 9 204 points and 32
  // threads) leaves up to eight points' work in the last thread's hands. CUDA does not read it.
  policy.set_chunk_size(1);

  const Grid grid{g.sig.data(), g.dden.data(), g.ecos.data(), g.esin.data(), g.mapwn.data()};
  const auto SP = g.b_spec, RW = g.b_rw;
  const DevUnmanaged<const Real> W1(w1_p, m * nk), C1(c1_p, m * nk), PI(pi_p, m * WW_SRCE_NPIN);
  const DevUnmanaged<Real> PS(ps_p, m * WW_SRCE_NPSC), WC(wc_p, m * 4), TB(tb_p, m * 2),
      TI(ti_p, m * 2);
  const DevUnmanaged<int> SH(sh_p, m);
  const DevUnmanaged<const int> JS(js_p, m);
  const auto IW = g.b_iw;
  const auto DW = g.b_dw;
  const auto SC = g.b_scal;
  const auto ST = g.b_state;
  const ww::resident::DevVa VA = resident ? *va : ww::resident::DevVa();
  const size_t nrw = static_cast<size_t>(g.work_reals), niw = static_cast<size_t>(g.work_ints),
               ndw = static_cast<size_t>(srce_work_doubles(p));
  const Real* ns = nsig.data();
  ww::timers::Region t_k("srce.batch.kernel", fence);
  const auto body = KOKKOS_LAMBDA(const Member& team) {
        const int i = team.league_rank();
        const size_t ii = static_cast<size_t>(i);
        const Real* x = PI.data() + ii * WW_SRCE_NPIN;
        const PointIn in{c.it,
                         c.inflags24,
                         c.flogrd57,
                         c.flogrd58,
                         x[WW_SRCE_IN_D_INP],
                         x[WW_SRCE_IN_U10ABS],
                         x[WW_SRCE_IN_U10DIR],
                         x[WW_SRCE_IN_AS],
                         x[WW_SRCE_IN_ICE],
                         c.dtg,
                         x[WW_SRCE_IN_DAIR],
                         x[WW_SRCE_IN_COEF]};
        Real* y = PS.data() + ii * WW_SRCE_NPSC;
        PointScalars& o = SC(i);
        Kokkos::single(Kokkos::PerTeam(team), [&]() {
          o = PointScalars{y[WW_SRCE_SC_USTAR],  y[WW_SRCE_SC_USTDIR], y[WW_SRCE_SC_TAUWX],
                           y[WW_SRCE_SC_TAUWY],  y[WW_SRCE_SC_DTDYN],  y[WW_SRCE_SC_FCUT],
                           y[WW_SRCE_SC_TAUOX],  y[WW_SRCE_SC_TAUOY],  y[WW_SRCE_SC_TAUWIX],
                           y[WW_SRCE_SC_TAUWIY], y[WW_SRCE_SC_TAUWNX], y[WW_SRCE_SC_TAUWNY],
                           y[WW_SRCE_SC_PHIAW],  y[WW_SRCE_SC_CHARN],  y[WW_SRCE_SC_TWS],
                           y[WW_SRCE_SC_PHIOC],  y[WW_SRCE_SC_PHIBBL], y[WW_SRCE_SC_PHICE],
                           y[WW_SRCE_SC_TAUOCX], y[WW_SRCE_SC_TAUOCY], y[WW_SRCE_SC_WNMEAN],
                           SH(i)};
        });
        team.team_barrier();
        const snl1::TeamScratch w = snl1::snl1_team_scratch(team, level, nc, nt);
        Real* const sp = resident ? VA.data() + static_cast<size_t>(JS(i)) * p.nspec
                                  : SP.data() + ii * p.nspec;
        w3srce_team(team, p, grid, q, f, nc, nt, ns, w, in, sp, W1.data() + ii * p.nk,
                    C1.data() + ii * p.nk, o, WC.data() + ii * 4,
                    TB.data() + ii * 2, TI.data() + ii * 2, ST(i), RW.data() + ii * nrw,
                    IW.data() + ii * niw, DW.data() + ii * ndw);
        team.team_barrier();
        Kokkos::single(Kokkos::PerTeam(team), [&]() {
          y[WW_SRCE_SC_USTAR] = o.ustar;
          y[WW_SRCE_SC_USTDIR] = o.ustdir;
          y[WW_SRCE_SC_TAUWX] = o.tauwx;
          y[WW_SRCE_SC_TAUWY] = o.tauwy;
          y[WW_SRCE_SC_DTDYN] = o.dtdyn;
          y[WW_SRCE_SC_FCUT] = o.fcut;
          y[WW_SRCE_SC_TAUOX] = o.tauox;
          y[WW_SRCE_SC_TAUOY] = o.tauoy;
          y[WW_SRCE_SC_TAUWIX] = o.tauwix;
          y[WW_SRCE_SC_TAUWIY] = o.tauwiy;
          y[WW_SRCE_SC_TAUWNX] = o.tauwnx;
          y[WW_SRCE_SC_TAUWNY] = o.tauwny;
          y[WW_SRCE_SC_PHIAW] = o.phiaw;
          y[WW_SRCE_SC_CHARN] = o.charn;
          y[WW_SRCE_SC_TWS] = o.tws;
          y[WW_SRCE_SC_PHIOC] = o.phioc;
          y[WW_SRCE_SC_PHIBBL] = o.phibbl;
          y[WW_SRCE_SC_PHICE] = o.phice;
          y[WW_SRCE_SC_TAUOCX] = o.tauocx;
          y[WW_SRCE_SC_TAUOCY] = o.tauocy;
          y[WW_SRCE_SC_WNMEAN] = o.wnmean;
          SH(i) = o.shave;
        });
      };
  // With no more points than threads no thread gets a second point under either schedule, and
  // the dynamic one costs a rendezvous of every thread per launch (ww3_ts1, one sea point, 2 161
  // launches: srce.batch.kernel 0.51-0.53 s static, 0.80-0.97 s dynamic, 32 threads). The
  // schedule decides only which thread runs a point, never the point's arithmetic.
  if (n > ex.concurrency())
    Kokkos::parallel_for("ww_srce_batch", policy, body);
  else
    Kokkos::parallel_for("ww_srce_batch", Kokkos::TeamPolicy<ExecSpace>(policy), body);
  ex.fence();
  t_k.stop();

  ww::timers::Region t_out("srce.batch.d2h", fence);
  if (!(resident && st.in_place)) {
    if (!resident)
      Kokkos::deep_copy(ex, HostUnmanaged<float>(spec + o0 * nspec, m * nspec),
                        head(g.b_spec, m * nspec));
    Kokkos::deep_copy(ex, HostUnmanaged<float>(psc + o0 * WW_SRCE_NPSC, m * WW_SRCE_NPSC),
                      head(g.b_psc, m * WW_SRCE_NPSC));
    Kokkos::deep_copy(ex, HostUnmanaged<float>(whitecap + o0 * 4, m * 4), head(g.b_whitecap, m * 4));
    Kokkos::deep_copy(ex, HostUnmanaged<float>(taubbl + o0 * 2, m * 2), head(g.b_taubbl, m * 2));
    Kokkos::deep_copy(ex, HostUnmanaged<float>(tauice + o0 * 2, m * 2), head(g.b_tauice, m * 2));
    Kokkos::deep_copy(ex, HostUnmanaged<int>(shave + o0, m), head(g.b_shave, m));
  }
  ex.fence();
  t_out.stop();
  if (resident) {
    if (!st.in_place || st.keep) {
      g_out_copies.fetch_add(5, std::memory_order_relaxed);
      g_out_bytes.fetch_add(static_cast<long long>(m * (WW_SRCE_NPSC + 4 + 2 + 2) * sizeof(float) +
                                                   m * sizeof(int)),
                            std::memory_order_relaxed);
    }
    remember_out(g, st, kInPsc, psc + o0 * WW_SRCE_NPSC, m * WW_SRCE_NPSC * sizeof(float));
    remember_out(g, st, kInWc, whitecap + o0 * 4, m * 4 * sizeof(float));
    remember_out(g, st, kInTb, taubbl + o0 * 2, m * 2 * sizeof(float));
    remember_out(g, st, kInTi, tauice + o0 * 2, m * 2 * sizeof(float));
  }
}

}  // namespace
}  // namespace ww::srce

using namespace ww::srce;
using ww::Real;
namespace snl1 = ww::snl1;
namespace st4 = ww::st4;

extern "C" {

int ww_srce_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                 const float* sig, const float* dden, const float* ecos, const float* esin,
                 const int* mapwn) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (nrpar != NRPAR || nipar != NIPAR || rpar == nullptr || ipar == nullptr ||
        sig == nullptr || dden == nullptr || ecos == nullptr || esin == nullptr ||
        mapwn == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: RPAR/IPAR length differs from srce_params.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    if (ipar[I_LAB_SWITCH] != 1) {
      set_error(WW_KOKKOS_ERR_UNSUPPORTED,
                "ww_srce_init: the Kokkos W3SRCE translates switch_lab_shrd's source terms only");
      return WW_KOKKOS_ERR_UNSUPPORTED;
    }
    const Params p = unpack(rpar, ipar);
    if (p.nk <= 0 || p.nth <= 0 || p.nspec != p.nk * p.nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: nspec != nk*nth");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
                "ww_srce_init before ww_snl1_init and ww_st4_commit for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (nc.nk != p.nk || nc.nth != p.nth || q.nk != p.nk || q.nth != p.nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_init: NK/NTH differ from the DIA or ST4 context");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    GridCtx g;
    g.p = p;
    g.sig = to_device<DevReal1D>("ww_srce.sig", sig, p.nk);
    g.dden = to_device<DevReal1D>("ww_srce.dden", dden, p.nk);
    g.ecos = to_device<DevReal1D>("ww_srce.ecos", ecos, p.nspec);
    g.esin = to_device<DevReal1D>("ww_srce.esin", esin, p.nspec);
    g.mapwn = to_device<DevInt1D>("ww_srce.mapwn", mapwn, p.nspec);
    g.spec = DevReal1D("ww_srce.spec", p.nspec);
    g.wn1 = DevReal1D("ww_srce.wn1", p.nk);
    g.cg1 = DevReal1D("ww_srce.cg1", p.nk);
    g.whitecap = DevReal1D("ww_srce.whitecap", 4);
    g.taubbl = DevReal1D("ww_srce.taubbl", 2);
    g.tauice = DevReal1D("ww_srce.tauice", 2);
    g.work_reals = srce_work_reals(p, q);
    g.work_ints = srce_work_ints(p, q);
    g.rw = DevReal1D("ww_srce.rw", g.work_reals);
    g.iw = DevInt1D("ww_srce.iw", g.work_ints);
    g.dw = DevDouble1D("ww_srce.dw", srce_work_doubles(p));
    g.scal = Kokkos::View<PointScalars*, DeviceSpace>("ww_srce.scal", 1);
    g.state = Kokkos::View<State*, DeviceSpace>("ww_srce.state", 1);
    grids()[igrid] = std::move(g);
    if (!g_hook_pushed) {
      Kokkos::push_finalize_hook(release);
      g_hook_pushed = true;
      if (flags().count) std::atexit(print_count);
    }
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::bad_alloc& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
    return WW_KOKKOS_ERR_KERNEL;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce_init: unknown exception");
    return WW_KOKKOS_ERR_KERNEL;
  }
}

void ww_srce(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float* spec,
             const float* wn1, const float* cg1, float d_inp, float u10abs, float u10dir,
             float as, float ice, float dtg, float dair, float coef, float* ustar,
             float* ustdir, float* tauwx, float* tauwy, float* dtdyn, float* fcut,
             float* tauox, float* tauoy, float* tauwix, float* tauwiy, float* tauwnx,
             float* tauwny, float* phiaw, float* charn, float* tws, float* phioc,
             float* whitecap, float* phibbl, float* taubbl, float* tauice, float* phice,
             float* tauocx, float* tauocy, float* wnmean, int* shave) {
  try {
    const auto it_g = grids().find(igrid);
    if (it_g == grids().end() || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce before ww_srce_init for this grid");
      return;
    }
    GridCtx& g = it_g->second;
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    // The module state of ST4 (the DEL* scalars, TAUHFT...) is read at every
    // call, as ww_sds4() does: the last grid to register it wins, as in WW3.
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce: the DIA or ST4 context of this grid is gone");
      return;
    }
    const PointIn in{it, inflags24, flogrd57, flogrd58, d_inp, u10abs, u10dir, as,
                     ice, dtg, dair, coef};
#if defined(WW_SRCE_SERIAL_NESTED)
    ww::timers::Region t_call("srce.call", !nested_in_openmp());
#else
    ww::timers::Region t_call("srce.call");
#endif
    PointScalars o{*ustar,  *ustdir, *tauwx,  *tauwy, *dtdyn,  *fcut,  *tauox, *tauoy,
                   *tauwix, *tauwiy, *tauwnx, *tauwny, *phiaw, *charn, *tws,  *phioc,
                   *phibbl, *phice,  *tauocx, *tauocy, *wnmean, *shave};
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested_in_openmp())
      run<Kokkos::Serial>(g, q, f, nc, nt, nsig, in, spec, wn1, cg1, o, whitecap, taubbl, tauice);
    else
#endif
      run<Kokkos::DefaultExecutionSpace>(g, q, f, nc, nt, nsig, in, spec, wn1, cg1, o, whitecap,
                                         taubbl, tauice);
    *ustar = o.ustar;
    *ustdir = o.ustdir;
    *tauwx = o.tauwx;
    *tauwy = o.tauwy;
    *dtdyn = o.dtdyn;
    *fcut = o.fcut;
    *tauox = o.tauox;
    *tauoy = o.tauoy;
    *tauwix = o.tauwix;
    *tauwiy = o.tauwiy;
    *tauwnx = o.tauwnx;
    *tauwny = o.tauwny;
    *phiaw = o.phiaw;
    *charn = o.charn;
    *tws = o.tws;
    *phioc = o.phioc;
    *phibbl = o.phibbl;
    *phice = o.phice;
    *tauocx = o.tauocx;
    *tauocy = o.tauocy;
    *wnmean = o.wnmean;
    *shave = o.shave;
    g_count.fetch_add(1, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce: unknown exception");
  }
}

// ww_srce_batch, called by W3KOKKOS_SRCE_BATCH (w3kokkosmd.F90 on the fork) with
// its pack buffers; IP is the packed index of sea point JSEA (ISEA from
// INIT_GET_ISEA), the points in JSEA order.
// | C         | Fortran                          | Type          | Intent | Shape            |
// | igrid     | IGRID                            | int           | in     |                  |
// | it        | IT                               | int           | in     |                  |
// | inflags24 | INFLAGS2(4) as 0/1               | int           | in     |                  |
// | flogrd57  | FLOGRD(5,7) as 0/1               | int           | in     |                  |
// | flogrd58  | FLOGRD(5,8) as 0/1               | int           | in     |                  |
// | dtg       | DTG (W3WAVE's, 1. when IT = 0)   | float         | in     |                  |
// | npts      | active points of the step        | int           | in     |                  |
// | spec      | BSPEC(:,IP)  = VA(:,JSEA)        | float*        | inout  | nspec x npts     |
// | wn1       | BWN1(:,IP)   = WN(1:NK,ISEA)     | const float*  | in     | nk x npts        |
// | cg1       | BCG1(:,IP)   = CG(1:NK,ISEA)     | const float*  | in     | nk x npts        |
// | pin       | BPIN(:,IP): DW, U10, U10D, AS,   | const float*  | in     | NPIN x npts      |
// |           |   ICE, RHOAIR, ASF of ISEA       |               |        |                  |
// | psc       | BPSC(:,IP): UST, USTDIR (ISEA);  | float*        | inout  | NPSC x npts      |
// |           |   TAUWX .. WNMEAN (JSEA), in the |               |        |                  |
// |           |   order of WwSrceBatchScal       |               |        |                  |
// | whitecap  | BWC(:,IP) = WHITECAP(JSEA,1:4)   | float*        | inout  | 4 x npts         |
// | taubbl    | BTB(:,IP) = TAUBBL(JSEA,1:2)     | float*        | inout  | 2 x npts         |
// | tauice    | BTI(:,IP) = TAUICE(JSEA,1:2)     | float*        | inout  | 2 x npts         |
// | shave     | BSH(IP), SHAVEIO as 0/1          | int*          | out    | npts             |
void ww_srce_batch(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float dtg,
                   int npts, float* spec, const float* wn1, const float* cg1,
                   const float* pin, float* psc, float* whitecap, float* taubbl,
                   float* tauice, int* shave) {
  try {
    const auto it_g = grids().find(igrid);
    if (it_g == grids().end() || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce_batch before ww_srce_init for this grid");
      return;
    }
    if (npts < 0 || (npts > 0 && (spec == nullptr || wn1 == nullptr || cg1 == nullptr ||
                                  pin == nullptr || psc == nullptr || whitecap == nullptr ||
                                  taubbl == nullptr || tauice == nullptr || shave == nullptr))) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_batch: npts < 0 or a null array");
      return;
    }
    g_batch_calls.fetch_add(1, std::memory_order_relaxed);
    if (npts == 0) {
      ok();
      return;
    }
    GridCtx& g = it_g->second;
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    // Read at every call, as ww_srce() does (the ST4 module state).
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
                "ww_srce_batch: the DIA or ST4 context of this grid is gone");
      return;
    }
    const long long per_point = batch_bytes_per_point(g, true);
    long long fit = batch_cap_bytes() / per_point;
    if (fit < 1) fit = 1;
    const int chunk = static_cast<int>(fit < npts ? fit : npts);
    batch_reserve(g, chunk, true);
    const BatchStep c{it, inflags24, flogrd57, flogrd58, dtg};
#if defined(WW_SRCE_SERIAL_NESTED)
    ww::timers::Region t_call("srce.batch.call", !nested_in_openmp());
#else
    ww::timers::Region t_call("srce.batch.call");
#endif
    for (int n0 = 0; n0 < npts; n0 += chunk) {
      const int n = (npts - n0 < chunk) ? npts - n0 : chunk;
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested_in_openmp())
        run_batch<Kokkos::Serial>(g, q, f, nc, nt, nsig, c, n0, n, spec, wn1, cg1, pin, psc,
                                  whitecap, taubbl, tauice, shave);
      else
#endif
        run_batch<Kokkos::DefaultExecutionSpace>(g, q, f, nc, nt, nsig, c, n0, n, spec, wn1,
                                                 cg1, pin, psc, whitecap, taubbl, tauice, shave);
      g_batch_launches.fetch_add(1, std::memory_order_relaxed);
    }
    g_batch_points.fetch_add(npts, std::memory_order_relaxed);
    g_count.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce_batch: unknown exception");
  }
}

// ww_srce_batch_va (resident_c.hpp), called by W3KOKKOS_SRCE_BATCH on the fork branch
// ww3-gpu/kokkos-resident: ww_srce_batch with the spectra read and written in the resident
// VA (WFIP-0005 unit 2). The arguments are ww_srce_batch's, spec replaced by:
// | C         | Fortran                          | Type          | Intent | Shape            |
// | va        | VA(1,0) (W3WDATMD)               | float*        | inout  | nspec x nsealm+1 |
// | nspec     | SIZE(VA,1)                       | int           | in     |                  |
// | nsealm    | UBOUND(VA,2)                     | int           | in     |                  |
// | jsea      | BJSEA(IP) = JSEA of point IP     | const int*    | in     | npts             |
// The host VA is neither read nor written here: ww::resident::device_va copies it to the
// device only when the host holds the newer bits, and the state is DEVICE afterwards.
// The other arrays (WFIP-0005 unit 4): on a backend that reads host memory (Serial, OpenMP)
// the kernel reads and writes them in place, with no copy. On a device each keeps its batch
// buffer between calls and, when the step runs in one launch, is copied in only when its bits
// differ from those the buffer holds (a host shadow compared with memcmp; for PSC, WC, TB, TI,
// which the kernel writes, the bits copied back). So WN, CG (rewritten by W3ULEV), the
// forcing of BPIN and the packed state cross only on the steps where the host changed them,
// and no writer of them in WW3 has to be listed for the device copy to stay current. PSC, WC,
// TB, TI and SHAVE still come back at every call: W3KOKKOS_SRCE_BATCH unpacks them into
// W3ADATMD, which the output and the next step read on the host.
void ww_srce_batch_va(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float dtg,
                      float* va, int nspec, int nsealm, int npts, const int* jsea,
                      const float* wn1, const float* cg1, const float* pin, float* psc,
                      float* whitecap, float* taubbl, float* tauice, int* shave) {
  try {
    const auto it_g = grids().find(igrid);
    if (it_g == grids().end() || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_srce_batch_va before ww_srce_init for this grid");
      return;
    }
    if (npts < 0 || (npts > 0 && (va == nullptr || jsea == nullptr || wn1 == nullptr ||
                                  cg1 == nullptr || pin == nullptr || psc == nullptr ||
                                  whitecap == nullptr || taubbl == nullptr ||
                                  tauice == nullptr || shave == nullptr))) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_batch_va: npts < 0 or a null array");
      return;
    }
    GridCtx& g = it_g->second;
    if (nspec != g.p.nspec || npts > nsealm) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_batch_va: SIZE(VA,1) != NSPEC or NPTS > NSEALM");
      return;
    }
    for (int k = 0; k < npts; ++k) {
      if (jsea[k] < 1 || jsea[k] > nsealm) {
        set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_batch_va: a JSEA outside 1..NSEALM");
        return;
      }
    }
    g_batch_calls.fetch_add(1, std::memory_order_relaxed);
    if (npts == 0) {
      ok();
      return;
    }
    snl1::Config nc;
    snl1::Tables nt;
    snl1::RealView1D nsig;
    st4::Params q;
    st4::Fields f;
    // Read at every call, as ww_srce() does (the ST4 module state).
    if (!snl1::grid_state(igrid, nc, nt, nsig) || !st4::grid_state(igrid, q, f)) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
                "ww_srce_batch_va: the DIA or ST4 context of this grid is gone");
      return;
    }
#if defined(WW_SRCE_SERIAL_NESTED)
    ww::timers::Region t_call("srce.batch.call", !nested_in_openmp());
#else
    ww::timers::Region t_call("srce.batch.call");
#endif
    ww::resident::DevVa dva;
    const int rc = ww::resident::device_va(igrid, va, nspec, nsealm, dva);
    if (rc != WW_KOKKOS_OK) {
      set_error(rc, "ww_srce_batch_va: the resident VA refused this call");
      return;
    }
    if (dva.stride(1) != static_cast<size_t>(nspec)) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_srce_batch_va: the resident VA is padded");
      return;
    }
    const long long per_point = batch_bytes_per_point(g, false);
    long long fit = batch_cap_bytes() / per_point;
    if (fit < 1) fit = 1;
    const int chunk = static_cast<int>(fit < npts ? fit : npts);
    batch_reserve(g, chunk, false);
    const BatchStep c{it, inflags24, flogrd57, flogrd58, dtg};
    for (int n0 = 0; n0 < npts; n0 += chunk) {
      const int n = (npts - n0 < chunk) ? npts - n0 : chunk;
#if defined(WW_SRCE_SERIAL_NESTED)
      if (nested_in_openmp())
        run_batch<Kokkos::Serial>(g, q, f, nc, nt, nsig, c, n0, n, nullptr, wn1, cg1, pin, psc,
                                  whitecap, taubbl, tauice, shave, &dva, jsea);
      else
#endif
        run_batch<Kokkos::DefaultExecutionSpace>(g, q, f, nc, nt, nsig, c, n0, n, nullptr, wn1,
                                                 cg1, pin, psc, whitecap, taubbl, tauice, shave,
                                                 &dva, jsea, n0 == 0 && n == npts);
      g_batch_launches.fetch_add(1, std::memory_order_relaxed);
    }
    ww::resident::device_written(igrid);
    g_batch_points.fetch_add(npts, std::memory_order_relaxed);
    g_count.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_srce_batch_va: unknown exception");
  }
}

int ww_srce_enabled(void) { return flags().srce ? 1 : 0; }

int ww_srce_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
