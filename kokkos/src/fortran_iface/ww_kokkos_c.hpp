// kokkos/src/fortran_iface/ww_kokkos_c.hpp
// The C ABI WAVEWATCH III calls the Kokkos port through: declarations only.
// snl1_shim.cpp implements it; w3kokkosmd.F90 mirrors it in ISO_C_BINDING
// interface blocks, one for one.
//
// Everything below obeys three rules, and every one of them is load-bearing:
//
//  1. Every array is Fortran-ordered (column-major) and caller-owned. The shim
//     borrows the pointer for the duration of the call and frees nothing.
//  2. `float` is WW3's default REAL (C_FLOAT). Compiling WW3 with an 8-byte
//     default REAL would need a different ABI, not a cast.
//  3. Nothing here can throw. A Fortran caller has no catch clause, so the
//     kernel's std::runtime_error (level-0 scratch exhausted) is trapped at this
//     boundary and reported through ww_snl1_last_error() instead.
//
// Phase 1 is deliberately MPI-free: ww_kokkos_init() takes the communicator only
// so the signature does not have to change in phase 2, and picks the device from
// the environment. See the comment on ww_kokkos_init().
//
// The shim keeps one context (tables and device buffers) per grid index
// `igrid`: W3INIT passes its IMOD, W3SRCE the IGRID that W3SETG selected, so each
// grid of a ww3_multi run is computed with its own NK, NTH and tables. Two limits
// of phase 1 follow from those contexts being file-static and unlocked:
//
//  * **One caller at a time.** Nothing here is locked. Two threads inside
//    ww_snl1() share the context, and one of them may be reallocating the device
//    buffers while the other copies into them. Concurrent calls are undefined
//    behaviour, and WW3 *does* call W3SRCE from inside an !$OMP PARALLEL region
//    under W3_OMPG and W3_OMP0 -- so the caller patch has to serialise or the
//    switch file has to leave those out.
//  * **The caller owns the batching.** Phase 1 copies host->device and back on
//    every call, so calling it once per sea point pays that cost once per sea
//    point.
//
// SPDX-License-Identifier: MIT
#pragma once

extern "C" {

/// Status codes. Every entry point either returns one of these or records it for
/// ww_snl1_last_error(); none of them ever throws or aborts.
enum WwKokkosStatus {
  WW_KOKKOS_OK = 0,                  ///< success
  WW_KOKKOS_ERR_NOT_INITIALISED = 1, ///< ww_kokkos_init/ww_snl1_init not done, or undone
  WW_KOKKOS_ERR_BAD_SHAPE = 2,       ///< negative extent, null pointer, nspec != nk*nth
  WW_KOKKOS_ERR_KERNEL = 3,          ///< the kernel threw (e.g. team scratch exhausted)
  WW_KOKKOS_ERR_RUNTIME = 4,         ///< Kokkos itself failed to start or allocate
  WW_KOKKOS_ERR_UNSUPPORTED = 5,     ///< a switch the port does not support (W3_FLX5 with ST4)
  WW_KOKKOS_ERR_TABLE = 6            ///< an ST4 array missing, or of another rank, LBOUND or shape
};

/// Start the Kokkos runtime, if nobody else has. Returns WW_KOKKOS_OK or
/// WW_KOKKOS_ERR_RUNTIME.
///
/// Idempotent: calling it again is a no-op that still returns WW_KOKKOS_OK, so a
/// caller that cannot easily tell whether it has already run may just call it.
///
/// Ownership: if Kokkos is already live when this is called, the shim records
/// that it did *not* start it and ww_kokkos_finalize() will leave it running.
///
/// `comm_f` is an MPI_Comm_c2f() handle, or -1 when there is none. **Phase 1
/// ignores it.** The shim links no MPI: deriving a device id from a communicator
/// would mean calling MPI_Comm_rank() here, which drags the MPI dependency into
/// the kernel library and makes the shim untestable without an MPI launcher.
/// Instead the device is read from the environment variable
/// `WW_KOKKOS_DEVICE_ID` (default 0) on backends that have a device at all; an
/// MPI job sets it per rank in its launcher (e.g. `rank % ndevices`). The
/// argument stays in the signature so phase 2 can honour it without an ABI break.
///
/// Also reads `WW_KOKKOS_SNL1`: "1" makes ww_snl1_enabled() return 1.
/// With it, `WW_KOKKOS_SNL1_COUNT=1` prints the number of spectra ww_snl1()
/// computed to stderr at process exit (one line per process).
int ww_kokkos_init(int comm_f);

/// Release the shim's device buffers and tables, and shut Kokkos down **only if
/// ww_kokkos_init() started it**. Safe to call when nothing was initialised, and
/// safe to call twice. After it returns, ww_snl1() reports
/// WW_KOKKOS_ERR_NOT_INITIALISED until ww_snl1_init() runs again.
void ww_kokkos_finalize(void);

/// Build the INSNL1 quadruplet tables for grid `igrid` (WW3's IMOD). Call once
/// per grid, after ww_kokkos_init(), never concurrently with ww_snl1(). The other
/// arguments are W3GDATMD's for that grid, under WW3's own names.
///
/// `sig` is **SIG(1:nk)**: the pointer must address the *first* frequency bin and
/// the shim reads `nk` floats from it. This matters because W3GDATMD allocates
/// `SIG(0:MK+1)` (w3gdatmd.F90:2066) -- a Fortran caller that passes the bare
/// array name to the assumed-size dummy hands over `&SIG(0)` and every quadruplet
/// is then built one bin low. WW3's own idiom is to pass `SIG(1)`. The array is
/// copied, not borrowed.
///
/// A second call for the same `igrid` replaces that grid's tables; other grids
/// are untouched.
///
/// Returns WW_KOKKOS_OK, or WW_KOKKOS_ERR_BAD_SHAPE (nk <= 0, nth <= 0, null
/// sig), WW_KOKKOS_ERR_NOT_INITIALISED (no live runtime) or WW_KOKKOS_ERR_RUNTIME.
/// On any failure that grid is left unconfigured rather than half-configured.
int ww_snl1_init(int igrid, int nk, int nth, float xfr, float dth, float lam, float snlc1,
                 float kdcon, float kdmn, float snls1, float snls2, float snls3,
                 float fachfe, const float* sig);

/// W3SNL1 for `npts` sea points of grid `igrid` in one launch. Phase 1: copy-in, kernel, copy-out.
///
/// `a`, `s` and `d` are (nspec, npts) and `cg` is (nk, npts), column-major, with
/// nspec = nk*nth from ww_snl1_init() of the same grid; `kdmean` is (npts). `s` and `d` are
/// overwritten, not accumulated, exactly as in the Fortran.
///
/// It is `void` because a Fortran CALL cannot inspect a return value; the outcome
/// is in ww_snl1_last_error(). On a validation error -- before the kernel runs --
/// nothing is written through `s` and `d`. `npts == 0` is a legal no-op: a WW3
/// rank may own no sea points.
///
/// **Not thread-safe.** It reads and may reallocate one shared, unlocked context,
/// so two concurrent calls are undefined behaviour. Batch the points into one
/// call, or serialise the calls at the caller.
void ww_snl1(int igrid, int npts, const float* a, const float* cg, const float* kdmean,
             float* s, float* d);

/// 1 if `WW_KOKKOS_SNL1=1` was in the environment at ww_kokkos_init(), else 0.
/// This is the run-time switch W3KOKKOSMD turns into `LOGICAL :: KOKKOS_SNL1`;
/// the shim itself does not consult it, the caller does.
int ww_snl1_enabled(void);

/// The status of the most recent ww_snl1_init() or ww_snl1() call: 0 for success,
/// one of the WwKokkosStatus codes otherwise. Every failure also prints one line
/// to stderr.
///
/// The code is a single relaxed atomic, so reading it never tears and never races
/// in the sense of the C++ memory model. That is *all* it provides: it does not
/// make ww_snl1() thread-safe, and concurrent ww_snl1() calls are undefined
/// behaviour whatever this returns. With a single caller -- the contract of
/// phase 1 -- a non-zero code identifies the call that just failed.
int ww_snl1_last_error(void);

// ---------------------------------------------------------------------------
// ST4 (w3src4md.F90 at WW3@bb08623a): W3SDS4, W3SIN4, W3SPR4 with CALC_USTAR,
// and INSIN4 with the TABU_* tables. st4_shim.cpp implements it.
//
// A grid is configured in three steps, after W3IOGR has read mod_def.ww3:
// ww_st4_init() with the scalars, one ww_st4_set_real/int() per array of
// ww_kokkos/st4_arrays.hpp (its ArrayId, rank, LBOUND and SHAPE as the Fortran
// has them), then ww_st4_commit(), which checks every array the configuration
// needs and copies them to the device. TAUT, TAUHFT, TAUHFT2, FWTABLE and the
// DEL* scalars are module variables in the Fortran: the shim keeps one copy per
// process, and the last grid to register them wins, as in the Fortran.
//
// The compute entry points take `npts` points; every per-point scalar is an
// array of npts (a Fortran scalar actual argument is an array of 1). Every OUT
// argument is copied in as well as out, because the Fortran leaves some of them
// unassigned on some paths (WHITECAP after the early return at :2581). LOGICAL
// LLWS crosses as int 0/1. Like ww_snl1(), they are void, not thread-safe, and
// report through ww_st4_last_error().

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// `rpar`/`ipar` are packed at the indices of st4_params.hpp; `nrpar`/`nipar`
/// must equal NRPAR/NIPAR. Returns WW_KOKKOS_OK, NOT_INITIALISED, BAD_SHAPE, or
/// UNSUPPORTED when ipar[I_FLX5] is set.
int ww_st4_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one REAL (or INTEGER) array of grid `igrid` under ArrayId `id`,
/// copied, not borrowed. `lbound` and `extent` have `rank` entries each, as
/// LBOUND() and SHAPE() give them. Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_st4_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);
int ww_st4_set_int(int igrid, int id, const int* data, int rank, const int* lbound,
                   const int* extent);

/// Check grid `igrid`'s arrays against its scalars and copy them to the device.
/// With WW_KOKKOS_ST4_TABLES=1 the tables are rebuilt by the C++ INSIN4 and
/// TABU_* first, and one line on stderr says how many values differ from the
/// Fortran's. Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_st4_commit(int igrid);

/// W3SDS4. a, srhs, ddiag, brlambda (nspec, npts); k, cg (nk, npts);
/// whitecap (4, npts); ustar, usdir, depth, dair, dlwmean (npts).
/// flogrd57, flogrd58: FLOGRD(5,7), FLOGRD(5,8) as 0/1.
void ww_sds4(int igrid, int npts, const float* a, const float* k, const float* cg,
             const float* ustar, const float* usdir, const float* depth, const float* dair,
             const float* dlwmean, int flogrd57, int flogrd58, float* srhs, float* ddiag,
             float* brlambda, float* whitecap);

/// W3SIN4, argument order of the call at w3srcemd.F90:1250. a, k, s, d, llws
/// (nspec, npts); cg (nk, npts); the scalars (npts).
void ww_sin4(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* ustar, const float* drat, const float* as,
             const float* usdir, const float* z0, const float* cd, float* tauwx, float* tauwy,
             float* tauwnx, float* tauwny, float* s, float* d, int* llws);

/// W3SPR4 (and CALC_USTAR, which it calls). a, llws (nspec, npts); cg, wn
/// (nk, npts); ustar, usdir INOUT; the other scalars (npts).
void ww_spr4(int igrid, int npts, const float* a, const float* cg, const float* wn,
             const float* u, const float* udir, const float* tauwx, const float* tauwy,
             const int* llws, float* ustar, float* usdir, float* emean, float* fmean,
             float* fmean1, float* wnmean, float* amax, float* cd, float* z0, float* charn,
             float* fmeanws, float* dlwmean);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SDS4, 2 WW_KOKKOS_SIN4, 3 WW_KOKKOS_SPR4, 4 WW_KOKKOS_ST4_TABLES.
/// Every flag defaults to 0, so the Fortran runs unless a person asks otherwise.
int ww_st4_enabled(int which);

/// The status of the most recent ww_st4_* or ww_sds4/sin4/spr4 call.
int ww_st4_last_error(void);

}  // extern "C"
