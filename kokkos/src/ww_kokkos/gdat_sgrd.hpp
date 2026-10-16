// kokkos/src/ww_kokkos/gdat_sgrd.hpp
// The W3GDATMD state that W3DIMS sizes, for one model IMOD: the spectral grid
// SGRDS(IMOD) (TYPE SGRD) and the ST4 tables of MPARS(IMOD)%SRCPS that W3DIMS
// allocates. Phase 1, translated, not wired into WW3, not regtested.
//
// WW3 heritage: WW3/model/src/w3gdatmd.F90 at h0ffmann/WW3 8e433f99; author
// H. L. Tolman. TYPE SGRD :795-804; TYPE SRCP :879-924, of which only the six
// arrays of :904-905 (W3_ST4) are mirrored; module counters :619-620; ST4 table
// sizes :637-638.
//
// One struct per model replaces the arrays SGRDS(NLOW:NGRIDS) and MPARS(NLOW:NGRIDS)
// (:1092-1093, allocated by W3NMOD :1544-1547): the caller keeps one Sgrd and one
// Srcps per IMOD, and W3SETG's pointer aliases (NK => SGRDS(IMOD)%NK, :2498-2534,
// :2707-2712) become the caller holding a reference to that model's structs. A
// file-static instance would break ww3_multi (ww3-port skill, "Per-grid state").
//
// Memory: every array is a Kokkos::View in an explicit memory space, LayoutLeft, so
// a Fortran element A(i,j) is view(i - lb1, j - lb2) with the same column-major
// order. Index maps, Fortran -> C++ (lower bound 1 unless stated):
//   MAPWN, MAPTH, ESIN, ECOS, ES2, ESC, EC2 (MSPEC+MTH)  A(i)   -> a(i-1)
//   TH (MTH); DSII, DDEN (MK); SIG2, DDEN2 (MSPEC)       A(i)   -> a(i-1)
//   SIG, DSIP (0:MK+1)                                   A(k)   -> a(k)    extent MK+2
//   IKTAB (MK,NDTAB)                                     A(i,j) -> a(i-1,j-1)
//   DCKI, QBI (NKHS,NKD)                                 A(i,j) -> a(i-1,j-1)
//   SATINDICES, SATWEIGHTS (2*SDSNTH+1,MTH)              A(i,j) -> a(i-1,j-1)
//   CUMULW (MSPEC,MSPEC)                                 A(i,j) -> a(i-1,j-1)
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::gdat {

// :637-638, W3_ST4: dimensions of the dissipation tables, fixed at compile time.
inline constexpr int kNkhs = 2000;
inline constexpr int kNkd = 1300;
inline constexpr int kNdtab = 2000;

/// The module counters W3DIMS reads and W3SETG writes (:619-620). NGRIDS and the
/// selections start at -1 as in the Fortran; NAUXGR has no initialiser there and is
/// set by W3NMOD (:1543) before W3DIMS may run, so 0 here is only a placeholder.
struct GdatModule {
  int ngrids = -1;
  int igrid = -1;
  int isgrd = -1;
  int ipars = -1;
  int nauxgr = 0;
};

/// TYPE SGRD (:795-804) for one model. The scalars keep the Fortran's default
/// initialisation (:796, :798-799, :803); W3DIMS sets only NK, NK2, NTH, NSPEC and
/// SINIT and allocates the arrays; the REAL scalars are set later by W3IOGR/ww3_grid.
template <class MemSpace>
struct Sgrd {
  using IntV = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  using RealV = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;

  int nk = 0, nk2 = 0, nth = 0, nspec = 0;
  IntV mapwn, mapth;
  Real dth = 0.0f, xfr = 0.0f, fr1 = 0.0f, fte = 0.0f, ftf = 0.0f, ftwn = 0.0f, fttr = 0.0f,
       ftwl = 0.0f, facti1 = 0.0f, facti2 = 0.0f, fachfa = 0.0f, fachfe = 0.0f;
  RealV th, esin, ecos, es2, esc, ec2, sig, sig2, dsip, dsii, dden, dden2;
  bool sinit = false;
};

/// The arrays of TYPE SRCP that W3DIMS allocates under W3_ST4 (:904-905); the
/// scalars of SRCP are set by W3IOGR and are not mirrored here.
template <class MemSpace>
struct Srcps {
  using IntM = Kokkos::View<int**, Kokkos::LayoutLeft, MemSpace>;
  using RealM = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;

  IntM iktab, satindices;
  RealM dcki, satweights, cumulw, qbi;
};

}  // namespace ww::gdat
