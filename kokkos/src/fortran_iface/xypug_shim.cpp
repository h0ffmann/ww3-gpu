// kokkos/src/fortran_iface/xypug_shim.cpp
// The bind(C) shim of the xypug wiring unit (xypug_c.hpp, issue #196): W3WAVE section 3.6.3's
// ISPEC loop, W3GATH + W3XYPUG (FSN) + W3SCAT under W3_SHRD, on an unstructured grid, on the
// resident VA. Marshalling only; the arithmetic is ww_kokkos/ug_xypug.hpp.
//
// Memory contract. VA never crosses: the kernel reads and writes d_va (ww::resident::device_va
// syncs it first, and the state is DEVICE afterwards). Per grid (igrid), a context keeps the
// geometry on the device (TRIGP, IEN, SI, the node lists built from TRIGP and the node to sea
// point map built from MAPSF(:,3)), uploaded again only when a host shadow of TRIGP, IEN, SI or
// MAPSF(:,3) differs from the last call. The per-call inputs (CG, CX, CY, CLATS, ECOS, ESIN,
// MAPSTA, IOBP, IOBPD, IOBDP, IOBPA) are compared with a host shadow and copied when they differ
// (WW_KOKKOS_XYPUG_SKIP_H2D=0 copies them every call); the boundary arrays under FLBPI and ITER
// are copied every call; they are small next to VA. ITER is copied back whole after the launches: only the ITER(IK,ITH)
// of the components run under LCALC change. Work buffers are kept between calls and grown on
// demand, at most WW_KOKKOS_PROP3_BATCH_MIB MiB (default 1024, the prop3 cap); a step with more
// components than fit runs in several launches. No Unified Memory.
//
// Threads. One Kokkos team per component: of one thread on a space that reads host memory
// (Serial, OpenMP: the components fill the threads), of Kokkos' choice on a device. The bits do
// not depend on it (ug_xypug.hpp). A call inside an OpenMP region is refused, as prop3's resident
// entries refuse it: the resident context is not locked, and the caller in W3WAVE is outside one.
//
// The shim checks what the kernel relies on and the Fortran would have read out of bounds:
// TRIGP in 1..NX, MAPSF(:,3) in 1..NX and one to one, and under FLBPI ISBPI in 1..NSEA with
// MAPSF(ISBPI,1) in 1..NX.
//
// WW_KOKKOS_PROP3_COUNT=1 prints the xypug counters with the prop3 ones (prop3_shim.cpp calls
// ww_xypug_report); "W3XYPUG spectra computed: N" counts components, the line the regtest
// matrix sums (regtests/cmake/run_entry.cmake). WW_KOKKOS_TIMERS=1 times
// xypug.call/.h2d/.kernel/.d2h (timers.hpp).
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/xypug_c.hpp"
#include "fortran_iface/timers.hpp"

#include <Kokkos_Core.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include "ww_kokkos/resident_state.hpp"
#include "ww_kokkos/ug_xypug.hpp"

#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_XYPUG_OMP_CHECK 1
#include <omp.h>
#endif

namespace ww::xypugshim {
namespace {

using Exec = Kokkos::DefaultExecutionSpace;
using Mem = Exec::memory_space;
template <class T>
using HostUnmanaged =
    Kokkos::View<T*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

std::atomic<long long> g_calls, g_items, g_launches, g_substeps;

int fail(int code, const char* what) {
  std::fprintf(stderr, "ww_kokkos: xypug: %s (code %d)\n", what, code);
  return code;
}

bool nested_in_openmp() {
#if defined(WW_XYPUG_OMP_CHECK)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// The work cap in bytes: WW_KOKKOS_PROP3_BATCH_MIB as prop3 reads it, else 1024 MiB.
long long cap_bytes() {
  static const long long cap = [] {
    long long mib = 1024;
    if (const char* v = std::getenv("WW_KOKKOS_PROP3_BATCH_MIB")) {
      char* end = nullptr;
      const long long x = std::strtoll(v, &end, 10);
      if (end != v && *end == '\0' && x > 0 && x <= (1LL << 30)) mib = x;
    }
    return mib * 1024 * 1024;
  }();
  return cap;
}

/// One grid's state: the geometry and its host shadows, the per-call buffers, the work.
struct Ctx {
  std::vector<int> sh_trigp, sh_mapsf3;
  std::vector<double> sh_ien, sh_si;
  bool geom_valid = false;
  /// Host shadows of the per-call inputs as last uploaded (WW_KOKKOS_XYPUG_SKIP_H2D).
  std::vector<int> sh_mapsta, sh_iobp, sh_iobpd, sh_iobdp, sh_iobpa;
  std::vector<float> sh_clats, sh_ecos, sh_esin, sh_cg, sh_cx, sh_cy;
  ww::ug::XypugGrid<Mem> g;
  ww::ug::XypugStep<Mem> s;
  ww::ug::XypugWork<Mem> w;
};

/// Contexts by igrid, emptied by a Kokkos finalize hook (before Kokkos::finalize); never
/// destroyed, as prop3's buffers.
std::map<int, Ctx>& contexts() {
  static auto* m = [] {
    auto* p = new std::map<int, Ctx>();
    Kokkos::push_finalize_hook([p] { p->clear(); });
    return p;
  }();
  return *m;
}

std::mutex g_mutex;

/// A bit-neutral speed-up's switch, read once: "1" on, "0" off, unset or other the default.
/// Each is WW_KOKKOS_XYPUG_<NAME>; with a switch off the code is the translation as first
/// written (docs/log/2026-10-19-xypug-speedups.md).
bool speedup_flag(const char* name, bool def) {
  const char* v = std::getenv(name);
  if (v == nullptr) return def;
  if (std::strcmp(v, "1") == 0) return true;
  if (std::strcmp(v, "0") == 0) return false;
  return def;
}

template <class T>
void grow(Kokkos::View<T*, Mem>& v, size_t n, const char* name) {
  const size_t want = std::max<size_t>(n, 1);
  if (v.extent(0) < want) {
    v = Kokkos::View<T*, Mem>();
    v = Kokkos::View<T*, Mem>(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(name)),
                              want);
  }
}

/// Copies n elements of host array p into v (grown to fit).
template <class T>
void put(Kokkos::View<T*, Mem>& v, const T* p, size_t n, const char* name) {
  grow(v, n, name);
  if (n > 0)
    Kokkos::deep_copy(Exec(), Kokkos::subview(v, std::make_pair(size_t{0}, n)),
                      HostUnmanaged<const T>(p, n));
}

template <class T>
bool same(const std::vector<T>& sh, const T* p, size_t n) {
  return sh.size() == n && (n == 0 || std::memcmp(sh.data(), p, n * sizeof(T)) == 0);
}

/// put(), skipped when `skip` and the n elements equal (bit for bit, memcmp) the ones last
/// uploaded, kept in sh. The device copy then holds the same bits, so the kernel reads the same
/// values; a NaN or a -0 is compared by its bits.
template <class T>
void put_if_changed(Kokkos::View<T*, Mem>& v, std::vector<T>& sh, const T* p, size_t n,
                    const char* name, bool skip) {
  if (skip && v.extent(0) >= std::max<size_t>(n, 1) && same(sh, p, n)) return;
  put(v, p, n, name);
  if (skip) sh.assign(p, p + n);
}

/// put_if_changed() of tr(p) instead of p: the array as the kernel's layout switch reads it, made
/// only when the host array changed (or the skip is off).
template <class T, class Tr>
void put_t_if_changed(Kokkos::View<T*, Mem>& v, std::vector<T>& sh, const T* p, size_t n,
                      const char* name, bool skip, Tr tr) {
  if (skip && v.extent(0) >= std::max<size_t>(n, 1) && same(sh, p, n)) return;
  std::vector<T> t(n);
  tr(p, t.data());
  put(v, t.data(), n, name);
  if (skip) sh.assign(p, p + n);
}

/// The arguments as received, with IPAR/RPAR unpacked.
struct Args {
  int nx, ntri, nk, nth, nspec, nsea, nbi, flcur, flbpi, fsbccfl, lcalc;
  float facx, facy, dtg, rd1, rd2;
  const int *mapsf, *mapsta, *trigp, *iobp, *iobpd, *iobdp, *iobpa, *isbpi, *on;
  const float *clats, *ecos, *esin, *cg, *cx, *cy, *bbpi0, *bbpin;
  const double *ien, *si;
  int* iter;
};

/// Uploads the geometry when it changed, after checking it. Returns a status.
int upload_geometry(Ctx& c, const Args& a) {
  const size_t nt3 = 3 * static_cast<size_t>(a.ntri);
  const size_t nien = 6 * static_cast<size_t>(a.ntri);
  const int* mapsf3 = a.mapsf + 2 * static_cast<size_t>(a.nsea);
  if (c.geom_valid && c.g.nx == a.nx && c.g.ntri == a.ntri && same(c.sh_trigp, a.trigp, nt3) &&
      same(c.sh_ien, a.ien, nien) && same(c.sh_si, a.si, static_cast<size_t>(a.nx)) &&
      same(c.sh_mapsf3, mapsf3, static_cast<size_t>(a.nsea)))
    return WW_KOKKOS_OK;
  c.geom_valid = false;

  // The node lists: for IE = 1..NTRI, K = 1..3 in order, so each node's list is in ascending
  // IE then K (the Fortran's order of additions onto that node, ug_xypug.hpp).
  std::vector<int> off(static_cast<size_t>(a.nx) + 1, 0);
  for (size_t j = 0; j < nt3; ++j) {
    const int ip = a.trigp[j];
    if (ip < 1 || ip > a.nx) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "TRIGP outside 1..NX");
    ++off[static_cast<size_t>(ip)];
  }
  for (int p = 0; p < a.nx; ++p) off[p + 1] += off[p];
  std::vector<int> ent(nt3 > 0 ? nt3 : 1);
  std::vector<int> fill(off.begin(), off.end() - 1);
  for (size_t j = 0; j < nt3; ++j) ent[static_cast<size_t>(fill[a.trigp[j] - 1]++)] = static_cast<int>(j);

  std::vector<int> pos(nt3 > 0 ? nt3 : 1);  // the inverse of ent
  for (size_t j = 0; j < nt3; ++j) pos[static_cast<size_t>(ent[j])] = static_cast<int>(j);

  std::vector<int> isea_of(static_cast<size_t>(a.nx), 0);
  for (int isea = 1; isea <= a.nsea; ++isea) {
    const int ixy = mapsf3[isea - 1];
    if (ixy < 1 || ixy > a.nx) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "MAPSF(:,3) outside 1..NX");
    if (isea_of[ixy - 1] != 0)
      return fail(WW_KOKKOS_ERR_UNSUPPORTED, "MAPSF(:,3) not one to one");
    isea_of[ixy - 1] = isea;
  }

  c.g.nx = a.nx;
  c.g.ntri = a.ntri;
  put(c.g.trigp, a.trigp, nt3, "ww_xypug.trigp");
  put(c.g.ien, a.ien, nien, "ww_xypug.ien");
  put(c.g.si, a.si, static_cast<size_t>(a.nx), "ww_xypug.si");
  put(c.g.node_isea, isea_of.data(), isea_of.size(), "ww_xypug.node_isea");
  put(c.g.node_off, off.data(), off.size(), "ww_xypug.node_off");
  put(c.g.node_ent, ent.data(), nt3, "ww_xypug.node_ent");
  put(c.g.pos, pos.data(), nt3, "ww_xypug.pos");
  c.sh_trigp.assign(a.trigp, a.trigp + nt3);
  c.sh_ien.assign(a.ien, a.ien + nien);
  c.sh_si.assign(a.si, a.si + a.nx);
  c.sh_mapsf3.assign(mapsf3, mapsf3 + a.nsea);
  c.geom_valid = true;
  Exec().fence("ww_xypug_upload_geometry");  // the put() sources (off, ent, pos, isea_of) are locals
  return WW_KOKKOS_OK;
}

/// The call on d_va: inputs up, the launches, ITER down.
int batch(Ctx& c, const Args& a, float* dva) {
  std::vector<int> lst;
  lst.reserve(static_cast<size_t>(a.nspec));
  for (int isp = 1; isp <= a.nspec; ++isp)
    if (a.on[isp - 1] != 0) lst.push_back(isp);
  // The boundary list: ISBPI and MAPSF(ISBPI,1), read only under FLBPI (else dummies).
  std::vector<int> bisea, bip;
  if (a.flbpi != 0) {
    bisea.assign(a.isbpi, a.isbpi + a.nbi);
    bip.resize(static_cast<size_t>(a.nbi));
    for (int ibi = 0; ibi < a.nbi; ++ibi) {
      const int isb = bisea[static_cast<size_t>(ibi)];
      if (isb < 1 || isb > a.nsea) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ISBPI outside 1..NSEA");
      const int ip = a.mapsf[isb - 1];  // MAPSF(ISBPI(IBI),1)
      if (ip < 1 || ip > a.nx) return fail(WW_KOKKOS_ERR_BAD_SHAPE, "MAPSF(ISBPI,1) outside 1..NX");
      bip[static_cast<size_t>(ibi)] = ip;
    }
  }

  ww::timers::Region t_in("xypug.h2d");
  if (const int rc = upload_geometry(c, a); rc != WW_KOKKOS_OK) return rc;
  auto& s = c.s;
  s.nsea = a.nsea;
  s.nk = a.nk;
  s.nth = a.nth;
  s.nspec = a.nspec;
  s.nbi = a.flbpi != 0 ? a.nbi : 0;
  s.flcur = a.flcur;
  s.flbpi = a.flbpi;
  s.fsbccfl = a.fsbccfl;
  s.lcalc = a.lcalc;
  static const int fuse_ut = speedup_flag("WW_KOKKOS_XYPUG_FUSE_UT", true) ? 1 : 0;
  s.fuse_ut = fuse_ut;
  s.facx = a.facx;
  s.facy = a.facy;
  s.dtg = a.dtg;
  s.rd1 = a.rd1;
  s.rd2 = a.rd2;
  const size_t nx = static_cast<size_t>(a.nx);
  const size_t nsea = static_cast<size_t>(a.nsea);
  static const bool skip = speedup_flag("WW_KOKKOS_XYPUG_SKIP_H2D", true);
  put_if_changed(s.mapsta, c.sh_mapsta, a.mapsta, nx, "ww_xypug.mapsta", skip);
  put_if_changed(s.clats, c.sh_clats, a.clats, nsea, "ww_xypug.clats", skip);
  put_if_changed(s.ecos, c.sh_ecos, a.ecos, static_cast<size_t>(a.nth), "ww_xypug.ecos", skip);
  put_if_changed(s.esin, c.sh_esin, a.esin, static_cast<size_t>(a.nth), "ww_xypug.esin", skip);
  static const int layout = speedup_flag("WW_KOKKOS_XYPUG_LAYOUT", true) ? 1 : 0;
  s.layout = layout;
  static const int cfl_par = speedup_flag("WW_KOKKOS_XYPUG_CFL_PAR", true) ? 1 : 0;
  s.cfl_par = cfl_par;
  const size_t ncg = static_cast<size_t>(a.nk + 2) * (nsea + 1);
  if (layout != 0) {
    // CG(IK,ISEA) as cg[IK*(NSEA+1) + ISEA]; IOBPD(ITH,IP) as iobpd[ITH*NX + IP]: same values.
    const size_t nk2 = static_cast<size_t>(a.nk) + 2;
    put_t_if_changed(s.cg, c.sh_cg, a.cg, ncg, "ww_xypug.cg", skip, [&](const float* p, float* o) {
      for (size_t isea = 0; isea <= nsea; ++isea)
        for (size_t ik = 0; ik < nk2; ++ik) o[ik * (nsea + 1) + isea] = p[isea * nk2 + ik];
    });
    const size_t nth = static_cast<size_t>(a.nth);
    put_t_if_changed(s.iobpd, c.sh_iobpd, a.iobpd, nth * nx, "ww_xypug.iobpd", skip,
                     [&](const int* p, int* o) {
                       for (size_t ip = 0; ip < nx; ++ip)
                         for (size_t ith = 0; ith < nth; ++ith) o[ith * nx + ip] = p[ip * nth + ith];
                     });
  } else {
    put_if_changed(s.cg, c.sh_cg, a.cg, ncg, "ww_xypug.cg", skip);
    put_if_changed(s.iobpd, c.sh_iobpd, a.iobpd, static_cast<size_t>(a.nth) * nx,
                   "ww_xypug.iobpd", skip);
  }
  put_if_changed(s.cx, c.sh_cx, a.cx, nsea + 1, "ww_xypug.cx", skip);
  put_if_changed(s.cy, c.sh_cy, a.cy, nsea + 1, "ww_xypug.cy", skip);
  put_if_changed(s.iobp, c.sh_iobp, a.iobp, nx, "ww_xypug.iobp", skip);
  put_if_changed(s.iobdp, c.sh_iobdp, a.iobdp, nx, "ww_xypug.iobdp", skip);
  put_if_changed(s.iobpa, c.sh_iobpa, a.iobpa, nx, "ww_xypug.iobpa", skip);
  put(s.iter, a.iter, static_cast<size_t>(a.nk) * static_cast<size_t>(a.nth), "ww_xypug.iter");
  put(s.lst, lst.data(), lst.size(), "ww_xypug.lst");
  if (a.flbpi != 0) {
    const size_t nb = static_cast<size_t>(a.nspec) * (static_cast<size_t>(a.nbi) + 1);
    put(s.bnd_isea, bisea.data(), bisea.size(), "ww_xypug.bnd_isea");
    put(s.bnd_ip, bip.data(), bip.size(), "ww_xypug.bnd_ip");
    put(s.bbpi0, a.bbpi0, nb, "ww_xypug.bbpi0");
    put(s.bbpin, a.bbpin, nb, "ww_xypug.bbpin");
  }
  t_in.stop();

  const int m = static_cast<int>(lst.size());
  const long long per =
      static_cast<long long>(ww::ug::XypugWork<Mem>::bytes_per_component(a.nx, a.ntri));
  const int chunk = static_cast<int>(
      std::max(1LL, std::min<long long>(m, cap_bytes() / std::max(1LL, per))));
  constexpr bool host = Kokkos::SpaceAccessibility<Exec, Kokkos::HostSpace>::accessible;
  const int team = host ? 1 : 0;
  long long launches = 0;
  if (m > 0) {
    c.w.reserve(a.nx, a.ntri, std::min(chunk, m));
    ww::timers::Region t_k("xypug.kernel");
    for (int k0 = 0; k0 < m; k0 += chunk) {
      ww::ug::xypug_launch(Exec(), c.g, s, c.w, dva, k0, std::min(chunk, m - k0), team);
      ++launches;
    }
    t_k.stop();
  }

  ww::timers::Region t_out("xypug.d2h");
  const size_t niter = static_cast<size_t>(a.nk) * static_cast<size_t>(a.nth);
  if (niter > 0)
    Kokkos::deep_copy(Exec(), HostUnmanaged<int>(a.iter, niter),
                      Kokkos::subview(s.iter, std::make_pair(size_t{0}, niter)));
  Exec().fence("ww_xypug_batch_va");
  t_out.stop();

  long long sub = 0;
  for (const int isp : lst) {
    const int ith = 1 + (isp - 1) % a.nth;
    const int ik = 1 + (isp - 1) / a.nth;
    sub += std::max(0, a.iter[(ith - 1) * a.nk + ik - 1]);
  }
  g_calls.fetch_add(1, std::memory_order_relaxed);
  g_items.fetch_add(m, std::memory_order_relaxed);
  g_launches.fetch_add(launches, std::memory_order_relaxed);
  g_substeps.fetch_add(sub, std::memory_order_relaxed);
  return WW_KOKKOS_OK;
}

}  // namespace
}  // namespace ww::xypugshim

extern "C" {

int ww_xypug_batch_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar,
                      const int* mapsf, const int* mapsta, const float* clats, const float* ecos,
                      const float* esin, const float* cg, const float* cx, const float* cy,
                      const int* trigp, const double* ien, const double* si, const int* iobp,
                      const int* iobpd, const int* iobdp, const int* iobpa, const int* isbpi,
                      const float* bbpi0, const float* bbpin, const int* on, int* iter,
                      float* va, int nspec, int nsealm) {
  using namespace ww::xypugshim;
  if (ipar == nullptr || rpar == nullptr || nipar != WW_XYPUG_NIPAR || nrpar != WW_XYPUG_NRPAR)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_xypug_batch_va: bad packet");
  Args a{};
  a.nx = ipar[0];
  a.ntri = ipar[1];
  a.nk = ipar[2];
  a.nth = ipar[3];
  a.nspec = ipar[4];
  a.nsea = ipar[5];
  a.nbi = ipar[6];
  a.flcur = ipar[7];
  a.flbpi = ipar[8];
  a.fsbccfl = ipar[9];
  a.lcalc = ipar[10];
  a.facx = rpar[0];
  a.facy = rpar[1];
  a.dtg = rpar[2];
  a.rd1 = rpar[3];
  a.rd2 = rpar[4];
  a.mapsf = mapsf;
  a.mapsta = mapsta;
  a.trigp = trigp;
  a.iobp = iobp;
  a.iobpd = iobpd;
  a.iobdp = iobdp;
  a.iobpa = iobpa;
  a.isbpi = isbpi;
  a.on = on;
  a.clats = clats;
  a.ecos = ecos;
  a.esin = esin;
  a.cg = cg;
  a.cx = cx;
  a.cy = cy;
  a.bbpi0 = bbpi0;
  a.bbpin = bbpin;
  a.ien = ien;
  a.si = si;
  a.iter = iter;
  if (a.nx < 1 || a.ntri < 0 || a.nk < 1 || a.nth < 1 || a.nsea < 1 || a.nsea > a.nx ||
      a.nspec != a.nk * a.nth || nspec != a.nspec || nsealm < a.nsea || mapsf == nullptr ||
      mapsta == nullptr || clats == nullptr || ecos == nullptr || esin == nullptr ||
      cg == nullptr || cx == nullptr || cy == nullptr || (a.ntri > 0 && (trigp == nullptr ||
      ien == nullptr)) || si == nullptr || iobp == nullptr || iobpd == nullptr ||
      iobdp == nullptr || iobpa == nullptr || on == nullptr || iter == nullptr || va == nullptr ||
      (a.flbpi != 0 && (a.nbi < 0 || (a.nbi > 0 && isbpi == nullptr) || bbpi0 == nullptr ||
                        bbpin == nullptr)))
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_xypug_batch_va: bad sizes or null pointer");
  try {
    if (!Kokkos::is_initialized())
      return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    if (nested_in_openmp())
      return fail(WW_KOKKOS_ERR_UNSUPPORTED, "the resident VA inside an OpenMP region");
    std::lock_guard<std::mutex> lock(g_mutex);
    ww::timers::Region t("xypug.call");
    ww::resident::DevVa dva;
    const int rc = ww::resident::device_va(igrid, va, nspec, nsealm, dva);
    if (rc != WW_KOKKOS_OK) return fail(rc, "the resident VA refused this call");
    if (dva.stride(1) != static_cast<size_t>(nspec))
      return fail(WW_KOKKOS_ERR_BAD_SHAPE, "the resident VA is padded");
    const int st = batch(contexts()[igrid], a, dva.data());
    // The launches wrote d_va; on a failure the model stops, so the state is moot.
    ww::resident::device_written(igrid);
    return st;
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}

void ww_xypug_report(void) {
  using namespace ww::xypugshim;
  std::fprintf(stderr, "ww_kokkos: W3XYPUG batched calls on the resident VA: %lld\n",
               g_calls.load());
  std::fprintf(stderr, "ww_kokkos: W3XYPUG batched launches: %lld\n", g_launches.load());
  std::fprintf(stderr, "ww_kokkos: W3XYPUG spectra computed: %lld\n", g_items.load());
  std::fprintf(stderr, "ww_kokkos: W3XYPFSN2 sub-steps: %lld\n", g_substeps.load());
}

}  // extern "C"
