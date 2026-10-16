// kokkos/src/ww_kokkos/snl1_f1f1f1.hpp
// F1F1F1: the table of ratios f1/f of the GQM form of NL1, one of seven fixed or geometric
// layouts chosen by IQ_OM1; a table INSNLGQM builds once.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl1md.F90:1349-1500 at NOAA-EMC/WW3 03adda91, adapted
// from TOMAWAC (E. Gagnaire-Renou, G. Mattarolo). Evidence: kokkos/port_report/F1F1F1_KOKKOS.md.
// Host only: std::pow is glibc's pow, which the object calls for IQ_OM1 = 5.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

namespace ww::snl1 {

namespace f1f1f1_detail {
/// RAISON = 9.D0**(1.D0/DBLE(NF1)) with NF1 = 20 (:1452): gfortran folds it with the exponent
/// known, correctly rounded, and calls no pow; glibc's pow is not guaranteed to round the same.
inline constexpr double kRaison4 = 0x1.1dba3f92c888bp+0;
#if defined(__GNUC__) && !defined(__clang__) && !defined(__CUDACC__)
static_assert(kRaison4 == __builtin_pow(9.0, 1.0 / 20.0));
#endif

/// Bits of the diagnostic code, one per WRITE the Fortran does when NF1 does not match the
/// layout; the shim writes the message with NF1 as it was on entry (IQ_OM1 = 4 changes it).
inline constexpr int kNf1Case1 = 1;   // :1382, '#1 Incorrect value for NF1'
inline constexpr int kNf1Case2 = 2;   // :1401, '#2 Incorrect value for NF1'
inline constexpr int kNf1Case3 = 4;   // :1432, 'Incorrect value for NF1'
inline constexpr int kNf1Case4 = 8;   // :1448, 'Incorrect value for NF1'
inline constexpr int kNf1Case7 = 16;  // :1475, 'Incorrect value for NF1'
}  // namespace f1f1f1_detail

/// F1F1F1, :1349-1500. f1sf is 0-based (Fortran F1SF(1:NF1+1), assumed size). Returns 0 or the
/// f1f1f1_detail bit of the WRITE the Fortran does; the table is filled in every case.
/// IQ_OM1 = 1, 2, 3, 7 write 15, 27, 12, 21 entries whatever NF1 is; IQ_OM1 = 4 sets NF1 to 40
/// and writes 41 entries; IQ_OM1 outside 1..7 writes nothing (:1380-1498).
inline int f1f1f1(double* f1sf, int& nf1, int iq_om1) {
  using namespace f1f1f1_detail;
  int diag = 0;
  if (iq_om1 == 1) {
    if (nf1 != 14) diag = kNf1Case1;
    f1sf[0] = 0.30;
    f1sf[1] = 0.40;
    f1sf[2] = 0.50;
    f1sf[3] = 0.60;
    f1sf[4] = 0.70;
    f1sf[5] = 0.80;
    f1sf[6] = 0.90;
    f1sf[7] = 1.00;
    f1sf[8] = 1.11;
    f1sf[9] = 1.25;
    f1sf[10] = 1.42;
    f1sf[11] = 1.67;
    f1sf[12] = 2.00;
    f1sf[13] = 2.50;
    f1sf[14] = 3.30;
  } else if (iq_om1 == 2) {
    if (nf1 != 26) diag = kNf1Case2;
    f1sf[0] = 0.32;
    f1sf[1] = 0.35;
    f1sf[2] = 0.39;
    f1sf[3] = 0.44;
    f1sf[4] = 0.50;
    f1sf[5] = 0.56;
    f1sf[6] = 0.63;
    f1sf[7] = 0.70;
    f1sf[8] = 0.78;
    f1sf[9] = 0.86;
    f1sf[10] = 0.92;
    f1sf[11] = 0.97;
    f1sf[12] = 1.00;
    f1sf[13] = 1.03;
    f1sf[14] = 1.08;
    f1sf[15] = 1.13;
    f1sf[16] = 1.20;
    f1sf[17] = 1.28;
    f1sf[18] = 1.37;
    f1sf[19] = 1.48;
    f1sf[20] = 1.50;
    f1sf[21] = 1.65;
    f1sf[22] = 1.85;
    f1sf[23] = 2.10;
    f1sf[24] = 2.40;
    f1sf[25] = 2.70;
    f1sf[26] = 3.20;
  } else if (iq_om1 == 3) {
    if (nf1 != 11) diag = kNf1Case3;
    f1sf[0] = 0.30;
    f1sf[1] = 0.48;
    f1sf[2] = 0.64;
    f1sf[3] = 0.78;
    f1sf[4] = 0.90;
    f1sf[5] = 1.00;
    f1sf[6] = 1.12;
    f1sf[7] = 1.28;
    f1sf[8] = 1.50;
    f1sf[9] = 1.80;
    f1sf[10] = 2.40;
    f1sf[11] = 3.40;
  } else if (iq_om1 == 4) {
    if (nf1 != 40) diag = kNf1Case4;
    // :1451-1460, with NF1 = 20 and M = 10: 1/3 at F1SF(11), geometric both ways.
    const int m = 10;
    nf1 = 20;
    const double raison = kRaison4;
    f1sf[m] = 1.0 / 3.0;
    nf1 = 2 * m + nf1;
    for (int i = m + 2; i <= nf1 + 1; ++i) f1sf[i - 1] = f1sf[i - 2] * raison;
    for (int i = m; i >= 1; --i) f1sf[i - 1] = f1sf[i] / raison;
  } else if (iq_om1 == 5) {
    // :1462-1466. The object computes RAISON only when the loop runs (NF1 >= 1).
    const double raison = std::pow(9.0, 1.0 / static_cast<double>(nf1));
    f1sf[0] = 1.0 / 3.0;
    for (int i = 2; i <= nf1 + 1; ++i) f1sf[i - 1] = f1sf[i - 2] * raison;
  } else if (iq_om1 == 6) {
    // :1468-1472.
    const double raison = (3.0 - 1.0 / 3.0) / static_cast<double>(nf1);
    f1sf[0] = 1.0 / 3.0;
    for (int i = 2; i <= nf1 + 1; ++i) f1sf[i - 1] = f1sf[i - 2] + raison;
  } else if (iq_om1 == 7) {
    if (nf1 != 20) diag = kNf1Case7;
    f1sf[0] = 1.0 / 3.0;
    f1sf[1] = 0.40;
    f1sf[2] = 0.46;
    f1sf[3] = 0.52;
    f1sf[4] = 0.60;
    f1sf[5] = 0.70;
    f1sf[6] = 0.79;
    f1sf[7] = 0.86;
    f1sf[8] = 0.92;
    f1sf[9] = 0.97;
    f1sf[10] = 1.00;
    f1sf[11] = 1.04;
    f1sf[12] = 1.10;
    f1sf[13] = 1.18;
    f1sf[14] = 1.28;
    f1sf[15] = 1.42;
    f1sf[16] = 1.60;
    f1sf[17] = 1.84;
    f1sf[18] = 2.14;
    f1sf[19] = 2.52;
    f1sf[20] = 3.00;
  }
  return diag;
}

}  // namespace ww::snl1
