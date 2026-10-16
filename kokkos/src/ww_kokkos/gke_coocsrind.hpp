// kokkos/src/ww_kokkos/gke_coocsrind.hpp
// CooCsrInd of W3GKEMD: the index map from coordinate (COO) to compressed sparse row (CSR) storage.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:874-979 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/COOCSRIND_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

namespace ww::gke {

/// CooCsrInd(nrow, nnz, ir, jc, ind_translate, iao), :874-979. Integer only; runs on one
/// thread (the counting and the fill scatter into iao(ir(k)) in k order).
/// ir[nnz], jc[nnz], ind_translate[nnz] (base 1), iao[nrow+1] (base 1). ir holds Fortran
/// (1-based) row numbers; ind_translate and iao are written as Fortran (1-based) positions.
/// jc is read by the Fortran (:959) into a variable never used; it is kept in the signature.
KOKKOS_INLINE_FUNCTION void coocsrind(int nrow, std::int64_t nnz, const int* ir, const int* jc,
                                      int* ind_translate, int* iao) {
  // gfortran converts the INTEGER(8) loop limit nnz to the kind of k (INTEGER(4)) before the
  // two k loops (:942, :957); the conversion is modular, as in C++20.
  const int nnz4 = static_cast<int>(nnz);
  (void)jc;
  // :941.
  for (int j = 0; j < nrow + 1; ++j) iao[j] = 0;
  // :942-944.
  for (int k = 1; k <= nnz4; ++k) iao[ir[k - 1] - 1] = iao[ir[k - 1] - 1] + 1;
  // :949-954.
  int k = 1;
  for (int j = 1; j <= nrow + 1; ++j) {
    const int k0 = iao[j - 1];
    iao[j - 1] = k;
    k = k + k0;
  }
  // :957-969.
  for (k = 1; k <= nnz4; ++k) {
    const int i = ir[k - 1];
    const int iad = iao[i - 1];
    ind_translate[iad - 1] = k;
    iao[i - 1] = iad + 1;
  }
  // :972-975.
  for (int j = nrow; j >= 1; --j) iao[j] = iao[j - 1];
  iao[0] = 1;
}

}  // namespace ww::gke
