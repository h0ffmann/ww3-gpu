// kokkos/src/ww_kokkos/psmc_smcdhxy.hpp
// SMCDHXY: water-depth gradients of an SMC grid, limited, spread on the full NY x NX grid,
// rotated to map east in the Arctic part, and the directional refraction limiter DHLMT.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:2903-3036 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCDHXY_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_params.hpp"
#include "real.hpp"

namespace ww::psmc {

/// One iteration n (1-based) of DO n=1,NSEA at :2952-2973. dhdx, dhdy: DHDX, DHDY(NSEA),
/// 0-based, limited and rotated in place. ijkcel: IJKCel(4,-9:MCel) at its column n=1, so cell n
/// is ijkcel[4*(n-1) .. 4*(n-1)+3]. dddx, dddy: (NY,NX) column-major, leading dimension ny.
/// angarc: ANGARC(NARC), 0-based. Unlike SMCDCXY, the full grid gets the value before the
/// Arctic rotation (:2963-2964 come before :2968).
/// Under W3_OMPG the loop is a parallel DO; it writes disjoint blocks only when no two cells
/// cover the same grid point, which an SMC cell list satisfies.
KOKKOS_INLINE_FUNCTION void smcdhxy_grad_point(int n, Real* dhdx, Real* dhdy, int mrfct,
                                               const int* ijkcel, int ny, Real* dddx, Real* dddy,
                                               bool arctc, int nglo, const Real* angarc) {
  const Real lim = static_cast<Real>(0.1);
  // SIGN(0.1, x) is copysignf (GIMPLE :2955); a NaN fails the test and stays.
  if (Kokkos::fabs(dhdx[n - 1]) > lim) dhdx[n - 1] = Kokkos::copysign(lim, dhdx[n - 1]);
  if (Kokkos::fabs(dhdy[n - 1]) > lim) dhdy[n - 1] = Kokkos::copysign(lim, dhdy[n - 1]);

  const int* cel = ijkcel + 4L * static_cast<long>(n - 1);
  const int i = cel[0] / mrfct + 1;
  const int j = cel[1] / mrfct + 1;
  const int k = Kokkos::max(1, cel[2] / mrfct);
  const int m = Kokkos::max(1, cel[3] / mrfct);
  const Real gx = dhdx[n - 1];
  const Real gy = dhdy[n - 1];
  for (int ic = i; ic <= i + k - 1; ++ic) {
    const long col = static_cast<long>(ic - 1) * static_cast<long>(ny);
    for (int jr = j; jr <= j + m - 1; ++jr) dddx[col + (jr - 1)] = gx;
  }
  for (int ic = i; ic <= i + k - 1; ++ic) {
    const long col = static_cast<long>(ic - 1) * static_cast<long>(ny);
    for (int jr = j; jr <= j + m - 1; ++jr) dddy[col + (jr - 1)] = gy;
  }

  if (arctc && n > nglo) {
    const Real cnst0 = angarc[n - nglo - 1] * ww::prop::kDera;
    // gfortran calls sincosf once for COS and SIN of CNST0.
    const Real c = Kokkos::cos(cnst0);
    const Real s = Kokkos::sin(cnst0);
    const Real cnst1 = dhdx[n - 1] * c - dhdy[n - 1] * s;
    const Real cnst2 = dhdx[n - 1] * s + dhdy[n - 1] * c;
    dhdx[n - 1] = cnst1;
    dhdy[n - 1] = cnst2;
  }
}

/// One iteration n (1-based) of DO n=1,NSEA at :2988-3017: the refraction limiter
/// DHLMT(1:NTH,n). dhlmt: (NTH,NSEA) column-major. ecos, esin: ECOS, ESIN(1:NTH), 0-based.
/// refran, dth: Refran, DTH (w3gdatmd).
KOKKOS_INLINE_FUNCTION void smcdhxy_dhlmt_point(int n, int nth, const Real* dhdx, const Real* dhdy,
                                                const Real* ecos, const Real* esin, Real refran,
                                                Real dth, Real* dhlmt) {
  Real* col = dhlmt + static_cast<long>(nth) * static_cast<long>(n - 1);
  const Real cnst4 =
      Kokkos::sqrt(dhdx[n - 1] * dhdx[n - 1] + dhdy[n - 1] * dhdy[n - 1]) *
      static_cast<Real>(1.0001);
  if (cnst4 > static_cast<Real>(1.0E-5)) {
    for (int i = 1; i <= nth; ++i) {
      // -(...)/CNST4 is NEGATE after the division in the GIMPLE (:3008).
      const Real q = (dhdx[n - 1] * ecos[i - 1] + dhdy[n - 1] * esin[i - 1]) / cnst4;
      const Real cnst6 = Kokkos::acos(-q);
      // minss operand order of the object: MIN(CNST6, ABS(PI-CNST6)) keeps CNST6 unless the
      // other is strictly less; MIN(Refran, x) keeps Refran unless x is strictly less.
      const Real a = Kokkos::fabs(ww::kPi - cnst6);
      const Real m1 = (a < cnst6) ? a : cnst6;
      const Real x = m1 * static_cast<Real>(0.75);
      const Real m2 = (x < refran) ? x : refran;
      col[i - 1] = m2 / dth;
    }
  } else {
    for (int i = 0; i < nth; ++i) col[i] = static_cast<Real>(0.0);
  }
}

/// SMCDHXY on the host. dw: DW(0:NSEA) (w3adatmd), indexed with the Fortran index (dw[k],
/// k=1..NSEA; DW(0) is not read). dddx, dddy: (NY,NX) column-major, overwritten. dhdx, dhdy:
/// DHDX, DHDY(NSEA), 0-based. dhlmt: DHLMT(NTH,NSEA) column-major. gradn: the caller's SMCGradn
/// (:2341), called as gradn(const Real* cvq, Real* grdx, Real* grdy, int l0r1) on NSEA arrays;
/// SMCGradn is not ported yet. hcel, grhx, grhy: the automatic arrays HCel, GrHx, GrHy(NSEA)
/// (:2915), caller-provided.
template <class Gradn>
inline void smcdhxy(int nx, int ny, int nsea, int nth, int mrfct, const int* ijkcel, int nglo,
                    const Real* angarc, bool arctc, Real dmin, Real refran, Real dth,
                    const Real* ecos, const Real* esin, const Real* dw, Real* dddx, Real* dddy,
                    Real* dhdx, Real* dhdy, Real* dhlmt, Gradn&& gradn, Real* hcel, Real* grhx,
                    Real* grhy) {
  const long nxy = static_cast<long>(nx) * static_cast<long>(ny);
  for (int k = 1; k <= nsea; ++k) hcel[k - 1] = dw[k];
  for (int k = 1; k <= nsea; ++k)
    if (dw[k] < dmin) hcel[k - 1] = dmin;
  for (long p = 0; p < nxy; ++p) dddx[p] = static_cast<Real>(0.0);
  for (long p = 0; p < nxy; ++p) dddy[p] = static_cast<Real>(0.0);
  const int l = 1;
  gradn(static_cast<const Real*>(hcel), grhx, grhy, l);
  for (int n = 0; n < nsea; ++n) dhdx[n] = grhx[n];
  for (int n = 0; n < nsea; ++n) dhdy[n] = grhy[n];
  for (int n = 1; n <= nsea; ++n)
    smcdhxy_grad_point(n, dhdx, dhdy, mrfct, ijkcel, ny, dddx, dddy, arctc, nglo, angarc);
  for (int n = 1; n <= nsea; ++n)
    smcdhxy_dhlmt_point(n, nth, dhdx, dhdy, ecos, esin, refran, dth, dhlmt);
}

}  // namespace ww::psmc
