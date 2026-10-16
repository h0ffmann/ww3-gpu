// kokkos/src/ww_kokkos/adat_w3dmnl.hpp
// W3DMNL, ported to Kokkos: allocate the DIA (W3_NL1) address and scaling tables of
// W3ADATMD for model IMOD and store NSPECX. Host-side, outside any kernel. Phase 1,
// translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3DMNL, WW3/model/src/w3adatmd.F90:2426-2623 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman.
// Called once per model by INSNL1 (w3snl1md.F90:654) with NSP = NSPEC and
// NSPX = NSPECX. Ported as switches/switch_lab_shrd compiles it: W3_NL1 is set, so
// the NLINIT check (:2524-2529), the ALLOCATE (:2539-2573) and NSPECX = NSPX
// (:2593-2595) are in; the W3_S STRACE and the W3_T prints are not compiled and not
// ported. Without W3_NL1 the routine reduces to the two checks and W3SETA, not ported.
//
// Relation to W3DIMA (adat_w3dima.hpp, adat_wadat.hpp): the components here are the
// #ifdef W3_NL1 block of the same TYPE WADAT (w3adatmd.F90:531-547), which Wadat
// leaves out. WadatsNl1::models[imod-1] is the NL1 part of Wadats::wadats[imod-1];
// NADATA and IADATA are Wadats' (one set of module counters for both). Only what
// W3DMNL reads or writes is here: the INSNL1 scalars NFR, NFRHGH, NFRCHG, NSPECY,
// DALn, AWGn, SWGn belong to an INSNL1 port.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3adatmd.F90 with WW3's Release flags and the switch_lab_shrd defines:
//  - The 33 arrays are malloc'd in the order of the ALLOCATE list, IP11..IM24 and
//    AF11 of NSPX elements, IC11..IC82 of NSP, all with lower bound 1 and 4-byte
//    elements; nothing is stored into them (no fill).
//  - The first NULL malloc jumps to CHECK_ALLOC_STATUS (EXTCDE 99); the arrays after
//    it are not allocated.
//  - NLINIT = .TRUE. is stored into WADATS(IMOD) before W3SETA(IMOD); NSPECX = NSPX
//    is stored through the module pointer after it, i.e. into WADATS(IMOD)%NSPECX.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE) messages and
// EXTCDE (returned as the code EXTCDE would exit with, W3dmnlStatus); W3SETA's
// pointer aliasing (:2584), except IADATA.
#pragma once

#include <cstddef>
#include <exception>
#include <vector>

#include <Kokkos_Core.hpp>

#include "adat_w3dima.hpp"
#include "adat_wadat.hpp"
#include "real.hpp"

namespace ww::adat {

/// @brief The W3_NL1 components of TYPE WADAT (w3adatmd.F90:531-547) that W3DMNL
///        allocates or writes, for one model.
/// @details Index map: every table is Fortran X(1:N), X(i) -> x(i-1). IPnn, IMnn,
///          ICnn are INTEGER (int), AF11 is REAL (ww::Real). Default-constructed
///          Views are unallocated and nlinit is false, the state W3NAUX leaves
///          (w3adatmd.F90:809 sets NLINIT = .FALSE.; NSPECX is not set there).
template <class ExecSpace>
struct WadatNl1 {
  using Mem = typename ExecSpace::memory_space;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Mem>;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, Mem>;

  // (NSPX), :2539-2554
  I1 ip11, ip12, ip13, ip14, im11, im12, im13, im14;
  I1 ip21, ip22, ip23, ip24, im21, im22, im23, im24;
  // (NSP), :2555-2570
  I1 ic11, ic12, ic21, ic22, ic31, ic32, ic41, ic42;
  I1 ic51, ic52, ic61, ic62, ic71, ic72, ic81, ic82;
  // (NSPX), :2571
  R1 af11;

  int nspecx = 0;  ///< W3NAUX leaves it unset; INSNL1 writes the alias (w3snl1md.F90:650) first
  bool nlinit = false;
};

/// @brief The NL1 parts of WADATS(NADATA); models[imod-1] goes with
///        Wadats::wadats[imod-1]. The caller sizes it to NADATA, as W3NAUX's
///        ALLOCATE(WADATS(NGRIDS)) (w3adatmd.F90:790) sizes the one Fortran array.
template <class ExecSpace>
struct WadatsNl1 {
  std::vector<WadatNl1<ExecSpace>> models;
};

/// EXTCDE exit codes of W3DMNL, returned instead of stopping the program.
enum class W3dmnlStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NGRIDS == -1, :2514-2517
  illegal_model = 2,          ///< IMOD < 1 or IMOD > NADATA, :2519-2522
  already_allocated = 3,      ///< WADATS(IMOD)%NLINIT, :2525-2528
  alloc_failed = 99,          ///< CHECK_ALLOC_STATUS (:2573), w3macros.h:49-51
  /// Not a WW3 code: nl1.models has no entry imod-1. In the Fortran the NL1
  /// components live inside WADATS(IMOD), so this cannot happen there.
  nl1_not_sized = -1,
};

/// @brief W3DMNL(IMOD, NDSE, NDST, NSP, NSPX), W3_NL1 build.
/// @param reg    WADATS counters NADATA and IADATA (in/out: IADATA = IMOD on ok).
/// @param nl1    the NL1 parts of WADATS (in/out).
/// @param imod   model number, 1-based as in the Fortran.
/// @param ngrids W3GDATMD NGRIDS (-1 before W3NMOD).
/// @param nsp    NSP, the extent of the ICnn tables (INSNL1 passes NSPEC).
/// @param nspx   NSPX, the extent of IPnn, IMnn and AF11 (INSNL1 passes NSPECX).
/// @return W3dmnlStatus::ok, or the code EXTCDE would have exited with. On a code
///         other than ok the Fortran stops the program; here reg and nl1 are left
///         as far as the Fortran had got and must not be used further.
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3dmnlStatus w3dmnl(Wadats<ExecSpace>& reg, WadatsNl1<ExecSpace>& nl1, int imod, int ngrids,
                    int nsp, int nspx) {
  using N = WadatNl1<ExecSpace>;
  using R1 = typename N::R1;
  using I1 = typename N::I1;
  using detail::alloc;
  using detail::fext;

  if (ngrids == -1) return W3dmnlStatus::grids_not_initialized;          // :2514
  if (imod < 1 || imod > reg.nadata) return W3dmnlStatus::illegal_model;  // :2519
  if (static_cast<std::size_t>(imod) > nl1.models.size()) return W3dmnlStatus::nl1_not_sized;
  N& w = nl1.models[static_cast<std::size_t>(imod - 1)];
  if (w.nlinit) return W3dmnlStatus::already_allocated;  // :2525

  // :2539-2573, one ALLOCATE(..., STAT=ISTAT) statement; the first failure stops it.
  const std::size_t nx = fext(1, nspx);
  const std::size_t ns = fext(1, nsp);
  try {
    w.ip11 = alloc<I1>("ww::adat::IP11", nx);
    w.ip12 = alloc<I1>("ww::adat::IP12", nx);
    w.ip13 = alloc<I1>("ww::adat::IP13", nx);
    w.ip14 = alloc<I1>("ww::adat::IP14", nx);
    w.im11 = alloc<I1>("ww::adat::IM11", nx);
    w.im12 = alloc<I1>("ww::adat::IM12", nx);
    w.im13 = alloc<I1>("ww::adat::IM13", nx);
    w.im14 = alloc<I1>("ww::adat::IM14", nx);
    w.ip21 = alloc<I1>("ww::adat::IP21", nx);
    w.ip22 = alloc<I1>("ww::adat::IP22", nx);
    w.ip23 = alloc<I1>("ww::adat::IP23", nx);
    w.ip24 = alloc<I1>("ww::adat::IP24", nx);
    w.im21 = alloc<I1>("ww::adat::IM21", nx);
    w.im22 = alloc<I1>("ww::adat::IM22", nx);
    w.im23 = alloc<I1>("ww::adat::IM23", nx);
    w.im24 = alloc<I1>("ww::adat::IM24", nx);
    w.ic11 = alloc<I1>("ww::adat::IC11", ns);
    w.ic12 = alloc<I1>("ww::adat::IC12", ns);
    w.ic21 = alloc<I1>("ww::adat::IC21", ns);
    w.ic22 = alloc<I1>("ww::adat::IC22", ns);
    w.ic31 = alloc<I1>("ww::adat::IC31", ns);
    w.ic32 = alloc<I1>("ww::adat::IC32", ns);
    w.ic41 = alloc<I1>("ww::adat::IC41", ns);
    w.ic42 = alloc<I1>("ww::adat::IC42", ns);
    w.ic51 = alloc<I1>("ww::adat::IC51", ns);
    w.ic52 = alloc<I1>("ww::adat::IC52", ns);
    w.ic61 = alloc<I1>("ww::adat::IC61", ns);
    w.ic62 = alloc<I1>("ww::adat::IC62", ns);
    w.ic71 = alloc<I1>("ww::adat::IC71", ns);
    w.ic72 = alloc<I1>("ww::adat::IC72", ns);
    w.ic81 = alloc<I1>("ww::adat::IC81", ns);
    w.ic82 = alloc<I1>("ww::adat::IC82", ns);
    w.af11 = alloc<R1>("ww::adat::AF11", nx);
  } catch (const std::exception&) {
    return W3dmnlStatus::alloc_failed;
  }
  w.nlinit = true;  // :2574, into WADATS(IMOD) directly, before W3SETA

  // W3SETA(IMOD) :2584 -> :2746. Its own checks (:2729, :2734) cannot fire: IMOD is
  // already in 1..NADATA, so NADATA /= -1.
  reg.iadata = imod;
  w.nspecx = nspx;  // :2594, through the NSPECX alias W3SETA just set (:2788)
  return W3dmnlStatus::ok;
}

}  // namespace ww::adat
