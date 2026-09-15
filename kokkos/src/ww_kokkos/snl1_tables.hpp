// kokkos/src/ww_kokkos/snl1_tables.hpp
// The precomputed quadruplet addressing and weights of the DIA: INSNL1's output.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE INSNL1, lines 483-779; the tables themselves live in W3ADATMD.
//
// Index convention: WW3 addresses a spectral bin as ISP = ITH + (IFR-1)*NTH with
// ITH, IFR and ISP all 1-based; this port stores isp = ith + ifr*nth with all
// three 0-based, so every table entry here is the Fortran value minus one. The
// entries are allowed to be negative: INSNL1 clamps IF3..IF6 to 0, which makes
// Fortran indices as low as 1-NTH, i.e. -NTH here. W3SNL1 answers that by giving
// its scratch arrays a lower bound of 1-NTH; snl1_dia.cpp answers it by offsetting
// scratch by +nth, so a stored index j is read at scratch slot j + nth.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "snl1_config.hpp"

namespace ww::snl1 {

/// INSNL1's output: the scalars of W3ADATMD plus the 33 device tables.
///
/// The index tables are held as C arrays of Views because that is how W3SNL1
/// reads them -- IP11..IP14 are one interpolation stencil, IP21..IP24 the mirror
/// image -- and because a View is a copyable handle, so the whole struct can be
/// captured by value into a kernel.
struct Tables {
  int nfr;     ///< NFR     = NK
  int nfrhgh;  ///< NFRHGH  highest frequency of the extended spectrum
  int nfrchg;  ///< NFRCHG  highest frequency the interactions are computed for
  int nspecx;  ///< NSPECX  = NFRCHG*NTH
  int nspecy;  ///< NSPECY  = NFRHGH*NTH
  int nspec;   ///< NSPEC   = NK*NTH

  Real dal1, dal2, dal3;  ///< DAL1..DAL3, the lambda-dependent weights
  Real awg[8];            ///< AWG1..AWG8, the interpolation weights
  Real swg[8];            ///< SWG1..SWG8, their squares

  /// ip[0][j] = IP1(j+1)1..4, ip[1][j] = IP2(j+1); likewise im. Size NSPECX.
  IntView1D ip[2][4];
  IntView1D im[2][4];
  /// ic[j][0] = IC(j+1)1, ic[j][1] = IC(j+1)2. Size NSPEC.
  IntView1D ic[8][2];
  /// AF11, the (f**11) scaling array. Size NSPECX.
  RealView1D af11;
};

/// INSNL1 (w3snl1md.F90 lines 483-779). Host-side preprocessing: it computes the
/// tables in host mirrors and deep-copies them to the device once, at set-up.
/// `sig` is SIG(1:NK) on the host.
Tables make_tables(const Config& c, ConstHostRealView1D sig);

}  // namespace ww::snl1
