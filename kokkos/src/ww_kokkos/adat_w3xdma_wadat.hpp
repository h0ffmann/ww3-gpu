// kokkos/src/ww_kokkos/adat_w3xdma_wadat.hpp
// The X* components of W3ADATMD's TYPE WADAT, the output-field arrays sized for the
// whole grid (NSEALM*NAPROC points) that one process gathers for output. Phase 1: the
// storage W3XDMA (adat_w3xdma.hpp) allocates. The flag AINIT2 that says they exist is
// WadatScalars::ainit2 (adat_w3naux.hpp), its one home, which W3NAUX clears.
//
// WW3 heritage: TYPE WADAT, WW3/model/src/w3adatmd.F90:414-497 (the X* pointers) at
// the WW3/ pin 8e433f99 (same file as NOAA-EMC/WW3 03adda91); author
// H. L. Tolman. None of these components is behind a switch.
//
// Relation to Wadat (adat_wadat.hpp, which this file does not change): the X* arrays
// are the components of the same TYPE WADAT that Wadat leaves out. WadatsX::models[imod-1]
// is the X part of Wadats::wadats[imod-1]; NADATA, IADATA and NSEALM are Wadats' and
// AINIT2 is WadatScalars' (one home per Fortran field), as WadatsNl1 does for the
// W3_NL1 part.
//
// Layout: LayoutLeft Views in ExecSpace::memory_space (no UVM); REAL is ww::Real.
// Index maps (Fortran -> C++), with NXXX = NSEALM*NAPROC of the W3XDMA call:
//   X(NXXX) or X(1)                     X(i)     -> x(i-1)
//   X(NXXX,n) or X(1,n), n = 2, 3, 4    X(i,j)   -> x(i-1, j-1)
//   XUS3D, XUSSP (NXXX,2*NK) or (1,1)   X(i,j)   -> x(i-1, j-1)
//   XUSERO (NXXX,NOEXTR) or (1,NOEXTR)  X(i,j)   -> x(i-1, j-1)
//   XEF..XSTH2M (NXXX,E3DF(2,k):E3DF(3,k)) or (1,1)
//                                       X(i,j)   -> x(i-1, j-<name>_lb)
//   XP2SMS (NXXX,P2MSF(2):P2MSF(3)) or (1,1)       -> x(i-1, j-xp2sms_lb)
//   XPHS..XPEP (NXXX,0:NOSWLL) or (1,1) X(i,ip)  -> x(i-1, ip-<name>_lb)
// The trap: a 2-D array allocated as (1,1) because its output flag is off has second
// lower bound 1, not the 0 or E3DF(2,k) or P2MSF(2) of the full shape, so the lower
// bound is stored per array (*_lb), 1 until W3XDMA sets it.
//
// A zero-extent Fortran array (NXXX <= 0) is still ASSOCIATED; a zero-extent View
// carries no such flag. WadatScalars::ainit2 is the record that the arrays exist.
#pragma once

#include <vector>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::adat {

/// @brief The X* components of TYPE WADAT (w3adatmd.F90:414-497) for one model, as
///        W3XDMA allocates them.
/// @details Default-constructed Views are unallocated, the state W3NAUX leaves (it
///          does not nullify the pointers).
template <class ExecSpace>
struct WadatX {
  using Mem = typename ExecSpace::memory_space;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Mem>;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, Mem>;

  // Group 2, :414-420
  R1 xhs, xwlm, xt02, xt0m1, xt01, xfp0, xthm, xths, xthp0, xhsig, xstmaxe, xstmaxd,
      xhmaxe, xhcmaxe, xhmaxd, xhcmaxd, xqp, xwbt, xwnmean;
  // Group 3, :426-427
  R2 xef, xth1m, xsth1m, xth2m, xsth2m;
  int xef_lb = 1, xth1m_lb = 1, xsth1m_lb = 1, xth2m_lb = 1, xsth2m_lb = 1;
  // Group 4, :437-442
  R2 xphs, xptp, xplp, xpdir, xpsi, xpws;
  R1 xpwst, xpnr;
  R2 xpgw, xpthp0, xpqp, xppe, xpsw, xptm1, xpt1, xpt2, xpep;
  int xphs_lb = 1, xptp_lb = 1, xplp_lb = 1, xpdir_lb = 1, xpsi_lb = 1, xpws_lb = 1,
      xpgw_lb = 1, xpthp0_lb = 1, xpqp_lb = 1, xppe_lb = 1, xpsw_lb = 1, xptm1_lb = 1,
      xpt1_lb = 1, xpt2_lb = 1, xpep_lb = 1;
  // Group 5, :449-451
  R1 xcharn, xcge, xphiaw, xtauwix, xtauwiy, xtauwnx, xtauwny;
  R2 xwhitecap;
  R1 xtws;
  // Group 6, :462-468
  R1 xsxx, xsyy, xsxy, xtauox, xtauoy, xbhd, xphioc, xtusx, xtusy, xussx, xussy, xtauocx,
      xtauocy, xprms, xtpms, xphice;
  R2 xtauice;
  R2 xp2sms, xus3d, xussp;
  int xp2sms_lb = 1;
  // Group 7, :475-477
  R1 xaba, xabd, xuba, xubd;
  R2 xbedforms;
  R1 xphibbl;
  R2 xtaubbl;
  // Group 8, :483-485
  R1 xmssx, xmssy, xmssd, xmscx, xmscy, xmscd, xqkk, xskew, xembia1, xembia2;
  // Group 9, :491-492
  R1 xdtdyn, xfcut, xcflxymax, xcflthmax, xcflkmax;
  // Group 10, :497
  R2 xusero;
};

/// @brief The X parts of WADATS(NADATA); models[imod-1] goes with Wadats::wadats[imod-1].
///        The caller sizes it to NADATA, as W3NAUX's ALLOCATE(WADATS(NGRIDS))
///        (w3adatmd.F90:790) sizes the one Fortran array.
template <class ExecSpace>
struct WadatsX {
  std::vector<WadatX<ExecSpace>> models;
};

}  // namespace ww::adat
