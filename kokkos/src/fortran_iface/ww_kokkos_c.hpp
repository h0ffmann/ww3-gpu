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
// Three limits of phase 1, all of them consequences of the shim holding exactly
// one set of tables and one set of device buffers in a file-static context:
//
//  * **One spectral grid per process.** ww_snl1_init() *replaces* the previous
//    grid's tables; it does not add a second grid. A ww3_multi run with two grids
//    of different NK/NTH would compute the second grid's spectra with the first
//    grid's nspec, which is an out-of-bounds read, not a wrong number. The caller
//    must refuse that configuration -- see PATCH.md, the W3INIT hunk.
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
  WW_KOKKOS_ERR_RUNTIME = 4          ///< Kokkos itself failed to start or allocate
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
int ww_kokkos_init(int comm_f);

/// Release the shim's device buffers and tables, and shut Kokkos down **only if
/// ww_kokkos_init() started it**. Safe to call when nothing was initialised, and
/// safe to call twice. After it returns, ww_snl1() reports
/// WW_KOKKOS_ERR_NOT_INITIALISED until ww_snl1_init() runs again.
void ww_kokkos_finalize(void);

/// Build the INSNL1 quadruplet tables for one spectral grid. Call once, after
/// ww_kokkos_init(). The arguments are W3GDATMD's, under WW3's own names.
///
/// `sig` is **SIG(1:nk)**: the pointer must address the *first* frequency bin and
/// the shim reads `nk` floats from it. This matters because W3GDATMD allocates
/// `SIG(0:MK+1)` (w3gdatmd.F90:2066) -- a Fortran caller that passes the bare
/// array name to the assumed-size dummy hands over `&SIG(0)` and every quadruplet
/// is then built one bin low. WW3's own idiom is to pass `SIG(1)`. The array is
/// copied, not borrowed.
///
/// **Phase 1 keeps one grid per process.** A second call replaces the first
/// grid's tables rather than adding to them, so a multi-grid run (`ww3_multi`)
/// must not call this once per grid; see the header comment and PATCH.md.
///
/// Returns WW_KOKKOS_OK, or WW_KOKKOS_ERR_BAD_SHAPE (nk <= 0, nth <= 0, null
/// sig), WW_KOKKOS_ERR_NOT_INITIALISED (no live runtime) or WW_KOKKOS_ERR_RUNTIME.
/// On any failure the shim is left unconfigured rather than half-configured.
int ww_snl1_init(int nk, int nth, float xfr, float dth, float lam, float snlc1,
                 float kdcon, float kdmn, float snls1, float snls2, float snls3,
                 float fachfe, const float* sig);

/// W3SNL1 for `npts` sea points in one launch. Phase 1: copy-in, kernel, copy-out.
///
/// `a`, `s` and `d` are (nspec, npts) and `cg` is (nk, npts), column-major, with
/// nspec = nk*nth from ww_snl1_init(); `kdmean` is (npts). `s` and `d` are
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
void ww_snl1(int npts, const float* a, const float* cg, const float* kdmean, float* s,
             float* d);

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

}  // extern "C"
