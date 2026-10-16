// kokkos/src/ww_kokkos/pdlib_deallocate_pdlib_global_arrays.hpp
// The nineteen GRIDS(IMOD) unstructured-grid arrays that DEALLOCATE_PDLIB_GLOBAL releases, as
// owning Kokkos views of one grid. A minimal mirror for that routine only: the same arrays live,
// for other ports, in IogrState (iogr_state.hpp, host std::vector) and partly in PdlibGeometry
// (pdlib_params.hpp, which holds the yownodepool PDLIB_* copies, not these). The shim must unify
// the three into one per-grid context before any of them is wired.
// WW3 heritage: WW3/model/src/w3gdatmd.F90:753-778 (TYPE GRID components, all POINTER) and
// W3DIMUG's ALLOCATE (:3235-3260) at h0ffmann/WW3 8e433f99.
// Evidence: kokkos/port_report/DEALLOCATE_PDLIB_GLOBAL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// LayoutLeft with the Fortran's extents from W3DIMUG (MX = NX nodes, MTRI = NTRI elements,
// COUNTOTA = COUNTOT); AREA_SI allocates JAA, IAA, POSI again with NNZ, NX+1, (3,COUNT_MAX)
// (w3triamd.F90:1505-1507). Element types follow the declarations: REAL(8) is double, REAL is
// Real (float), INTEGER is int, INTEGER*1 is std::int8_t.
// Not here, because the routine does not release them: TRIGP, XGRD, YGRD, ZB, IOBP, IOBPD
// (the first and the last two are commented out of its list, w3profsmd_pdlib.F90:7416, :7434-7435).
#pragma once

#include <Kokkos_Core.hpp>

#include <cstdint>

#include "real.hpp"

namespace ww::pdlib {

/// GRIDS(IMOD)'s unstructured-grid arrays released by DEALLOCATE_PDLIB_GLOBAL, one grid.
/// A default-constructed view stands for a disassociated POINTER.
template <class MemSpace>
struct PdlibGlobalArrays {
  using memory_space = MemSpace;
  template <class T>
  using V = Kokkos::View<T, Kokkos::LayoutLeft, MemSpace>;

  V<double*> si;              ///< SI(MX), REAL(8), w3gdatmd.F90:765, :3236
  V<double*> tria;            ///< TRIA(MTRI), REAL(8), :777, :3240
  V<Real**> crossdiff;        ///< CROSSDIFF(6,MTRI), REAL, :778, :3241
  V<double**> ien;            ///< IEN(MTRI,6), REAL(8), :765, :3242
  V<double**> len;            ///< LEN(MTRI,3), REAL(8), :765, :3243
  V<Real**> angle;            ///< ANGLE(MTRI,3), REAL, :768, :3244
  V<Real**> angle0;           ///< ANGLE0(MTRI,3), REAL, :768, :3245
  V<int*> ccon;               ///< CCON(MX), :770, :3246
  V<int*> countcon;           ///< COUNTCON(MX), :770, :3247
  V<int*> index_cell;         ///< INDEX_CELL(MX+1), :772, :3248
  V<int*> ie_cell;            ///< IE_CELL(COUNTOTA), :770, :3249
  V<int*> pos_cell;           ///< POS_CELL(COUNTOTA), :771, :3250
  V<int*> iaa;                ///< IAA(NX+1), :772, :3251
  V<int*> jaa;                ///< JAA(NNZ), :772, :3252
  V<int**> posi;              ///< POSI(3,COUNTOTA), :772, :3253
  V<int*> i_diag;             ///< I_DIAG(NX), :773, :3254
  V<int***> ja_ie;            ///< JA_IE(3,3,MTRI), :773, :3255
  V<std::int8_t*> iobdp;      ///< IOBDP(MX), INTEGER*1, :775, :3258
  V<std::int8_t*> iobpa;      ///< IOBPA(MX), INTEGER*1, :775, :3259
};

}  // namespace ww::pdlib
