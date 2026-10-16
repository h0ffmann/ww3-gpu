// kokkos/src/ww_kokkos/pdlib_jacobi_init.hpp
// JACOBI_INIT: allocate the arrays of the PDLIB implicit solver run as Jacobi: the matrix
// ASPAR_JAC, the right-hand side B_JAC, the refraction velocities CAD_THE, the frequency-shift
// velocities CAS_SIG (FreqShiftMethod 1) or CWNB_SIG_M2 (FreqShiftMethod 2), and U_JAC when the
// solver is not the block Gauss-Seidel. Allocation only: no fill, no arithmetic on REAL values.
// Phase 1, translated, not wired into WW3, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:7515-7613 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/JACOBI_INIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGINIT). IMEM is the PARAMETER 1 (w3parall.F90:90), so the IMEM == 2 branch (:7591-7593,
// ASPAR_DIAG_ALL) is folded away by gfortran; it is kept below behind `if constexpr`, as dead as in
// the Fortran. Not ported: W3_S (USE of STRACE and IENT, :7560-7562, :7579-7581; no executable
// statement) and W3_DEBUGINIT (USE of IAPROC, :7568-7570).
//
// Errors: PDLIB_ABORT(istat) prints and ends in the STOP of yowerr.F90:159
// (PDLIB/yowfunction.F90:47-53). Here istat is returned and the caller (the shim) calls
// PDLIB_ABORT with it, as in BLOCK_SOLVER_INIT, W3DIMS and W3IOGR: a host routine of the port
// never prints or stops. The allocations after the failing one are not made, since the Fortran
// has stopped there.
//
// Each ALLOCATE(..., stat=istat) has one item, which the -O3 object tests in this order:
// already allocated (data /= NULL) sets istat without allocating; else malloc(MAX(bytes,1)) of
// uninitialised memory, istat /= 0 when it returns NULL. Either way PDLIB_ABORT(code) follows,
// with the codes 9, 10, 11, 11, 11, 12 of :7590-7608 (read from .rodata of the object). Only
// CWNB_SIG_M2's ALLOCATE carries gfortran's overflow test (elements > 2^62-1), the one whose
// lower bound is not 1; the C++ keeps it.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include "real.hpp"

namespace ww::pdlib {

/// The scalars the routine reads: argument IMOD (unused, as in the Fortran), W3GDATMD NSPEC, NTH,
/// NSEAL, B_JGS_BLOCK_GAUSS_SEIDEL (LOGICAL(4) as the word gfortran tests, `/= 0`;
/// w3gdatmd.F90:1419), yowNodepool PDLIB_NNZ (yownodepool.F90:90) and npa, and the module
/// variable FreqShiftMethod of PDLIB_W3PROFSMD (w3profsmd_pdlib.F90:125, initialised to 2 and
/// assigned nowhere at the pin).
struct JacobiInitConfig {
  int imod = 1;
  int nspec = 0, nth = 0, nseal = 0;
  int pdlib_nnz = 0, npa = 0;
  int freq_shift_method = 2;
  int b_jgs_block_gauss_seidel = 0;
};

/// The REAL ALLOCATABLE module arrays of PDLIB_W3PROFSMD (w3profsmd_pdlib.F90:114-117), LayoutLeft
/// with the Fortran's extents, uninitialised after allocation as in the Fortran:
///   aspar_jac(NSPEC, PDLIB_NNZ), aspar_diag_all(NSPEC, npa), b_jac(NSPEC, NSEAL),
///   cad_the(NSPEC, NSEAL), cas_sig(NSPEC, NSEAL), u_jac(NSPEC, npa): element (i-1, j-1);
///   cwnb_sig_m2(1-NTH:NSPEC, NSEAL): element (isp + nth - 1, ip - 1) = CWNB_SIG_M2(ISP, IP), the
///   convention of pdlib_jacobi_spectral.hpp.
/// The *_allocated flags are Fortran's ALLOCATED(); a View of extent 0 does not say it by itself
/// (gfortran allocates 1 byte for an empty array, so it is allocated).
template <class MemSpace>
struct JacobiArrays {
  using memory_space = MemSpace;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  R2 aspar_jac, aspar_diag_all, b_jac, cad_the, cas_sig, cwnb_sig_m2, u_jac;
  bool aspar_jac_allocated = false, aspar_diag_all_allocated = false;
  bool b_jac_allocated = false, cad_the_allocated = false;
  bool cas_sig_allocated = false, cwnb_sig_m2_allocated = false;
  bool u_jac_allocated = false;
};

/// What jacobi_init returns: 0, the Fortran returned; otherwise the istat of the
/// PDLIB_ABORT(istat) the Fortran calls and stops in (:7590-7608). The int is what
/// BlockSolverInitHooks::jacobi_init returns.
inline constexpr int kJacobiInitOk = 0;
inline constexpr int kJacobiInitAspar = 9;     ///< ASPAR_JAC (or ASPAR_DIAG_ALL), :7590
inline constexpr int kJacobiInitBJac = 10;     ///< B_JAC, :7596
inline constexpr int kJacobiInitVelocity = 11; ///< CAD_THE, CAS_SIG, CWNB_SIG_M2, :7598-7604
inline constexpr int kJacobiInitUJac = 12;     ///< U_JAC, :7608

namespace jinit_detail {

/// W3PARALL IMEM, INTEGER PARAMETER (w3parall.F90:90).
inline constexpr int kImem = 1;

/// One ALLOCATE(A(e0, e1), stat=istat): false when A is already allocated or when the
/// allocation throws, the two cases in which gfortran sets istat. e0, e1 are the extents
/// MAX(ub - lb + 1, 0) the object computes in INTEGER(8).
template <class View>
bool allocate_item(View& v, bool& allocated, const char* label, std::int64_t e0,
                   std::int64_t e1) {
  if (allocated) return false;
  try {
    v = View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)),
             static_cast<std::size_t>(e0), static_cast<std::size_t>(e1));
  } catch (const std::exception&) {
    return false;
  }
  allocated = true;
  return true;
}

inline std::int64_t extent(int n) { return n > 0 ? static_cast<std::int64_t>(n) : 0; }

}  // namespace jinit_detail

/// JACOBI_INIT(IMOD), w3profsmd_pdlib.F90:7515-7613, under switch_PDLIB. Host only: allocation
/// launches no kernel and writes no element. Returns kJacobiInitOk or a PDLIB_ABORT istat; on an
/// istat the Fortran has stopped, and a is left as far as it had got.
template <class ExecSpace>
int jacobi_init(const JacobiInitConfig& c, JacobiArrays<typename ExecSpace::memory_space>& a) {
  using jinit_detail::allocate_item;
  using jinit_detail::extent;
  using jinit_detail::kImem;
  (void)c.imod;  // INTENT(IN) :: IMOD, never read (:7585)

  if constexpr (kImem == 1) {  // :7588-7590
    if (!allocate_item(a.aspar_jac, a.aspar_jac_allocated, "ASPAR_JAC", extent(c.nspec),
                       extent(c.pdlib_nnz)))
      return kJacobiInitAspar;
  } else if constexpr (kImem == 2) {  // :7591-7593
    if (!allocate_item(a.aspar_diag_all, a.aspar_diag_all_allocated, "ASPAR_DIAG_ALL",
                       extent(c.nspec), extent(c.npa)))
      return kJacobiInitAspar;
  }
  if (!allocate_item(a.b_jac, a.b_jac_allocated, "B_JAC", extent(c.nspec), extent(c.nseal)))
    return kJacobiInitBJac;  // :7595-7596
  if (!allocate_item(a.cad_the, a.cad_the_allocated, "CAD_THE", extent(c.nspec),
                     extent(c.nseal)))
    return kJacobiInitVelocity;  // :7597-7598
  // :7599, FreqShiftMethod read once and compared with 1, then with 2, as in the object.
  if (c.freq_shift_method == 1) {  // :7599-7601
    if (!allocate_item(a.cas_sig, a.cas_sig_allocated, "CAS_SIG", extent(c.nspec),
                       extent(c.nseal)))
      return kJacobiInitVelocity;
  } else if (c.freq_shift_method == 2) {  // :7602-7604
    // 1-NTH:NSPEC; the object forms NSPEC + 1 - (1 - NTH) in INTEGER(8) (1 - NTH in INTEGER(4)).
    const std::int64_t span = (static_cast<std::int64_t>(c.nspec) + 1) -
                              static_cast<std::int64_t>(1 - c.nth);
    const std::int64_t e0 = span > 0 ? span : 0;
    const std::int64_t e1 = extent(c.nseal);
    // gfortran's overflow test on the element count (e1 < 2^31, so the product fits int64).
    if (e0 * e1 > INT64_C(4611686018427387903)) return kJacobiInitVelocity;
    if (!allocate_item(a.cwnb_sig_m2, a.cwnb_sig_m2_allocated, "CWNB_SIG_M2", e0, e1))
      return kJacobiInitVelocity;
  }
  if (c.b_jgs_block_gauss_seidel == 0) {  // :7606-7609
    if (!allocate_item(a.u_jac, a.u_jac_allocated, "U_JAC", extent(c.nspec), extent(c.npa)))
      return kJacobiInitUJac;
  }
  return kJacobiInitOk;
}

}  // namespace ww::pdlib
