// kokkos/src/ww_kokkos/pdlib_pdlib_init.hpp
// PDLIB_INIT: the set-up of the PDLIB domain decomposition for one unstructured grid. It calls
// the PDLIB library's initFromGridDim (which partitions the mesh), counts this rank's wet nodes
// PDLIB_NSEAL, builds the maps JX_TO_JSEA (local node -> local sea point) and ISEA_TO_JSEA
// (global sea point -> local sea point), takes the maximum PDLIB_NSEALM over ranks by MPI, and
// fills COFRM4(NK) = COEF4*GRAV/FR**4 and the spectral offsets IS0_pdlib(NSPEC).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:143-399 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/PDLIB_INIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGSOLVER, no DEBUGINIT). Not ported: W3_S (the USE of STRACE, IENT and the call, :188-190,
// :220-222, :238-240), W3_DEBUGSOLVER (the USEs of NP and NSEAL, :201-203, :208-210, and the WRITE
// and FLUSH blocks at :241-246, :254-259, :267-270, :281-288, :290-293, :308-311, :325-328) and
// W3_DEBUGINIT (:363-367).
// Not translated, behind PdlibInitHooks: MPI_COMM_RANK (:252), initFromGridDim (:262, :264;
// yowpdlibmain.F90:65-174, whose runParmetis partitions through SCOTCH's ParMETIS interface under
// SCOTCH_707), the read of YOWNODEPOOL's npa and iplg it sets, the list-directed WRITEs
// (:321-322, :389-390), MPI_SEND and MPI_RECV (:342, :349, :356, :359) and
// ComputeListNP_ListNPA_ListIPLG (:369; yowfunction.F90:198-281).
//
// Errors: where the Fortran stops, pdlib_init returns a nonzero code and the caller (the shim)
// stops, as for W3DIMS, W3IOGR and BLOCK_SOLVER_INIT: a host routine of the port never stops.
// PDLIB_ABORT(istat) (:294, :391; yowfunction.F90:47-53, ending in the STOP of yowerr.F90:159)
// returns istat for the shim to call PDLIB_ABORT with. The bare STOP (:323) and gfortran's run
// time errors of the ALLOCATEs without stat= (:339, :370, :376) return negative codes, since the
// STOP exits with status 0 and the run time errors need the line and the variable.
//
// Host code: every loop runs serially in the Fortran's order. The JSEA loop (:298-306) carries
// its counter from node to node, and the object keeps every loop's order (the vectorised ones
// are integer counts, an integer MAXVAL and element-wise float arithmetic, which no order changes).
#pragma once

#include <Kokkos_Core.hpp>

#include <climits>
#include <cstddef>
#include <exception>
#include <string>
#include <vector>

#include "real.hpp"

namespace ww::pdlib {

namespace pdlib_init_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
inline constexpr Real kTpi = static_cast<Real>(2.0) * kPi;  // TPI, constants.F90:72-73
inline constexpr Real kCoef4 = static_cast<Real>(5.0E-7);  // COEF4, w3profsmd_pdlib.F90:237
}  // namespace pdlib_init_detail

/// The MPI communicators the Fortran names (W3ADATMD MPI_COMM_WCMP, MPI_COMM_WAVE); the shim maps
/// them to the handles of the running process.
enum PdlibInitComm : int { kPdlibInitCommWcmp = 0, kPdlibInitCommWave = 1 };

/// The MPI datatype the Fortran names, MPI_INT of mpi_f08 (:342, :349, :356, :359); the shim maps
/// it to the handle of its MPI library.
enum PdlibInitMpiDatatype : int { kPdlibInitMpiInt = 0 };

/// The scalars the routine reads: argument IMOD (unused in the body), W3ODATMD IAPROC, NAPROC,
/// NTPROC, W3GDATMD NX, NSEA, NTRI, NSPEC, NTH, NK, FSTOTALEXP, FLCX, FLCY. LOGICAL(4) as the word
/// gfortran tests (`/= 0`).
struct PdlibInitConfig {
  int imod = 1;
  int iaproc = 1, naproc = 1, ntproc = 1;
  int nx = 0, nsea = 0, ntri = 0, nspec = 0, nth = 0, nk = 0;
  int fstotalexp = 0;
  int flcx = 0, flcy = 0;
};

/// The grid arrays the routine reads. mapfs is W3GDATMD MAPFS(NY,NX), LayoutLeft, so MAPFS(1,IX)
/// is mapfs(0, ix-1). sig is SIG(0:NK+1) (w3gdatmd.F90:2066), so SIG(IK) is sig(ik).
struct PdlibInitGrid {
  Kokkos::View<const int**, Kokkos::LayoutLeft, Kokkos::HostSpace> mapfs;
  Kokkos::View<const Real*, Kokkos::LayoutLeft, Kokkos::HostSpace> sig;
};

/// YOWNODEPOOL's npa (the local nodes with ghosts) and iplg(npa) (local node -> global node,
/// element ip-1 holds IPLG(IP)), as initFromGridDim leaves them (yownodepool.F90:99, :116).
struct PdlibNodePool {
  int npa = 0;
  Kokkos::View<const int*, Kokkos::LayoutLeft, Kokkos::HostSpace> iplg;
};

/// The module state the routine writes: W3PARALL PDLIB_NSEAL, PDLIB_NSEALM, JX_TO_JSEA(npa),
/// ISEA_TO_JSEA(NSEA) (w3parall.F90:82-83); PDLIB_W3PROFSMD FSGEOADVECT (LOGICAL(4) as 0/1),
/// COFRM4(NK), IS0_pdlib(NSPEC) (w3profsmd_pdlib.F90:126, :118, :124). Element i-1 holds Fortran
/// element i. The *_allocated flags are Fortran's ALLOCATED(); a View of extent 0 does not say it.
struct PdlibInitState {
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, Kokkos::HostSpace>;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Kokkos::HostSpace>;
  int pdlib_nseal = 0;
  int pdlib_nsealm = 0;
  int fsgeoadvect = 0;
  I1 jx_to_jsea, isea_to_jsea, is0_pdlib;
  R1 cofrm4;
  bool jx_to_jsea_allocated = false, isea_to_jsea_allocated = false;
  bool cofrm4_allocated = false, is0_pdlib_allocated = false;
};

/// What pdlib_init returns: 0, the Fortran returned; otherwise where the Fortran stops, the state
/// left as far as it had got. Positive: the istat of the PDLIB_ABORT(istat) the shim calls.
/// Negative: not WW3 codes; the shim writes gfortran's message and exits as the comment says.
inline constexpr int kPdlibInitOk = 0;
inline constexpr int kPdlibInitAllocFailed = 3;  ///< PDLIB_ABORT(3), :294
inline constexpr int kPdlibInitIseaMismatch = 20;  ///< PDLIB_ABORT(20), :391
inline constexpr int kPdlibInitStop = -1;  ///< STOP, :323; _gfortran_stop_string, exit status 0
/// "Attempting to allocate already allocated variable", _gfortran_runtime_error_at, exit 2.
inline constexpr int kPdlibInitCofrm4Allocated = -2;  ///< COFRM4, "At line 370"
inline constexpr int kPdlibInitIs0Allocated = -3;     ///< IS0_pdlib, "At line 376"
/// "Error allocating %lu bytes", _gfortran_os_error_at, exit 1, "around line" 339, 370, 376.
inline constexpr int kPdlibInitNsealArrNoMemory = -4;
inline constexpr int kPdlibInitCofrm4NoMemory = -5;
inline constexpr int kPdlibInitIs0NoMemory = -6;

/// The calls that stay in Fortran, in the PDLIB library or in MPI, filled by a future shim.
/// Buffers are host memory, counts in elements, ranks 0-based as the Fortran passes them
/// (IPROC-1, 0). The shim owns TRIGP (passed packed to initFromGridDim, as the object does with
/// _gfortran_internal_pack), the MPI status (YOWDATAPOOL istatus, yowdatapool.F90:75) and every
/// IERR, which the Fortran never checks. A hook needed on a path and left null aborts.
struct PdlibInitHooks {
  void* ctx = nullptr;
  int (*mpi_comm_rank)(void* ctx, int comm) = nullptr;  ///< MPI_COMM_RANK, :252; myrank unused
  /// initFromGridDim(NX, NTRI, TRIGP, secDim, MPI_COMM_WCMP), :262 and :264.
  void (*init_from_grid_dim)(void* ctx, int mnp, int mne, int sec_dim, int comm) = nullptr;
  PdlibNodePool (*node_pool)(void* ctx) = nullptr;  ///< YOWNODEPOOL npa, iplg
  /// WRITE(*,*) 'Logical error in computation of NSEA / nb' and WRITE(*,*) 'nb=', nb,
  /// ' NSEA=', NSEA, :321-322.
  void (*write_nb_error)(void* ctx, int nb, int nsea) = nullptr;
  void (*mpi_send)(void* ctx, const int* buf, int count, int datatype, int dest, int tag,
                   int comm) = nullptr;  ///< MPI_SEND, :349, :356
  void (*mpi_recv)(void* ctx, int* buf, int count, int datatype, int source, int tag,
                   int comm) = nullptr;  ///< MPI_RECV, :342, :359
  void (*compute_list_np_npa_iplg)(void* ctx) = nullptr;  ///< :369
  /// WRITE(*,*) JSEA, PDLIB_NSEAL, IP, IP_glob, ISEA and WRITE(*,*) 'ISEA .ne. IP_glob', :389-390.
  void (*write_isea_error)(void* ctx, int jsea, int pdlib_nseal, int ip, int ip_glob,
                           int isea) = nullptr;
};

namespace pdlib_init_detail {

template <class Fn>
Fn need(Fn fn, const char* what) {
  if (fn == nullptr) Kokkos::abort(what);
  return fn;
}

/// One item of ALLOCATE(..., stat=istat): false when already allocated or when the allocation
/// throws, the two cases in which gfortran sets istat and skips the remaining items. The extent
/// is MAX(n, 0), as the descriptor's size.
template <class View>
bool allocate_item(View& v, bool& allocated, const char* label, int n) {
  if (allocated) return false;
  try {
    const std::size_t extent = n > 0 ? static_cast<std::size_t>(n) : 0;
    v = View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)), extent);
  } catch (const std::exception&) {
    return false;
  }
  allocated = true;
  return true;
}

/// ALLOCATE without stat=: 0, or already_allocated (gfortran's run time error on an allocated
/// array) or no_memory (its OS error on a failed allocation), both fatal in the Fortran.
template <class View>
int allocate_or_status(View& v, bool& allocated, const char* label, int n, int already_allocated,
                       int no_memory) {
  if (allocated) return already_allocated;
  if (!allocate_item(v, allocated, label, n)) return no_memory;
  return kPdlibInitOk;
}

}  // namespace pdlib_init_detail

/// PDLIB_INIT(IMOD), w3profsmd_pdlib.F90:143-399, under switch_PDLIB. Returns kPdlibInitOk or
/// one of the kPdlibInit* codes above.
[[nodiscard]] inline int pdlib_init(const PdlibInitConfig& c, const PdlibInitGrid& g,
                                    PdlibInitState& s, const PdlibInitHooks& h) {
  namespace d = pdlib_init_detail;
  s.pdlib_nseal = 0;  // :248

  if (c.iaproc <= c.naproc) {  // :250-329
    (void)d::need(h.mpi_comm_rank, "PDLIB_INIT: mpi_comm_rank hook missing")(
        h.ctx, kPdlibInitCommWcmp);
    const auto init = d::need(h.init_from_grid_dim, "PDLIB_INIT: init_from_grid_dim hook missing");
    if (c.fstotalexp != 0) {  // :261-265
      init(h.ctx, c.nx, c.ntri, c.nth, kPdlibInitCommWcmp);
    } else {
      init(h.ctx, c.nx, c.ntri, c.nspec, kPdlibInitCommWcmp);
    }

    const PdlibNodePool pool = d::need(h.node_pool, "PDLIB_INIT: node_pool hook missing")(h.ctx);
    for (int ip = 1; ip <= pool.npa; ++ip) {  // :276-280
      const int ix = pool.iplg(ip - 1);
      const int isea = g.mapfs(0, ix - 1);
      if (isea > 0) s.pdlib_nseal = s.pdlib_nseal + 1;
    }

    // :289, the two items in the order of the list.
    const bool ok = d::allocate_item(s.jx_to_jsea, s.jx_to_jsea_allocated, "JX_TO_JSEA",
                                     pool.npa) &&
                    d::allocate_item(s.isea_to_jsea, s.isea_to_jsea_allocated, "ISEA_TO_JSEA",
                                     c.nsea);
    if (!ok) return kPdlibInitAllocFailed;  // :294
    int jsea = 0;  // :295-297
    Kokkos::deep_copy(s.jx_to_jsea, 0);
    Kokkos::deep_copy(s.isea_to_jsea, 0);
    for (int ip = 1; ip <= pool.npa; ++ip) {  // :298-306
      const int ix = pool.iplg(ip - 1);
      const int isea = g.mapfs(0, ix - 1);
      if (isea > 0) {
        jsea = jsea + 1;
        s.jx_to_jsea(ip - 1) = jsea;
        s.isea_to_jsea(isea - 1) = jsea;
      }
    }

    int nb = 0;  // :315-318
    for (int ix = 1; ix <= c.nx; ++ix) {
      if (g.mapfs(0, ix - 1) > 0) nb = nb + 1;
    }
    if (nb != c.nsea) {  // :320-324
      d::need(h.write_nb_error, "PDLIB_INIT: write_nb_error hook missing")(h.ctx, nb, c.nsea);
      return kPdlibInitStop;  // :323
    }
  }

  s.fsgeoadvect = 0;  // :330-333, FLCY read only when FLCX is set
  if (c.flcx != 0 && c.flcy != 0) s.fsgeoadvect = 1;

  int iscal = 0;  // IScal(1); the Fortran leaves it undefined until a store
  if (c.iaproc <= c.naproc) {  // :337-351
    if (c.iaproc == 1) {
      std::vector<int> nseal_arr;  // :339, NSEAL_arr(NAPROC); DEALLOCATE at :346 is its scope
      try {
        nseal_arr.resize(static_cast<std::size_t>(c.naproc));
      } catch (const std::exception&) {
        return kPdlibInitNsealArrNoMemory;
      }
      nseal_arr[0] = s.pdlib_nseal;
      for (int iproc = 2; iproc <= c.naproc; ++iproc) {  // :341-344
        d::need(h.mpi_recv, "PDLIB_INIT: mpi_recv hook missing")(
            h.ctx, &iscal, 1, kPdlibInitMpiInt, iproc - 1, 23, kPdlibInitCommWave);
        nseal_arr[static_cast<std::size_t>(iproc - 1)] = iscal;
      }
      int limit = INT_MIN;  // :345, MAXVAL of INTEGER starts at -HUGE-1 in the object
      for (int iproc = 1; iproc <= c.naproc; ++iproc) {
        const int v = nseal_arr[static_cast<std::size_t>(iproc - 1)];
        if (v > limit) limit = v;
      }
      s.pdlib_nsealm = limit;
    } else {  // :348-349
      iscal = s.pdlib_nseal;
      d::need(h.mpi_send, "PDLIB_INIT: mpi_send hook missing")(
          h.ctx, &iscal, 1, kPdlibInitMpiInt, 0, 23, kPdlibInitCommWave);
    }
  }

  if (c.iaproc == 1) {  // :353-361
    iscal = s.pdlib_nsealm;
    for (int iproc = 2; iproc <= c.ntproc; ++iproc) {
      d::need(h.mpi_send, "PDLIB_INIT: mpi_send hook missing")(
          h.ctx, &iscal, 1, kPdlibInitMpiInt, iproc - 1, 24, kPdlibInitCommWave);
    }
  } else {
    d::need(h.mpi_recv, "PDLIB_INIT: mpi_recv hook missing")(
        h.ctx, &iscal, 1, kPdlibInitMpiInt, 0, 24, kPdlibInitCommWave);
    s.pdlib_nsealm = iscal;
  }

  d::need(h.compute_list_np_npa_iplg, "PDLIB_INIT: compute_list_np_npa_iplg hook missing")(
      h.ctx);  // :369

  const int st_cofrm4 = d::allocate_or_status(s.cofrm4, s.cofrm4_allocated, "COFRM4", c.nk,
                                              kPdlibInitCofrm4Allocated, kPdlibInitCofrm4NoMemory);
  if (st_cofrm4 != kPdlibInitOk) return st_cofrm4;  // :370
  for (int ik = 1; ik <= c.nk; ++ik) {  // :371-375
    const Real esig = g.sig(ik);
    const Real efr = esig / d::kTpi;
    const Real efr2 = efr * efr;  // eFR**4 as the object's power tree: (efr*efr)*(efr*efr)
    const Real efr4 = efr2 * efr2;
    s.cofrm4(ik - 1) = d::kCoef4 * d::kGrav / efr4;  // COEF4*GRAV folds to one REAL constant
  }

  const int st_is0 = d::allocate_or_status(s.is0_pdlib, s.is0_pdlib_allocated, "IS0_pdlib",
                                           c.nspec, kPdlibInitIs0Allocated, kPdlibInitIs0NoMemory);
  if (st_is0 != kPdlibInitOk) return st_is0;  // :376
  for (int isp = 1; isp <= c.nspec; ++isp) {  // :377-379
    s.is0_pdlib(isp - 1) = isp - 1;
  }
  // :380-382, the object's trip count: (NSPEC-1)/NTH + 1 for NTH >= 0 and NSPEC >= 1,
  // (1-NSPEC)/(-NTH) + 1 for NTH < 0 and NSPEC <= 1, in unsigned arithmetic. NTH = 0 divides by
  // zero, in the object before the NSPEC test and here after it.
  const bool runs = c.nth < 0 ? c.nspec <= 1 : c.nspec >= 1;
  if (runs) {
    const unsigned count =
        c.nth < 0 ? (1u - static_cast<unsigned>(c.nspec)) / (0u - static_cast<unsigned>(c.nth))
                  : (static_cast<unsigned>(c.nspec) - 1u) / static_cast<unsigned>(c.nth);
    int isp = 1;
    for (unsigned k = 0; k <= count; ++k, isp += c.nth) {
      s.is0_pdlib(isp - 1) = s.is0_pdlib(isp - 1) + c.nth;
    }
  }

  if (s.pdlib_nseal >= 1) {  // :384-393
    const PdlibNodePool pool = d::need(h.node_pool, "PDLIB_INIT: node_pool hook missing")(h.ctx);
    for (int jsea = 1; jsea <= s.pdlib_nseal; ++jsea) {
      const int ip = jsea;
      const int ip_glob = pool.iplg(ip - 1);
      const int isea = g.mapfs(0, ip_glob - 1);
      if (isea != ip_glob) {
        d::need(h.write_isea_error, "PDLIB_INIT: write_isea_error hook missing")(
            h.ctx, jsea, s.pdlib_nseal, ip, ip_glob, isea);
        return kPdlibInitIseaMismatch;  // :391
      }
    }
  }
  return kPdlibInitOk;
}

}  // namespace ww::pdlib
