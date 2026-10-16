// kokkos/src/ww_kokkos/srce_st2_inptab.hpp
// INPTAB and its internal function W3BETA: the table of the Chalikov and Belevich (1993)
// wind-wave interaction parameter that W3SIN2 interpolates (BETATB, DSIGA, DDRAG), built on
// the host once per process. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src2md.F90 at NOAA-EMC/WW3 03adda91 (table and its
// sizes :91-99, INPTAB :847-1228, W3BETA :1081-1224); authors H. L. Tolman, D. Chalikov.
// Evidence: kokkos/port_report/INPTAB_KOKKOS.md, W3BETA_KOKKOS.md.
// W3IOGR calls INPTAB once per process, on the first mod_def read (FLINP is SAVE,
// w3iogrmd.F90:355,1493): the table depends on no grid. The W3_T, W3_T0 and W3_T1 test
// output (:960-962, :977-1029, :1160-1162, :1185-1188, :1206-1208) and NDST, which only
// they read, are dropped; W3_S is tracing.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "real.hpp"

namespace ww::srce::st2 {

/// NRSIGA, NRDRAG, SIGAMX, DRAGMX (w3src2md.F90:93-96).
inline constexpr int kNrSiga = 400;
inline constexpr int kNrDrag = 20;
inline constexpr Real kSigaMx = 40.f;
inline constexpr Real kDragMx = 1.E-2f;

/// BETATB(-NRSIGA:NRSIGA+1, NRDRAG+1) (:99) as a flat column-major array:
/// BETATB(isiga, idrag) is betatb[(isiga + kNrSiga) + kBetatbN1 * (idrag - 1)].
inline constexpr int kBetatbN1 = 2 * kNrSiga + 2;
inline constexpr int kBetatbN2 = kNrDrag + 1;
inline constexpr int kBetatbSize = kBetatbN1 * kBetatbN2;

/// W3BETA (:1081-1224), host: beta * 1e-4 for the non-dimensional apparent frequency oma
/// and the drag coefficient cl. The Fortran's third argument NDST is read only by W3_T0
/// output and is dropped. All literals are default REAL in the Fortran.
inline Real w3beta(Real oma, Real cl) {
  const Real om1 = 1.075f + 75.f * cl;  // :1166
  const Real om2 = 1.2f + 300.f * cl;   // :1167

  const Real a1 = 0.25f + 395.f * cl;   // :1171
  const Real a2 = 0.35f + 150.f * cl;   // :1172
  const Real a4 = 0.3f + 300.f * cl;    // :1173
  const Real a9 = 0.35f + 240.f * cl;   // :1174
  const Real a10 = -0.06f + 470.f * cl; // :1175

  const Real a5 = a4 * om1;                                        // :1177
  const Real a0 = 0.25f * (a5 * a5) / a4;                          // :1178
  const Real a3 = (a0 - a2 - a1) / (a0 + a4 + a5);                 // :1179
  const Real a6 = a0 * (1.f - a3);                                 // :1180
  const Real a7 = (a9 * ((om2 - 1.f) * (om2 - 1.f)) + a10) / (om2 - om1);  // :1181, integer 1
  const Real a8 = a7 * om1;                                        // :1182

  Real beta;
  if (oma < -1.f) {  // :1191-1201
    beta = -(a1 * (oma * oma)) - a2;
  } else if (oma < om1 / 2.f) {
    beta = a3 * oma * (a4 * oma - a5) - a6;
  } else if (oma < om1) {
    beta = oma * (a4 * oma - a5);
  } else if (oma < om2) {
    beta = a7 * oma - a8;
  } else {
    beta = a9 * ((oma - 1.f) * (oma - 1.f)) + a10;
  }

  return beta * 1.E-4f;  // :1205
}

/// INPTAB (:847-1228), host: fills betatb (kBetatbSize, layout above) and returns the
/// module scalars DSIGA and DDRAG (:958-959).
inline void inptab(Real* betatb, Real& dsiga, Real& ddrag) {
  dsiga = kSigaMx / static_cast<Real>(kNrSiga);
  ddrag = kDragMx / static_cast<Real>(kNrDrag);

  for (int isiga = -kNrSiga; isiga <= kNrSiga + 1; ++isiga) {  // :967-973
    const Real siga = static_cast<Real>(isiga) * dsiga;
    for (int idrag = 1; idrag <= kNrDrag + 1; ++idrag) {
      const Real drag = static_cast<Real>(idrag) * ddrag;
      betatb[(isiga + kNrSiga) + kBetatbN1 * (idrag - 1)] = w3beta(siga, drag);
    }
  }
}

}  // namespace ww::srce::st2
