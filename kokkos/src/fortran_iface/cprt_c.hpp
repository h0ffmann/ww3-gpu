// kokkos/src/fortran_iface/cprt_c.hpp
// The C ABI of the cprt wiring unit: W3CPRT's spectral partitioning on the resident VA (PATCH.md
// section 27). Declarations only; cprt_shim.cpp implements them, and model/src/w3kokkos_cprtmd.F90
// on the fork (h0ffmann/WW3 ww3-gpu/u9-cprt) mirrors them. The rules of ww_kokkos_c.hpp hold:
// Fortran order, caller-owned arrays, float is WW3's REAL, nothing throws.
//
// SPDX-License-Identifier: MIT
#pragma once

extern "C" {

/// The packed integers of ww_cprt_va, in this order (1-based in the Fortran).
enum WwCprtIpar {
  WW_CPRT_NK = 0,
  WW_CPRT_NTH,
  WW_CPRT_NSEAL,
  WW_CPRT_NSEA,    ///< rows of MAPSF, U10, U10D; columns 0:NSEA of CG, WN and DW
  WW_CPRT_NX,      ///< MAPSTA(NY,NX)
  WW_CPRT_NY,
  WW_CPRT_NASF,    ///< SIZE(ASF), at least NSEA
  WW_CPRT_PTMETH,  ///< PTMETH, IHMAX, FLCOMB (1 or 0) of W3ODATMD for the current output grid
  WW_CPRT_IHMAX,
  WW_CPRT_FLCOMB,
  WW_CPRT_DIMP,    ///< DIMP of W3ODATMD, 15
  WW_CPRT_NAR1D,   ///< NAR1D and N1MAX of W3DISPMD
  WW_CPRT_N1MAX,
  WW_CPRT_NIPAR
};

/// The packed reals of ww_cprt_va, in this order.
enum WwCprtRpar {
  WW_CPRT_DTH = 0,  ///< DTH, XFR, FACHFE, FTE of W3GDATMD
  WW_CPRT_XFR,
  WW_CPRT_FACHFE,
  WW_CPRT_FTE,
  WW_CPRT_HSPMIN,   ///< HSPMIN, WSMULT, WSCUT, PTFCUT of W3ODATMD
  WW_CPRT_WSMULT,
  WW_CPRT_WSCUT,
  WW_CPRT_PTFCUT,
  WW_CPRT_UNDEF,    ///< UNDEF of CONSTANTS (a variable, constants.F90:85)
  WW_CPRT_DSIE,     ///< DSIE of W3DISPMD
  WW_CPRT_NRPAR
};

/// Sections 1-5 of W3CPRT (w3iosfmd.F90:109-344) on d_va of grid `igrid`
/// (ww::resident::device_va: VA crosses only when the host holds the newer bits). The partition
/// table is kept in the shim's buffers until ww_cprt_fetch. Out: *finsiz, FINSIZ = ICPRT(NSEAL+1,2)-1
/// (:312); *nxpfull, the number of PTMEAN calls that met a full XP array, each a format 1000 line
/// the caller writes on NDSE when IAPROC == NAPERR (w3partmd.F90:1416); *fallback, 1 when a point
/// met PTMEAN's IFPMAX = 0 (a NaN spectrum, part_ptmean.hpp), where the Fortran reads out of
/// bounds: the caller then discards this result, reads VA to the host and calls the Fortran
/// W3CPRT. The caller has stopped already on a NaN depth (:270-274). va is VA(1,0), nspec =
/// SIZE(VA,1), nsealm = UBOUND(VA,2); mapsf (NSEA,3), mapsta (NY,NX), u10, u10d (NSEA), asf
/// (NASF), dw (0:NSEA), cg, wn (0:NK+1,0:NSEA), sig, dsip (0:NK+1), dsii (NK), th, ecos, esin (NTH),
/// ewn1, ecg1 (0:NAR1D). Returns a WwKokkosStatus.
int ww_cprt_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar, float* va,
               int nspec, int nsealm, const int* mapsf, const int* mapsta, const float* u10,
               const float* u10d, const float* asf, const float* dw, const float* cg,
               const float* wn, const float* sig, const float* dsip, const float* dsii,
               const float* th, const float* ecos, const float* esin, const float* ewn1,
               const float* ecg1, int* finsiz, int* nxpfull, int* fallback);

/// The table of the last ww_cprt_va of grid `igrid` into WW3's arrays: icprt is
/// ICPRT(NSEALM+1,2), every entry written; dtprt DTPRT(DIMP,MAX(1,FINSIZ)), columns 1..finsiz
/// written (the caller zeroes DTPRT when finsiz is 0, :325). Returns a WwKokkosStatus,
/// WW_KOKKOS_ERR_BAD_SHAPE when igrid, nsealm or finsiz differ from that call's.
int ww_cprt_fetch(int igrid, int nsealm, int* icprt, int finsiz, float* dtprt);

}  // extern "C"
