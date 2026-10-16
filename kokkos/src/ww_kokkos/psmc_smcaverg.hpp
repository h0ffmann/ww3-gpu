// kokkos/src/ww_kokkos/psmc_smcaverg.hpp
// SMCAverg: 1-2-1 average of a sea-point field on an SMC grid, from the face sums of the
// neighbouring cell values. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3psmcmd.F90:2549-2700 at NOAA-EMC/WW3 03adda91;
// author Jian-Guo Li. Evidence: kokkos/port_report/SMCAVERG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::psmc {

/// One U-face i (1-based) of DO i=1,NUFc at :2585-2615.
/// cvf, aun: CVF, AUN(-9:NSEA) as v[c+9]. ijkufc: IJKUFc(7,NUFc) column-major,
/// IJKUFc(r,i) = ijkufc[(r-1)+7*(i-1)]. ijkufc5, ijkufc6: IJKUFc5, IJKUFc6(NUFc), 0-based.
/// Under W3_B4B the scaling of CNST5 at :2594 is a comment, so no variant here.
/// Under W3_OMPG the two updates are OMP ATOMIC (a compare-exchange loop computing
/// CNST5 + old): run in parallel, the order of the sums into one cell is not fixed.
KOKKOS_INLINE_FUNCTION void smcaverg_ufc_point(int i, const Real* cvf, Real* aun,
                                               const int* ijkufc, const int* ijkufc5,
                                               const int* ijkufc6) {
  const int l = ijkufc5[i - 1];
  const int m = ijkufc6[i - 1];
  const Real cnst5 =
      static_cast<Real>(ijkufc[2 + 7L * static_cast<long>(i - 1)]) * (cvf[m + 9] + cvf[l + 9]);
  if (l > 0) aun[l + 9] = aun[l + 9] + cnst5;
  if (m > 0) aun[m + 9] = aun[m + 9] + cnst5;
}

/// One V-face j (1-based) of DO j=1,NVFc at :2632-2662. avn: AVN(-9:NSEA) as v[c+9].
/// ijkvfc: IJKVFc(7,NVFc) column-major. B4bOmpg: W3_B4B and W3_OMPG both set, :2641, where
/// INT() is cvttss2si (out of range or NaN gives INT_MIN, as fint).
template <bool B4bOmpg = false>
KOKKOS_INLINE_FUNCTION void smcaverg_vfc_point(int j, const Real* cvf, Real* avn,
                                               const int* ijkvfc) {
  const long c = 7L * static_cast<long>(j - 1);
  const int l = ijkvfc[c + 4];
  const int m = ijkvfc[c + 5];
  Real cnst6 = static_cast<Real>(ijkvfc[c + 2]) * (cvf[m + 9] + cvf[l + 9]);
  if constexpr (B4bOmpg) cnst6 = static_cast<Real>(ww::st4::fint(cnst6 * static_cast<Real>(1e6)));
  if (l > 0) avn[l + 9] = avn[l + 9] + cnst6;
  if (m > 0) avn[m + 9] = avn[m + 9] + cnst6;
}

/// One sea cell n (1-based) of DO n=1,NSEA at :2679-2687. cvq: CVQ(-9:NSEA) as v[c+9].
/// ijkcel: IJKCel(4,-9:MCel) at its column n=1, so cell n is ijkcel[4*(n-1) .. 4*(n-1)+3].
KOKKOS_INLINE_FUNCTION void smcaverg_cel_point(int n, Real* cvq, const Real* aun,
                                               const Real* avn, const int* ijkcel) {
  const long c = 4L * static_cast<long>(n - 1);
  const Real cnst3 = static_cast<Real>(0.125) / static_cast<Real>(ijkcel[c + 2]);
  const Real cnst4 = static_cast<Real>(0.125) / static_cast<Real>(ijkcel[c + 3]);
  cvq[n + 9] = aun[n + 9] * cnst4 + avn[n + 9] * cnst3;
}

/// SMCAverg on the host, serially, in the Fortran's loop order. cvq: CVQ(-9:NSEA) as
/// cvq[c+9], averaged in place. nsea, nufc, nvfc, ijkcel, ijkufc, ijkvfc, ijkufc5, ijkufc6,
/// arctc: the w3gdatmd state of the same names (layouts as the point functions). cvf, aun, avn:
/// the automatic arrays CVF, AUN, AVN(-9:NSEA) (:2565), caller-provided, NSEA+10 each.
/// B4bOmpg: W3_B4B and W3_OMPG both set (:2622-2624, :2640-2642, :2669-2671); AUN is divided
/// by 1.0e6 there although its face sums were never scaled (:2594 is a comment).
template <bool B4bOmpg = false>
inline void smcaverg(Real* cvq, int nsea, int nufc, int nvfc, const int* ijkcel,
                     const int* ijkufc, const int* ijkvfc, const int* ijkufc5,
                     const int* ijkufc6, bool arctc, Real* cvf, Real* aun, Real* avn) {
  const long len = static_cast<long>(nsea) + 10;
  for (long k = 0; k < len; ++k) cvf[k] = cvq[k];
  for (long k = 0; k < len; ++k) aun[k] = static_cast<Real>(0.0);
  for (long k = 0; k < len; ++k) avn[k] = static_cast<Real>(0.0);
  const Real cnst0 = cvq[nsea + 9];

  for (int i = 1; i <= nufc; ++i) smcaverg_ufc_point(i, cvf, aun, ijkufc, ijkufc5, ijkufc6);
  if constexpr (B4bOmpg)
    for (long k = 0; k < len; ++k) aun[k] = aun[k] / static_cast<Real>(1.0e6);

  for (int j = 1; j <= nvfc; ++j) smcaverg_vfc_point<B4bOmpg>(j, cvf, avn, ijkvfc);
  if constexpr (B4bOmpg)
    for (long k = 0; k < len; ++k) avn[k] = avn[k] / static_cast<Real>(1.0e6);

  for (int n = 1; n <= nsea; ++n) smcaverg_cel_point(n, cvq, aun, avn, ijkcel);

  if (arctc) cvq[nsea + 9] = cnst0;
}

}  // namespace ww::psmc
