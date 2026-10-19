// kokkos/src/fortran_iface/xypug_c.hpp
// The C ABI of the xypug wiring unit (PATCH.md section 30, issue #196): W3XYPUG with its
// explicit N-scheme W3XYPFSN2 (w3profsmd.F90:69-272, :470-727) for every spectral component of
// a propagation step on an unstructured grid (GTYPE = UNGTYPE, W3_SHRD, no PDLIB), in one call,
// on the resident VA of resident_c.hpp. It is W3WAVE section 3.6.3's ISPEC loop (W3GATH,
// W3XYPUG, W3SCAT, w3wavemd.F90:2049-2143 at WW3 43601904) for that grid. xypug_shim.cpp
// implements it; w3kokkos_prop3md.F90 on the fork binds it. The rules of ww_kokkos_c.hpp hold:
// Fortran order, caller-owned arrays, float is REAL, double is REAL(8) / DOUBLE PRECISION,
// nothing throws.
//
// SETDEPTH (w3profsmd.F90:1601-1678) is not translated: the Fortran wrapper calls it once before
// this entry. W3XYPUG calls it once per component, and it sets IOBDP from DW and DMIN only,
// which the ISPEC loop does not change, so one call leaves IOBDP with the same bits.
//
// SPDX-License-Identifier: MIT
#pragma once

#include "fortran_iface/ww_kokkos_c.hpp"

extern "C" {

/// Lengths of the IPAR/RPAR packets of ww_xypug_batch_va; the shim refuses other lengths.
/// IPAR: NX, NTRI, NK, NTH, NSPEC, NSEA, NBI, FLCUR, FLBPI, FSBCCFL, LCALC (logicals as 0/1).
/// RPAR: FACX, FACY, DTG, RD1, RD2. RD1, RD2 are W3XYPUG's DSEC21 (TBPI0, TIME) and
/// DSEC21 (TBPI0, TBPIN) under FLBPI, else 1 and 0 (w3profsmd.F90, W3XYPUG).
enum { WW_XYPUG_NIPAR = 11, WW_XYPUG_NRPAR = 5 };

/// W3GATH, W3XYPUG (FSN), W3SCAT (W3_SHRD) for every component ISPEC with on[ISPEC-1] /= 0.
/// | C       | Fortran                              | Type          | Intent | Length          |
/// | igrid   | IGRID (W3GDATMD)                     | int           | in     |                 |
/// | ipar    | see WW_XYPUG_NIPAR                   | const int*    | in     | nipar = 11      |
/// | rpar    | see WW_XYPUG_NRPAR                   | const float*  | in     | nrpar = 5       |
/// | mapsf   | MAPSF(NSEA,3)                        | const int*    | in     | 3*NSEA          |
/// | mapsta  | MAPSTA(NY,NX), NY = 1 here           | const int*    | in     | NX              |
/// | clats   | CLATS(1:NSEA)                        | const float*  | in     | NSEA            |
/// | ecos    | ECOS, ESIN(1:NSPEC+NTH)              | const float*  | in     | NSPEC+NTH each  |
/// | cg      | CG(0:NK+1,0:NSEA)                    | const float*  | in     | (NK+2)*(NSEA+1) |
/// | cx, cy  | CX, CY(0:NSEA)                       | const float*  | in     | NSEA+1 each     |
/// | trigp   | TRIGP(3,NTRI)                        | const int*    | in     | 3*NTRI          |
/// | ien     | IEN(NTRI,6), REAL(8)                 | const double* | in     | 6*NTRI          |
/// | si      | SI(NX), REAL(8)                      | const double* | in     | NX              |
/// | iobp    | IOBP(NX), INTEGER*2, widened         | const int*    | in     | NX              |
/// | iobpd   | IOBPD(NTH,NX), INTEGER*1, widened    | const int*    | in     | NTH*NX          |
/// | iobdp   | IOBDP(NX) after SETDEPTH, widened    | const int*    | in     | NX              |
/// | iobpa   | IOBPA(NX), INTEGER*1, widened        | const int*    | in     | NX              |
/// | isbpi   | ISBPI(1:NBI)                         | const int*    | in     | NBI^            |
/// | bbpi0   | BBPI0, BBPIN(NSPEC,0:NBI)            | const float*  | in     | NSPEC*(NBI+1)^  |
/// | on      | IAPPRO(ISPEC) == IAPROC per ISPEC    | const int*    | in     | NSPEC           |
/// | iter    | ITER(NK,NTH) (W3ADATMD)              | int*          | inout  | NK*NTH          |
/// | va      | VA(1,0) (W3WDATMD), bind only        | float*        | inout  | nspec*(nsealm+1)|
/// | nspec   | SIZE(VA,1)                           | int           | in     |                 |
/// | nsealm  | UBOUND(VA,2)                         | int           | in     |                 |
/// ^ Under FLBPI. With FLBPI 0 the caller passes NBI = 0 and 1-element dummies for isbpi, bbpi0,
/// bbpin; the shim neither reads nor copies them then.
/// ITER is read on every component and written where LCALC recomputes it, as W3XYPFSN2 does.
/// The kernels read and write d_va (synced first); the state is DEVICE afterwards. VGX, VGY are
/// not passed: W3XYPUG reads them only under W3_MGP, which the set-up refuses, as it refuses
/// W3_REF1, FSPSI, FSFCT and FSNIMP. Returns a WwKokkosStatus.
int ww_xypug_batch_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar,
                      const int* mapsf, const int* mapsta, const float* clats, const float* ecos,
                      const float* esin, const float* cg, const float* cx, const float* cy,
                      const int* trigp, const double* ien, const double* si, const int* iobp,
                      const int* iobpd, const int* iobdp, const int* iobpa, const int* isbpi,
                      const float* bbpi0, const float* bbpin, const int* on, int* iter,
                      float* va, int nspec, int nsealm);

/// The xypug counters (components propagated, calls, sub-steps), to stderr, under
/// WW_KOKKOS_PROP3_COUNT=1 with the prop3 lines.
void ww_xypug_report(void);

}  // extern "C"
