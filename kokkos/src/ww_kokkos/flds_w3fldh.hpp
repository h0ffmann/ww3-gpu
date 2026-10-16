// kokkos/src/ww_kokkos/flds_w3fldh.hpp
// W3FLDH, ported to Kokkos: update the homogeneous input field J of the WW3 generic shell
// (water level, current, wind, ice, momentum, air density, ice and mud parameters, moving
// grid) for the interval T0 to TN, from the list of homogeneous values ww3_shel read. Phase 1,
// translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3FLDH, WW3/model/src/w3fldsmd.F90:2149-2504 at the WW3/ pin
// 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman. Header Purpose
// (:2167-2170): "Update homogeneous input fields for the WAVEWATCH III generic shell."
// Ported as switches/switch_lab_shrd compiles it: no W3_S (STRACE, :2256-2258, :2286-2288),
// no W3_T (the WRITE (NDST,90xx) test prints and FORMATs 9000-9062); neither is ported.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of w3fldsmd.F90
// with WW3's Release flags and the switch_lab_shrd defines (port report):
//  - DERA = ATAN(1.)/45. (:2291) is folded at compile time to one REAL constant; the C++
//    expression folds to the same bits.
//  - COS(DIR) and SIN(DIR) (:2377-2379, :2392-2394) are default REAL, one sincosf call.
//  - NH .NE. 0. (:2361) converts NH to REAL and compares; kept as written.
//  - The field loops (DO IX / DO IY) are stores of one value or copies, no arithmetic.
//
// Memory contract (phase 1). The six fields FX0..FSN live in ExecSpace's memory space and
// are written only by the loop kernels, in the Fortran's statement order on one execution
// space instance; everything else (the times, NH, THO, HA, HD, HS, the scalars X, Y, DIR,
// DTTST) is host-side, as in ww3_shel. The function fences exec before it returns.
#pragma once

#include <array>
#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::flds {

/// IERR of W3FLDH (:2211-2214). An output of the routine, not a stop.
inline constexpr int kFldhOk = 0;            ///< 0 OK
inline constexpr int kFldhIllegalField = 1;  ///< 1 Illegal field number, :2302-2306
inline constexpr int kFldhPastLastData = -1; ///< -1 Past last data, :2426

/// Field numbers J, lower and upper bound of the second index of THO, HA, HD, HS (:2268, :2270).
inline constexpr int kFldhJmin = -7;
inline constexpr int kFldhJmax = 10;

/// The calls W3FLDH makes to routines this file does not port, in WW3's argument order.
struct FldhHooks {
  void* ctx = nullptr;
  /// W3TIMEMD DSEC21(T1, T2), seconds from T1 to T2 as the REAL WW3 returns (:2436).
  float (*dsec21)(void* ctx, const int* t1, const int* t2) = nullptr;
  /// W3TIMEMD TICK21(T, DTIME), T advanced by DTIME seconds in place (:2425).
  void (*tick21)(void* ctx, int* t, float dtime) = nullptr;
  /// WRITE (NDSE,1001) J (:2303), called only when NDSE .GE. 0, as in WW3. May be null.
  void (*write_illegal_field)(void* ctx, int ndse, int j) = nullptr;
};

/// THO(2,-7:10,NHM) -> tho(i-1, j+7, ih-1); host-side, as ww3_shel keeps it.
using FldhTho = Kokkos::View<int***, Kokkos::LayoutLeft, Kokkos::HostSpace>;
/// HA, HD, HS (NHM,-7:10) -> h(ih-1, j+7); host-side.
using FldhHval = Kokkos::View<Real**, Kokkos::LayoutLeft, Kokkos::HostSpace>;
/// FX0..FSN (MX,MY) -> f(ix-1, iy-1), in ExecSpace's memory space.
template <class ExecSpace>
using FldhField = Kokkos::View<Real**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>;

namespace detail {

/// DO IX=1, NX / DO IY=1, NY with body(ix, iy) (0-based). The IX loop is the parallel one; each
/// IX keeps the Fortran's inner IY loop. The bodies are stores and copies without arithmetic,
/// so the order across IX does not change a bit.
template <class ExecSpace, class Body>
void loop_ixiy(const ExecSpace& exec, int nx, int ny, const Body& body) {
  if (nx <= 0) return;
  Kokkos::parallel_for(
      "ww::flds::w3fldh::loop", Kokkos::RangePolicy<ExecSpace>(exec, 0, nx),
      KOKKOS_LAMBDA(const int ix) {
        for (int iy = 0; iy < ny; ++iy) body(ix, iy);
      });
}

}  // namespace detail

/// @brief W3FLDH(J, NDST, NDSE, MX, MY, NX, NY, T0, TN, NH, NHM, THO, HA, HD, HS, TF0, FX0,
///        FY0, FS0, TFN, FXN, FYN, FSN, IERR), w3fldsmd.F90:2149-2504.
/// @param exec   execution space instance the field loops run on; fenced before the return
///               after the loop (the illegal-field return launches nothing).
/// @param j      field number, -7..10 (header :2181-2196).
/// @param ndse   unit number of error messages, passed to write_illegal_field; no output if < 0.
/// @param nx,ny  field dimensions; MX, MY are the extents of the Views, not checked (:2236).
/// @param t0,tn  time interval considered.
/// @param nh     number of homogeneous fields J left (in/out).
/// @param tho,ha,hd,hs  times, amplitudes, directions (degr., nautical) and air-sea
///               temperature differences of the homogeneous fields (in/out), second index j+7.
/// @param tf0,tfn       times of the input fields (in/out).
/// @param fx0..fsn      input fields X, Y, scalar, at TF0 and TFN (in/out).
/// @return IERR: kFldhOk, kFldhIllegalField or kFldhPastLastData.
/// @note A missing dsec21 or tick21 hook aborts, also for an illegal j: every pass of the loop
///       reaches DSEC21.
/// @author ww3-gpu; heritage H. L. Tolman (W3FLDSMD)
/// @date 2026-10-16
template <class ExecSpace>
int w3fldh(const ExecSpace& exec, const FldhHooks& hooks, int j, int ndse, int nx, int ny,
           const std::array<int, 2>& t0, const std::array<int, 2>& tn, int& nh,
           const FldhTho& tho, const FldhHval& ha, const FldhHval& hd, const FldhHval& hs,
           std::array<int, 2>& tf0, const FldhField<ExecSpace>& fx0,
           const FldhField<ExecSpace>& fy0, const FldhField<ExecSpace>& fs0,
           std::array<int, 2>& tfn, const FldhField<ExecSpace>& fxn,
           const FldhField<ExecSpace>& fyn, const FldhField<ExecSpace>& fsn) {
  using detail::loop_ixiy;
  if (hooks.dsec21 == nullptr || hooks.tick21 == nullptr)
    Kokkos::abort("ww::flds::w3fldh: dsec21 and tick21 hooks are required");

  int ierr = kFldhOk;                                    // :2290
  const Real dera = std::atan(1.0f) / 45.0f;             // :2291

  // :2302-2306
  if (j < kFldhJmin || j > kFldhJmax) {
    if (ndse >= 0 && hooks.write_illegal_field != nullptr)
      hooks.write_illegal_field(hooks.ctx, ndse, j);
    ierr = kFldhIllegalField;
    return ierr;
  }
  const bool flfrst = tfn[0] == -1;                      // :2307
  const int jj = j - kFldhJmin;                          // second index of THO, HA, HD, HS

  // Loop over times / fields, :2315-2446.
  for (;;) {
    // Shift fields, :2317-2357.
    tf0[0] = tfn[0];
    tf0[1] = tfn[1];
    if (tfn[0] != -1) {
      if ((j == 2) || (j == 5)) {
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fx0(ix, iy) = fxn(ix, iy);
          fy0(ix, iy) = fyn(ix, iy);
        });
      } else if (j == 3) {
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fx0(ix, iy) = fxn(ix, iy);
          fy0(ix, iy) = fyn(ix, iy);
          fs0(ix, iy) = fsn(ix, iy);
        });
      } else if (j == 6) {
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fs0(ix, iy) = fsn(ix, iy);
        });
      }
    }

    // New field, :2359-2431.
    if (static_cast<Real>(nh) != 0.0f) {                 // :2361, NH .NE. 0.
      tfn[0] = tho(0, jj, 0);
      tfn[1] = tho(1, jj, 0);
      // ic* md* lev ice, :2364-2374
      if ((j <= 1) || (j == 4) || (j == 6)) {
        const Real a = ha(0, jj);
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fsn(ix, iy) = a;
        });
      }
      // cur, :2375-2389
      if ((j == 2) || (j == 5)) {
        const Real dir = (270.0f - hd(0, jj)) * dera;
        const Real x = ha(0, jj) * std::cos(dir);
        const Real y = ha(0, jj) * std::sin(dir);
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fxn(ix, iy) = x;
          fyn(ix, iy) = y;
        });
      }
      // wnd, :2390-2405
      if (j == 3) {
        const Real dir = (270.0f - hd(0, jj)) * dera;
        const Real x = ha(0, jj) * std::cos(dir);
        const Real y = ha(0, jj) * std::sin(dir);
        const Real s = hs(0, jj);
        loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
          fxn(ix, iy) = x;
          fyn(ix, iy) = y;
          fsn(ix, iy) = s;
        });
      }
      // Shift data arrays, :2407-2416; sequential, each I reads I+1 before it is shifted.
      for (int i = 1; i <= nh - 1; ++i) {
        tho(0, jj, i - 1) = tho(0, jj, i);
        tho(1, jj, i - 1) = tho(1, jj, i);
        ha(i - 1, jj) = ha(i, jj);
        hd(i - 1, jj) = hd(i, jj);
        hs(i - 1, jj) = hs(i, jj);
      }
      nh = nh - 1;
    } else {
      // :2423-2426
      tfn[0] = tn[0];
      tfn[1] = tn[1];
      hooks.tick21(hooks.ctx, tfn.data(), 1.0f);
      ierr = kFldhPastLastData;
    }

    // Check time, :2436-2445.
    const Real dttst = hooks.dsec21(hooks.ctx, t0.data(), tfn.data());
    if (dttst > 0.0f) break;
    if ((j <= 1) || (j == 4) || (j == 6)) {
      if (flfrst && dttst == 0.0f) break;
    }
  }

  // Check if first field, :2450-2464.
  if (j != 1 && tfn[0] == -1) {
    tf0[0] = t0[0];
    tf0[1] = t0[1];
    loop_ixiy(exec, nx, ny, KOKKOS_LAMBDA(const int ix, const int iy) {
      fx0(ix, iy) = fxn(ix, iy);
      fy0(ix, iy) = fyn(ix, iy);
      fs0(ix, iy) = fsn(ix, iy);
    });
  }

  exec.fence("ww::flds::w3fldh");
  return ierr;
}

}  // namespace ww::flds
