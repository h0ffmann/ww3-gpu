// kokkos/src/ww_kokkos/snls_expand.hpp
// EXPAND of W3SNLS (NLS): the spectrum widened by one direction on each side, a zero row below
// the first frequency and two tail frequencies, and the same spectrum divided by the wavenumber.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snlsmd.F90:597-651 at NOAA-EMC/WW3 03adda91, a routine
// contained in W3SNLS and called once from it (:276); author H. L. Tolman.
// Evidence: kokkos/port_report/W3SNLSMD_EXPAND_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::snls {

/// EXPAND ( PSPC, SPEC ), arrays 0-based. pspc and spec are PSPC(0:NTH+1,0:NFR+2) and
/// SPEC(0:NTH+1,0:NFR+2), column-major: element (ith, ifr) is [ith + ifr*(nth+2)].
/// Host-associated W3SNLS state follows: nth, nfr (NTH, NFR => NK), a = A(NTH,NFR) of the
/// caller (Fortran base 1), fachfa = FACHFA, xwn = XWN(NFR+2) (Fortran base 1).
/// W3SNLS passes UP as PSPC and UN as SPEC (:276).
/// PSPC(:,0) is not written, as in the Fortran (:643-645); see the port report.
KOKKOS_INLINE_FUNCTION void expand(Real* pspc, Real* spec, int nth, int nfr, const Real* a,
                                   Real fachfa, const Real* xwn) {
  const int nthx = nth + 2;
  auto sp = [&](int ith, int ifr) -> Real& { return spec[ith + ifr * nthx]; };

  // :634
  for (int ith = 0; ith <= nth + 1; ++ith) sp(ith, 0) = static_cast<Real>(0.);
  // :636
  for (int ifr = 1; ifr <= nfr; ++ifr)
    for (int ith = 1; ith <= nth; ++ith) sp(ith, ifr) = a[(ith - 1) + (ifr - 1) * nth];
  // :637-638
  for (int ith = 1; ith <= nth; ++ith) sp(ith, nfr + 1) = sp(ith, nfr) * fachfa;
  for (int ith = 1; ith <= nth; ++ith) sp(ith, nfr + 2) = sp(ith, nfr + 1) * fachfa;
  // :640-641; the rows written (NTH+1, 0) are never the rows read (1, NTH).
  for (int ifr = 1; ifr <= nfr + 2; ++ifr) sp(nth + 1, ifr) = sp(1, ifr);
  for (int ifr = 1; ifr <= nfr + 2; ++ifr) sp(0, ifr) = sp(nth, ifr);
  // :643-645, a true division (divps/divss in the object), not a reciprocal.
  for (int ifr = 1; ifr <= nfr + 2; ++ifr) {
    const Real k = xwn[ifr - 1];
    for (int ith = 0; ith <= nth + 1; ++ith) pspc[ith + ifr * nthx] = sp(ith, ifr) / k;
  }
}

}  // namespace ww::snls
