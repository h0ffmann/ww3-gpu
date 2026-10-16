// kokkos/src/ww_kokkos/pdlib_deallocate_pdlib_global.hpp
// DEALLOCATE_PDLIB_GLOBAL(IMOD): releases nineteen unstructured-grid arrays of GRIDS(IMOD).
// W3INIT calls it under W3_PDLIB, after W3IOPP (w3initmd.F90:1364-1366).
// No arithmetic. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:7345-7441 at h0ffmann/WW3 8e433f99 (the same
// lines at NOAA-EMC/WW3 03adda91); A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/DEALLOCATE_PDLIB_GLOBAL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles, which is the one
// DEALLOCATE (:7415-7437). Not ported: the W3_S branch (STRACE's USE and IENT, :7390-7392,
// :7407-7409; the routine never calls STRACE) and the W3_DEBUGINIT USE of IAPROC (:7396-7398),
// neither in that switch and neither with an executable statement. The USEs of pdlib_abort and
// IMEM (:7394-7395) are never referenced.
//
// What the -O3 object does, item by item in list order (GIMPLE of the CMake Release build):
// load GRIDS(IMOD)%X%data; if it is NULL, _gfortran_runtime_error_at with "Attempt to DEALLOCATE
// unallocated 'grids'" and the line (7415 for SI, the statement's line, then each item's own
// line), which terminates the program; else free it and store NULL in the descriptor's data
// field. Items before a disassociated one are already released when the program stops. Only
// the data field is reset: bounds, GUGINIT (w3gdatmd.F90:3285) and the W3SETG aliases
// (SI => GRIDS(IMOD)%SI, w3gdatmd.F90:2401, and the others) are left as they were, the aliases
// dangling.
//
// Translation: the struct of grid IMOD is the argument (one context per grid, as W3SETG
// selects); a View release is `view = {}`, which drops this reference and frees the
// allocation when no other View holds it, where the Fortran frees at once and leaves every
// alias dangling. The two agree wherever the Fortran is defined: reading a dangling alias is
// undefined. An unmanaged View that a shim binds to Fortran memory frees nothing on `= {}`:
// with such a binding the Fortran DEALLOCATE has to stay in Fortran. A disassociated item ends
// the routine with exit code 2 (the status a gfortran 16.2 program exits with after this
// error, port report) and the Fortran's line, after the items before it were released, as
// the Fortran stops; the shim writes the message and stops, as for W3DIMS and W3IOGR: a host
// routine never prints.
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_deallocate_pdlib_global_arrays.hpp"

namespace ww::pdlib {

/// What DEALLOCATE_PDLIB_GLOBAL ends with. exit_code 0: it returned. Otherwise 2, the status the
/// gfortran run time exits with, and line, the N of its "At line N of file
/// w3profsmd_pdlib.F90"; the caller writes that and "Fortran runtime error: Attempt to
/// DEALLOCATE unallocated 'grids'" and stops.
struct DeallocatePdlibGlobalStatus {
  int exit_code = 0;
  int line = 0;
};

namespace dpg_detail {

inline constexpr int kExitGfortranRuntime = 2;

/// Fortran ASSOCIATED for a view: gfortran tests data != NULL, and its ALLOCATE of an empty
/// array still returns a non-NULL block (malloc(1)), so a tracked empty view counts as
/// associated; an unmanaged view of Fortran memory has use_count 0 and is tested by data.
template <class View>
bool associated(const View& v) {
  return v.data() != nullptr || v.use_count() > 0;
}

/// One item of the DEALLOCATE list: false, and v untouched, when it is disassociated.
template <class View>
[[nodiscard]] bool release(View& v) {
  if (!associated(v)) return false;
  v = {};
  return true;
}

}  // namespace dpg_detail

/// DEALLOCATE_PDLIB_GLOBAL(IMOD), w3profsmd_pdlib.F90:7345-7441: g is GRIDS(IMOD).
/// Host only; no kernel is launched. Each line is the one _gfortran_runtime_error_at reports.
template <class MemSpace>
[[nodiscard]] DeallocatePdlibGlobalStatus deallocate_pdlib_global(PdlibGlobalArrays<MemSpace>& g) {
  using dpg_detail::release;
  constexpr int stop = dpg_detail::kExitGfortranRuntime;
  if (!release(g.si)) return {stop, 7415};  // :7417; gfortran reports the statement's line
  if (!release(g.tria)) return {stop, 7418};
  if (!release(g.crossdiff)) return {stop, 7419};
  if (!release(g.ien)) return {stop, 7420};
  if (!release(g.len)) return {stop, 7421};
  if (!release(g.angle)) return {stop, 7422};
  if (!release(g.angle0)) return {stop, 7423};
  if (!release(g.ccon)) return {stop, 7424};
  if (!release(g.countcon)) return {stop, 7425};
  if (!release(g.index_cell)) return {stop, 7426};
  if (!release(g.ie_cell)) return {stop, 7427};
  if (!release(g.pos_cell)) return {stop, 7428};
  if (!release(g.iaa)) return {stop, 7429};
  if (!release(g.jaa)) return {stop, 7430};
  if (!release(g.posi)) return {stop, 7431};
  if (!release(g.i_diag)) return {stop, 7432};
  if (!release(g.ja_ie)) return {stop, 7433};
  // IOBP, IOBPD (:7434-7435) are commented out of the list and kept.
  if (!release(g.iobdp)) return {stop, 7436};
  if (!release(g.iobpa)) return {stop, 7437};
  return {};
}

}  // namespace ww::pdlib
