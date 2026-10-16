// kokkos/src/ww_kokkos/pdlib_block_solver_init.hpp
// BLOCK_SOLVER_INIT: the tables of the PDLIB implicit block solver. POS_TRICK(3,2), the two
// other corners of a triangle counter-clockwise from a corner, and the four neighbour lists of
// each spectral component ISP = ITH + (IK-1)*NTH: previous and next direction (periodic in ITH),
// previous and next frequency (-1 at IK = 1 and IK = NK). Then JACOBI_INIT when the implicit
// scheme runs Jacobi (FSTOTALIMP and B_JGS_USE_JACOBI). Integer work only, no floating point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:6752-6888 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/BLOCK_SOLVER_INIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGINIT). Not ported: W3_S (STRACE, :6797-6799, a USE only) and W3_DEBUGINIT (the USE of
// IAPROC, :6810-6812, and the six WRITE and FLUSH pairs, :6828-6887). Not translated, behind
// BlockSolverInitHooks: JACOBI_INIT(IMOD) (:6878; :7515-7613).
//
// Errors: PDLIB_ABORT(istat) prints and ends in the STOP of yowerr.F90:159
// (PDLIB/yowfunction.F90:47-53). Here istat is returned and the caller (the shim) calls
// PDLIB_ABORT with it, as W3DIMS and W3IOGR return EXTCDE's code: a host routine of the port
// never prints or stops.
//
// ALLOCATE(..., stat=istat) (:6832) as gfortran -O3 emits it: the four arrays in the order of the
// list; an array already allocated, or a failed malloc, sets istat and leaves the rest of the
// list unallocated. The arrays are module variables of W3PARALL (w3parall.F90:86-87) that no
// routine deallocates, so a second call in one process ends in PDLIB_ABORT(8).
// Structural change, no arithmetic: the ISP loop (:6838-6868) is one parallel_for; each element
// of each list is written once from its own ISP, so the order cannot change a bit.
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <exception>
#include <string>

namespace ww::pdlib {

/// The scalars the routine reads: argument IMOD, W3GDATMD NSPEC, NTH, NK, FSTOTALIMP,
/// B_JGS_USE_JACOBI (w3gdatmd.F90:1234, :1409, :1418); LOGICAL(4) as the word gfortran tests
/// (`/= 0`).
struct BlockSolverInitConfig {
  int imod = 1;
  int nspec = 0, nth = 0, nk = 0;
  int fstotalimp = 0;
  int b_jgs_use_jacobi = 0;
};

/// The state the routine writes. POS_TRICK(3,2) of PDLIB_W3PROFSMD (w3profsmd_pdlib.F90:129),
/// column-major as the Fortran stores it: pos_trick[(pos-1) + (k-1)*3]. The W3PARALL arrays
/// ListISPnextDir, ListISPprevDir, ListISPnextFreq, ListISPprevFreq (NSPEC) (w3parall.F90:86-87):
/// element isp-1 holds the 1-based ISP of the neighbour, or -1. The *_allocated flags are
/// Fortran's ALLOCATED(); a View of extent 0 does not say it by itself.
template <class MemSpace>
struct BlockSolverTables {
  using memory_space = MemSpace;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  int pos_trick[6] = {};
  I1 isp_next_dir, isp_prev_dir, isp_next_freq, isp_prev_freq;
  bool next_dir_allocated = false, prev_dir_allocated = false;
  bool next_freq_allocated = false, prev_freq_allocated = false;
};

/// What block_solver_init returns: 0, the Fortran returned; otherwise the istat of the
/// PDLIB_ABORT(istat) the Fortran calls and stops in.
inline constexpr int kBlockSolverInitOk = 0;
inline constexpr int kBlockSolverInitAllocFailed = 8;  ///< PDLIB_ABORT(8), :6833

/// The calls that stay in Fortran, filled by a future shim. jacobi_init returns 0, or the istat
/// of the PDLIB_ABORT JACOBI_INIT ends in (9 to 12, :7590-7608); a hook that calls the Fortran
/// JACOBI_INIT stops there and only returns 0. A hook needed on a path and left null aborts.
struct BlockSolverInitHooks {
  void* ctx = nullptr;
  int (*jacobi_init)(void* ctx, int imod) = nullptr;  ///< JACOBI_INIT(IMOD), :6878
};

namespace bsi_detail {

/// One item of ALLOCATE(...,stat=istat): false when already allocated or when the allocation
/// throws, the two cases in which gfortran sets istat and skips the remaining items.
template <class View>
bool allocate_item(View& v, bool& allocated, const char* label, int nspec) {
  if (allocated) return false;
  try {
    const std::size_t n = nspec > 0 ? static_cast<std::size_t>(nspec) : 0;  // 1:NSPEC
    v = View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)), n);
  } catch (const std::exception&) {
    return false;
  }
  allocated = true;
  return true;
}

}  // namespace bsi_detail

/// BLOCK_SOLVER_INIT(IMOD), w3profsmd_pdlib.F90:6752-6888, under switch_PDLIB. Returns
/// kBlockSolverInitOk or a PDLIB_ABORT istat; on an istat the Fortran has stopped, and t is left
/// as far as it had got.
template <class ExecSpace>
int block_solver_init(const BlockSolverInitConfig& c,
                      BlockSolverTables<typename ExecSpace::memory_space>& t,
                      const BlockSolverInitHooks& h) {
  // :6821-6826; the object stores {2,3,1,3,1,2} at &POS_TRICK.
  t.pos_trick[0] = 2;  // POS_TRICK(1,1)
  t.pos_trick[3] = 3;  // POS_TRICK(1,2)
  t.pos_trick[1] = 3;  // POS_TRICK(2,1)
  t.pos_trick[4] = 1;  // POS_TRICK(2,2)
  t.pos_trick[2] = 1;  // POS_TRICK(3,1)
  t.pos_trick[5] = 2;  // POS_TRICK(3,2)

  const bool ok =  // :6832, in the order of the list
      bsi_detail::allocate_item(t.isp_next_dir, t.next_dir_allocated, "ListISPnextDir", c.nspec) &&
      bsi_detail::allocate_item(t.isp_prev_dir, t.prev_dir_allocated, "ListISPprevDir", c.nspec) &&
      bsi_detail::allocate_item(t.isp_next_freq, t.next_freq_allocated, "ListISPnextFreq",
                                c.nspec) &&
      bsi_detail::allocate_item(t.isp_prev_freq, t.prev_freq_allocated, "ListISPprevFreq",
                                c.nspec);
  if (!ok) return kBlockSolverInitAllocFailed;  // :6833

  if (c.nspec >= 1) {  // :6838-6868
    const int nth = c.nth;
    const int nk = c.nk;
    const auto next_dir = t.isp_next_dir;
    const auto prev_dir = t.isp_prev_dir;
    const auto next_freq = t.isp_next_freq;
    const auto prev_freq = t.isp_prev_freq;
    Kokkos::parallel_for(
        "ww::pdlib::block_solver_init", Kokkos::RangePolicy<ExecSpace>(1, c.nspec + 1),
        KOKKOS_LAMBDA(const int isp) {
          const int ith = 1 + (isp - 1) % nth;  // MOD truncates as C++ % does
          const int ik = 1 + (isp - 1) / nth;
          int isp_prev_freq;
          if (ik == 1) {
            isp_prev_freq = -1;
          } else {
            isp_prev_freq = ith + (ik - 1 - 1) * nth;
          }
          prev_freq(isp - 1) = isp_prev_freq;
          int isp_next_freq;
          if (ik == nk) {
            isp_next_freq = -1;
          } else {
            isp_next_freq = ith + (ik + 1 - 1) * nth;
          }
          next_freq(isp - 1) = isp_next_freq;
          int jth;
          if (ith == 1) {
            jth = nth;
          } else {
            jth = ith - 1;
          }
          prev_dir(isp - 1) = jth + (ik - 1) * nth;
          if (ith == nth) {
            jth = 1;
          } else {
            jth = ith + 1;
          }
          next_dir(isp - 1) = jth + (ik - 1) * nth;
        });
    ExecSpace().fence();
  }

  // :6873, FSTOTALIMP read first, B_JGS_USE_JACOBI only when it is set, as in the object.
  if (c.fstotalimp != 0 && c.b_jgs_use_jacobi != 0) {
    if (h.jacobi_init == nullptr) Kokkos::abort("BLOCK_SOLVER_INIT: jacobi_init hook missing");
    return h.jacobi_init(h.ctx, c.imod);
  }
  return kBlockSolverInitOk;
}

}  // namespace ww::pdlib
