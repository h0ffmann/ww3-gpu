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

// W3SRCE ------------------------------------------------------------------------
// The source-term integration of one sea point (W3SRCE(srce_direct, ...)) with
// every source term it calls on Kokkos: ST4, the DIA, W3SLN1, W3SBT1, W3SDB1.
// ww_srce_init() needs ww_snl1_init() and ww_st4_commit() done for the grid: the
// kernel reads their tables. ww_srce(): one point per call, copied in and out
// (phase 1). ww_srce_batch(): every active point of a time step in one call.

/// Configure grid `igrid`: rpar/ipar at the indices of srce_params.hpp; sig, dden
/// (nk), ecos, esin, mapwn (nspec), copied. Returns WW_KOKKOS_OK,
/// NOT_INITIALISED, BAD_SHAPE, or UNSUPPORTED when ipar[I_LAB_SWITCH] is not 1
/// (a source-term switch the kernel does not translate).
int ww_srce_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar,
                 const float* sig, const float* dden, const float* ecos, const float* esin,
                 const int* mapwn);

/// W3SRCE(srce_direct, ...) for one point, the arguments switch_lab_shrd reads,
/// in the order of the Fortran dummy list. spec (nspec), whitecap (4), taubbl
/// (2), tauice (2) and every pointer scalar are INOUT; wn1, cg1 (nk) IN;
/// inflags24, flogrd57, flogrd58 are INFLAGS2(4), FLOGRD(5,7), FLOGRD(5,8) as
/// 0/1; shave is SHAVEIO as 0/1. Status in ww_srce_last_error().
void ww_srce(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float* spec,
             const float* wn1, const float* cg1, float d_inp, float u10abs, float u10dir,
             float as, float ice, float dtg, float dair, float coef, float* ustar,
             float* ustdir, float* tauwx, float* tauwy, float* dtdyn, float* fcut,
             float* tauox, float* tauoy, float* tauwix, float* tauwiy, float* tauwnx,
             float* tauwny, float* phiaw, float* charn, float* tws, float* phioc,
             float* whitecap, float* phibbl, float* taubbl, float* tauice, float* phice,
             float* tauocx, float* tauocy, float* wnmean, int* shave);

/// Row indices of ww_srce_batch()'s per-point input packet pin(WW_SRCE_NPIN, npts):
/// the by-value scalars of ww_srce() that differ from point to point.
enum WwSrceBatchIn {
  WW_SRCE_IN_D_INP = 0,  ///< D_INP  = DW(ISEA)
  WW_SRCE_IN_U10ABS,     ///< U10ABS = U10(ISEA)
  WW_SRCE_IN_U10DIR,     ///< U10DIR = U10D(ISEA)
  WW_SRCE_IN_AS,         ///< AS     = AS(ISEA)
  WW_SRCE_IN_ICE,        ///< ICE    = ICE(ISEA)
  WW_SRCE_IN_DAIR,       ///< DAIR   = RHOAIR(ISEA)
  WW_SRCE_IN_COEF,       ///< COEF   = ASF(ISEA)
  WW_SRCE_NPIN
};

/// Row indices of ww_srce_batch()'s INOUT packet psc(WW_SRCE_NPSC, npts): the
/// pointer scalars of ww_srce() in its argument order, shave excepted.
enum WwSrceBatchScal {
  WW_SRCE_SC_USTAR = 0, WW_SRCE_SC_USTDIR, WW_SRCE_SC_TAUWX, WW_SRCE_SC_TAUWY,
  WW_SRCE_SC_DTDYN, WW_SRCE_SC_FCUT, WW_SRCE_SC_TAUOX, WW_SRCE_SC_TAUOY,
  WW_SRCE_SC_TAUWIX, WW_SRCE_SC_TAUWIY, WW_SRCE_SC_TAUWNX, WW_SRCE_SC_TAUWNY,
  WW_SRCE_SC_PHIAW, WW_SRCE_SC_CHARN, WW_SRCE_SC_TWS, WW_SRCE_SC_PHIOC,
  WW_SRCE_SC_PHIBBL, WW_SRCE_SC_PHICE, WW_SRCE_SC_TAUOCX, WW_SRCE_SC_TAUOCY,
  WW_SRCE_SC_WNMEAN, WW_SRCE_NPSC
};

/// ww_srce() for `npts` points in one call (phase 2, one launch per time step):
/// every array is packed point by point, the point index slowest, i.e. a
/// Fortran (rows, npts) array. spec (nspec, npts), psc (WW_SRCE_NPSC, npts),
/// whitecap (4, npts), taubbl (2, npts), tauice (2, npts) INOUT; wn1, cg1 (nk,
/// npts) and pin (WW_SRCE_NPIN, npts) IN; shave (npts) OUT as 0/1. it, the
/// three flags and dtg belong to the time step and are shared by every point.
/// Per chunk of points (srce_shim.cpp, WW_KOKKOS_SRCE_BATCH_MIB): one copy of
/// each array in, one launch of one team per point, one copy out. npts = 0 is
/// a no-op. Status in ww_srce_last_error().
void ww_srce_batch(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float dtg,
                   int npts, float* spec, const float* wn1, const float* cg1,
                   const float* pin, float* psc, float* whitecap, float* taubbl,
                   float* tauice, int* shave);

/// 1 if WW_KOKKOS_SRCE=1 in the environment, else 0 (the default: the Fortran).
int ww_srce_enabled(void);

/// The status of the most recent ww_srce_init(), ww_srce() or ww_srce_batch() call.
int ww_srce_last_error(void);

// ---------------------------------------------------------------------------
// ST0 (w3src0md.F90 at NOAA-EMC/WW3 03adda91): W3SPR0. st0_shim.cpp implements it.
//
// A grid is configured as ST4's: ww_st0_init() with the scalars, one
// ww_st0_set_real() per array (SIG, DDEN, with their LBOUND and SHAPE as the
// Fortran has them; the shim keeps (1:NK)), then ww_st0_commit(). The prefix
// WW_ST0_ keeps these names apart from the ST4 enums that st4_shim.cpp brings
// into the global namespace.

/// RPAR indices (0-based): W3GDATMD FTE, FTF, FTWN.
enum WwSt0Rpar { WW_ST0_R_FTE = 0, WW_ST0_R_FTF = 1, WW_ST0_R_FTWN = 2, WW_ST0_NRPAR = 3 };
/// IPAR indices (0-based): W3GDATMD NK, NTH, NSPEC.
enum WwSt0Ipar { WW_ST0_I_NK = 0, WW_ST0_I_NTH = 1, WW_ST0_I_NSPEC = 2, WW_ST0_NIPAR = 3 };
/// Array ids of ww_st0_set_real(): W3GDATMD SIG(0:MK+1), DDEN(MK).
enum WwSt0Array { WW_ST0_A_SIG = 0, WW_ST0_A_DDEN = 1, WW_ST0_NARRAYS = 2 };

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, NK <= 0,
/// NSPEC /= NK*NTH).
int ww_st0_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one rank-1 REAL array of grid `igrid` under WwSt0Array `id`, copied.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_st0_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check that SIG and DDEN cover (1:NK) and copy that part to the device.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_st0_commit(int igrid);

/// W3SPR0. a (nspec, npts); cg, wn (nk, npts); emean, fmean, wnmean, amax
/// (npts), OUT. Void, not thread-safe, status in ww_st0_last_error().
void ww_spr0(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SPR0. Defaults to 0, the Fortran. WW_KOKKOS_ST0_COUNT=1 prints
/// "ww_kokkos: W3SPR0 spectra computed: N" to stderr at exit.
int ww_st0_enabled(int which);

/// The status of the most recent ww_st0_* or ww_spr0 call.
int ww_st0_last_error(void);

// ---------------------------------------------------------------------------
// ST1 (w3src1md.F90 at NOAA-EMC/WW3 03adda91): W3SPR1, W3SIN1, W3SDS1, the
// WAM cycle 3 source terms. st1_shim.cpp implements it.
//
// A grid is configured as ST4's is: ww_st1_init() with the scalars, one
// ww_st1_set_real() per array below (registered whole, with its LBOUND; the
// shim keeps elements 1..NK of SIG and DDEN, 1..NSPEC of SIG2, ECOS, ESIN),
// then ww_st1_commit(). ST1 has no tables and no module state. Compute entry
// points take `npts` points, every per-point scalar an array of npts; they are
// void, not thread-safe, and report through ww_st1_last_error().

/// RPAR indices (0-based), W3GDATMD scalars.
enum WwSt1Rpar {
  WW_ST1_R_FTE = 0,  ///< FTE
  WW_ST1_R_FTF,      ///< FTF
  WW_ST1_R_FTWN,     ///< FTWN
  WW_ST1_R_SINC1,    ///< SINC1
  WW_ST1_R_SDSC1,    ///< SDSC1
  WW_ST1_NRPAR
};

/// IPAR indices (0-based).
enum WwSt1Ipar { WW_ST1_I_NK = 0, WW_ST1_I_NTH, WW_ST1_I_NSPEC, WW_ST1_NIPAR };

/// Array ids of ww_st1_set_real(), W3GDATMD arrays.
enum WwSt1Array {
  WW_ST1_A_SIG = 0,  ///< SIG(0:MK+1), elements 1..NK kept
  WW_ST1_A_DDEN,     ///< DDEN(MK), 1..NK
  WW_ST1_A_SIG2,     ///< SIG2(MSPEC), 1..NSPEC
  WW_ST1_A_ECOS,     ///< ECOS(MSPEC+MTH), 1..NSPEC
  WW_ST1_A_ESIN,     ///< ESIN(MSPEC+MTH), 1..NSPEC
  WW_ST1_NARRAYS
};

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE.
int ww_st1_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one REAL array of grid `igrid` under WwSt1Array `id`, copied.
/// rank must be 1; `lbound`, `extent` are LBOUND() and SHAPE(). Returns
/// WW_KOKKOS_OK, NOT_INITIALISED, or TABLE when elements 1..n are not all there.
int ww_st1_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check every array is registered and copy them to the device. Returns
/// WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_st1_commit(int igrid);

/// W3SPR1. a (nspec, npts); cg, wn (nk, npts); emean, fmean, wnmean, amax (npts) OUT.
void ww_spr1(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax);

/// W3SIN1. a, k, s, d (nspec, npts); ustar, usdir (npts). s, d OUT.
void ww_sin1(int igrid, int npts, const float* a, const float* k, const float* ustar,
             const float* usdir, float* s, float* d);

/// W3SDS1. a, k, s, d (nspec, npts); emean, fmean, wnmean (npts). s, d OUT.
void ww_sds1(int igrid, int npts, const float* a, const float* k, const float* emean,
             const float* fmean, const float* wnmean, float* s, float* d);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SPR1, 2 WW_KOKKOS_SIN1, 3 WW_KOKKOS_SDS1. Default 0: the Fortran.
/// WW_KOKKOS_ST1_COUNT=1 prints the spectra each routine computed at exit.
int ww_st1_enabled(int which);

/// The status of the most recent ww_st1_* or ww_spr1/sin1/sds1 call.
int ww_st1_last_error(void);

// ---------------------------------------------------------------------------
// ST2 (w3src2md.F90 at NOAA-EMC/WW3 03adda91): W3SPR2, W3SIN2, W3SDS2, and the
// table INPTAB builds for W3SIN2. st2_shim.cpp implements it.
//
// A grid is configured as ST4's: ww_st2_init() with the scalars, one
// ww_st2_set_real() per array (SIG, SIG2, DDEN, ECOS, ESIN, with their LBOUND and
// SHAPE as the Fortran has them), then ww_st2_commit(). BETATB, DSIGA and DDRAG
// are PRIVATE module variables of W3SRC2MD that INPTAB fills once per process
// (w3iogrmd.F90:1493, FLINP is SAVE): the Fortran cannot pass them, so the first
// ww_st2_commit() of the process builds them with the C++ INPTAB, one copy per
// process, as in the Fortran. The prefix WW_ST2_ keeps these names apart from
// the ST4 enums that st4_shim.cpp brings into the global namespace.
//
// The compute entry points take `npts` points; every per-point scalar is an
// array of npts. Every OUT argument of the three routines is assigned on every
// path, so OUT arrays are copied out only. Like ww_snl1(), they are void, not
// thread-safe, and report through ww_st2_last_error().

/// RPAR indices (0-based), the W3GDATMD variables of the same name.
enum WwSt2Rpar {
  WW_ST2_R_DTH = 0,
  WW_ST2_R_XFR = 1,
  WW_ST2_R_FTE = 2,
  WW_ST2_R_FTF = 3,
  WW_ST2_R_FTWN = 4,
  WW_ST2_R_FTTR = 5,
  WW_ST2_R_FPIMIN = 6,
  WW_ST2_R_ZWIND = 7,
  WW_ST2_R_FSWELL = 8,
  WW_ST2_R_FACTI1 = 9,
  WW_ST2_R_FACTI2 = 10,
  WW_ST2_R_XF1 = 11,
  WW_ST2_R_XF2 = 12,
  WW_ST2_R_XFH = 13,
  WW_ST2_R_SDSALN = 14,
  WW_ST2_R_CDSA0 = 15,
  WW_ST2_R_CDSA1 = 16,
  WW_ST2_R_CDSA2 = 17,
  WW_ST2_R_CDSB0 = 18,
  WW_ST2_R_CDSB1 = 19,
  WW_ST2_R_CDSB2 = 20,
  WW_ST2_R_CDSB3 = 21,
  WW_ST2_NRPAR = 22
};
/// IPAR indices (0-based): W3GDATMD NK, NTH, NSPEC.
enum WwSt2Ipar { WW_ST2_I_NK = 0, WW_ST2_I_NTH = 1, WW_ST2_I_NSPEC = 2, WW_ST2_NIPAR = 3 };
/// Array ids of ww_st2_set_real(): W3GDATMD SIG(0:MK+1), SIG2(MSPEC), DDEN(MK),
/// ECOS(MSPEC+MTH), ESIN(MSPEC+MTH).
enum WwSt2Array {
  WW_ST2_A_SIG = 0,
  WW_ST2_A_SIG2 = 1,
  WW_ST2_A_DDEN = 2,
  WW_ST2_A_ECOS = 3,
  WW_ST2_A_ESIN = 4,
  WW_ST2_NARRAYS = 5
};

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, NK <= 0,
/// NSPEC /= NK*NTH).
int ww_st2_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one rank-1 REAL array of grid `igrid` under WwSt2Array `id`, copied.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_st2_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check that SIG covers (1:NK+1) (W3SDS2 reads SIG(NK+1)), DDEN (1:NK), SIG2,
/// ECOS, ESIN (1:NSPEC), copy those parts to the device, and build BETATB with
/// the C++ INPTAB if this process has not yet. Returns WW_KOKKOS_OK,
/// NOT_INITIALISED, TABLE or RUNTIME.
int ww_st2_commit(int igrid);

/// W3SPR2, the Fortran's argument order. a (nspec, npts); cg, wn (nk, npts);
/// depth, fpi, u, ustar (npts) IN; emean, fmean, wnmean, amax, fp (npts) and
/// alfa (nk, npts) OUT.
void ww_spr2(int igrid, int npts, const float* a, const float* cg, const float* wn,
             const float* depth, const float* fpi, const float* u, const float* ustar,
             float* emean, float* fmean, float* wnmean, float* amax, float* alfa, float* fp);

/// W3SIN2, the Fortran's argument order. a, k (nspec, npts); cg (nk, npts);
/// u, udir, cd, z0 (npts) IN; fpi (npts), s, d (nspec, npts) OUT.
void ww_sin2(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* udir, const float* cd, const float* z0, float* fpi,
             float* s, float* d);

/// W3SDS2, the Fortran's argument order. a (nspec, npts); cg, k, alfa (nk, npts);
/// fpi, ustar (npts) IN; s, d (nspec, npts) OUT.
void ww_sds2(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* fpi, const float* ustar, const float* alfa, float* s, float* d);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SPR2, 2 WW_KOKKOS_SIN2, 3 WW_KOKKOS_SDS2. Every flag defaults to 0,
/// the Fortran. WW_KOKKOS_ST2_COUNT=1 prints the number of spectra each entry
/// point computed to stderr at exit.
int ww_st2_enabled(int which);

/// The status of the most recent ww_st2_* or ww_spr2/sin2/sds2 call.
int ww_st2_last_error(void);

// ---------------------------------------------------------------------------
// ST3 (w3src3md.F90 at NOAA-EMC/WW3 03adda91): W3SPR3, W3SIN3, W3SDS3, and the
// tables INSIN3 builds for them. st3_shim.cpp implements it.
//
// A grid is configured as ST4's: ww_st3_init() with the scalars, one
// ww_st3_set_real() per array (SIG, SIG2, DDEN, DDEN2, ECOS, ESIN, with their
// LBOUND and SHAPE as the Fortran has them), then ww_st3_commit(). TAUT, DELU,
// DELTAUW, TAUHFT, DELUST and DELALP are W3SRC3MD module variables that INSIN3
// fills once per process, from the first mod_def read (w3iogrmd.F90:1531-1534,
// FLINP is SAVE): the first ww_st3_commit() of the process builds them with the
// C++ INSIN3 from that grid's SIG(NK), ZZWND, AALPHA, ZZ0MAX, BBETA, ZZALP, one
// copy per process, as in the Fortran; later grids use them unchanged.
//
// W3_STAB3 changes W3SIN3 only (w3src3md.F90:557-567, :642-649). The shim is one
// object for every switch file, so the setup passes WW_ST3_I_STAB3 = 1 when the
// WW3 build has W3_STAB3 and the shim picks the kernel at run time.
//
// The compute entry points take `npts` points; every per-point scalar is an
// array of npts. LLWS crosses as int 0/1. Arguments the Fortran routine never
// reads (W3SIN3: CD, ICE, IX, IY; W3SDS3: CG, USTAR, USDIR, DEPTH, IX, IY) are
// not part of the C ABI. Like ww_snl1(), they are void, not thread-safe, and
// report through ww_st3_last_error().

/// RPAR indices (0-based), the W3GDATMD variables of the same name;
/// SSWELLF1 is SSWELLF(1).
enum WwSt3Rpar {
  WW_ST3_R_DTH = 0,
  WW_ST3_R_ZZWND = 1,
  WW_ST3_R_AALPHA = 2,
  WW_ST3_R_BBETA = 3,
  WW_ST3_R_ZZ0MAX = 4,
  WW_ST3_R_ZZALP = 5,
  WW_ST3_R_SSINTHP = 6,
  WW_ST3_R_SSWELLF1 = 7,
  WW_ST3_R_SSDSC1 = 8,
  WW_ST3_R_DDELTA1 = 9,
  WW_ST3_R_DDELTA2 = 10,
  WW_ST3_R_WWNMEANP = 11,
  WW_ST3_R_WWNMEANPTAIL = 12,
  WW_ST3_R_FTE = 13,
  WW_ST3_R_SSTXFTF = 14,
  WW_ST3_R_SSTXFTWN = 15,
  WW_ST3_R_SSTXFTFTAIL = 16,
  WW_ST3_NRPAR = 17
};
/// IPAR indices (0-based): W3GDATMD NK, NTH, NSPEC; STAB3 1 when the WW3 build
/// has the W3_STAB3 switch, else 0.
enum WwSt3Ipar {
  WW_ST3_I_NK = 0,
  WW_ST3_I_NTH = 1,
  WW_ST3_I_NSPEC = 2,
  WW_ST3_I_STAB3 = 3,
  WW_ST3_NIPAR = 4
};
/// Array ids of ww_st3_set_real(): W3GDATMD SIG(0:MK+1), SIG2(MSPEC), DDEN(MK),
/// DDEN2(MSPEC), ECOS(MSPEC+MTH), ESIN(MSPEC+MTH).
enum WwSt3Array {
  WW_ST3_A_SIG = 0,
  WW_ST3_A_SIG2 = 1,
  WW_ST3_A_DDEN = 2,
  WW_ST3_A_DDEN2 = 3,
  WW_ST3_A_ECOS = 4,
  WW_ST3_A_ESIN = 5,
  WW_ST3_NARRAYS = 6
};

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, NK <= 0,
/// NSPEC /= NK*NTH, STAB3 not 0 or 1).
int ww_st3_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one rank-1 REAL array of grid `igrid` under WwSt3Array `id`, copied.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_st3_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check that SIG and DDEN cover (1:NK) and SIG2, DDEN2, ECOS, ESIN (1:NSPEC),
/// copy those parts to the device, and build the INSIN3 tables from this grid if
/// this process has not yet. Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or
/// RUNTIME.
int ww_st3_commit(int igrid);

/// W3SPR3, the Fortran's argument order. a (nspec, npts); cg, wn (nk, npts);
/// u, udir, tauwx, tauwy (npts) and llws (nspec, npts) IN; ustar, usdir (npts)
/// INOUT; emean, fmean, fmeans, wnmean, amax, cd, z0, charn, fmeanws (npts) OUT.
void ww_spr3(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* fmeans, float* wnmean, float* amax,
             const float* u, const float* udir, float* ustar, float* usdir,
             const float* tauwx, const float* tauwy, float* cd, float* z0, float* charn,
             const int* llws, float* fmeanws);

/// W3SIN3, the Fortran's argument order less CD, ICE, IX, IY. a, k (nspec, npts);
/// cg (nk, npts); u, ustar, drat, as, usdir, z0 (npts) IN; tauwx, tauwy, tauwnx,
/// tauwny (npts), s, d (nspec, npts) and llws (nspec, npts, 0/1) OUT.
void ww_sin3(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* ustar, const float* drat, const float* as,
             const float* usdir, const float* z0, float* tauwx, float* tauwy, float* tauwnx,
             float* tauwny, float* s, float* d, int* llws);

/// W3SDS3, the Fortran's argument order less CG, USTAR, USDIR, DEPTH, IX, IY.
/// a (nspec, npts); k (nk, npts); emean, fmean, wnmean (npts) IN; s, d
/// (nspec, npts) OUT.
void ww_sds3(int igrid, int npts, const float* a, const float* k, const float* emean,
             const float* fmean, const float* wnmean, float* s, float* d);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SPR3, 2 WW_KOKKOS_SIN3, 3 WW_KOKKOS_SDS3. Every flag defaults to 0,
/// the Fortran. WW_KOKKOS_ST3_COUNT=1 prints the number of spectra each entry
/// point computed to stderr at exit.
int ww_st3_enabled(int which);

/// The status of the most recent ww_st3_* or ww_spr3/sin3/sds3 call.
int ww_st3_last_error(void);

// ---------------------------------------------------------------------------
// FLX (w3flx1md.F90 .. w3flx5md.F90 at NOAA-EMC/WW3 03adda91): W3FLX1..W3FLX5,
// the friction velocity, roughness length and drag coefficient of W3SRCE. At
// most one of the switches FLX1..FLX5 is compiled. flx_shim.cpp implements it.
//
// A grid is configured as ST4's is: ww_flx_init() with the scalars, then (FLX2
// and FLX3 only) ww_flx_set_real() with W3DISPMD's EWN1(0:NAR1D), then
// ww_flx_commit(). EWN1, DSIE and N1MAX are W3DISPMD module variables: the
// shim keeps one copy per process, the last grid to register them wins (DISTAB
// fills them once per process from constants, so every grid registers the
// same bits). Compute entry points take `npts` points, every per-point scalar
// an array of npts; every OUT argument is copied in as well as out (W3FLX2
// leaves Z0, CD as they were when NITTIN <= 0; W3FLX3 then reads CD). They are
// void, not thread-safe, and report through ww_flx_last_error().
//
// Where the Fortran calls EXTCDE (W3FLX1, W3FLX4, W3FLX5: ZWND not 10 m) the
// kernel writes its code to `iext` (0 otherwise) and leaves that point's
// outputs as they came in; the caller prints the routine's message and stops.

/// RPAR indices (0-based). A slot the compiled switch does not have is 0.
enum WwFlxRpar {
  WW_FLX_R_CINXSI = 0,  ///< W3GDATMD CINXSI (FLX2, FLX3)
  WW_FLX_R_CD_MAX,      ///< W3GDATMD CD_MAX (FLX3)
  WW_FLX_R_FLX4A0,      ///< W3GDATMD FLX4A0 (FLX4)
  WW_FLX_R_DSIE,        ///< W3DISPMD DSIE, module (FLX2, FLX3)
  WW_FLX_NRPAR
};

/// IPAR indices (0-based).
enum WwFlxIpar {
  WW_FLX_I_FLX = 0,  ///< n of the compiled W3_FLXn, 1..5
  WW_FLX_I_NITTIN,   ///< W3GDATMD NITTIN (FLX2, FLX3)
  WW_FLX_I_CAP_ID,   ///< W3GDATMD CAP_ID (FLX3)
  WW_FLX_I_N1MAX,    ///< W3DISPMD N1MAX, module (FLX2, FLX3)
  WW_FLX_NIPAR
};

/// Array ids of ww_flx_set_real().
enum WwFlxArray {
  WW_FLX_A_EWN1 = 0,  ///< W3DISPMD EWN1(0:NAR1D), NAR1D = 121: rank 1, LBOUND 0, 122 values
  WW_FLX_NARRAYS
};

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, FLX not 1..5).
int ww_flx_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register EWN1 (`id` WW_FLX_A_EWN1), copied, into the process-wide copy.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE (id, rank, LBOUND, extent).
int ww_flx_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check grid `igrid` (FLX2, FLX3: EWN1 registered, N1MAX + 1 <= NAR1D) and copy
/// EWN1 to the device. With WW_KOKKOS_FLX_TABLES=1, EWN1, DSIE and N1MAX are
/// rebuilt by the C++ DISTAB first and one line on stderr says how many differ
/// from the Fortran's. Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_flx_commit(int igrid);

/// W3FLX1(ZWND, U10, U10D, UST, USTD, Z0, CD). zwnd, u10, u10d IN; ust, ustd,
/// z0, cd INOUT; iext OUT (npts each).
void ww_flx1(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             float* ust, float* ustd, float* z0, float* cd, int* iext);

/// W3FLX2(ZWIND, DEPTH, FP, U, UDIR, UST, USTD, Z0, CD). ust INOUT (it starts
/// the iteration); ustd, z0, cd INOUT (npts each).
void ww_flx2(int igrid, int npts, const float* zwind, const float* depth, const float* fp,
             const float* u, const float* udir, float* ust, float* ustd, float* z0, float* cd);

/// W3FLX3, the arguments of W3FLX2.
void ww_flx3(int igrid, int npts, const float* zwind, const float* depth, const float* fp,
             const float* u, const float* udir, float* ust, float* ustd, float* z0, float* cd);

/// W3FLX4(ZWND, U10, U10D, UST, USTD, Z0, CD), the arguments of ww_flx1.
void ww_flx4(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             float* ust, float* ustd, float* z0, float* cd, int* iext);

/// W3FLX5(ZWND, U10, U10D, TAUA, TAUADIR, RHOAIR, UST, USTD, Z0, CD, CHARN).
void ww_flx5(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             const float* taua, const float* tauadir, const float* rhoair, float* ust,
             float* ustd, float* z0, float* cd, float* charn, int* iext);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1..5
/// WW_KOKKOS_FLX1..WW_KOKKOS_FLX5, 6 WW_KOKKOS_FLX_TABLES. Every flag defaults
/// to 0, the Fortran. WW_KOKKOS_FLX_COUNT=1 prints the points computed per
/// routine to stderr at exit.
int ww_flx_enabled(int which);

/// The status of the most recent ww_flx_* or ww_flx1..ww_flx5 call.
int ww_flx_last_error(void);

// ---------------------------------------------------------------------------
// SWL (w3swldmd.F90 at NOAA-EMC/WW3 03adda91): W3SWL6. swl_shim.cpp implements it.
// W3SWL4 has no caller in WW3 and has no entry point.
//
// A grid is configured as ST4's: ww_swl_init() with the scalars, one
// ww_swl_set_real() per array (SIG, DDEN, with their LBOUND and SHAPE as the
// Fortran has them; the shim keeps (1:NK)), then ww_swl_commit(). W3SWL6's SAVE
// IKN is one per process, built by the first ww_swl6() call from that grid's
// NSPEC and NTH, as the Fortran builds it on its first call.

/// RPAR indices (0-based): W3GDATMD DTH, SWL6B1.
enum WwSwlRpar { WW_SWL_R_DTH = 0, WW_SWL_R_SWL6B1 = 1, WW_SWL_NRPAR = 2 };
/// IPAR indices (0-based): W3GDATMD NK, NTH, NSPEC, SWL6CSTB1 (1 = .TRUE.).
enum WwSwlIpar {
  WW_SWL_I_NK = 0,
  WW_SWL_I_NTH = 1,
  WW_SWL_I_NSPEC = 2,
  WW_SWL_I_SWL6CSTB1 = 3,
  WW_SWL_NIPAR = 4
};
/// Array ids of ww_swl_set_real(): W3GDATMD SIG(0:MK+1), DDEN(MK).
enum WwSwlArray { WW_SWL_A_SIG = 0, WW_SWL_A_DDEN = 1, WW_SWL_NARRAYS = 2 };
/// ww_swl6()'s return where W3SWL6 calls EXTCDE(2) after FORMAT 6001
/// (SIZE(IKN) /= NK, w3swldmd.F90:374-377).
enum WwSwlCode { WW_SWL_EXTCDE = 2 };

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, NK <= 0,
/// NTH <= 0, NSPEC /= NK*NTH).
int ww_swl_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one rank-1 REAL array of grid `igrid` under WwSwlArray `id`, copied.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_swl_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check that SIG and DDEN cover (1:NK) and copy that part to the device.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_swl_commit(int igrid);

/// W3SWL6. a, s, d (nspec, npts); cg, wn (nk, npts); s, d OUT. Returns 0, or
/// WW_SWL_EXTCDE with s and d not written and *nikn = SIZE(IKN), for the
/// caller's FORMAT 6001 and EXTCDE(2). Not thread-safe; failures of the shim
/// itself are in ww_swl_last_error(), not in the return value.
int ww_swl6(int igrid, int npts, const float* a, const float* cg, const float* wn, float* s,
            float* d, int* nikn);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SWL6. Defaults to 0, the Fortran. WW_KOKKOS_SWL_COUNT=1 prints
/// "ww_kokkos: W3SWL6 spectra computed: N" to stderr at exit.
int ww_swl_enabled(int which);

/// The status of the most recent ww_swl_* or ww_swl6 call.
int ww_swl_last_error(void);

// ---------------------------------------------------------------------------
// ST6 (w3src6md.F90 at NOAA-EMC/WW3 03adda91): W3SPR6, W3SIN6 (with LFACTOR and
// TAU_WAVE_ATMOS), W3SDS6. st6_shim.cpp implements it.
//
// A grid is configured as ST4's: ww_st6_init() with the scalars, one
// ww_st6_set_real() per array (with its LBOUND and SHAPE as the Fortran has
// them; the shim keeps (1:NK) or (1:NSPEC)), then ww_st6_commit().
//
// W3SRC6MD's SAVE tables are one per process, as in the Fortran (w3src6md.F90:103-108):
// IKN is built by the first ww_sin6() or ww_sds6() call from that call's grid
// (:449-456, :696-704); MK10Hz, IK10Hz, SIG10Hz, DSII10Hz by the first ww_sin6()
// call, from that grid's SIG2(IKN), DDEN2(IKN)/DTH/SIG2(IKN) and XFR (the first
// LFACTOR call, :955-1013). Never rebuilt, never per grid; DSII10Hz is rewritten
// after every ww_sin6() where TAU_WAVE_ATMOS rewrites it (:1294-1297).

/// RPAR indices (0-based): W3GDATMD DTH, XFR, FTE, FTF, FTWN, SIN6A0, SIN6WS,
/// SIN6CHKMIN, SIN6CHKCAP, SIN6CHKINF, SIN6CHKSIG, SDS6A1, SDS6A2.
enum WwSt6Rpar {
  WW_ST6_R_DTH = 0,
  WW_ST6_R_XFR = 1,
  WW_ST6_R_FTE = 2,
  WW_ST6_R_FTF = 3,
  WW_ST6_R_FTWN = 4,
  WW_ST6_R_SIN6A0 = 5,
  WW_ST6_R_SIN6WS = 6,
  WW_ST6_R_SIN6CHKMIN = 7,
  WW_ST6_R_SIN6CHKCAP = 8,
  WW_ST6_R_SIN6CHKINF = 9,
  WW_ST6_R_SIN6CHKSIG = 10,
  WW_ST6_R_SDS6A1 = 11,
  WW_ST6_R_SDS6A2 = 12,
  WW_ST6_NRPAR = 13
};
/// IPAR indices (0-based): W3GDATMD NK, NTH, NSPEC, SIN6FLCAP (1 = .TRUE.),
/// SDS6P1, SDS6P2, SDS6ET (1 = .TRUE.).
enum WwSt6Ipar {
  WW_ST6_I_NK = 0,
  WW_ST6_I_NTH = 1,
  WW_ST6_I_NSPEC = 2,
  WW_ST6_I_SIN6FLCAP = 3,
  WW_ST6_I_SDS6P1 = 4,
  WW_ST6_I_SDS6P2 = 5,
  WW_ST6_I_SDS6ET = 6,
  WW_ST6_NIPAR = 7
};
/// Array ids of ww_st6_set_real(): W3GDATMD SIG(0:MK+1), DDEN(MK), DSII(MK),
/// SIG2(MSPEC), DDEN2(MSPEC), ECOS(MSPEC+MTH), ESIN(MSPEC+MTH).
enum WwSt6Array {
  WW_ST6_A_SIG = 0,
  WW_ST6_A_DDEN = 1,
  WW_ST6_A_DSII = 2,
  WW_ST6_A_SIG2 = 3,
  WW_ST6_A_DDEN2 = 4,
  WW_ST6_A_ECOS = 5,
  WW_ST6_A_ESIN = 6,
  WW_ST6_NARRAYS = 7
};
/// Return values of ww_sin6() and ww_sds6(): where the Fortran stops or warns,
/// with the two integers of its FORMAT in info[0], info[1].
enum WwSt6Code {
  /// SIZE(IKN) /= NK when IKN is built (W3SIN6 :451-454, W3SDS6 :699-702):
  /// the calling routine's FORMAT 601, EXTCDE(2). info = SIZE(IKN), NK.
  WW_ST6_IKN = 1,
  /// SIZE(IK10Hz) /= NK10Hz in the first LFACTOR call (:958-961): LFACTOR's
  /// FORMAT 601, EXTCDE(2). info = SIZE(IK10Hz), NK10Hz.
  WW_ST6_LF_IK10HZ = 2,
  /// MK10Hz /= NK10Hz in LFACTOR (:983-986): its FORMAT 602, EXTCDE(3).
  /// info = MK10Hz, NK10Hz.
  WW_ST6_LF_MK10HZ = 3,
  /// MK10Hz /= NK10Hz in TAU_WAVE_ATMOS (:1274-1277): its FORMAT 602,
  /// EXTCDE(3). info = MK10Hz, NK10Hz.
  WW_ST6_TWA_MK10HZ = 4,
  /// LFACTOR reached ITERMAX (:1093-1094): the Fortran writes FORMAT 280 to
  /// NDST and goes on. Outputs written; info[0] = the first such point (0-based).
  WW_ST6_LF_ITERMAX = 5
};

/// Start (or restart) the configuration of grid `igrid` (W3INIT's IMOD).
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or BAD_SHAPE (lengths, NK <= 0,
/// NTH <= 0, NSPEC /= NK*NTH).
int ww_st6_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar);

/// Register one rank-1 REAL array of grid `igrid` under WwSt6Array `id`, copied.
/// Returns WW_KOKKOS_OK, NOT_INITIALISED or TABLE.
int ww_st6_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent);

/// Check that every array covers (1:NK) or (1:NSPEC) and copy that part to the
/// device. Returns WW_KOKKOS_OK, NOT_INITIALISED, TABLE or RUNTIME.
int ww_st6_commit(int igrid);

/// W3SPR6. a (nspec, npts); cg, wn (nk, npts); emean, fmean, wnmean, amax, fp
/// (npts) OUT. Not thread-safe; failures in ww_st6_last_error().
void ww_spr6(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax, float* fp);

/// W3SIN6. a, wn2, s, d (nspec, npts); cg (nk, npts); uabs, ustar, usdir, cd, dair,
/// tauwx, tauwy, taunwx, taunwy, charn (npts). Returns 0 or a WwSt6Code with info
/// (2 ints) set. On IKN, LF_IK10HZ, LF_MK10HZ, TWA_MK10HZ (the first such point in
/// index order) nothing is written; on 0 and LF_ITERMAX every output is. Not
/// thread-safe; failures of the shim itself in ww_st6_last_error().
int ww_sin6(int igrid, int npts, const float* a, const float* cg, const float* wn2,
            const float* uabs, const float* ustar, const float* usdir, const float* cd,
            const float* dair, float* tauwx, float* tauwy, float* taunwx, float* taunwy,
            float* charn, float* s, float* d, int* info);

/// W3SDS6. a, s, d (nspec, npts); cg, wn (nk, npts). Returns 0, or WW_ST6_IKN with
/// s, d not written and info set. Not thread-safe.
int ww_sds6(int igrid, int npts, const float* a, const float* cg, const float* wn, float* s,
            float* d, int* info);

/// 1 if the run-time flag is "1" in the environment, else 0: which = 1
/// WW_KOKKOS_SPR6, 2 WW_KOKKOS_SIN6, 3 WW_KOKKOS_SDS6. Defaults to 0, the
/// Fortran. WW_KOKKOS_ST6_COUNT=1 prints "ww_kokkos: W3<R> spectra computed: N"
/// for R = SPR6, SIN6, SDS6 to stderr at exit.
int ww_st6_enabled(int which);

/// The status of the most recent ww_st6_*, ww_spr6, ww_sin6 or ww_sds6 call.
int ww_st6_last_error(void);

// ---------------------------------------------------------------------------
// Run-time flags (part_flags.cpp). Every ww_*_enabled() above and W3KOKKOS_SETUP
// resolve WW_KOKKOS_<PART> through this one function.

/// The state of the flag `name` (e.g. "WW_KOKKOS_SNL1"): 0 off, 1 on because the
/// variable is "1", 2 on because it is unset and WW_KOKKOS_ALL is "1". A set
/// variable other than "1" is off whatever WW_KOKKOS_ALL says.
int ww_kokkos_flag(const char* name);

// ---------------------------------------------------------------------------
// W3WAVE's per-step support routines (wave_support_shim.cpp): W3GATH, W3SCAT
// (W3_SHRD), W3UWND (WNT1, RWND), W3UTRN (IC0), W3DZXY, W3MAP3, W3MAPT (PR3), and
// DISTAB. No per-grid state: every call passes its sizes, copies its arrays in and
// its results out (phase 1). Each returns WW_KOKKOS_OK, NOT_INITIALISED, BAD_SHAPE,
// KERNEL or RUNTIME. Not thread-safe: the Fortran wrappers serialise them in
// !$OMP CRITICAL (WW_KOKKOS_WAVE). The argument tables are in the .cpp.
// WW_KOKKOS_WAVE_COUNT=1 prints "ww_kokkos: W3<R> calls: N" per routine at exit.

int ww_wave_gath(int ispec, int nspec, int nsea, int ncol, int msea, int nx, int ny,
                 const float* va, const int* mapsf, float* field);
int ww_wave_scat(int ispec, int nspec, int nsea, int ncol, int msea, int nx, int ny,
                 const int* mapsta, const float* field, const int* mapsf, float* va);
int ww_wave_uwnd(int flfrst, float dt0n, float dt0t, int flcur, float rwindc, int nsea, int msea,
                 int nx, int ny, const int* mapsf, const float* wx0, const float* wy0,
                 const float* dt0, const float* wxn, const float* wyn, const float* dtn,
                 const float* cx, const float* cy, float* ua0, float* uai, float* ud0,
                 float* udi, float* as0, float* asi, float* ua, float* ud, float* as,
                 float* u10, float* u10d);
int ww_wave_utrn(int nx, int ny, int nsea, int msea, int trflag, float fice0, float ficen,
                 float ficel, float ffacberg, int flagll, const float* trnx, const float* trny,
                 const int* mapsf, const int* mapsta, const float* hpfac, const float* hqfac,
                 const float* ice, const float* berg, float* atrnx, float* atrny);
int ww_wave_dzxy(int nsea, int nx, int ny, const float* zz, const int* mapsta, const int* mapfs,
                 const float* dpdx, const float* dpdy, const float* dqdx, const float* dqdy,
                 int flagll, int iclose, float* dzzdx, float* dzzdy);
int ww_wave_map3(int nx, int ny, int nsea, int msea, int nk, int nth, int gtype, const int* mapsta,
                 const int* mapsf, int* mapx2, int* mapy2, int* mapaxy, int* mapcxy, int* mapth2,
                 int* mapwn2, int* counts);
int ww_wave_mapt(int nsea, int msea, int ncell, const int* mapsf, const float* atrnx,
                 const float* atrny, int* maptrn);
int ww_wave_distab(int nar1d, float* ewn1, float* ecg1, int* n1max, float* dsie);

}  // extern "C"
