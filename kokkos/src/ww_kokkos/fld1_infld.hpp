// kokkos/src/ww_kokkos/fld1_infld.hpp
// INFLD: copies the grid's tail parameters into the module state of the Reichl flux
// parametrisations (FLD1, FLD2). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3fld1md.F90:781-875 at NOAA-EMC/WW3 03adda91 (state :76-88);
// author B. G. Reichl.
// Evidence: kokkos/port_report/INFLD_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "real.hpp"

namespace ww::fld1 {

/// The SAVE variables of W3FLD1MD that INFLD sets (w3fld1md.F90:77-84). They are
/// `!$omp threadprivate` (:79, :87-88): under OpenMP each thread has its own copy, set by
/// that thread's first call of W3FLD1 or W3FLD2.
struct TailState {
  int tail_choice;
  Real tail_level;
  Real tail_transition_ratio1;
  Real tail_transition_ratio2;
};

/// INFLD. No dummies; reads TAIL_ID, TAIL_LEV, TAIL_TRAN1, TAIL_TRAN2 of W3GDATMD (the grid
/// that is current when it is called) and writes the module state.
inline void infld(int tail_id, Real tail_lev, Real tail_tran1, Real tail_tran2, TailState& st) {
  st.tail_choice = tail_id;
  st.tail_level = tail_lev;
  st.tail_transition_ratio1 = tail_tran1;
  st.tail_transition_ratio2 = tail_tran2;
}

}  // namespace ww::fld1
