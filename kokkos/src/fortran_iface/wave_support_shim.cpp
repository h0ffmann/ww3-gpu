// kokkos/src/fortran_iface/wave_support_shim.cpp
// The bind(C) shim of W3WAVE's per-step support routines: the C ABI of
// ww_kokkos_c.hpp ("W3WAVE support") over the headers of
// docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md. Marshalling only: every entry point
// copies its Fortran arrays to the device, calls the translated launcher, and
// copies the results back (phase 1, no Unified Memory, nothing kept between calls).
//
// The caller is W3WAVE (w3wavemd.F90) or W3INIT (DISTAB), through the wrappers
// W3KOKKOS_<R> of w3kokkosmd.F90, each inside !$OMP CRITICAL (WW_KOKKOS_WAVE). The
// shim holds no grid state, so no per-grid context: every call carries its sizes.
// Inside an OpenMP parallel region (host builds) the kernels run on Kokkos::Serial,
// as in the other shims. Nothing throws across the boundary; every entry point
// returns a WwKokkosStatus.
//
// Two marshalling choices change no arithmetic and are worth knowing:
//  * W3GATH/W3SCAT move one spectral component. The shim copies the row
//    VA(ISPEC, 0:NSEA) into a (1, NSEA+1) View and calls the launcher with ISPEC = 1,
//    so a call costs O(NSEA), not O(NSPEC*NSEA). The kernel is a pure copy, and the
//    row holds the same bits as VA.
//  * W3UTRN reads TRNY(0) with TRFLAG = 1, 3, 5 when the cell IXY = 1 is a sea point
//    (w3updtmd.F90:2954 at the pin, outside the array, as the Fortran does). The shim
//    reads trny[-1], the same address the Fortran reads, only in that case.
//
// WW_KOKKOS_WAVE_COUNT=1 prints "ww_kokkos: W3<R> calls: N" per routine at exit,
// the evidence that a run exercised the port.
//
// SPDX-License-Identifier: MIT

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
#include <vector>

#include "ww_kokkos/disp_distab.hpp"
#include "ww_kokkos/prop_map3.hpp"
#include "ww_kokkos/prop_mapt.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/updt_dzxy.hpp"
#include "ww_kokkos/updt_utrn.hpp"
#include "ww_kokkos/updt_uwnd.hpp"
#include "ww_kokkos/wave_gath_scat.hpp"

#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_WAVE_SERIAL_NESTED 1
#include <omp.h>
#endif

namespace ww::waveshim {
namespace {

using ww::Real;
using Host = Kokkos::HostSpace;
template <class T>
using Borrowed = Kokkos::View<T*, Kokkos::LayoutLeft, Host, Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

enum Routine { GATH, SCAT, UWND, UTRN, DZXY, MAP3, MAPT, DISTAB, NROUTINES };
const char* const kName[NROUTINES] = {"W3GATH", "W3SCAT", "W3UWND", "W3UTRN",
                                      "W3DZXY", "W3MAP3", "W3MAPT", "DISTAB"};
const char* const kRegion[NROUTINES] = {"wave.gath.call", "wave.scat.call", "wave.uwnd.call",
                                        "wave.utrn.call", "wave.dzxy.call", "wave.map3.call",
                                        "wave.mapt.call", "wave.distab.call"};
std::atomic<long long> g_calls[NROUTINES];

void print_counts() {
  for (int r = 0; r < NROUTINES; ++r)
    std::fprintf(stderr, "ww_kokkos: %s calls: %lld\n", kName[r], g_calls[r].load());
}

/// Counts one call; registers the exit print once when WW_KOKKOS_WAVE_COUNT=1.
void count(Routine r) {
  static const bool on = [] {
    const char* v = std::getenv("WW_KOKKOS_WAVE_COUNT");
    const bool c = v != nullptr && std::strcmp(v, "1") == 0;
    if (c) std::atexit(print_counts);
    return c;
  }();
  (void)on;
  g_calls[r].fetch_add(1, std::memory_order_relaxed);
}

int fail(int code, const char* what) {
  std::fprintf(stderr, "ww_kokkos: W3WAVE support: %s (code %d)\n", what, code);
  return code;
}

bool nested_in_openmp() {
#if defined(WW_WAVE_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// A View of `n` (or n0 x n1) elements on E's memory space filled from caller memory
/// p (a bitwise copy), and the copy back. LayoutLeft, Fortran order.
template <class T, class E>
Kokkos::View<T*, Kokkos::LayoutLeft, E> in1(const E& e, const char* name, const T* p, size_t n) {
  Kokkos::View<T*, Kokkos::LayoutLeft, E> d(Kokkos::view_alloc(e, std::string(name)), n);
  Kokkos::deep_copy(e, d, Borrowed<const T>(p, n));
  return d;
}
template <class T, class E>
Kokkos::View<T**, Kokkos::LayoutLeft, E> in2(const E& e, const char* name, const T* p, size_t n0,
                                             size_t n1) {
  Kokkos::View<T**, Kokkos::LayoutLeft, E> d(Kokkos::view_alloc(e, std::string(name)), n0, n1);
  Kokkos::View<const T**, Kokkos::LayoutLeft, Host, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(
      p, n0, n1);
  Kokkos::deep_copy(e, d, h);
  return d;
}
template <class V, class T, class E>
void out(const E& e, const V& d, T* p) {
  Kokkos::View<T*, Kokkos::LayoutLeft, Host, Kokkos::MemoryTraits<Kokkos::Unmanaged>> h(p, d.size());
  auto flat = Kokkos::View<T*, Kokkos::LayoutLeft, typename V::device_type,
                           Kokkos::MemoryTraits<Kokkos::Unmanaged>>(d.data(), d.size());
  Kokkos::deep_copy(e, h, flat);
  e.fence("ww_wave copy out");
}

/// Runs f(E()) on Kokkos::Serial inside an OpenMP region, else on the default space,
/// and turns an exception into a status.
template <class F>
int run(Routine r, const F& f) {
  try {
    if (!Kokkos::is_initialized()) return fail(WW_KOKKOS_ERR_NOT_INITIALISED, "before ww_kokkos_init");
    int rc;
    ww::timers::Region t(kRegion[r], !nested_in_openmp());
#if defined(WW_WAVE_SERIAL_NESTED)
    if (nested_in_openmp())
      rc = f(Kokkos::Serial());
    else
#endif
      rc = f(Kokkos::DefaultExecutionSpace());
    if (rc == WW_KOKKOS_OK) count(r);
    return rc;
  } catch (const std::bad_alloc& e) {
    return fail(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    return fail(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    return fail(WW_KOKKOS_ERR_KERNEL, "unknown exception");
  }
}

}  // namespace
}  // namespace ww::waveshim

using namespace ww::waveshim;

extern "C" {

// ww_wave_gath: W3GATH(ISPEC, FIELD), W3_SHRD (w3wavemd.F90, W3GATH).
// | C      | Fortran                 | Type          | Intent | Shape            |
// | ispec  | ISPEC                   | int           | in     | 1..NSPEC         |
// | nspec  | NSPEC                   | int           | in     | rows of VA       |
// | nsea   | NSEA                    | int           | in     |                  |
// | ncol   | SIZE(VA,2) = NSEALM+1   | int           | in     | columns of VA    |
// | msea   | SIZE(MAPSF,1)           | int           | in     | rows of MAPSF    |
// | nx, ny | NX, NY                  | int           | in     |                  |
// | va     | VA(NSPEC,0:NSEALM)      | const float*  | in     | nspec x ncol     |
// | mapsf  | MAPSF(MSEA,3)           | const int*    | in     | msea x 3         |
// | field  | FIELD(1-NY:NY*(NX+2))   | float*        | out    | NY*(NX+3)        |
int ww_wave_gath(int ispec, int nspec, int nsea, int ncol, int msea, int nx, int ny,
                 const float* va, const int* mapsf, float* field) {
  if (ispec < 1 || ispec > nspec || nsea < 0 || ncol < nsea + 1 || msea < nsea || nx < 1 ||
      ny < 1 || va == nullptr || mapsf == nullptr || field == nullptr)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_gath: bad sizes or null pointer");
  return run(GATH, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    const size_t nf = static_cast<size_t>(ny) * static_cast<size_t>(nx + 3);
    std::vector<Real> row(static_cast<size_t>(nsea) + 1);
    for (int isea = 0; isea <= nsea; ++isea)
      row[static_cast<size_t>(isea)] =
          va[static_cast<size_t>(isea) * static_cast<size_t>(nspec) + static_cast<size_t>(ispec - 1)];
    auto drow = in2(e, "ww_wave.va_row", row.data(), 1, static_cast<size_t>(nsea) + 1);
    auto dmap = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    Kokkos::View<Real*, Kokkos::LayoutLeft, E> dfield(Kokkos::view_alloc(e, std::string("ww_wave.field")), nf);
    ww::wave::w3gath<E>(e, 1, nsea, ny, drow, dmap, dfield);
    out(e, dfield, field);
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_scat: W3SCAT(ISPEC, MAPSTA, FIELD), W3_SHRD. As ww_wave_gath, and:
// | mapsta | MAPSTA(NY*NX) dummy     | const int*    | in     | NY*NX            |
// | field  | FIELD(1-NY:NY*(NX+2))   | const float*  | in     | NY*(NX+3)        |
// | va     | VA(NSPEC,0:NSEALM)      | float*        | inout  | row ISPEC, 1:NSEA written |
int ww_wave_scat(int ispec, int nspec, int nsea, int ncol, int msea, int nx, int ny,
                 const int* mapsta, const float* field, const int* mapsf, float* va) {
  if (ispec < 1 || ispec > nspec || nsea < 0 || ncol < nsea + 1 || msea < nsea || nx < 1 ||
      ny < 1 || va == nullptr || mapsf == nullptr || field == nullptr || mapsta == nullptr)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_scat: bad sizes or null pointer");
  return run(SCAT, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    const size_t nf = static_cast<size_t>(ny) * static_cast<size_t>(nx + 3);
    const size_t ns = static_cast<size_t>(nspec);
    std::vector<Real> row(static_cast<size_t>(nsea) + 1);
    for (int isea = 0; isea <= nsea; ++isea)
      row[static_cast<size_t>(isea)] = va[static_cast<size_t>(isea) * ns + static_cast<size_t>(ispec - 1)];
    auto drow = in2(e, "ww_wave.va_row", row.data(), 1, static_cast<size_t>(nsea) + 1);
    auto dmap = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    auto dsta = in1(e, "ww_wave.mapsta", mapsta, static_cast<size_t>(nx) * static_cast<size_t>(ny));
    auto dfield = in1(e, "ww_wave.field", field, nf);
    ww::wave::w3scat<E>(e, 1, nsea, ny, dsta, dfield, dmap, drow);
    out(e, drow, row.data());
    for (int isea = 1; isea <= nsea; ++isea)
      va[static_cast<size_t>(isea) * ns + static_cast<size_t>(ispec - 1)] = row[static_cast<size_t>(isea)];
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_uwnd: W3UWND(FLFRST, VGX, VGY) under WNT1 (+ RWND), w3updtmd.F90.
// | C            | Fortran                          | Type         | Intent | Shape     |
// | flfrst       | FLFRST                           | int 0/1      | in     |           |
// | dt0n, dt0t   | DSEC21(TW0,TWN), DSEC21(TW0,TIME)| float        | in     | as stored |
// | flcur        | FLCUR (0 without RWND)           | int 0/1      | in     |           |
// | rwindc       | RWINDC (0 without RWND)          | float        | in     |           |
// | nsea, msea   | NSEA, SIZE(MAPSF,1)              | int          | in     |           |
// | nx, ny       | NX, NY                           | int          | in     |           |
// | mapsf        | MAPSF(MSEA,3)                    | const int*   | in     | msea x 3  |
// | wx0..dtn     | WX0, WY0, DT0, WXN, WYN, DTN     | const float* | in     | NX x NY   |
// | cx, cy       | CX(1:NSEA), CY(1:NSEA)           | const float* | in     | NSEA      |
// | ua0..asi     | UA0, UAI, UD0, UDI, AS0, ASI     | float*       | inout  | NSEA      |
// | ua, ud, as   | UA(1:NSEA), UD(1:NSEA), AS(1:NSEA)| float*      | out    | NSEA      |
// | u10, u10d    | U10, U10D                        | float*       | out    | NSEA      |
int ww_wave_uwnd(int flfrst, float dt0n, float dt0t, int flcur, float rwindc, int nsea, int msea,
                 int nx, int ny, const int* mapsf, const float* wx0, const float* wy0,
                 const float* dt0, const float* wxn, const float* wyn, const float* dtn,
                 const float* cx, const float* cy, float* ua0, float* uai, float* ud0,
                 float* udi, float* as0, float* asi, float* ua, float* ud, float* as,
                 float* u10, float* u10d) {
  if (nsea < 1 || msea < nsea || nx < 1 || ny < 1 || !mapsf || !wx0 || !wy0 || !dt0 || !wxn ||
      !wyn || !dtn || !cx || !cy || !ua0 || !uai || !ud0 || !udi || !as0 || !asi || !ua || !ud ||
      !as || !u10 || !u10d)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_uwnd: bad sizes or null pointer");
  return run(UWND, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    const size_t n = static_cast<size_t>(nsea), mx = static_cast<size_t>(nx),
                 my = static_cast<size_t>(ny);
    auto dmap = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    auto dwx0 = in2(e, "ww_wave.wx0", wx0, mx, my);
    auto dwy0 = in2(e, "ww_wave.wy0", wy0, mx, my);
    auto ddt0 = in2(e, "ww_wave.dt0", dt0, mx, my);
    auto dwxn = in2(e, "ww_wave.wxn", wxn, mx, my);
    auto dwyn = in2(e, "ww_wave.wyn", wyn, mx, my);
    auto ddtn = in2(e, "ww_wave.dtn", dtn, mx, my);
    auto dcx = in1(e, "ww_wave.cx", cx, n);
    auto dcy = in1(e, "ww_wave.cy", cy, n);
    // UA0..ASI are read when FLFRST is false: copied in either way.
    auto dua0 = in1(e, "ww_wave.ua0", ua0, n);
    auto duai = in1(e, "ww_wave.uai", uai, n);
    auto dud0 = in1(e, "ww_wave.ud0", ud0, n);
    auto dudi = in1(e, "ww_wave.udi", udi, n);
    auto das0 = in1(e, "ww_wave.as0", as0, n);
    auto dasi = in1(e, "ww_wave.asi", asi, n);
    auto dua = in1(e, "ww_wave.ua", ua, n);
    auto dud = in1(e, "ww_wave.ud", ud, n);
    auto das = in1(e, "ww_wave.as", as, n);
    auto du10 = in1(e, "ww_wave.u10", u10, n);
    auto du10d = in1(e, "ww_wave.u10d", u10d, n);
    ww::updt::w3uwnd<E>(e, flfrst != 0, dt0n, dt0t, flcur != 0, rwindc, dmap, dwx0, dwy0, ddt0, dwxn,
                     dwyn, ddtn, dcx, dcy, dua0, duai, dud0, dudi, das0, dasi, dua, dud, das,
                     du10, du10d);
    out(e, dua0, ua0);
    out(e, duai, uai);
    out(e, dud0, ud0);
    out(e, dudi, udi);
    out(e, das0, as0);
    out(e, dasi, asi);
    out(e, dua, ua);
    out(e, dud, ud);
    out(e, das, as);
    out(e, du10, u10);
    out(e, du10d, u10d);
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_utrn: W3UTRN(TRNX, TRNY) under IC0, w3updtmd.F90.
// | C             | Fortran                     | Type         | Intent | Shape        |
// | nx, ny, nsea  | NX, NY, NSEA                | int          | in     |              |
// | msea          | SIZE(MAPSF,1)               | int          | in     |              |
// | trflag        | TRFLAG                      | int          | in     |              |
// | fice0, ficen, ficel, ffacberg | same names  | float        | in     |              |
// | flagll        | FLAGLL                      | int 0/1      | in     |              |
// | trnx, trny    | TRNX(NY*NX), TRNY(NY*NX)    | const float* | in     | NY*NX; trny[-1] read as the Fortran's TRNY(0) |
// | mapsf         | MAPSF(MSEA,3)               | const int*   | in     | msea x 3     |
// | mapsta        | MAPSTA(NY,NX)               | const int*   | in     | NY x NX      |
// | hpfac, hqfac  | HPFAC(NY,NX), HQFAC(NY,NX)  | const float* | in     | NY x NX      |
// | ice           | ICE(0:NSEA)                 | const float* | in     | NSEA+1       |
// | berg          | BERG(NSEA)                  | const float* | in     | NSEA         |
// | atrnx, atrny  | ATRNX(NY*NX,-1:1), ATRNY    | float*       | out    | NY*NX x 3    |
int ww_wave_utrn(int nx, int ny, int nsea, int msea, int trflag, float fice0, float ficen,
                 float ficel, float ffacberg, int flagll, const float* trnx, const float* trny,
                 const int* mapsf, const int* mapsta, const float* hpfac, const float* hqfac,
                 const float* ice, const float* berg, float* atrnx, float* atrny) {
  if (nx < 1 || ny < 1 || nsea < 0 || msea < nsea || !trnx || !trny || !mapsf || !mapsta ||
      !hpfac || !hqfac || !ice || !berg || !atrnx || !atrny)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_utrn: bad sizes or null pointer");
  return run(UTRN, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    using M = typename E::memory_space;
    const size_t nc = static_cast<size_t>(nx) * static_cast<size_t>(ny);
    ww::updt::UtrnScalars s;
    s.nx = nx;
    s.ny = ny;
    s.nsea = nsea;
    s.trflag = trflag;
    s.fice0 = fice0;
    s.ficen = ficen;
    s.ficel = ficel;
    s.ffacberg = ffacberg;
    s.flagll = flagll != 0;
    if (trflag == 1 || trflag == 3 || trflag == 5) {
      for (int isea = 0; isea < nsea; ++isea) {
        if (mapsf[static_cast<size_t>(2) * static_cast<size_t>(msea) + static_cast<size_t>(isea)] == 1) {
          s.trny_before = trny[-1];  // the Fortran's TRNY(0), w3updtmd.F90:2954
          break;
        }
      }
    }
    ww::updt::UtrnViews<M> v;
    v.trnx = in1(e, "ww_wave.trnx", trnx, nc);
    v.trny = in1(e, "ww_wave.trny", trny, nc);
    v.ice = in1(e, "ww_wave.ice", ice, static_cast<size_t>(nsea) + 1);
    v.berg = in1(e, "ww_wave.berg", berg, static_cast<size_t>(nsea > 0 ? nsea : 1));
    v.mapsf = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    v.mapsta = in2(e, "ww_wave.mapsta", mapsta, static_cast<size_t>(ny), static_cast<size_t>(nx));
    v.hpfac = in2(e, "ww_wave.hpfac", hpfac, static_cast<size_t>(ny), static_cast<size_t>(nx));
    v.hqfac = in2(e, "ww_wave.hqfac", hqfac, static_cast<size_t>(ny), static_cast<size_t>(nx));
    v.atrnx = Kokkos::View<Real**, Kokkos::LayoutLeft, M>(Kokkos::view_alloc(e, std::string("ww_wave.atrnx")), nc, 3);
    v.atrny = Kokkos::View<Real**, Kokkos::LayoutLeft, M>(Kokkos::view_alloc(e, std::string("ww_wave.atrny")), nc, 3);
    v.trix = Kokkos::View<Real*, Kokkos::LayoutLeft, M>(Kokkos::view_alloc(e, std::string("ww_wave.trix")), nc);
    v.triy = Kokkos::View<Real*, Kokkos::LayoutLeft, M>(Kokkos::view_alloc(e, std::string("ww_wave.triy")), nc);
    ww::updt::w3utrn<E>(e, s, v);
    out(e, v.atrnx, atrnx);
    out(e, v.atrny, atrny);
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_dzxy: W3DZXY(ZZ, ZUNIT, DZZDX, DZZDY), w3updtmd.F90; ZUNIT is read only by W3_T.
// | C            | Fortran                      | Type         | Intent | Shape   |
// | nsea, nx, ny | NSEA, NX, NY                 | int          | in     |         |
// | zz           | ZZ(NSEA)                     | const float* | in     | NSEA    |
// | mapsta, mapfs| MAPSTA(NY,NX), MAPFS(NY,NX)  | const int*   | in     | NY x NX |
// | dpdx..dqdy   | DPDX, DPDY, DQDX, DQDY       | const float* | in     | NY x NX |
// | flagll       | FLAGLL                       | int 0/1      | in     |         |
// | iclose       | ICLOSE                       | int          | in     |         |
// | dzzdx, dzzdy | DZZDX(NY,NX), DZZDY(NY,NX)   | float*       | out    | NY x NX |
int ww_wave_dzxy(int nsea, int nx, int ny, const float* zz, const int* mapsta, const int* mapfs,
                 const float* dpdx, const float* dpdy, const float* dqdx, const float* dqdy,
                 int flagll, int iclose, float* dzzdx, float* dzzdy) {
  if (nsea < 1 || nx < 1 || ny < 1 || !zz || !mapsta || !mapfs || !dpdx || !dpdy || !dqdx ||
      !dqdy || !dzzdx || !dzzdy)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_dzxy: bad sizes or null pointer");
  return run(DZXY, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    const size_t my = static_cast<size_t>(ny), mx = static_cast<size_t>(nx);
    auto dzz = in1(e, "ww_wave.zz", zz, static_cast<size_t>(nsea));
    auto dsta = in2(e, "ww_wave.mapsta", mapsta, my, mx);
    auto dfs = in2(e, "ww_wave.mapfs", mapfs, my, mx);
    auto dpx = in2(e, "ww_wave.dpdx", dpdx, my, mx);
    auto dpy = in2(e, "ww_wave.dpdy", dpdy, my, mx);
    auto dqx = in2(e, "ww_wave.dqdx", dqdx, my, mx);
    auto dqy = in2(e, "ww_wave.dqdy", dqdy, my, mx);
    Kokkos::View<Real**, Kokkos::LayoutLeft, E> gx(Kokkos::view_alloc(e, std::string("ww_wave.dzzdx")), my, mx);
    Kokkos::View<Real**, Kokkos::LayoutLeft, E> gy(Kokkos::view_alloc(e, std::string("ww_wave.dzzdy")), my, mx);
    ww::updt::w3dzxy<E>(e, dzz, dsta, dfs, dpx, dpy, dqx, dqy, flagll != 0, iclose, gx, gy);
    out(e, gx, dzzdx);
    out(e, gy, dzzdy);
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_map3: W3MAP3 (w3pro3md.F90), GTYPE < 3 only (the caller keeps the Fortran
// otherwise, where MAPCXY may not be allocated).
// | C              | Fortran                     | Type       | Intent | Shape         |
// | nx, ny, nsea   | NX, NY, NSEA                | int        | in     |               |
// | msea           | SIZE(MAPSF,1)               | int        | in     |               |
// | nk, nth, gtype | NK, NTH, GTYPE              | int        | in     |               |
// | mapsta         | MAPSTA(NY,NX)               | const int* | in     | NY*NX         |
// | mapsf          | MAPSF(MSEA,3)               | const int* | in     | msea x 3      |
// | mapx2, mapy2, mapaxy | same names (W3ADATMD) | int*       | inout  | NY*NX         |
// | mapcxy         | MAPCXY(NSEA)                | int*       | inout  | NSEA          |
// | mapth2         | MAPTH2((NK+2)*NTH)          | int*       | inout  | (NK+2)*NTH    |
// | mapwn2         | MAPWN2(NSPEC+NTH)           | int*       | inout  | NK*NTH+NTH    |
// | counts         | NMX0..NMX2, NMY0..NMY2, NACT, NCENT | int* | out | 8             |
int ww_wave_map3(int nx, int ny, int nsea, int msea, int nk, int nth, int gtype, const int* mapsta,
                 const int* mapsf, int* mapx2, int* mapy2, int* mapaxy, int* mapcxy, int* mapth2,
                 int* mapwn2, int* counts) {
  if (nx < 1 || ny < 1 || nsea < 1 || msea < nsea || nk < 1 || nth < 1 || gtype < 1 ||
      gtype >= 3 || !mapsta || !mapsf || !mapx2 || !mapy2 || !mapaxy || !mapcxy || !mapth2 ||
      !mapwn2 || !counts)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_map3: bad sizes, GTYPE >= 3 or null pointer");
  return run(MAP3, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    using M = typename E::memory_space;
    const size_t nc = static_cast<size_t>(nx) * static_cast<size_t>(ny);
    const size_t nth2 = static_cast<size_t>(nk + 2) * static_cast<size_t>(nth);
    const size_t nwn2 = static_cast<size_t>(nk) * static_cast<size_t>(nth) + static_cast<size_t>(nth);
    ww::prop::Map3Scalars s;
    s.nx = nx;
    s.ny = ny;
    s.nsea = nsea;
    s.nk = nk;
    s.nth = nth;
    s.gtype = gtype;
    ww::prop::Map3Views<M> v;
    v.mapsta = in1(e, "ww_wave.mapsta", mapsta, nc);
    v.mapsf = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    auto dx2 = in1(e, "ww_wave.mapx2", static_cast<const int*>(mapx2), nc);
    auto dy2 = in1(e, "ww_wave.mapy2", static_cast<const int*>(mapy2), nc);
    auto daxy = in1(e, "ww_wave.mapaxy", static_cast<const int*>(mapaxy), nc);
    auto dcxy = in1(e, "ww_wave.mapcxy", static_cast<const int*>(mapcxy), static_cast<size_t>(nsea));
    auto dth2 = in1(e, "ww_wave.mapth2", static_cast<const int*>(mapth2), nth2);
    auto dwn2 = in1(e, "ww_wave.mapwn2", static_cast<const int*>(mapwn2), nwn2);
    v.mapx2 = dx2;
    v.mapy2 = dy2;
    v.mapaxy = daxy;
    v.mapcxy = dcxy;
    v.mapth2 = dth2;
    v.mapwn2 = dwn2;
    v.counts = Kokkos::View<int*, Kokkos::LayoutLeft, M>(Kokkos::view_alloc(e, std::string("ww_wave.counts")),
                                                        ww::prop::kMap3NCounts);
    ww::prop::Map3Counts c;
    ww::prop::w3map3<E>(e, s, v, c);
    out(e, dx2, mapx2);
    out(e, dy2, mapy2);
    out(e, daxy, mapaxy);
    out(e, dcxy, mapcxy);
    out(e, dth2, mapth2);
    out(e, dwn2, mapwn2);
    counts[0] = c.nmx0;
    counts[1] = c.nmx1;
    counts[2] = c.nmx2;
    counts[3] = c.nmy0;
    counts[4] = c.nmy1;
    counts[5] = c.nmy2;
    counts[6] = c.nact;
    counts[7] = c.ncent;
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_mapt: W3MAPT (w3pro3md.F90).
// | C            | Fortran                    | Type         | Intent | Shape     |
// | nsea, msea   | NSEA, SIZE(MAPSF,1)        | int          | in     |           |
// | ncell        | NY*NX                      | int          | in     |           |
// | mapsf        | MAPSF(MSEA,3)              | const int*   | in     | msea x 3  |
// | atrnx, atrny | ATRNX(NY*NX,-1:1), ATRNY   | const float* | in     | ncell x 3 |
// | maptrn       | MAPTRN(NY*NX), LOGICAL as 0/1 | int*      | inout  | ncell     |
int ww_wave_mapt(int nsea, int msea, int ncell, const int* mapsf, const float* atrnx,
                 const float* atrny, int* maptrn) {
  if (nsea < 0 || msea < nsea || ncell < 1 || !mapsf || !atrnx || !atrny || !maptrn)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_mapt: bad sizes or null pointer");
  return run(MAPT, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    const size_t nc = static_cast<size_t>(ncell);
    auto dmap = in2(e, "ww_wave.mapsf", mapsf, static_cast<size_t>(msea), 3);
    auto dax = in2(e, "ww_wave.atrnx", atrnx, nc, 3);
    auto day = in2(e, "ww_wave.atrny", atrny, nc, 3);
    auto dtr = in1(e, "ww_wave.maptrn", static_cast<const int*>(maptrn), nc);
    ww::prop::w3mapt<E>(e, nsea, dmap, dax, day, dtr);
    out(e, dtr, maptrn);
    return int{WW_KOKKOS_OK};
  });
}

// ww_wave_distab: DISTAB (w3dispmd.F90), the W3DISPMD tables.
// | C       | Fortran             | Type   | Intent | Shape   |
// | nar1d   | NAR1D               | int    | in     | 121     |
// | ewn1    | EWN1(0:NAR1D)       | float* | inout  | NAR1D+1, in: the Fortran's table, compared |
// | ecg1    | ECG1(0:NAR1D)       | float* | inout  | NAR1D+1 |
// | n1max   | N1MAX               | int*   | inout  | 1       |
// | dsie    | DSIE                | float* | inout  | 1       |
int ww_wave_distab(int nar1d, float* ewn1, float* ecg1, int* n1max, float* dsie) {
  if (nar1d != ww::disp::kNar1d || !ewn1 || !ecg1 || !n1max || !dsie)
    return fail(WW_KOKKOS_ERR_BAD_SHAPE, "ww_wave_distab: NAR1D differs from the port's 121");
  return run(DISTAB, [&](const auto& e) {
    using E = std::decay_t<decltype(e)>;
    using M = typename E::memory_space;
    Kokkos::View<Real*, M> dw(Kokkos::view_alloc(e, std::string("ww_wave.ewn1")), static_cast<size_t>(nar1d) + 1);
    Kokkos::View<Real*, M> dc(Kokkos::view_alloc(e, std::string("ww_wave.ecg1")), static_cast<size_t>(nar1d) + 1);
    const ww::disp::DistabScalars s = ww::disp::distab<E>(e, dw, dc);
    // The caller's arrays hold the Fortran DISTAB's tables (W3IOGR ran it first):
    // count the values the Kokkos tables change, as the ST4 and FLX table rebuilds do.
    const size_t n = static_cast<size_t>(nar1d) + 1;
    std::vector<Real> fw(ewn1, ewn1 + n), fc(ecg1, ecg1 + n);
    out(e, dw, ewn1);
    out(e, dc, ecg1);
    long long diff = 0;
    for (size_t i = 0; i < n; ++i) {
      if (std::memcmp(&fw[i], &ewn1[i], sizeof(Real)) != 0) ++diff;
      if (std::memcmp(&fc[i], &ecg1[i], sizeof(Real)) != 0) ++diff;
    }
    if (std::memcmp(&s.dsie, dsie, sizeof(Real)) != 0) ++diff;
    if (s.n1max != *n1max) ++diff;
    std::fprintf(stderr,
                 "ww_kokkos: DISTAB tables rebuilt in C++: %lld of EWN1, ECG1(0:%d), DSIE, N1MAX "
                 "differ from the Fortran's\n",
                 diff, nar1d);
    *n1max = s.n1max;
    *dsie = s.dsie;
    return int{WW_KOKKOS_OK};
  });
}

}  // extern "C"
