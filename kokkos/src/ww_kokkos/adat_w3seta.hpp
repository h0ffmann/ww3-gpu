// kokkos/src/ww_kokkos/adat_w3seta.hpp
// W3SETA, ported to Kokkos: select model IMOD in W3ADATMD. Host-side, no View is
// created and no kernel runs. Phase 1, translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3SETA, WW3/model/src/w3adatmd.F90:2638-3100 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman.
// Ported as switches/switch_lab_shrd compiles it: W3_PR3 :2773-2782, :3018-3026 and
// W3_NL1 :2784-2810, :3037-3073 are in; W3_PR2, W3_IC3, W3_SMC, W3_PR1, W3_MPI,
// W3_S STRACE and the W3_T print are not compiled and not ported.
//
// What a pointer alias becomes here. The Fortran re-points module pointers (DW,
// CG, FL_ALL, ...) at the components of WADATS(IMOD); the C++ has no module
// pointers, a consumer indexes the model vector instead. The aliases fall into
// groups that W3SETA re-points together under one condition, and a group whose
// condition is false keeps pointing at the model of an earlier W3SETA. So the
// state W3SETA leaves is one model number per group, W3setaTargets (in
// adat_wadat.hpp, so that W3DIMA and W3DMNL can set it), 0 while no W3SETA
// has associated the group (the Fortran declares these pointers without
// => NULL(), w3adatmd.F90:578-690, so their status is undefined until then).
// IADATA (:2746, Wadats::iadata) is not a group target: W3XETA writes it too
// (:3174) and re-points only HS .. USERO, so after a W3XETA the unconditional
// aliases (NSEALM, AINIT, AINIT2, FL_ALL, ...) may name another model than IADATA.
// A model number stands for the components of WADATS(k) as they are now; the
// Fortran copies each array component's association at the =>, which is the same
// thing while nothing re-allocates a component after W3SETA: W3DIMA and W3DMNL
// refuse to (:976, :2525), and model/src at the pin has no other ALLOCATE,
// DEALLOCATE, NULLIFY or => of these components or aliases (the name matches
// outside W3ADATMD are local arrays and USE renames).
// The accessors at the end turn a target into the model's struct, nullptr for 0.
//
// The conditions are read as the Fortran reads them, through the aliases set a few
// lines above, so from model IMOD: AINIT (:2826) is WADATS(IMOD)%AINIT, FL_ALL
// (:2947) WADATS(IMOD)%FL_ALL, NLINIT (:3038) WADATS(IMOD)%NLINIT.
// INPUTS(IMOD)%INFLAGS1(2,3,5,6) (:2974-2997) are the caller's, of model IMOD; GTYPE (:3028) is
// W3GDATMD's of the grid the last W3SETG selected, not necessarily IMOD's.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE) messages and
// EXTCDE (returned as the code EXTCDE would exit with, W3setaStatus); NDSE and
// NDST, which only those use.
#pragma once

#include <cstddef>
#include <vector>

#include "adat_w3dmnl.hpp"
#include "adat_w3naux.hpp"
#include "adat_wadat.hpp"

namespace ww::adat {

/// EXTCDE exit codes of W3SETA, returned instead of stopping the program.
enum class W3setaStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NADATA == -1, :2729-2732
  illegal_model = 2,          ///< IMOD < 1 or IMOD > NADATA, :2734-2737
  /// Not a WW3 code: nl1.models has no entry imod-1, so NLINIT (:3038) cannot be
  /// read. In the Fortran NLINIT lives inside WADATS(IMOD).
  nl1_not_sized = -1,
};

/// @brief The state of other modules W3SETA reads.
struct W3setaInputs {
  /// INPUTS(IMOD)%INFLAGS1(2), (3), (5), (6) of W3IDATMD: FLCUR, FLWIND, FLTAUA,
  /// FLRHOA of model IMOD (w3idatmd.F90:572, :585, :587, :588).
  bool inflags1_2 = false, inflags1_3 = false, inflags1_5 = false, inflags1_6 = false;
  int gtype = 0;    ///< W3GDATMD GTYPE, of the last W3SETG
  int ungtype = 3;  ///< W3GDATMD UNGTYPE, PARAMETER (w3gdatmd.F90:627)
};

/// @brief W3SETA(IMOD, NDSE, NDST), shared memory (W3_SHRD) build with W3_PR3 and
///        W3_NL1.
/// @param reg  WADATS, NADATA and IADATA (in/out: IADATA = IMOD on ok).
/// @param nl1  the NL1 parts of WADATS (read: NLINIT of model IMOD).
/// @param t    the alias targets (in/out).
/// @param imod model number, 1-based as in the Fortran.
/// @param in   the W3IDATMD and W3GDATMD values the conditions read.
/// @return W3setaStatus::ok, or the code EXTCDE would have exited with; on any code
///         other than ok nothing has been written.
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3setaStatus w3seta(Wadats<ExecSpace>& reg, const WadatsNl1<ExecSpace>& nl1,
                    W3setaTargets& t, int imod, const W3setaInputs& in) {
  if (reg.nadata == -1) return W3setaStatus::grids_not_initialized;      // :2729
  if (imod < 1 || imod > reg.nadata) return W3setaStatus::illegal_model;  // :2734
  if (static_cast<std::size_t>(imod) > nl1.models.size()) return W3setaStatus::nl1_not_sized;

  const std::size_t i = static_cast<std::size_t>(imod - 1);
  const Wadat<ExecSpace>& w = reg.wadats[i];

  reg.iadata = imod;  // :2746
  t.base = imod;      // ITIME .. FL_ALL, NMX0 .. NCENT, NFR .. NLINIT, :2751-2809

  if (w.ainit) {  // :2826
    t.ainit = imod;
    t.x = 0;
    if (w.fl_all) {  // :2947
      t.fl_all = imod;
      if (in.inflags1_2) t.cur = imod;           // :2974
      if (in.inflags1_3) t.wind = imod;          // :2981
      if (in.inflags1_5) t.taua = imod;          // :2990
      if (in.inflags1_6) t.rhoa = imod;          // :2997
      if (in.gtype == in.ungtype) t.iter = imod;  // :3028
    }
  }
  if (nl1.models[i].nlinit) t.nlinit = imod;  // :3038
  return W3setaStatus::ok;
}

namespace detail {

template <class T>
T* model_at(std::vector<T>& v, int imod) {
  return imod >= 1 && static_cast<std::size_t>(imod) <= v.size()
             ? &v[static_cast<std::size_t>(imod - 1)]
             : nullptr;
}

}  // namespace detail

/// WADATS(target) of a group: selected(reg, t.base) for NSEALM, AINIT, FL_ALL,
/// selected(reg, t.fl_all) for CG.
template <class ExecSpace>
Wadat<ExecSpace>* selected(Wadats<ExecSpace>& reg, int target) {
  return detail::model_at(reg.wadats, target);
}

/// With t.base, the target of ITIME, IPASS, IDLAST, ITSTEP, FLCOLD, FLIWND, AINIT2
/// (W3NAUX's WadatScalars, parallel to reg.wadats); W3XETA reads AINIT2 here (:3179).
inline WadatScalars* selected_scalars(std::vector<WadatScalars>& scalars, int target) {
  return detail::model_at(scalars, target);
}

/// With t.base, the target of NSPECX and NLINIT; with t.nlinit, of IP11 .. AF11.
template <class ExecSpace>
WadatNl1<ExecSpace>* selected_nl1(WadatsNl1<ExecSpace>& nl1, int target) {
  return detail::model_at(nl1.models, target);
}

}  // namespace ww::adat
