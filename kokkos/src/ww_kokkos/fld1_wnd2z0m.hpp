// kokkos/src/ww_kokkos/fld1_wnd2z0m.hpp
// WND2Z0M of W3FLD1MD: bulk momentum roughness length z0 from the 10 m neutral wind, the 2016
// HWRF fit (znot_m_v1 piecewise polynomial, then the icoef_sf=4 drag reduction of znot_wind10m).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3fld1md.F90:1251-1375 at NOAA-EMC/WW3 03adda91;
// authors B. G. Reichl, J. Meixner (2016 HWRF z0).
// Evidence: kokkos/port_report/WND2Z0M_KOKKOS.md.
// ⚠ On a device logf, expf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::fld1 {

/// WND2Z0M(W10M,ZNOTM). Coefficients are default-REAL literals, rounded once to float.
/// A NaN W10M fails every test and takes the ELSE (cf) polynomial and AAA=0.75, as comiss does.
KOKKOS_INLINE_FUNCTION void wnd2z0m(Real w10m, Real& znotm) {
  // :1348. gfortran folds the whole U10=85 path (MPFR logf/expf/sqrtf) into this constant;
  // 0x1.6fd714p-12 is the PHI operand of the GIMPLE and the .rodata load of the object.
  if (w10m > static_cast<Real>(85.0)) {
    znotm = 0x1.6fd714p-12F;
    return;
  }
  const Real u10 = w10m;
  const Real u2 = u10 * u10;  // U10**2, computed once before the first test
  Real z;
  if (u10 <= static_cast<Real>(5.0)) {
    // :1352. 0.0185/9.8 folds to one REAL; (...)**2 is p*p.
    const Real p = 7.59e-4F * u2 + 2.46e-2F * u10;
    z = 0.0185F / 9.8F * (p * p);
  } else if (u10 > static_cast<Real>(5.0) && u10 < static_cast<Real>(10.0)) {
    z = .00000235F * (u2 - static_cast<Real>(25)) + 3.805129199617346e-05F;  // :1354
  } else {
    // :1356-1360. The object's power tree: u3 = u*u2, u4 = u2*u2, u5 = u3*u2, u6 = u3*u3.
    const Real u3 = u10 * u2;
    const Real u4 = u2 * u2;
    const Real u5 = u3 * u2;
    const Real u6 = u3 * u3;
    if (u10 >= static_cast<Real>(10.0) && u10 < static_cast<Real>(60.0)) {
      z = -0.0010745704660847233F + 0.00044745137674732834F * u10 +
          -6.508676881906914e-05F * u2 + 4.507055294438727e-06F * u3 +
          -1.331896578363359e-07F * u4 + 1.7398510865876079e-09F * u5 +
          -8.367276172397277e-12F * u6;
    } else {
      z = 0.0406766967657759F + -0.0013914681964973246F * u10 + 7.107636825694182e-06F * u2 +
          1.7648562021709124e-07F * u3 + -3.329705958751961e-10F * u4 +
          -3.2260663894433345e-11F * u5 + 2.1151080765239772e-13F * u6;
    }
  }
  // :1366, TMP = 0.4*0.4/ALOG(10/ZNOTM)**2; 0.4*0.4 folds to 0x1.47ae16p-3, not 0.16.
  const Real l = Kokkos::log(static_cast<Real>(10.0) / z);
  const Real tmp = 0.4F * 0.4F / (l * l);
  Real aaa = static_cast<Real>(0.75);
  if (u10 < static_cast<Real>(20)) {
    aaa = static_cast<Real>(0.99);
  } else if (u10 < static_cast<Real>(45.0)) {
    // :1371. (0.75-0.99) and (45-20) fold; the division by 25 stays.
    aaa = 0.99F + (u10 - static_cast<Real>(20)) * (0.75F - 0.99F) / (45.0F - 20.0F);
  }
  znotm = static_cast<Real>(10.0) / Kokkos::exp(Kokkos::sqrt(0.4F * 0.4F / (tmp * aaa)));  // :1373
}

}  // namespace ww::fld1
