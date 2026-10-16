// kokkos/src/ww_kokkos/adat_w3naux.hpp
// W3NAUX, ported to Kokkos: allocate WADATS(NGRIDS), set NADATA and give every
// model's scalar counters and flags their first value. Host-side, no View is
// created and no kernel runs. Phase 1, translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3NAUX, WW3/model/src/w3adatmd.F90:705-831 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman.
// Ported as switches/switch_lab_shrd compiles it: W3_NL1 :808-810 is in; W3_S
// STRACE (:774-777) and the W3_T print (:813-815) are not compiled and not ported.
//
// How it relates to adat_wadat.hpp and adat_w3dima.hpp: W3NAUX creates the
// container W3DIMA fills. Wadats::wadats is WADATS, Wadats::nadata is NADATA;
// Wadat holds the three scalars W3DIMA reads or writes (NSEALM, AINIT, FL_ALL).
// The other scalars of TYPE WADAT that W3NAUX sets (ITIME, IPASS, IDLAST, ITSTEP,
// FLCOLD, FLIWND, AINIT2) are WadatScalars below, in a vector parallel to
// Wadats::wadats: scalars[imod-1] is WADATS(IMOD)'s. NLINIT belongs to the W3_NL1
// block, which adat_w3dmnl.hpp holds as WadatsNl1; W3NAUX sizes that vector too
// and clears its nlinit, so the one Fortran field has one C++ home, the one
// W3DMNL and W3SETA read.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3adatmd.F90 with WW3's Release flags and the switch_lab_shrd defines:
//  - The NGRIDS == -1 test (:782) comes before the ALLOCATE; it exits 1 (:784).
//  - ALLOCATE(WADATS(NGRIDS), STAT=ISTAT) (:790) sets ISTAT to 5014 when WADATS is
//    already allocated and 5020 when malloc returns NULL, with WADATS left as it
//    was; CHECK_ALLOC_STATUS (:791, w3macros.h:49-51) exits 99 on either. NGRIDS
//    <= 0 is a zero-size allocation, not an error.
//  - The ALLOCATE stores nothing into the elements: TYPE WADAT has no default
//    initialisation under this switch, so its pointer components are undefined
//    until W3DIMA. Here they are default-constructed (unallocated) Views.
//  - NADATA = NGRIDS (:792), then one loop over I = 1..NGRIDS (:797-811) of integer
//    and logical stores of 0 / .FALSE., no arithmetic.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE,1001) message
// and EXTCDE (returned as the code EXTCDE would exit with, W3nauxStatus); NDSE and
// NDST, which only those use; the module-private ISTAT.
#pragma once

#include <cstddef>
#include <exception>
#include <utility>
#include <vector>

#include "adat_w3dmnl.hpp"
#include "adat_wadat.hpp"

namespace ww::adat {

/// EXTCDE exit codes of W3NAUX, returned instead of stopping the program.
enum class W3nauxStatus : int {
  ok = 0,
  ngrids_not_set = 1,  ///< NGRIDS == -1, :782-785
  alloc_failed = 99,   ///< CHECK_ALLOC_STATUS after :790, w3macros.h:49-51
};

/// @brief The scalars of TYPE WADAT (w3adatmd.F90:546, :566, :568) that W3NAUX
///        sets and Wadat does not hold, for one model.
/// @details LOGICAL as bool, as Wadat::ainit and Wadat::fl_all. Values are the
///          ones W3NAUX stores (:798-809).
struct WadatScalars {
  int itime = 0;        ///< ITIME, discrete time step counter (:310)
  int ipass = 0;        ///< IPASS, pass counter for log file (:311)
  int idlast = 0;       ///< IDLAST, last day ID for log file (:312)
  int itstep = 0;       ///< ITSTEP (:566), the W3WAVE step counter
  bool flcold = false;  ///< FLCOLD, 'cold start' of model (:315)
  bool fliwnd = false;  ///< FLIWND, initialization based on wind (:316-317)
  bool ainit2 = false;  ///< AINIT2 (:568), set .TRUE. by W3XDMA (:2375)
};

/// @brief W3NAUX(NDSE, NDST), shared memory (W3_SHRD) build with W3_NL1.
/// @param reg     WADATS, NADATA and IADATA (in/out). WADATS counts as allocated
///                when reg.nadata != -1 (see the check below).
/// @param scalars the WadatScalars of each model, parallel to reg.wadats (out).
/// @param nl1     the W3_NL1 block of each model, parallel to reg.wadats (out);
///                its nlinit is WADATS(IMOD)%NLINIT.
/// @param ngrids  W3GDATMD NGRIDS, as W3NMOD left it.
/// @return W3nauxStatus::ok, or the code EXTCDE would have exited with. On a code
///         other than ok, reg, scalars and nl1 are as on entry.
/// @post On ok: reg.wadats.size() == scalars.size() == nl1.models.size()
///       == max(ngrids, 0),
///       reg.nadata == ngrids, reg.iadata unchanged.
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3nauxStatus w3naux(Wadats<ExecSpace>& reg, std::vector<WadatScalars>& scalars,
                    WadatsNl1<ExecSpace>& nl1, int ngrids) {
  if (ngrids == -1) return W3nauxStatus::ngrids_not_set;  // :782-785

  // :790, gfortran's "already allocated" (ISTAT 5014). WADATS is never deallocated
  // in model/src and NADATA is written only at :792, with NGRIDS != -1, so
  // NADATA != -1 is exactly "WADATS is allocated"; an empty vector cannot tell a
  // zero-size allocation (NGRIDS <= 0) from none.
  if (reg.nadata != -1) return W3nauxStatus::alloc_failed;

  // :790, the allocation itself (ISTAT 5020 when malloc fails); Fortran's
  // zero-size extent for NGRIDS <= 0.
  const std::size_t n = ngrids > 0 ? static_cast<std::size_t>(ngrids) : std::size_t{0};
  std::vector<Wadat<ExecSpace>> w;
  std::vector<WadatScalars> s;
  std::vector<WadatNl1<ExecSpace>> q;
  try {
    w.resize(n);
    s.resize(n);
    q.resize(n);
  } catch (const std::exception&) {
    return W3nauxStatus::alloc_failed;
  }
  reg.wadats = std::move(w);
  scalars = std::move(s);
  nl1.models = std::move(q);

  reg.nadata = ngrids;  // :792

  for (std::size_t i = 0; i < n; ++i) {  // :797-811, I = i+1
    WadatScalars& c = scalars[i];
    Wadat<ExecSpace>& a = reg.wadats[i];
    c.itime = 0;      // :798
    c.ipass = 0;      // :799
    c.idlast = 0;     // :800
    a.nsealm = 0;     // :801
    c.itstep = 0;     // :802
    c.flcold = false; // :803
    c.fliwnd = false; // :804
    a.ainit = false;  // :805
    c.ainit2 = false; // :806
    a.fl_all = false; // :807
    nl1.models[i].nlinit = false; // :809, W3_NL1
  }
  return W3nauxStatus::ok;
}

}  // namespace ww::adat
