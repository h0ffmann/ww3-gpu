// kokkos/src/ww_kokkos/snl5_insnl5.hpp
// INSNL5 of W3SNL5MD: copies the NL5 grid parameters into W3GKEMD's state, has the kernel
// tables built and written (PrepKernelIO 'WRITE'), and returns the quartet count.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3snl5md.F90:433-541 at NOAA-EMC/WW3 03adda91;
// author Q. Liu. Evidence: kokkos/port_report/INSNL5_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <utility>

#include "real.hpp"

namespace ww::snl5 {

/// The W3GKEMD module variables INSNL5 sets and reads back (w3gkemd.F90:165-209).
/// PrepKernelIO clamps the first five in place (w3gkemd.F90:1256-1260) and sets qi_nnz.
struct GkeConfig {
  Real qr_depth = 0;
  Real qr_oml = 0;
  int qi_disc = 0;
  int qi_kev = 0;
  int qi_interp = 0;
  std::int64_t qi_nnz = 0;  // INTEGER(KIND=8)
};

/// INSNL5, :513-526. A host function: it runs once, from W3IOGR when it writes the
/// mod_def (w3iogrmd.F90:1761, under W3_NL5).
///
/// sig is SIG(1:NK) and th is TH(1:NTH), contiguous, 0-based (the Fortran passes packed
/// copies through _gfortran_internal_pack, :522). qi5nnz is W3GDATMD's QI5NNZ.
///
/// prep_kernel_io(nk, nth, sig, th, gke) stands for CALL PrepKernelIO(..., 'WRITE')
/// (w3gkemd.F90:1201), which is not translated here: it allocates the kernel arrays and
/// writes the gkev?_d????.cfg file. It must leave gke as the Fortran leaves the module
/// state, clamped values and qi_nnz included.
///
/// The WRITE(SCREEN, ...) block of :529-537 is the caller's: it prints gke after this
/// returns (the clamped values, not QR5DPT etc.) and QI5PMX, on IAPROC == NAPOUT only.
template <class PrepKernelIO>
inline void insnl5(int nk, int nth, const Real* sig, const Real* th, Real qr5dpt, Real qr5oml,
                   int qi5dis, int qi5kev, int qi5ipl, std::int64_t& qi5nnz, GkeConfig& gke,
                   PrepKernelIO&& prep_kernel_io) {
  gke.qr_depth = qr5dpt;
  gke.qr_oml = qr5oml;
  gke.qi_disc = qi5dis;
  gke.qi_kev = qi5kev;
  gke.qi_interp = qi5ipl;
  std::forward<PrepKernelIO>(prep_kernel_io)(nk, nth, sig, th, gke);
  qi5nnz = gke.qi_nnz;
}

}  // namespace ww::snl5
