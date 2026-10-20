# `PATCH.md`: wiring the Kokkos DIA into WAVEWATCH III

## The fork today, 2026-10-17

Read with `git -C WW3 fetch origin && git -C WW3 branch -r` and `git rev-list --count` (v,
2026-10-16):

| h0ffmann/WW3 branch | Head | Against the `WW3/` pin | What it wires |
|---|---|---|---|
| `ww3-gpu/strict-outg` | `fac74069` | one commit on `50a10730` (the pin before it, also the head of `ww3-gpu/kokkos-pnt`), pushed 2026-10-18 (v, `ls-remote`) | `W3KOKKOS_OUTG`'s call of the Fortran `W3OUTG` reports to `WW_KOKKOS_STRICT` ([section 31](#31-w3outgs-fortran-fallback-reports-to-strict-mode-ww3-gpustrict-outg)) |
| `ww3-gpu/integ-fixes` | `711afb80` | one commit on `ww3-gpu/integ-abbb0456` (`b733066b`, the merge of `u1-multi-va`, `u2-strict-preset`, `u8-out-readers` and `u9-cprt` onto `80644ba9`), pushed 2026-10-18 (v, `ls-remote`) | `W3KOKKOS_CPRT`'s two fallbacks to the Fortran `W3CPRT` report to `WW_KOKKOS_STRICT` ([section 28](#28-the-integration-fixes-ww3-gpuinteg-fixes)) |
| `ww3-gpu/u1-multi-va` | `f330dc53` | five commits on `80644ba9`, pushed 2026-10-18 (v, `ls-remote`) | `ww3_multi`'s exchanges sync only the columns of `VA` they read or write (`W3KOKKOS_VA_READ_COLS`, `W3KOKKOS_VA_WRITE_COLS`, `ww_va_host_wrote_cols`), no read at `W3WAVE`'s return, the ABI `0.10` ([section 26](#26-ww3_multi-and-the-resident-va-column-syncs-no-read-at-w3waves-return-ww3-gpuu1-multi-va)) |
| `ww3-gpu/kokkos-outg` | `e0cc8ee9` | the `WW3/` pin of h0ffmann/ww3-gpu `goal/42-gpu-step-units45`; one commit on `ww3-gpu/kokkos-prop3dev`, pushed 2026-10-17 (v) | `W3OUTG`'s integrals on the device copy of `VA` (`ww_outg_va`, `W3KOKKOS_OUTGMD`), the host read of `VA` at an output time split per reader, the ABI `0.7` ([section 15](#15-w3outg-on-the-resident-spectrum-ww3-gpukokkos-outg)) |
| `ww3-gpu/kokkos-prop3dev` | `90631fad` | the parent of `e0cc8ee9`, the `WW3/` pin of h0ffmann/ww3-gpu `goal/42-srce-prop3dev`; one commit on `ww3-gpu/kokkos-resident`, pushed 2026-10-17 (v) | `W3XYP3` and `W3KTP3` batched on the device copy of `VA` (`ww_xyp3_batch_va`, `ww_ktp3_batch_va`), their host syncs only on the Fortran path, the ABI `0.6` ([section 13](#13-the-propagation-on-the-resident-spectrum-ww3-gpukokkos-prop3dev)) |
| `ww3-gpu/kokkos-resident` | `513954a6` | the parent of `90631fad`, the `WW3/` pin of h0ffmann/ww3-gpu `goal/42-srce-resident`; two commits on `ww3-gpu/kokkos-prop3`, pushed 2026-10-17 (v) | the resident spectrum: `VA` synced at W3WAVE's host statements (`W3KOKKOS_RESIDENTMD`) and `W3SRCE` batched on the device copy of `VA` (`ww_srce_batch_va`), the ABI `0.5` ([section 12](#12-the-resident-spectrum-on-ww3-gpukokkos-resident)) |
| `ww3-gpu/kokkos-prop3` | `fcdfabef` | the parent of the resident branch, the `WW3/` pin of h0ffmann/ww3-gpu `goal/42-srce-prop3`; one commit on `ww3-gpu/kokkos-srce-batch`, pushed 2026-10-17 (v) | `W3XYP3` for every spectral component and `W3KTP3` for every sea point in one call each, opt-in (`WW_KOKKOS_XYP3`, `WW_KOKKOS_KTP3`, `ww_xyp3_batch`, `ww_ktp3_batch`) ([section 11](#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3)) |
| `ww3-gpu/kokkos-srce-batch` | `caa5da36` | the parent of `fcdfabef`, the `WW3/` pin of h0ffmann/ww3-gpu `goal/42-srce-batch`; one commit on `ww3-gpu/kokkos-wire-base`, pushed 2026-10-17 (v) | `W3SRCE` for every sea point of a time step in one call (`W3KOKKOS_SRCE_BATCH`, `ww_srce_batch`), the ABI `0.4` ([section 10](#10-w3srce-batched-on-ww3-gpukokkos-srce-batch)) |
| `ww3-gpu/kokkos-wire-base` | `4a94a8f6` | **the pin** since this change; one commit on `ww3-gpu/kokkos-wave-support`, pushed 2026-10-16 | no routine: the part registry, the per-unit module files and the generated `W3KOKKOS_UNITSMD` every wiring unit builds on ([section 9](#9-wiring-units-in-parallel)) |
| `ww3-gpu/kokkos-wave-support` | `357f575c` | the pin's parent; one commit on `ww3-gpu/kokkos-st-flx-swl`, pushed 2026-10-16 | `W3GATH`, `W3SCAT`, `W3UWND`, `W3UTRN`, `W3DZXY`, `W3MAP3`, `W3MAPT`, `DISTAB`, the master flag `WW_KOKKOS_ALL` and the parts line in `log.ww3` ([sections 7](#7-run-time-flags-one-per-part-and-a-master) and [8](#8-w3waves-support-routines-on-ww3-gpukokkos-wave-support)) |
| `ww3-gpu/kokkos-st4` | `c9a6a0c6` | contained in the pin | the DIA (`W3SNL1`, `INSNL1`) and ST4 (`W3SDS4`, `W3SIN4`, `W3SPR4` and their tables) in `w3srcemd.F90` and `w3initmd.F90`, behind the `KOKKOS` switch |
| `ww3-gpu/kokkos-srce` | `e062ff0b` | contained in the pin | `W3SRCE` itself (`WW_KOKKOS_SRCE`, with `LN1`, `BT1`, `DB1`): `w3wavemd.F90` +24, `w3initmd.F90` +23, `w3kokkosmd.F90` +212 lines |
| `ww3-gpu/kokkos-st-flx-swl` | `8e433f99` | the parent of `357f575c`; seven commits on `ww3-gpu/kokkos-srce`, pushed (`git ls-remote`, v 2026-10-16) | ST0, ST1, ST2, ST3, ST6 (`W3SPRn`, `W3SINn`, `W3SDSn`), `W3FLX1` to `W3FLX5` and `W3SWL6`, one commit per unit, compiled; ST4 and `W3SRCE` from it ran in this change's checkpoint, the other units not ([section 6](#6-the-source-term-units-on-ww3-gpukokkos-st-flx-swl)) |
| `hoff/kokkos-snl1` | `bb08623a` | two commits behind, contained in the pin | the DIA alone; superseded |
| `develop` | `03adda91` | the fork's base | none; equal to NOAA-EMC/WW3 `develop` on 2026-10-16, so there is no upstream change to take |

What the next plug-ins need from the fork:

- **`W3SRCE`, `W3SLN1`, `W3SBT1`, `W3SDB1`**: at the pin since `8e433f99`; the checkpoint of
  section 8 ran the Kokkos `W3SRCE` byte-identical. `kokkos/ports.tsv` says `wired` yes for them
  and for the 31 ST0 to ST6, FLX and SWL rows since 2026-10-16, each with its flag and its call
  line at `357f575c` (35 rows, v, read in the fork); `regtested` is unchanged, so the ST0 to ST6,
  FLX and SWL rows stay "compiled, never run".
- **Propagation** (`W3XYP3`, `W3KTP3`, `W3QCK1-3`): wired on `ww3-gpu/kokkos-prop3`, opt-in
  (`WW_KOKKOS_XYP3`, `WW_KOKKOS_KTP3`; [section 11](#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3)); the `W3WAVE`
  support routines are wired (section 8). The batched `W3XYP3` replaces the shared-memory
  `W3GATH`/`W3SCAT` loop, the only one ported; NOAA's operational switches are `DIST MPI`
  (`WW3-noaa/model/bin/switch_NCEP_st4`), whose branches are not.
- The fork's `model/src/CMakeLists.txt` already links `ww_kokkos` under `KOKKOS`, and a new shim
  is one more source in this repository's `fortran_iface` target. What has to move with each new
  set of entry points is the ABI version. `kokkos/CMakeLists.txt` declares `0.7.0` since
  `W3OUTG` on the resident spectrum (`ww_outg_va`, section 15; `ww3-gpu/kokkos-outg` asks for
  `0.7`), `0.6.0` came with the propagation on the resident spectrum (`ww_xyp3_batch_va`,
  `ww_ktp3_batch_va`, section 13; `ww3-gpu/kokkos-prop3dev` asks for `0.6`), and `0.5.0` came with the resident spectrum (`ww_va_*`, `ww_srce_batch_va`, section 12; `ww_xyp3_batch` and `ww_ktp3_batch` of section 11 came under 0.4.0 unbumped and are covered by 0.5.0 too; `0.4.0` added `ww_srce_batch`, section 10), with `SameMajorVersion` compatibility: an install satisfies a
  `find_package(ww_kokkos 0.x)` up to its own minor, so a fork branch that asks for `0.3` (the
  wiring base `4a94a8f6` and the units built on it) still configures against `0.4.0`, and only a
  branch that calls a new entry point asks for the new minor (`ww3-gpu/kokkos-srce-batch` asks
  for `0.4`). The rule that keeps this safe: entry points are only added; **removing or changing
  an entry point needs a major bump**, which every fork branch then has to ask for (v, 2026-10-17:
  the fork at `4a94a8f6`, `find_package(ww_kokkos 0.3)`, configured and built against the 0.4.0
  install, `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --build-only`). Before 0.4.0 the
  compatibility was `SameMinorVersion`, and `0.3.0` (`357f575c`) refused an install from before
  `ww_kokkos_flag` and `ww_wave_*`.
- The source-term path runs per sea point, so it works under `DIST MPI` as under `SHRD` (the
  regtest matrix's MPI entries ran it, `AGENTS.md`, "Bit for bit"). A propagation path is
  different: `W3GATH`/`W3SCAT` and the `PDLIB` solver exchange data between ranks, so their
  hunks keep the Fortran under those switches until a port of that branch exists: the
  `W3GATH`/`W3SCAT` wrappers compile only under `SHRD`, and `WW_KOKKOS_GATH=1` under `MPI` stops
  the model (section 7).

## How the first patch was made

> **Applied on 2026-10-14** as the h0ffmann/WW3 branch `hoff/kokkos-snl1`
> (commit `6d1bcdfe`), which the `WW3/` submodule now pins; `w3kokkosmd.F90` lives
> there, not here. Two things changed on the way: `SIG(1)` in hunk 2 became
> `SIG(1:NK)`, because `SIG` is a POINTER and gfortran rejects an element of a
> pointer array as an assumed-size actual; and hunk 4 enables C++ before
> `find_package(ww_kokkos)`, which the Kokkos package needs. `just smoke-ww3-gpu`
> builds and runs it (`scripts/ww3-gpu.sh`). The text below is the original recipe,
> kept as the account of why each hunk is there. Everything below was read off the tree at
> `WW3/model/src` (WAVEWATCH III 7.14, `develop`). The line numbers are real and
> were produced with the `grep` commands quoted next to each hunk.
>
> The Fortran shown in the hunks is a derivative of WW3's own source and carries
> WW3's licence.
>
> SPDX-License-Identifier: LGPL-3.0-or-later

The change is deliberately small and entirely opt-in. Without the `KOKKOS`
switch, nothing below is compiled; with the switch but without
`WW_KOKKOS_SNL1=1` in the environment, `KOKKOS_SNL1` is `.FALSE.` and the model
runs the Fortran `W3SNL1` exactly as before. That is what makes the port
bisectable against the model it is replacing.

## What phase 1 does not support

The shim held exactly one set of tables and one set of device buffers in a
file-static context, and nothing in it is locked. Three limits followed, and the
patch below had to respect all three: each one is a wrong answer or an
out-of-bounds read, not a slowdown. The first is lifted (per-grid contexts,
2026-10-15); the hunks below are the original recipe:

| limit | why | where the patch handles it |
|---|---|---|
| **One spectral grid per process** (lifted 2026-10-15) | `ww_snl1_init` *replaced* the tables; it did not add a grid. A `ww3_multi` run whose grids differ in `NK*NTH` would read past the end of the later grids' spectra. | hunk 2 aborted with `EXTCDE` when `IMOD > 1`. Since h0ffmann/WW3 `bb08623a` the shim keeps one context per grid: `WW_SNL1_INIT(IMOD, ...)` and `WW_SNL1(IGRID, ...)`, and the abort is gone |
| **One caller at a time** | Two threads in `ww_snl1` share the context, and one may be reallocating the device buffers while the other copies into them. | hunk 1c wraps the call in `!$OMP CRITICAL` |
| **`SIG` must start at bin 1** | the C dummy is assumed-size, so the actual argument's lower bound is lost; `W3GDATMD` allocates `SIG(0:MK+1)` | hunk 2 passes `SIG(1)` |

---

## 1. The call site: `model/src/w3srcemd.F90`

```console
$ grep -n "CALL W3SNL1\|W3_NL1" WW3/model/src/w3srcemd.F90
578:#ifdef W3_NL1
893:#if defined(W3_NL0) || defined(W3_NL1)
1258:#ifdef W3_NL1
1260:        CALL W3SNL1 ( SPEC, CG1, WNMEAN*DEPTH, VSNL, VDNL )
```

`W3SNL1` is called from one place, `SUBROUTINE W3SRCE` (line 190), section
*2.b Nonlinear interactions*, at lines **1258–1264**:

```fortran
1255       !
1256       ! 2.b Nonlinear interactions.
1257       !
1258 #ifdef W3_NL1
1259       IF (IQTPE.GT.0) THEN
1260         CALL W3SNL1 ( SPEC, CG1, WNMEAN*DEPTH, VSNL, VDNL )
1261       ELSE
1262         CALL W3SNLGQM ( SPEC, CG1, WN1, DEPTH, VSNL, VDNL )
1263       END IF
1264 #endif
```

Three facts about this call decide the shape of the patch:

* `KDMEAN` is the **expression** `WNMEAN*DEPTH`, not a variable, and the shim
  takes it as an array of `NPTS`. The patch needs a one-element local.
* `SPEC`, `VSNL` and `VDNL` are `REAL(NSPEC)` and `CG1` is `REAL(NK)`
  (declarations at lines 684–685 and 716), contiguous, column-major and **based
  at 1**: exactly the `NPTS = 1` case of the shim's batch layout, so they can be
  passed straight through with no copy and no reshape.

  `CG1` is worth a second look, because `W3ADATMD` allocates the group velocity
  as `CG(0:NK+1, 0:NSEA)` (`w3adatmd.F90:1325`), which would be the wrong base
  for an assumed-size dummy. It is safe here: `W3WAVE` already slices it at the
  call, `CALL W3SRCE ( ..., CG(1:NK,ISEA), ... )` (`w3wavemd.F90:2243`), and
  `W3SRCE`'s dummy is `CG1(NK)`. So the shim receives `CG(1:NK,ISEA)`, the `NK`
  physical bins, and nothing further is needed. The `SIG` argument of hunk 2 is
  the case where this *does* bite.
* The `IQTPE` branch must stay: `IQTPE <= 0` selects `W3SNLGQM`, a different
  routine that this port does not replace.

### Hunk 1a: the `USE` (line 578)

```diff
@@ -576,6 +576,9 @@
 #ifdef W3_NL1
     USE W3SNL1MD
     USE W3GDATMD, ONLY: IQTPE
+#ifdef W3_KOKKOS
+    USE W3KOKKOSMD, ONLY: KOKKOS_SNL1, WW_SNL1, WW_SNL1_LAST_ERROR
+    USE W3SERVMD,   ONLY: EXTCDE
+    USE W3ODATMD,   ONLY: NDSE
+#endif
 #endif
```

**Both of those `USE`s are needed, and it is easy to believe otherwise.**
`W3SRCE` does import `NDSE`, but at line 652, *inside* `#ifdef W3_NNT`:

```console
$ sed -n '650,653p' WW3/model/src/w3srcemd.F90
#ifdef W3_NNT
    USE W3SERVMD, ONLY: EXTOPN, EXTIOF
    USE W3ODATMD, ONLY: NDSE
#endif
```

Every other appearance of `NDSE` in the file (lines 1197–1417) sits inside that
same `W3_NNT` block, so on a default switch set the name is simply not declared
and `WRITE (NDSE, ...)` would not compile. `EXTCDE` is never imported here at
all: `W3SRCE` takes only `STRACE` (line 648, under `W3_S`) and `EXTOPN`/`EXTIOF`
(line 651, under `W3_NNT`) from `W3SERVMD`. Hence both lines, inside the
`W3_KOKKOS` guard so that neither is a duplicate when `W3_NNT` is on: `ONLY`
imports of the same entity from the same module are allowed to repeat.

### Hunk 1b: a one-element `KDMEAN` (near the locals at line 712)

```diff
@@ -712,6 +712,10 @@
     REAL :: SPECINIT(NSPEC), SPEC2(NSPEC)
+#ifdef W3_KOKKOS
+    ! The shim takes KDMEAN as an array of NPTS; here NPTS is 1.
+    REAL    :: KDM_K(1)
+    INTEGER :: IERR_K
+#endif
```

### Hunk 1c: the branch (lines 1258–1264)

```diff
@@ -1256,10 +1256,23 @@
       ! 2.b Nonlinear interactions.
       !
 #ifdef W3_NL1
       IF (IQTPE.GT.0) THEN
+#ifdef W3_KOKKOS
+        IF ( KOKKOS_SNL1 ) THEN
+          KDM_K(1) = WNMEAN*DEPTH
+          ! The shim has one unlocked context, and W3SRCE runs inside an
+          ! !$OMP PARALLEL region under W3_OMPG / W3_OMP0. Serialise. See
+          ! "Threading" below: this is the correctness step, not the fast one.
+          !$OMP CRITICAL (WW_KOKKOS_SNL1)
+          CALL WW_SNL1 ( 1, SPEC, CG1, KDM_K, VSNL, VDNL )
+          IERR_K = WW_SNL1_LAST_ERROR()
+          !$OMP END CRITICAL (WW_KOKKOS_SNL1)
+          IF ( IERR_K /= 0 ) THEN
+            WRITE (NDSE,'(A,I0)') '*** WAVEWATCH III ERROR IN W3SRCE : '//     &
+                 'KOKKOS W3SNL1 FAILED, CODE ', IERR_K
+            CALL EXTCDE ( 1 )
+          END IF
+        ELSE
+          CALL W3SNL1 ( SPEC, CG1, WNMEAN*DEPTH, VSNL, VDNL )
+        END IF
+#else
         CALL W3SNL1 ( SPEC, CG1, WNMEAN*DEPTH, VSNL, VDNL )
+#endif
       ELSE
         CALL W3SNLGQM ( SPEC, CG1, WN1, DEPTH, VSNL, VDNL )
       END IF
 #endif
```

The error check is not optional. `ww_snl1` is a `void` C function precisely
because a Fortran `CALL` cannot read a return value, so `WW_SNL1_LAST_ERROR()`
is the only channel a failure has; silently keeping whatever was in `VSNL`
would turn a failed launch into a plausible-looking wrong forecast.

### Threading: why the `!$OMP CRITICAL` is there

`W3SRCE` is not called from serial code. Under `W3_OMPG` it runs inside an
`!$OMP PARALLEL` region:

```console
$ sed -n '2240,2243p;2285p' WW3/model/src/w3wavemd.F90
              !$OMP PARALLEL PRIVATE (JSEA,ISEA,IX,IY,DELA,DELX,DELY,        &
              !$OMP&                  REFLEC,REFLED,D50,PSIC,TMP1,TMP2,TMP3,TMP4)
              !$OMP DO SCHEDULE (DYNAMIC,1)
              DO JSEA=1, NSEAL
                    CALL W3SRCE(srce_direct, IT, ISEA, JSEA, IX, IY, IMOD, ...
```

and under `W3_OMP0` from an `!$OMP PARALLEL DO` at `w3wavemd.F90:1551-1552`,
reaching `W3SRCE` at line 1618. The shim has **one** file-static context and no
lock: `grow()` may be reallocating the device buffers on one thread while
another `deep_copy`s into them. That is a data race, and on a GPU it is a race
over a device allocation: a segfault or silent corruption, not a rounding
difference.

The hunk therefore serialises the call with a named `!$OMP CRITICAL`. Two
reasons for that rather than "use a switch file without `OMPG`/`OMP0`":

* it keeps the patch correct on *any* switch file, including the ones the
  regtests ship with, which is what the L2 replay needs;
* `!$OMP` lines are comments when the model is built without OpenMP, so the
  hunk costs nothing when there are no threads.

The price is that the DIA becomes a serial section inside a parallel loop, so a
threaded run with `WW_KOKKOS_SNL1=1` will be *slower* than the Fortran, possibly
much slower. That is the right trade for phase 1: this patch exists to prove the
answers agree, and `WW_KOKKOS_SNL1=0` still gives the unmodified threaded
Fortran for timing comparisons. Phase 2 removes both the critical section and
the per-point call by hoisting the whole batch out of the loop.

**`NPTS = 1` is the phase-1 shape, and it is the wrong shape for a GPU.** One
sea point per launch is a kernel launch per point per timestep; the ledger in
`kokkos/PORT_STATUS.md` measures 0.75 ms for 1 000 points and 0.047 ms of that
is the kernel. Phase 2 hoists the call out of `W3SRCE` into the `JSEA` loop of
`W3WAVE` and hands the shim the whole `VA` block at once. This patch is the
correctness step, not the performance step.

---

## 2. Set-up: `model/src/w3initmd.F90`

`INSNL1` itself is called once per grid from `w3iogrmd.F90:1802`, out of
`W3IOGR('READ', ...)`, which `W3INIT` calls at line 735:

```console
$ grep -n "CALL W3IOGR" WW3/model/src/w3initmd.F90
735:    CALL W3IOGR ( 'READ', NDS(5), IMOD, FEXT )
```

That is the earliest point at which `NK`, `NTH` and `SIG` are known, so the
shim's set-up goes immediately after it, in `SUBROUTINE W3INIT` (line 163),
section *2.a Model definition* (line 733):

```diff
@@ -733,6 +733,25 @@
     ! 2.a Read model definition file
     !
     CALL W3IOGR ( 'READ', NDS(5), IMOD, FEXT )
+    !
+#ifdef W3_KOKKOS
+    ! 2.a.1 Start the Kokkos runtime and build the ported tables.
+    !
+    !       WW_KOKKOS_INIT is idempotent. WW_SNL1_INIT is not additive: it
+    !       REPLACES the tables, so phase 1 supports exactly one spectral grid
+    !       per process and a ww3_multi run has to be refused here rather than
+    !       read past the end of a later grid's spectra.
+    !
+    !       Phase 1 ignores the communicator and takes the device from
+    !       WW_KOKKOS_DEVICE_ID; pass MPI_Comm_c2f(MPI_COMM_WAVE) instead of -1
+    !       once phase 2 honours it.
+    !
+    IERR_K = WW_KOKKOS_INIT ( -1 )
+    IF ( IERR_K /= 0 ) CALL EXTCDE ( 1 )
+    CALL W3KOKKOS_SETUP
+    IF ( KOKKOS_SNL1 ) THEN
+      IF ( IMOD > 1 ) THEN
+        WRITE (NDSE,'(A,I0,A)') '*** WAVEWATCH III ERROR IN W3INIT : '//      &
+             'THE KOKKOS PORT SUPPORTS ONE GRID PER PROCESS, GOT IMOD=',      &
+             IMOD, '. RUN WITHOUT WW_KOKKOS_SNL1=1.'
+        CALL EXTCDE ( 1 )
+      END IF
+      ! SIG(1), not SIG: W3GDATMD allocates SIG(0:MK+1) (w3gdatmd.F90:2066)
+      ! and the C dummy is assumed-size, so the bare name would pass SIG(0).
+      IERR_K = WW_SNL1_INIT ( NK, NTH, XFR, DTH, LAM, SNLC1, KDCON, KDMN,     &
+                              SNLS1, SNLS2, SNLS3, FACHFE, SIG(1) )
+      IF ( IERR_K /= 0 ) CALL EXTCDE ( 1 )
+      WRITE (NDSO,'(A)') '  Kokkos DIA (W3SNL1) enabled.'
+    END IF
+#endif
```

with `USE W3KOKKOSMD` and `INTEGER :: IERR_K` added to the declarations of
`W3INIT`, and `XFR, DTH, LAM, SNLC1, KDCON, KDMN, SNLS1, SNLS2, SNLS3, FACHFE`
added to the existing `USE W3GDATMD, ONLY: ...` list. `NK`, `NTH` and `SIG` are
already on it (`w3initmd.F90:391-393`), and unlike in `W3SRCE`, `NDSO`, `NDSE`
and `EXTCDE` are all unconditionally in scope here (`w3initmd.F90:384`, `:398`). `WW_KOKKOS_FINALIZE`
belongs at the end of `W3WAVE`'s teardown; leaving it out leaks the device
buffers until process exit, which is untidy but not incorrect.

### `SIG`, not `SIG(1)`, is a silent wrong answer

This is the one argument in the whole patch that fails quietly, so it is worth
spelling out. `W3GDATMD` allocates the frequency array with a zeroth bin:

```console
$ grep -n "SIG(0:MK+1)" WW3/model/src/w3gdatmd.F90
2066:         SGRDS(IMOD)%SIG(0:MK+1),                             &
```

and `W3SETG` points the module-level `SIG` at it (`w3gdatmd.F90:2529`), lower
bound and all. The C interface declares `REAL(C_FLOAT), INTENT(IN) :: SIG(*)`,
an assumed-size dummy, which carries **no** lower bound: what crosses the
boundary is the address of the first element of the actual argument. Pass the
bare name `SIG` and the shim reads `SIG(0:NK-1)`: every quadruplet built one
frequency bin low, every `AF11` scaled by the wrong `f**11`, and a source term
that is smooth, plausible and wrong. `SIG(1)` is WW3's own sequence-association
idiom for exactly this and costs nothing.

The lab's standalone reference could not catch this: it allocated `SIG(NK)`, so
its `shim_roundtrip` test passed either way (both were removed on 2026-10-14 under
the owner's rule that the port has no tests of its own). The check belongs on the
fork branch, in the `WW_KOKKOS_SNL1=0` vs `=1` comparison of section 5, which is a
WW3 regtest replay.

### Multi-grid

`ww3_multi` calls `W3INIT` once per grid. The guard above turns that into a
clean abort with a message rather than corruption, which is the honest phase-1
behaviour: the shim keeps one `Config`, one `Tables` and one set of device
buffers, and a second `WW_SNL1_INIT` releases the first grid's. Supporting
several grids means giving the shim a handle per grid: a real API change, and
deliberately out of scope here.

---

## 3. The switch: `model/src/cmake/switches.json`

`check_switches.cmake` turns each selected switch into `-DW3_<NAME>` and adds
its `build_files` to the library, so one new category is the whole build change:

```diff
@@ (append to the array in switches.json)
+  {
+    "name": "kokkos",
+    "num_switches": "upto1",
+    "description": "C++/Kokkos port of the source terms",
+    "valid-options": [
+      {
+        "name": "KOKKOS",
+        "build_files": ["w3kokkosmd.F90"],
+        "requires": ["NL1"]
+      }
+    ]
+  }
```

**`model/src/cmake/src_list.cmake` needs no change**, and the brief's
instruction to edit it is one step more than the tree actually requires:
`check_switches.cmake:78-85` collects `build_files` into `switch_files`, and
`model/src/CMakeLists.txt:19` already passes that variable to
`add_library(ww3_lib STATIC ${ftn_src} ${switch_files})`. Adding
`w3kokkosmd.F90` to `ftn_src` as well would compile it unconditionally and
break every build without the Kokkos library. Listing it once, in
`switches.json`, is both the smaller patch and the correct one.

`w3kokkosmd.F90` is copied from
`kokkos/src/fortran_iface/w3kokkosmd.F90` into `model/src/` unchanged. It has
no dependency on anything else in this lab, which is why it is built as its own
target here (`ww_kokkos_f`) rather than folded into the C++ library.

---

## 4. The link: `model/src/CMakeLists.txt`

```diff
@@ -19,6 +19,17 @@
 add_library(ww3_lib STATIC ${ftn_src} ${switch_files})
+
+# The Kokkos port. Built separately (see kokkos/README.md) and found here; the
+# switch and the library have to agree, so mismatching them is a hard error
+# rather than a link failure a thousand lines later.
+option(WW_KOKKOS "Link the C++/Kokkos port of the source terms" OFF)
+if(WW_KOKKOS)
+  if(NOT "KOKKOS" IN_LIST switches)
+    message(FATAL_ERROR "-DWW_KOKKOS=ON needs the KOKKOS switch in the switch file")
+  endif()
+  find_package(ww_kokkos REQUIRED)
+  target_link_libraries(ww3_lib PUBLIC ww_kokkos::ww_kokkos)
+endif()
```

`ww3_lib` is a Fortran target linking a C++ static library, so the link line
also needs the C++ runtime. CMake handles this by itself when the imported
target carries `IMPORTED_LINK_INTERFACE_LANGUAGES CXX`; if the Kokkos tree is
pulled in with `add_subdirectory()` instead of `find_package()`, CMake gets it
from the target's own linker language and nothing extra is needed. This is the
same mechanism that let `kokkos/tests/fixtures/shim_driver` (a Fortran
program linking `ww_kokkos_f`, removed on 2026-10-14) link here.

`ww_kokkos` does **not** export a CMake package config yet; that is the one
piece of this section that is speculative, and the fork branch will either add
an `install(EXPORT)` to `kokkos/CMakeLists.txt` or use `add_subdirectory`.

---

## 5. Applying and checking it

```bash
# on the fork branch, with WW3 as a submodule or a sibling checkout
cd WW3
git switch -c feat/kokkos-snl1
#   ... apply hunks 1-4 ...
echo "... NL1 ... KOKKOS ..." > ../switch          # add KOKKOS to the switch file
cmake -B build -DWW_KOKKOS=ON -Dww_kokkos_DIR=../kokkos/build/openmp-release
cmake --build build

# bisect against the Fortran: same binary, switch flipped by the environment
WW_KOKKOS_SNL1=0 ./build/bin/ww3_shel     # Fortran W3SNL1
WW_KOKKOS_SNL1=1 ./build/bin/ww3_shel     # Kokkos DIA
# then: kokkos/tools/nccmp-tol on the two out_grd.nc
```

That last pair is the point of the whole design: one executable, one switch,
two answers that must agree within `kokkos/tools/nccmp-tol/tolerances.txt`.
It is the L2 row of `kokkos/PORT_STATUS.md`, the port's only test since
ADR-0010 (2026-10-14), and it is what this patch is for.

## 6. The source-term units on `ww3-gpu/kokkos-st-flx-swl`

Seven commits on `ww3-gpu/kokkos-srce` (`e062ff0b`), one per unit: ST0 `ec644d4f`, ST1
`b1fdb745`, ST2 `bc55eaeb`, ST3 `888adace`, FLX `bee8c001`, SWL `050d79d4`, ST6 `8e433f99`.
Each unit follows ST4: a `bind(C)` interface block and a `W3KOKKOS_<UNIT>_SETUP (IMOD)` in
`w3kokkosmd.F90` under the unit's physics switch, a call to it in `W3INIT` after `W3IOGR`, and in
`W3SRCE` every call of the routine becomes `IF (KOKKOS_X) THEN CALL W3KOKKOS_X (...) ELSE CALL
W3X (...) END IF` under `W3_KOKKOS`, the original line in the `#else`. Each `W3KOKKOS_X` has
the Fortran routine's dummy list, calls the C entry point with `NPTS = 1` inside
`!$OMP CRITICAL (WW_KOKKOS_<UNIT>)`, and stops with `EXTCDE` on a shim failure. Every flag
defaults to off, so the Fortran runs; without `KOKKOS`, `gfortran -E` of `w3srcemd.F90` and
`w3initmd.F90` equals the parent's except for the `__FILE__` text and blank lines (v, the ST0,
ST1, ST2, ST3, ST6+FLX4, ST6+FLX5 and `switch_lab_shrd` switches, 2026-10-16). Compiled, not run:
no regtest has exercised any of them ⚠.

| Unit | Switch | Flags (`=1` turns the routine on) | Count at exit | W3SRCE calls hooked |
|---|---|---|---|---|
| ST0 | `ST0` | `WW_KOKKOS_SPR0` | `WW_KOKKOS_ST0_COUNT` | `W3SPR0` x2 |
| ST1 | `ST1` | `WW_KOKKOS_SPR1`, `SIN1`, `SDS1` | `WW_KOKKOS_ST1_COUNT` | `W3SPR1` x2, `W3SIN1`, `W3SDS1` |
| ST2 | `ST2` | `WW_KOKKOS_SPR2`, `SIN2`, `SDS2` | `WW_KOKKOS_ST2_COUNT` | `W3SPR2` x2, `W3SIN2`, `W3SDS2` |
| ST3 | `ST3` (`STAB3` read at set-up) | `WW_KOKKOS_SPR3`, `SIN3`, `SDS3` | `WW_KOKKOS_ST3_COUNT` | `W3SPR3` x4, `W3SIN3` x3, `W3SDS3` |
| FLX | `FLX1` .. `FLX5` | `WW_KOKKOS_FLX1` .. `FLX5`; `WW_KOKKOS_FLX_TABLES` | `WW_KOKKOS_FLX_COUNT` | `W3FLX2`, `W3FLX3` x2; `W3FLX1`, `W3FLX4`, `W3FLX5` x1 |
| SWL | `ST6` | `WW_KOKKOS_SWL6` | `WW_KOKKOS_SWL_COUNT` | `W3SWL6` |
| ST6 | `ST6` | `WW_KOKKOS_SPR6`, `SIN6`, `SDS6` | `WW_KOKKOS_ST6_COUNT` | `W3SPR6` x2, `W3SIN6`, `W3SDS6` |

Not hooked: the same routines called by `ww3_outp`, `ww3_ounp` and `gx_outp`, which never run
`W3INIT`'s Kokkos set-up, and the `W3FLX5` call inside `W3SPR4` (ST4 with FLX5), which
`w3src4md.F90` cannot reach without a module cycle (`W3KOKKOSMD` already uses `W3SRC4MD`).
Module tables stay one per process as in WW3, built from the first grid: ST2's `BETATB` and ST3's
`TAUT`/`TAUHFT` by the shim's first commit (W3IOGR's `SAVE` flag `FLINP`), `W3SWL6`'s and ST6's
`IKN` and ST6's 10 Hz tables by the first call. ST6's `DSII10Hz` is also rewritten after every
`ww_sin6` where `TAU_WAVE_ATMOS` rewrites it (`w3src6md.F90:1294-1297`), exact for `NPTS = 1`.

The argument tables below give, per C entry point, the Fortran dummy, the `W3SRCE` actual, the C
type, the intent and the shape per point. Every array is `REAL` (`float`), contiguous and based at
1 in the dummy; `A` is `W3SRCE`'s `SPEC(NSPEC)`, the action density spectrum. The meanings are
the routines' `@param` text at the pin; units are WW3's SI conventions and were not read from
each header ⚠ (`W3SDS2` states rad/s for `FPI` and m/s for `USTAR`). `igrid` is `W3GDATMD`
`IGRID` and `npts` is 1 in every call.

### `ww_spr0`, `ww_spr1` (`W3SPR0`, `W3SPR1`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg` | `CG` ← `CG1` | `const float*` | in | NK | group velocity (m/s) |
| `wn` | `WN` ← `WN1` | `const float*` | in | NK | wavenumber (rad/m) |
| `emean` | `EMEAN` ← `EMEAN` | `float*` | out | 1 | mean wave energy |
| `fmean` | `FMEAN` ← `FMEAN` | `float*` | out | 1 | mean frequency (Hz) |
| `wnmean` | `WNMEAN` ← `WNMEAN` | `float*` | out | 1 | mean wavenumber (rad/m) |
| `amax` | `AMAX` ← `AMAX` | `float*` | out | 1 | maximum action density |

### `ww_sin1` (`W3SIN1`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `k` | `K` ← `WN2` | `const float*` | in | NSPEC | wavenumber for the whole spectrum |
| `ustar` | `USTAR` ← `USTAR` | `const float*` | in | 1 | friction velocity (m/s) |
| `usdir` | `USDIR` ← `U10DIR` | `const float*` | in | 1 | direction of `USTAR` (rad) |
| `s`, `d` | `S` ← `VSIN`, `D` ← `VDIN` | `float*` | out | NSPEC | source term, its diagonal derivative |

### `ww_sds1` (`W3SDS1`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `k` | `K` ← `WN2` | `const float*` | in | NSPEC | wavenumber for the whole spectrum |
| `emean`, `fmean`, `wnmean` | same names | `const float*` | in | 1 each | mean energy, frequency, wavenumber |
| `s`, `d` | `S` ← `VSDS`, `D` ← `VDDS` | `float*` | out | NSPEC | source term, its diagonal derivative |

### `ww_spr2` (`W3SPR2`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a`, `cg`, `wn` | `A`, `CG`, `WN` ← `SPEC`, `CG1`, `WN1` | `const float*` | in | NSPEC, NK, NK | as `ww_spr0` |
| `depth` | `DEPTH` ← `DEPTH` | `const float*` | in | 1 | water depth (m) |
| `fpi` | `FPI` ← `FPI` | `const float*` | in | 1 | peak input frequency (rad/s) |
| `u` | `U` ← `U10ABS` | `const float*` | in | 1 | wind speed (m/s) |
| `ustar` | `USTAR` ← `USTAR` | `const float*` | in | 1 | friction velocity (m/s) |
| `emean`, `fmean`, `wnmean`, `amax` | same names | `float*` | out | 1 each | as `ww_spr0` |
| `alfa` | `ALFA` ← `ALPHA` | `float*` | out | NK | Phillips' constant |
| `fp` | `FP` ← `FP` | `float*` | out | 1 | peak frequency |

### `ww_sin2` (`W3SIN2`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg` | `CG` ← `CG1` | `const float*` | in | NK | group velocity (m/s) |
| `k` | `K` ← `WN2` | `const float*` | in | NSPEC | wavenumber for the whole spectrum |
| `u`, `udir` | `U`, `UDIR` ← `U10ABS`, `U10DIR` | `const float*` | in | 1 each | wind speed (m/s) and direction (rad) |
| `cd`, `z0` | `CD`, `Z0` ← `CD`, `Z0` | `const float*` | in | 1 each | drag coefficient at `ZWIND`, roughness length (m) |
| `fpi` | `FPI` ← `FPI` | `float*` | out | 1 | input peak frequency (rad/s) |
| `s`, `d` | `S` ← `VSIN`, `D` ← `VDIN` | `float*` | out | NSPEC | source term, its diagonal derivative |

### `ww_sds2` (`W3SDS2`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg`, `k` | `CG`, `K` ← `CG1`, `WN1` | `const float*` | in | NK each | group velocity, wavenumber |
| `fpi` | `FPI` ← `FPI` | `const float*` | in | 1 | peak frequency of the input (rad/s) |
| `ustar` | `USTAR` ← `USTAR` | `const float*` | in | 1 | friction velocity (m/s) |
| `alfa` | `ALFA` ← `ALPHA` | `const float*` | in | NK | Phillips' constant |
| `s`, `d` | `S` ← `VSDS`, `D` ← `VDDS` | `float*` | out | NSPEC | source term, its diagonal derivative |

### `ww_spr3` (`W3SPR3`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a`, `cg`, `wn` | `A`, `CG`, `WN` ← `SPEC`, `CG1`, `WN1` | `const float*` | in | NSPEC, NK, NK | as `ww_spr0` |
| `emean`, `fmean`, `fmeans`, `wnmean`, `amax` | same names | `float*` | out | 1 each | energy; mean frequency for the tail (`FMEAN`) and for the dissipation term (`FMEANS`); mean wavenumber; maximum action density |
| `u`, `udir` | `U`, `UDIR` ← `U10ABS`, `U10DIR` | `const float*` | in | 1 each | wind speed (m/s), direction (rad) |
| `ustar`, `usdir` | `USTAR`, `USDIR` ← `USTAR`, `USTDIR` | `float*` | inout | 1 each | friction velocity (m/s), wind stress direction (rad) |
| `tauwx`, `tauwy` | `TAUWX`, `TAUWY` ← same | `const float*` | in | 1 each | wave-supported stress components |
| `cd`, `z0`, `charn` | same names | `float*` | out | 1 each | drag coefficient, roughness length (m), Charnock coefficient |
| `llws` | `LLWS` ← `LLWS` | `const int*` | in | NSPEC | wind-sea mask, `LOGICAL` as 0/1 |
| `fmeanws` | `FMEANWS` ← `FMEANWS` | `float*` | out | 1 | mean frequency of the wind sea |

### `ww_sin3` (`W3SIN3`; `CD`, `ICE`, `IX`, `IY` are not read and not passed)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg` | `CG` ← `CG1` | `const float*` | in | NK | group velocity (m/s) |
| `k` | `K` ← `WN2` | `const float*` | in | NSPEC | wavenumber for the whole spectrum |
| `u`, `ustar`, `drat`, `as`, `usdir`, `z0` | `U` ← `U10ABS`, `USTAR`, `DRAT`, `AS`, `USDIR` ← `U10DIR`, `Z0` | `const float*` | in | 1 each | wind speed, friction velocity, air/water density ratio, air-sea temperature difference, wind stress direction, roughness length |
| `tauwx`, `tauwy`, `tauwnx`, `tauwny` | `TAUWX`, `TAUWY`, `TAUWNX` ← `TAUWAX`, `TAUWNY` ← `TAUWAY` | `float*` | out | 1 each | wave-supported stress, and its negative part |
| `s`, `d` | `S` ← `VSIN`, `D` ← `VDIN` | `float*` | out | NSPEC | source term, its diagonal derivative |
| `llws` | `LLWS` ← `LLWS` | `int*` | out | NSPEC | wind-sea mask, 0/1 |

### `ww_sds3` (`W3SDS3`; `CG`, `USTAR`, `USDIR`, `DEPTH`, `IX`, `IY` are not read and not passed)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `k` | `K` ← `WN1` | `const float*` | in | NK | wavenumber (rad/m) |
| `emean`, `fmean`, `wnmean` | `EMEAN`, `FMEAN` ← `FMEANS`, `WNMEAN` | `const float*` | in | 1 each | mean energy, frequency, wavenumber |
| `s`, `d` | `S` ← `VSDS`, `D` ← `VDDS` | `float*` | out | NSPEC | source term, its diagonal derivative |

### `ww_flx1`, `ww_flx4` (`W3FLX1`, `W3FLX4`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `zwnd` | `ZWND` ← `ZWND` | `const float*` | in | 1 | wind height (m); not 10 m stops the run |
| `u10`, `u10d` | `U10`, `U10D` ← `U10ABS`, `U10DIR` | `const float*` | in | 1 each | wind speed (m/s), direction (rad) |
| `ust`, `ustd`, `z0`, `cd` | `UST` ← `USTAR`, `USTD` ← `USTDIR`, `Z0`, `CD` | `float*` | inout (out in the Fortran) | 1 each | friction velocity, its direction, roughness length, drag coefficient |
| `iext` | none | `int*` | out | 1 | 1 where the Fortran calls `EXTCDE(1)`; the wrapper writes the routine's FORMAT 1000 and stops |

### `ww_flx2`, `ww_flx3` (`W3FLX2`, `W3FLX3`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `zwind`, `depth`, `fp` | `ZWIND` ← `ZWND`, `DEPTH`, `FP` | `const float*` | in | 1 each | wind height (m), depth (m), peak frequency |
| `u`, `udir` | `U`, `UDIR` ← `U10ABS`, `U10DIR` | `const float*` | in | 1 each | wind speed (m/s), direction (rad) |
| `ust`, `ustd`, `z0`, `cd` | `UST` ← `USTAR`, `USTD` ← `USTDIR`, `Z0`, `CD` | `float*` | inout | 1 each | as `ww_flx1`; `UST` starts the iteration, `Z0`, `CD` stay as passed when `NITTIN <= 0` |

### `ww_flx5` (`W3FLX5`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `zwnd`, `u10`, `u10d` | as `ww_flx1` | `const float*` | in | 1 each | as `ww_flx1` |
| `taua`, `tauadir`, `rhoair` | `TAUA`, `TAUADIR`, `RHOAIR` ← `TAUA`, `TAUADIR`, `DAIR` | `const float*` | in | 1 each | atmospheric stress, its direction, air density (kg/m3) |
| `ust`, `ustd`, `z0`, `cd`, `charn` | `UST` ← `USTAR`, `USTD` ← `USTDIR`, `Z0`, `CD`, `CHARN` | `float*` | inout | 1 each | as `ww_flx1`, and the Charnock coefficient |
| `iext` | none | `int*` | out | 1 | as `ww_flx1` |

### `ww_swl6` (`W3SWL6`), `ww_sds6` (`W3SDS6`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg`, `wn` | `CG`, `WN` ← `CG1`, `WN1` | `const float*` | in | NK each | group velocity, wavenumber |
| `s`, `d` | `S` ← `VSWL`/`VSDS`, `D` ← `VDWL`/`VDDS` | `float*` | out | NSPEC | source term, its diagonal derivative |
| `nikn` / `info` | none | `int*` | out | 1 / 2 | the integers of the FORMAT the wrapper writes before `EXTCDE(2)` (SIZE(IKN) /= NK) |
| return | none | `int` | | | 0, or the code of the Fortran's `EXTCDE` |

### `ww_spr6` (`W3SPR6`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a`, `cg`, `wn` | `A`, `CG`, `WN` ← `SPEC`, `CG1`, `WN1` | `const float*` | in | NSPEC, NK, NK | as `ww_spr0` |
| `emean`, `fmean`, `wnmean`, `amax`, `fp` | same names | `float*` | out | 1 each | as `ww_spr0`, and the peak frequency |

### `ww_sin6` (`W3SIN6`, with `LFACTOR` and `TAU_WAVE_ATMOS`)

| C | Dummy ← actual | Type | Intent | Shape | Meaning |
|---|---|---|---|---|---|
| `a` | `A` ← `SPEC` | `const float*` | in | NSPEC | action density spectrum |
| `cg` | `CG` ← `CG1` | `const float*` | in | NK | group velocity (m/s) |
| `wn2` | `WN2` ← `WN2` | `const float*` | in | NSPEC | wavenumber for the whole spectrum |
| `uabs`, `ustar`, `usdir`, `cd`, `dair` | `UABS` ← `U10ABS`, `USTAR`, `USDIR` ← `USTDIR`, `CD`, `DAIR` | `const float*` | in | 1 each | wind speed, friction velocity, its direction, drag coefficient, air density |
| `tauwx`, `tauwy`, `taunwx`, `taunwy`, `charn` | `TAUWX`, `TAUWY`, `TAUNWX` ← `TAUWAX`, `TAUNWY` ← `TAUWAY`, `CHARN` | `float*` | out | 1 each | wave-supported stress, its negative part, Charnock coefficient |
| `s`, `d` | `S` ← `VSIN`, `D` ← `VDIN` | `float*` | out | NSPEC | source term, its diagonal derivative |
| `info` | none | `int*` | out | 2 | the integers of the stopping FORMAT, or the first `ITERMAX` point |
| return | none | `int` | | | 0, a `WwSt6Code` the wrapper turns into the Fortran's FORMAT and `EXTCDE`, or `LF_ITERMAX`: that point is computed again by the Fortran `W3SIN6`, which writes the warning |

## 7. Run-time flags: one per part, and a master

Every part of WW3 that the fork can run on Kokkos has its own run-time flag `WW_KOKKOS_<PART>`,
and every flag defaults to the Fortran. Since `ww3-gpu/kokkos-wave-support` (`357f575c`) one
function resolves all of them, `ww_kokkos_flag` in
[`part_flags.cpp`](part_flags.cpp): every shim's `ww_*_enabled()` and `W3KOKKOS_SETUP` in
`w3kokkosmd.F90` ask it, so the Fortran `LOGICAL`s and the shims cannot disagree (v, read in
the code 2026-10-16).

| `WW_KOKKOS_<PART>` | `WW_KOKKOS_ALL` | The part runs on |
|---|---|---|
| `1` | any | Kokkos (explicit) |
| set, not `1` (`0`) | any | Fortran (explicit) |
| unset | `1` | Kokkos, where its translation covers the switch file; the Fortran otherwise |
| unset | unset, or not `1` | Fortran (the default since 2026-10-14) |

So `WW_KOKKOS_ALL=1` is "100 % Kokkos" and `WW_KOKKOS_ALL=0` "0 % Kokkos" in one place, and a
per-part flag set explicitly wins over the master. **100 % means 100 % of the wired parts, not
of WW3.** At the `WW3/` pin of this change, `kokkos/PORT_STATUS.md` (`port_score.py --write`,
2026-10-16) gives the wired rows ("ported" level) **94.0 %** of the run time of the
committed profile (`bench_small`, 1 thread, `WW3@6d1bcdfe`, 2026-10-14) (v); the rest of WW3,
the propagation (`W3XYP3`, `W3KTP3`, `W3QCK*`), the I/O and the set-up, stays Fortran whatever
the master says. Two rules keep the master safe on any switch file: a part whose translation does
not cover the switch file stays Fortran under `WW_KOKKOS_ALL=1` and stops the model under
`WW_KOKKOS_<PART>=1` (`KOKKOS_PART` in `w3kokkosmd.F90`); `W3SRCE` does the same outside
`ST4 NL1` and the lab switch (`W3INIT`, `W3KOKKOS_SRCE_SETUP`). The `*_COUNT` and `*_TABLES`
rows differ: `*_COUNT` are diagnostics and ignore the master; `FLX_TABLES` follows it, and
`ST4_TABLES` is opt-in like the parts of `kOptInOnly` (the owner, 2026-10-17): under
`WW_KOKKOS_ALL=1` the ST4 shim reads the tables the Fortran built instead of rebuilding them,
which cost about 8 s per run (coast, commit 4fbffbe) and gives the same values; with the
three `TABU_*` tables on a host `parallel_for` the rebuild is 0.44 s in the smoke and 0.90 s in
the coast case (`kokkos/port_report/TABU_TAUHF2_KOKKOS.md`), and it stays opt-in.

`W3INIT` writes one line to `log.ww3` after every set-up, the parts this build can run and where
each runs (v, smoke 2026-10-16):

```text
  Kokkos parts (K Kokkos, F Fortran): SNL1=K SDS4=K SIN4=K SPR4=K ST4_TABLES=K SRCE=K GATH=K SCAT=K UWND=K UTRN=K DZXY=K MAP3=K MAPT=K DISTAB=K (14 of 14 on Kokkos)
```

Since `ST4_TABLES` became opt-in, `--kokkos all` prints `ST4_TABLES=F` there (13 of 14) ⚠ (from
`part_flags.cpp` and `KOKKOS_PART`; not run); `--kokkos all,ST4_TABLES` is not a form the script
takes, so the rebuild is asked for with `--kokkos <parts>,ST4_TABLES` or
`WW_KOKKOS_ST4_TABLES=1`.

`scripts/ww3-gpu.sh --ww3-gpu <case> --kokkos all|none|<PART,...>` runs the Kokkos case with
`WW_KOKKOS_ALL=1`, `=0`, or `=0` plus the listed parts, unsets every `WW_KOKKOS_<PART>` in both
runs, gives the Fortran run `WW_KOKKOS_ALL=0`, sets every counter in the Kokkos run and prints
the parts line and the counters. Without `--kokkos` it keeps `WW3GPU_ROUTINES` as before, now
with `WW_KOKKOS_ALL=0` in both runs.

`--kokkos gpu` is `SRCE` alone, the part list for Kokkos' CUDA backend and the default of `--gpu`
when neither `--kokkos` nor `WW3GPU_ROUTINES` is given: phase 1 copies one sea point per call,
and `W3SRCE` routed whole makes one round trip per point and time step where the DIA and ST4
routed one by one make one each ([ADR-0013](../../../docs/ADRs/ADR-0013-operational-parts-default-to-kokkos.md)).
WW3's own defaults are unchanged: every flag unset is the Fortran.

The parity column is a judgement (⚠) from the port report's evidence unless it names a
measurement: what decides bit parity is the compile flags (`-O3`, `-ffp-contract=off` /
`--fmad=false`), `REAL(4)` kept as `float`, the reduction order, the libm calls (glibc on the host;
CUDA's libdevice on a GPU, whose bits against glibc are not known), the `MIN`/`MAX` operand order
for NaN and `-0.0`, the `!$OMP CRITICAL` around one caller, the per-call copies (bitwise), and the
shim's strides and lower bounds. "smoke" is `scripts/ww3-gpu.sh --ww3-gpu smoke --cpu` on
2026-10-16 (`ww3_ts1`, one point, 10 days, Kokkos 5.2.2 OpenMP, gfortran/g++ 16.2.0, the pinned
`ww3` shell, an Intel Core i9-14900 with 32 threads); "full" is the same on `bench_small` (120 x 80, 9 520 sea points, 12 h).
"byte-identical" means `cmp` found `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` and the
`ww3_ounf` NetCDF file equal to the Fortran run of the same executable, and the first three equal
to `--ww3` (pure NOAA WW3 at `WW3-noaa/`); the NetCDF file against `--ww3` differs only in its
`WAVEWATCH_III_switches` attribute, which names `KOKKOS` (`ncdump` diff, v).

| Part | Flag | Switch it needs | Replaces | Counter (at exit) | Parity |
|---|---|---|---|---|---|
| SNL1 | `WW_KOKKOS_SNL1` | `NL1` | `W3SNL1` in `W3SRCE` | `WW_KOKKOS_SNL1_COUNT` | measured, CPU: byte-identical, smoke, full and the 100-entry matrix 2026-10-15 (v, `port_report/W3SNL1_KOKKOS.md`), and smoke 2026-10-16 with ST4 on (2 339 spectra) (v); GPU ⚠, never run in WW3 |
| SDS4, SIN4, SPR4 | `WW_KOKKOS_SDS4`, `_SIN4`, `_SPR4` | `ST4` | the three in `W3SRCE` | `WW_KOKKOS_ST4_COUNT` | measured, CPU: byte-identical in the smoke 2026-10-16 with `SNL1,SDS4,SIN4,SPR4,ST4_TABLES` (2 339, 6 838, 6 660 spectra) (v) and the ST4 checkpoint of 2026-10-15 (v); GPU ⚠: `expf`, `logf`, `tanhf`, `powf`, `atan2f` and the reproduced libmvec loops become libdevice |
| ST4_TABLES | `WW_KOKKOS_ST4_TABLES` | `ST4` | the ST4 tables read from `mod_def.ww3` and `ST4TABUHF2.bin`, rebuilt in C++ | (prints the count of differing values) | measured: "0 values differ from the Fortran's" in the smoke 2026-10-16 (v), and 2026-10-17 with the three `TABU_*` on a host `parallel_for` over their independent table elements in smoke, full and coast, `switch_lab_ompg` and `switch_lab_shrd` (v); the rebuild is the timer region `st4.tables.call`, 7.57 s serial and 0.44 s parallel in the smoke (32 threads) |
| SRCE | `WW_KOKKOS_SRCE` | `ST4 NL1` and exactly the lab switch's source terms | `W3SRCE(srce_direct)` in `W3WAVE`, with ST4, `W3SNL1`, `W3SLN1`, `W3SBT1`, `W3SDB1` inside | `WW_KOKKOS_SRCE_COUNT` | measured, CPU: byte-identical in the smoke 2026-10-16 with `WW_KOKKOS_ALL=1` (2 161 spectra, all of them; the per-routine ST4 and DIA counters were 0 there) (v), and in the full  (230 100 spectra, v); GPU ⚠ (`logf`, `tanhf`, `sincosf`) |
| SPR0 | `WW_KOKKOS_SPR0` | `ST0` | `W3SPR0` | `WW_KOKKOS_ST0_COUNT` | judgement ⚠: likely on any backend, `sqrtf` only (IEEE), sums kept in the Fortran's order (`port_report/W3SPR0_KOKKOS.md`); never run |
| SPR1, SIN1, SDS1 | `WW_KOKKOS_SPR1`, `_SIN1`, `_SDS1` | `ST1` | the three in `W3SRCE` | `WW_KOKKOS_ST1_COUNT` | judgement ⚠: CPU likely (`sincosf`, libmvec loops reproduced); GPU unlikely (libdevice `sincosf`); never run |
| SPR2, SIN2, SDS2 | `WW_KOKKOS_SPR2`, `_SIN2`, `_SDS2` | `ST2` | the three in `W3SRCE` | `WW_KOKKOS_ST2_COUNT` | judgement ⚠: CPU likely (`logf`, `sincosf`, `powf`, the x87 `fprem` of `MOD`, libmvec); GPU unlikely; never run |
| SPR3, SIN3, SDS3 | `WW_KOKKOS_SPR3`, `_SIN3`, `_SDS3` | `ST3` | the three in `W3SRCE` | `WW_KOKKOS_ST3_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `powf`, `sincosf`, libmvec); GPU unlikely; never run |
| SPR6, SIN6, SDS6 | `WW_KOKKOS_SPR6`, `_SIN6`, `_SDS6` | `ST6` | the three in `W3SRCE` | `WW_KOKKOS_ST6_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `tanhf`, `powf`, `__powisf2` kept, libmvec); GPU unlikely; never run |
| SWL6 | `WW_KOKKOS_SWL6` | `ST6` | `W3SWL6` | `WW_KOKKOS_SWL_COUNT` | judgement ⚠: likely on CPU (`sqrtf`, one libmvec loop reproduced); never run |
| FLX1 .. FLX5 | `WW_KOKKOS_FLX1` .. `_FLX5` | the matching `FLXn` | `W3FLXn` in `W3SRCE` | `WW_KOKKOS_FLX_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `powf`, `tanhf`, `__powisf2` kept, libmvec); GPU unlikely; never run |
| FLX_TABLES | `WW_KOKKOS_FLX_TABLES` | `FLX2` or `FLX3` | `W3DISPMD`'s `EWN1`, `DSIE`, `N1MAX` as the FLX shim reads them, rebuilt in C++ | (prints the count of differing values) | judgement ⚠: the same `distab` as DISTAB below, so as DISTAB; never run |
| GATH, SCAT | `WW_KOKKOS_GATH`, `_SCAT` | `SHRD` | `W3GATH`, `W3SCAT` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16, alone with the other support parts and under `WW_KOKKOS_ALL=1` (13 824 calls each) (v) and in the coast case 2026-10-17 under `WW_KOKKOS_ALL=1` (41 472 each) (v); 0 calls in the smoke (one point, no spatial propagation). Pure copies (`movss`, no arithmetic, `port_report/W3GATH_KOKKOS.md`), so any backend gives the bits; GPU ⚠ never run. Only the `SHRD` branch exists |
| UWND | `WW_KOKKOS_UWND` | `WNT1` (with or without `RWND`), none of `WNT0 WNT2 OASACM MGW SMC WCOR STAB2` | `W3UWND` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the smoke 2026-10-16, alone with the other support parts (2 161 calls) and under `WW_KOKKOS_ALL=1` (v) and in the full (25 calls) (v); GPU ⚠ (`atan2f`, `fmodf`, `sincosf` from libdevice, `port_report/W3UWND_KOKKOS.md`) |
| UTRN | `WW_KOKKOS_UTRN` | `IC0` | `W3UTRN` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | judgement ⚠: likely on CPU (`expf`, `minss`/`maxss` operand order read from the object); the smoke and the full ran only `TRFLAG = 0` (`ATRNX = ATRNY = 1`), byte-identical (v); GPU ⚠ (`expf`); the `NX` or `NY` = 1 edge reads past the array go past the shim's copy instead of WW3's memory ⚠ |
| DZXY | `WW_KOKKOS_DZXY` | any (the `REFRX` call keeps the Fortran) | `W3DZXY` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16 (1 call, the depth gradient; the case has no current) (v) and in the coast case 2026-10-17 under `WW_KOKKOS_ALL=1`, both lab switches (159 calls, the depth and current gradients) (v); 0 calls in the smoke. No libm call; three products commuted in the assembly change only which NaN payload survives when two NaN meet (`port_report/W3DZXY_KOKKOS.md`) ⚠; GPU ⚠ never run, needs `--fmad=false` |
| MAP3, MAPT | `WW_KOKKOS_MAP3`, `_MAPT` | `PR3` (`MAP3` only where `GTYPE < 3`) | `W3MAP3`, `W3MAPT` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16 (1 call each on the 120 x 80 grid; the smoke grid is one cell) (v), and in the coast case 2026-10-17 under `WW_KOKKOS_ALL=1`, both lab switches (13 calls each, the maps rebuilt after the hourly ice and level updates) (v). `W3MAP3` is integer only, `W3MAPT` three `minss` and a compare with their operand order kept (port reports); GPU ⚠ never run. `W3MAP3`'s out-of-bounds read at `IX = 2, IY = 1` or `IX = NX-1, IY = NY` would read past the shim's copy, not WW3's memory ⚠ |
| DISTAB | `WW_KOKKOS_DISTAB` | any | `DISTAB` (with `WAVNU2`), run by `W3IOGR('READ')`: the Kokkos tables overwrite the Fortran's in `W3INIT` | `WW_KOKKOS_WAVE_COUNT`, and the count of differing values | measured, CPU: "0 of EWN1, ECG1(0:121), DSIE, N1MAX differ from the Fortran's" and byte-identical output in the smoke 2026-10-16 (v); GPU ⚠ (`tanhf`, `coshf`, `sinhf` from libdevice, `port_report/DISTAB_KOKKOS.md`) |
| XYP3, KTP3 | `WW_KOKKOS_XYP3`, `_KTP3`, opt-in only (`WW_KOKKOS_ALL` does not set them) | `PR3` `UQ`, a regular or curvilinear grid, no `PDLIB` `MGP` `MGG`; `XYP3` also `SHRD` and not tripole, `KTP3` no `REFRX` | the `DO ISPEC` loop of `W3GATH`, `W3XYP3`, `W3SCAT` and the two `DO ITLOC` loops of `W3KTP3` in `W3WAVE`, one call each (section 11) | `WW_KOKKOS_PROP3_COUNT` | measured, CPU: byte-identical in the full 2026-10-17 with `switch_lab_ompg` and `switch_lab_shrd` (13 824 spectra, 441 792 point calls) (v); 0 calls in the smoke; `W3QCK2`, `FLCUR` and boundary input byte-identical in the coast case 2026-10-17 with `--kokkos resident`, both switches, with and without `WW_KOKKOS_VA_RESIDENT=1` (41 472 spectra, 517 660 point calls) (v); closure never ran ⚠; GPU ⚠ never run |
| IOPE | `WW_KOKKOS_IOPE`, opt-in only | `SHRD`, no `MPI` | the host read of `VA` before `W3IOPE` in `W3WAVE`: only the columns `W3IOPE` reads come back from the resident `VA` (section 17); `W3IOPE` itself stays Fortran | `WW_KOKKOS_VA_COUNT` (`VA partial ...`) | measured, CPU: smoke, full and coast byte-identical, `out_pnt.ww3` included, 2026-10-17, `switch_lab_ompg` and `switch_lab_shrd`, with and without `WW_KOKKOS_VA_RESIDENT=1` (v); copies bits only, so any backend gives them; GPU ⚠ never run |
| UPDV | `WW_KOKKOS_UPDV`, opt-in only | `SHRD`, no `MPI`, no `PDLIB`; `IC0` for any column to be zeroed | the host write of `VA` before `W3UICE` in `W3WAVE`: the Fortran `W3UICE` runs on the host `VA` and its `VA(:,JSEA) = 0.` is repeated on the resident `VA` (section 18) | `WW_KOKKOS_VA_COUNT` (`VA device zeroing ...`) | measured, CPU: smoke, full and coast byte-identical 2026-10-17, both lab switches, with and without `WW_KOKKOS_VA_RESIDENT=1`, whose host/device check passed after all 13 coast updates (8 launches, 91 columns) (v); the output alone does not show the kernel (section 17); GPU ⚠ never run |

## 8. W3WAVE's support routines on `ww3-gpu/kokkos-wave-support`

One commit on `ww3-gpu/kokkos-st-flx-swl` (`8e433f99`): `357f575c`, pushed to h0ffmann/WW3 on
2026-10-16. The shim is [`wave_support_shim.cpp`](wave_support_shim.cpp), whose comments carry
the argument table of each entry point (`ww_wave_gath`, `_scat`, `_uwnd`, `_utrn`, `_dzxy`,
`_map3`, `_mapt`, `_distab`), declared in `ww_kokkos_c.hpp`; the ABI is `ww_kokkos` 0.3 on both
sides (`kokkos/CMakeLists.txt`, the fork's `find_package(ww_kokkos 0.3)`), so an install from
before these entry points is refused at configure time, not at link.

- **`w3kokkosmd.F90`**: the `bind(C)` interfaces, the flags (read in `W3KOKKOS_SETUP` through
  `KOKKOS_PART`), `W3KOKKOS_REPORT`, and one wrapper `W3KOKKOS_<R>` per routine with the Fortran
  routine's dummy list (`W3KOKKOS_UWND` takes only `FLFRST`: `VGX`, `VGY` are read only under
  `MGW`). A wrapper passes the module arrays the Fortran routine reads, by reference, calls the
  shim inside `!$OMP CRITICAL (WW_KOKKOS_WAVE)` and stops with `EXTCDE` on a non-zero status.
  It associates no pointer and allocates no module array; it writes the arrays the Fortran
  routine writes (and `NMX0` .. `NCENT` through their pointers, as `W3MAP3` does).
- **`w3wavemd.F90`**: seven hunks under `#ifdef W3_KOKKOS`, each `IF (KOKKOS_X) THEN CALL
  W3KOKKOS_X (...) ELSE` with the original call unchanged in the `ELSE`: `W3DZXY` for the current
  and depth gradients, `W3UWND`, `W3MAP3` (also `GTYPE < 3`), `W3UTRN`, `W3MAPT`, `W3GATH`,
  `W3SCAT`. The `W3DZXY` call under `REFRX` passes the strided section `DCDX(IK,:,:)` and keeps
  the Fortran.
- **`w3initmd.F90`**: after the source-term set-ups, `IF (KOKKOS_DISTAB) CALL W3KOKKOS_DISTAB`,
  then `CALL W3KOKKOS_REPORT (NDSO)`; the `W3SRCE` refusal outside `ST4 NL1` now applies only to
  an explicit `WW_KOKKOS_SRCE=1`. `DISTAB` cannot be hooked in `w3iogrmd.F90`: `W3IOGR('READ')`
  runs it inside `W3INIT` before `WW_KOKKOS_INIT`, and 20 other files call `W3IOGR`, most of them
  programs that never start Kokkos (`grep -l "CALL W3IOGR" WW3/model/src/*.F90`, 21 files), so the hunk rebuilds the tables after
  Kokkos is up, once per process as `W3IOGR`'s `SAVE` flag `FLDISP` does.
- **Memory contract**: phase 1, copy per call, no Unified Memory. `W3GATH`/`W3SCAT` copy one row
  `VA(ISPEC, 0:NSEA)` per call into a `(1, NSEA+1)` View and call the launcher with `ISPEC = 1`, so
  a call moves `O(NSEA)` values, not the whole `VA`; the kernel is a copy and the row holds VA's
  bits. `W3UTRN`'s `TRNY(0)` read (`w3updtmd.F90:2954` at the pin, `TRFLAG` 1, 3 or 5 with a sea
  point at `IXY = 1`) is done by the shim at `trny[-1]`, the address the Fortran reads.
- **Without `KOKKOS`**: the hunks are not compiled, so the routines are the Fortran's. With
  `KOKKOS` and every flag off, the built binary ran `ww3_ts1` byte-identical to `--ww3` (smoke
  with `--kokkos none`, v 2026-10-16).

Not wired, one line each (the six host-side translations of goal 42, read in
`/home/hoffmann/code/ww3-gpu-six/kokkos/src/ww_kokkos/` and this tree on 2026-10-16):

- `W3MPIO`: under `SHRD` its body compiles to nothing (the `W3_MPI` blocks are the routine), so there is nothing to call on the lab switch; under `MPI` it builds MPI requests, which phase 1 does not own.
- `W3SETG`: it is Fortran pointer association (`MAPSF => GRIDS(IMOD)%MAPSF`, ...); C++ cannot point a Fortran module pointer without taking ownership of that state.
- `W3INIT`: it allocates the module arrays and associates their pointers (`W3DIMX`, `W3SETG`, `W3SETW`, ...), opens the files and calls `W3IOGR`/`W3IORS`; it is the owner of the state the shims borrow.
- `W3IORS`: it reads and writes the restart file with Fortran unformatted I/O into module arrays it may allocate; phase 1 has no C++ I/O path.
- `W3IOGO`: it writes the gridded output file with Fortran unformatted I/O, record by record; same reason.
- `W3IOBC`: it reads and writes the boundary file with Fortran unformatted I/O and allocates its module buffers; same reason.

Not covered, and kept on the Fortran by the switch guards or the hunk conditions: the `MPI`
branches of `W3GATH`/`W3SCAT` and `PDLIB`, SMC grids (`W3GATHSMC`, `FSWND`), unstructured
grids (`W3MAP3` with `GTYPE >= 3`), and the `W3UWND` variants `WNT0`, `WNT2`, `OASACM`, `MGW`,
`WCOR`, `STAB2`.

### The checkpoint, 2026-10-16

ADR-0011's pin-move checkpoint on the CPU (OpenMP backend; never `--gpu`). The commands and
their output are in this change's `Tested:` record.

| Run | Parts on Kokkos | Calls made (counters) | Against the Fortran, same executable | Against `--ww3` |
|---|---|---|---|---|
| `just smoke-ww3-gpu` (default, `WW3GPU_ROUTINES=SNL1`) | SNL1 | (no counter set) | byte-identical | byte-identical; `nccmp-tol` PASS |
| smoke `--kokkos none` (`WW_KOKKOS_ALL=0`) | none (0 of 14) | none | byte-identical | byte-identical |
| smoke `--kokkos all` (`WW_KOKKOS_ALL=1`) | all 14 | `W3SRCE` 2 161; `W3UWND` 2 161; `W3UTRN`, `W3MAP3`, `W3MAPT`, `DISTAB` 1 each; `W3GATH`, `W3SCAT`, `W3DZXY` 0; `W3SNL1`, `W3SDS4`, `W3SIN4`, `W3SPR4` 0 (inside the Kokkos `W3SRCE`) | byte-identical | byte-identical |
| smoke `--kokkos SNL1,SDS4,SIN4,SPR4,ST4_TABLES` | 5 | `W3SNL1` 2 339, `W3SDS4` 2 339, `W3SIN4` 6 838, `W3SPR4` 6 660; ST4 tables 0 values differ | byte-identical | byte-identical |
| smoke `--kokkos UWND,UTRN,MAP3,MAPT,DISTAB` | 5 | `W3UWND` 2 161, the rest 1; `W3GATH`, `W3SCAT`, `W3DZXY` 0 | byte-identical | byte-identical |
| smoke `--kokkos DISTAB` | 1 | 1; 0 of 246 table values differ | byte-identical | byte-identical |
| full `--kokkos GATH,SCAT,UWND,UTRN,DZXY,MAP3,MAPT,DISTAB` | 8 | `W3GATH`, `W3SCAT` 13 824 each; `W3UWND` 25; `W3UTRN`, `W3DZXY`, `W3MAP3`, `W3MAPT`, `DISTAB` 1 each | byte-identical | byte-identical (`out_grd.ww3`) |
| full `--kokkos all` | all 14 | `W3SRCE` 230 100; `W3GATH`, `W3SCAT` 13 824 each; `W3UWND` 25; the rest 1; the per-routine ST4 and DIA counters 0 | byte-identical | byte-identical (`out_grd.ww3`) |

No bisection was needed: no run differed. A part whose counter is 0 did not exercise the port
in that run and is reported as such: in the smoke, `ww3_ts1` is one point, so there is no
spatial propagation (`W3GATH`, `W3SCAT`) and no gradient (`W3DZXY`); `W3UTRN` ran only its
`TRFLAG = 0` branch and `W3MAP3`, `W3MAPT` a one-cell grid; the full case (`bench_small`)
exercises them, but its `FLAGTR` is 0 too (`ww3_grid.out` of both cases), so `W3UTRN` stays
`regtested` no. `just ww3-gpu-matrix` and the OpenMP (`OMPG`) entries are the next checkpoint
steps before any of these rows may move to L2 ⚠.

## 9. Wiring units in parallel

`ww3-gpu/kokkos-wire-base` (`4a94a8f6`, h0ffmann/WW3, pushed 2026-10-16) is the branch every
wiring unit starts from. It wires no routine. It moves the lists that every unit used to edit
(`W3KOKKOS_SETUP`, `W3KOKKOS_REPORT`, the `USE` blocks, the source lists, `all_parts`) behind a
convention, so units built at the same time add their own files and their own call-site hunks and
nothing else. What the base adds, all under `KOKKOS` (v, read in the fork and built 2026-10-16):

- **One module file per unit.** The fork's `model/src/CMakeLists.txt` compiles every
  `model/src/w3kokkos_<unit>md.F90` by glob (`CONFIGURE_DEPENDS`) and generates
  `W3KOKKOS_UNITSMD` in the build tree. That module `USE`s every unit module and holds
  `W3KOKKOS_UNITS_SETUP (IMOD)`, which calls every `W3KOKKOS_<UNIT>_SETUP (IMOD)` in file-name
  order. `W3INIT` calls it after `DISTAB` and before `W3KOKKOS_REPORT`. `W3WAVE`, `W3SRCE` and
  `W3INIT` `USE W3KOKKOS_UNITSMD`, so a unit's flags and wrappers are visible at their call
  sites without a `USE` line per unit.
- **A part registry.** `W3KOKKOS_REGISTER (PART, ON)` in `w3kokkosmd.F90` records one part.
  `W3KOKKOS_REPORT` prints the 34 parts of `357f575c` and then every registered part, so the
  `Kokkos parts` line of `log.ww3` needs no edit per unit. A name registered again (W3INIT runs once
  per grid) is updated, not repeated.
- **In this repository.** `fortran_iface/CMakeLists.txt` compiles every `*_shim.cpp` and
  `*_c.hpp` by glob. `scripts/ww3-gpu.sh` reads its part list, its counters and the fork files a
  unit may touch from `parts.d/<unit>.txt`. Its check that the fork is `WW3-noaa/` plus the KOKKOS
  patch accepts `model/src/w3kokkos_<unit>md.F90` and every `FILE` a `parts.d` file names.
  `scripts/ww3-gpu.sh` checks the shape of every `parts.d` line (a `PART` is `[A-Z][A-Z0-9_]*`, a `COUNT` is `WW_KOKKOS_<NAME>_COUNT`, a `FILE` is `model/src/<name>.F90`) and refuses an exempted path that is not a regular file. A `FILE` line widens the fork check to a physics file, so review it like the fork hunk it covers: the unit's gate shows the file preprocesses without `KOKKOS` to the same source as `WW3-noaa/` (security review of PR #165, 2026-10-16).
  `part_flags.cpp` has no part list: `ww_kokkos_flag` resolves any `WW_KOKKOS_<PART>` name.
- **Unchanged.** The C ABI stays `ww_kokkos` 0.3. Without `KOKKOS`, `gfortran -E -P` of
  `w3initmd.F90`, `w3srcemd.F90` and `w3wavemd.F90` with the lab switch is identical to
  `WW3-noaa/`'s (blank lines dropped, v 2026-10-16).

The registry has one limit. The 34 parts that existed before it are still printed from
`W3KOKKOS_REPORT`'s own list, which is closed: a new part registers and is never added there.

### What a unit adds

A unit `<unit>` (lower case, digits and `_`; `units` is reserved) owns these files, and no other
unit edits them:

| Where | File | What it holds |
|---|---|---|
| fork, `ww3-gpu/kokkos-<unit>` from `ww3-gpu/kokkos-wire-base` | `model/src/w3kokkos_<unit>md.F90` | `MODULE W3KOKKOS_<UNIT>MD`: `PRIVATE` by default; the `bind(C)` interfaces; one `LOGICAL :: KOKKOS_<PART>` per part; the wrappers `W3KOKKOS_<R>`, with the Fortran routine's dummy list, the shim call inside `!$OMP CRITICAL`, `EXTCDE` on a non-zero status; `SUBROUTINE W3KOKKOS_<UNIT>_SETUP (IMOD)` |
| fork, same branch | the call-site hunks | `#ifdef W3_KOKKOS` / `IF (KOKKOS_<PART>) THEN` / `CALL W3KOKKOS_<R> (...)` / `ELSE` / the original call unchanged / `END IF` / `#else` / the original call / `#endif` |
| this repository | `kokkos/src/fortran_iface/<unit>_shim.cpp`, `<unit>_c.hpp` | the `extern "C"` entry points and their argument table (the section 6 format), the prototypes |
| this repository | `kokkos/src/fortran_iface/parts.d/<unit>.txt` | a `#` line naming the unit, then `PART <name>` per part, `COUNT <variable>` per counter, `FILE model/src/<file>.F90` per fork file outside the base set that its hunks edit |
| this repository | `kokkos/src/fortran_iface/units/<UNIT>.md` | the section draft: argument table, memory contract, what is not covered, build results |
| this repository | its rows of `kokkos/ports.tsv`, its port reports, one row of `kokkos/port_claims.tsv` before it starts | `wired` and `regtested` text with no `<`, `>`, `&` or `"` (the site test looks for the text verbatim, and the page escapes them) |

The set-up follows one pattern. Its body sits under the unit's switch, because the generated
module calls it on every switch file. It sets each flag with `KOKKOS_PART ('<PART>', SUPPORTED)`,
builds the unit's tables, and ends with one `CALL W3KOKKOS_REGISTER ('<PART>', KOKKOS_<PART>)` per
part. Its first statement reads `IMOD` (`IF (IMOD < 1) RETURN`, a no-op since grids count from 1),
so `-Wall` stays quiet on a switch file that compiles the body out. A minimal unit:

```fortran
MODULE W3KOKKOS_XYP3MD
  USE W3KOKKOSMD, ONLY: KOKKOS_PART, W3KOKKOS_REGISTER
  IMPLICIT NONE
  PRIVATE
  PUBLIC :: KOKKOS_XYP3, W3KOKKOS_XYP3, W3KOKKOS_XYP3_SETUP
  LOGICAL :: KOKKOS_XYP3 = .FALSE.
CONTAINS
  SUBROUTINE W3KOKKOS_XYP3_SETUP ( IMOD )
    INTEGER, INTENT(IN) :: IMOD
    IF ( IMOD < 1 ) RETURN
#ifdef W3_PR3
    KOKKOS_XYP3 = KOKKOS_PART ( 'XYP3', .TRUE. )
    CALL W3KOKKOS_REGISTER ( 'XYP3', KOKKOS_XYP3 )
#endif
  END SUBROUTINE W3KOKKOS_XYP3_SETUP
  ! SUBROUTINE W3KOKKOS_XYP3 ( ... ): the wrapper
END MODULE W3KOKKOS_XYP3MD
```

Rules that keep the units off each other:

- **Names.** Export only `KOKKOS_<PART>`, `W3KOKKOS_<R>` and `W3KOKKOS_<UNIT>_SETUP`. They reach
  `W3WAVE`, `W3SRCE` and `W3INIT` through a whole-module `USE`, so a clash with another unit or a
  local variable fails at compile time.
- **No cycles.** A unit module `USE`s only `ISO_C_BINDING`, `W3KOKKOSMD`, the data modules
  (`W3GDATMD`, `W3WDATMD`, `W3ADATMD`, `W3IDATMD`, `W3ODATMD`) and `W3SERVMD`. It never uses a
  module that holds a call site (`W3WAVEMD`, `W3SRCEMD`, `W3INITMD`, the parent-hook files) or
  `W3KOKKOS_UNITSMD`.
- **Call sites outside `W3WAVE`, `W3SRCE` and `W3INIT`.** A parent hook in another file
  (`w3iosfmd.F90`, `w3wavset.F90`, `w3profsmd.F90`, ...) or in another routine of
  `w3srcemd.F90` (`CALC_FPI`) puts `USE W3KOKKOS_<UNIT>MD` under `#ifdef W3_KOKKOS` in that routine,
  as part of the unit's hunk. A new file also goes in a `FILE` line of the unit's `parts.d` file.
- **Never edit** `w3kokkosmd.F90`, the fork's `CMakeLists.txt` or `cmake/switches.json`,
  `W3KOKKOS_SETUP` or `W3KOKKOS_REPORT`, `ww_kokkos_c.hpp`, `fortran_iface/CMakeLists.txt`,
  `scripts/ww3-gpu.sh`, the ABI version (`kokkos/CMakeLists.txt`, the fork's `find_package`),
  this file, `docs/GLOSSARY.md`, or the outputs of `scripts/port_score.py --write`
  (`PORT_STATUS.md`, `PORT_TABLE.tsv`, `PORT_RANKING.md`, `site/data/port_score.tsv`). Edit no
  existing hunk of the base either. What one of those needs goes into the unit's
  `units/<UNIT>.md` as a snippet for the integrator: section 7 table rows, a glossary entry, a
  `regtested` note.
- **`wired` stays no** until the integrator has merged the unit branches into one fork branch and
  moved the `WW3/` pin. Then it becomes `yes: <flag>, <fork file:line> at <pin>`. The unit writes
  the text it proposes in `units/<UNIT>.md`.

The gate of a unit is build only, until the owner asks for regtests (the owner, 2026-10-16):

```bash
bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --build-only   # the lab switch with KOKKOS
# and the unit's own switch file (plan, "Build switch"), with KOKKOS added
# switch off: gfortran -E -P <switch -D flags> of every touched fork file, run from
#   WW3/model/src and WW3-noaa/model/src, identical after dropping blank lines
just kokkos-test serial-debug && just kokkos-test openmp-release
bash scripts/ww3-gpu.sh --self-test                           # parts.d parses, no duplicate part
```

No regtest, smoke run, matrix or `--gpu` run. Every row of a unit stays `regtested` no:
"compiled, never run inside WW3".

## 10. `W3SRCE` batched on `ww3-gpu/kokkos-srce-batch`

One commit on `ww3-gpu/kokkos-wire-base` (`4a94a8f6`): `caa5da36`, pushed to h0ffmann/WW3 on
2026-10-17. It is phase 2, step 4 of
[`H100_OPTIMIZATION_STRATEGY_202610.md`](../../../docs/H100_OPTIMIZATION_STRATEGY_202610.md): the
source terms of a time step reach Kokkos in one call, without a copy per point and without the
`!$OMP CRITICAL (WW_KOKKOS_SRCE)`. The argument table is in the comment above `ww_srce_batch` in
[`srce_shim.cpp`](srce_shim.cpp); the packet rows are `WwSrceBatchIn` and `WwSrceBatchScal` in
`ww_kokkos_c.hpp`, mirrored by the `SRCEB_*` parameters of `w3kokkosmd.F90` (v, read in both).

- **`w3wavemd.F90`**: one hunk before the `!$OMP PARALLEL` of the source-term loop (section 3.7
  of W3WAVE, `:2325-2331`, and its `END IF` at `:2469-2471` at `caa5da36`), under
  `W3_KOKKOS`, `W3_ST4`, `W3_NL1` and not `W3_PDLIB`: `IF (W3KOKKOS_SRCE_ON(IT)) THEN CALL
  W3KOKKOS_SRCE_BATCH (IT, DTG, TAUWX, TAUWY) ELSE` the loop, unchanged, `END IF`; plus one `USE`
  line (`:452-454`). `TAUWX`, `TAUWY` are W3WAVE's own arrays (`w3wavemd.F90:660`), so they are
  passed. `W3KOKKOS_SRCE_ON` reads no point, so it decides for the whole step. The per-point call
  in the loop is kept, but no build reaches it any more: without `PDLIB` the batch takes the step,
  and `W3KOKKOS_SRCE_SETUP` refuses `PDLIB`.
- **`w3kokkosmd.F90`**: the `bind(C)` interface `WW_SRCE_BATCH`, the `SRCEB_*` row indices and
  `W3KOKKOS_SRCE_BATCH`. It loops `JSEA = 1, NSEAL` (`INIT_GET_ISEA`), packs every point with
  `MAPSTA(IY,IX) == 1 .AND. FLAGST(ISEA)` as `W3KOKKOS_SRCE` receives it, calls the shim once,
  stops with `EXTCDE` on a non-zero status, unpacks, and writes `UNDEF` to `UST`, `USTDIR`,
  `DTDYN`, `FCUT` of the other points, as the loop's `ELSE` does. `VSioDummy`, `VDioDummy` and
  `SHAVETOTioDummy`, which the per-point call also writes, are not written: W3WAVE reads them only
  under `PDLIB` (`w3wavemd.F90:1710-1711`, inside the `#ifdef W3_PDLIB` of `:1640`). The pack
  buffers are `SAVE`, sized `NSEAL`, reallocated only when `NSEAL`, `NK` or `NSPEC` changes.
- **The shim**: chunks of points, one `TeamPolicy(n)` launch per chunk, team *i* running the
  unchanged `w3srce_team` on point *i* with its own `PointScalars`, `State` and work slice. The
  teams share only tables that every callee reads and none writes (the list is in the shim's
  header comment, v read 2026-10-17). `WW_KOKKOS_SRCE_BATCH_MIB` caps the batch buffers (default
  1024 MiB, about 56 KB per point at NK = NTH = 24, so about 19 000 points per launch); the buffers
  are kept between calls and grown on demand. Inside an OpenMP region it runs on Kokkos::Serial,
  as `ww_srce`; W3WAVE calls it outside one.
- **Memory contract**: phase 2 delivery, still phase 1 residency. One host-to-device copy of each
  packed array and one copy back per chunk; the spectrum does not stay on the device between time
  steps. No Unified Memory.
- **Without `KOKKOS`**, the hunk is not compiled: `gfortran -E -P` of `w3wavemd.F90` with
  `switch_lab_shrd` and with `switch_lab_ompg` is identical to `WW3-noaa/`'s, blank lines dropped
  (v, 2026-10-17).

The checkpoint on the CPU, 2026-10-17 (OpenMP backend; never `--gpu`; `kokkos/port_report/W3SRCE_KOKKOS.md` has the table):
`--kokkos SRCE` smoke and full, with `switch_lab_ompg` and `switch_lab_shrd`, byte-identical to
the Fortran of the same executable and to `--ww3`, `nccmp-tol` PASS both; the full also with
`WW_KOKKOS_SRCE_BATCH_MIB=64` (200 launches instead of 25), byte-identical (v). The CUDA build
compiles (`--gpu --build-only`); whether it gives the CPU's bits is not known ⚠. The regtest matrix
did not run ⚠.

## 11. The PR3 propagation batched on `ww3-gpu/kokkos-prop3`

One commit on `ww3-gpu/kokkos-srce-batch` (`caa5da36`): `fcdfabef`, pushed to h0ffmann/WW3 on
2026-10-17. A wiring unit in the shape of section 9, `prop3`: the fork module
`model/src/w3kokkos_prop3md.F90`, three hunks in `w3wavemd.F90`, and in this repository
[`prop3_shim.cpp`](prop3_shim.cpp), [`prop3_c.hpp`](prop3_c.hpp) and `parts.d/prop3.txt`. It
edits none of the shared files of section 9 except this one and `part_flags.cpp`. The argument
tables are in the comments above `ww_xyp3_batch` and `ww_ktp3_batch` in the shim.

- **Flags, opt-in.** `WW_KOKKOS_XYP3=1` and `WW_KOKKOS_KTP3=1`; both are in `kOptInOnly` of
  `part_flags.cpp`, so `WW_KOKKOS_ALL=1` leaves them on the Fortran until their rows are
  regtested. `W3KOKKOS_PROP3_SETUP` covers `PR3` with `UQ` on a regular or curvilinear grid, and
  none of `PDLIB`, `MGP`, `MGG`; `XYP3` also needs `SHRD` and a grid that is not tripole, `KTP3`
  no `REFRX`. Outside that, `=1` stops the model (`KOKKOS_PART`).
- **`w3wavemd.F90`**, under `W3_KOKKOS` only, each `IF (KOKKOS_<PART>) THEN CALL ... ELSE` with
  the Fortran loop unchanged in the `ELSE`:
  - `:1864-1865`, section 3.6.2: `W3KOKKOS_KTP3 (FACTH, FACK, ITLOCH)` in place of
    `DO ITLOC = 1, ITLOCH` and its `!$OMP` loop over `JSEA`;
  - `:2022-2023`, section 3.6.3: `W3KOKKOS_XYP3 (DTG, MAPSTA)` in place of the `DO ISPEC` loop
    (`W3GATH`, `W3XYP3`, `W3SCAT` per component), after `FIELD = 0.`;
  - `:2224-2225`, section 3.6.4: `W3KOKKOS_KTP3 (FACTH, FACK, NTLOC-ITLOCH)` for part 2.
  Without `KOKKOS`, `gfortran -E -P` of `w3wavemd.F90` with `switch_lab_shrd` and with
  `switch_lab_ompg` is identical to `WW3-noaa/`'s, blank lines dropped (v, 2026-10-17).
- **Why one call per loop keeps the bits.** `W3GATH` sets all of `FIELD` for its component,
  `W3XYP3` writes only `FIELD` and its automatic arrays, and `W3SCAT` writes only `VA(ISPEC,:)`,
  so the components are independent and any order gives the Fortran loop's result. Each work item
  runs the unchanged serial body of `w3xyp3` on its own `FIELD`. `W3KTP3` reads and writes only
  its point's `VA(:,JSEA)`, `CFLTHMAX(JSEA)`, `CFLKMAX(JSEA)`, and its other arguments do not
  change between sub-steps, so a point's `NTIMES` calls run back to back in one work item. No
  reduction is reordered. The scatter of `W3XYP3`'s results into `VA` is a second launch over sea
  points, a copy.
- **What the wrappers compute in Fortran.** `W3KOKKOS_XYP3` packs the scalars, converts
  `MAPTRN` to integers, and evaluates `W3XYP3`'s three `DSEC21` calls once (they are the same for
  every component). `W3KOKKOS_KTP3` lists the `JSEA` with `MAPSTA(IY,IX) == 1` and packs each
  point's scalars as W3WAVE passes them, `MAX(DMIN, DW(ISEA))` included; whether gfortran gives
  that `MAX` the same operand order in both places (it matters only for a NaN depth) was not read
  in the object ⚠.
- **Memory contract.** Phase 1 residency: on Serial and OpenMP the kernels read and write WW3's
  arrays in place; on CUDA every input is copied to the device per call and `VA` copied in and out
  whole. No Unified Memory. The work buffers (about 0.6 MB per component and 18 KB per point on
  `bench_small`) are kept between calls, capped by `WW_KOKKOS_PROP3_BATCH_MIB` (default 1024 MiB).
- **What is not exercised.** The full case (`bench_small`) has refraction on, no current-induced
  wavenumber shift (`FLCK` false, so `W3QCK2` never ran), no current (`FLCUR` false), index
  closure none and no boundary input (`ww3_grid.out`); the smoke case is one point and makes no
  call of either part. Those branches were translated and statically argued; since 2026-10-17
  the coast case (`scripts/ww3-gpu.sh --ww3-gpu coast`, bench/README.md) runs the current, the
  wavenumber shift and the boundary input, byte-identical to the Fortran and to `--ww3` on both lab
  switches (v; the rows are in `port_report/W3KTP3_KOKKOS.md`, "The coast case"). Index closure
  still never ran ⚠.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, an
Intel Core i9-14900 with 32 threads, shared with other sessions; never `--gpu`):

| Run | Calls (counters) | Against the Fortran, same executable | Against `--ww3` |
|---|---|---|---|
| full `--kokkos XYP3,KTP3,SRCE`, `switch_lab_ompg` | `W3XYP3` 24 calls, 13 824 spectra; `W3KTP3` 48 calls, 441 792 point calls; `W3SRCE` 230 100 | byte-identical (`out_grd.ww3`, the NetCDF field) | `out_grd.ww3` byte-identical; the NetCDF differs only in `WAVEWATCH_III_switches`; `nccmp-tol` PASS |
| the same, `WW_KOKKOS_PROP3_BATCH_MIB=64` | the same counts, in 144 launches each (6 per call of `W3XYP3`, 3 of `W3KTP3`) | byte-identical | byte-identical (`out_grd.ww3`) |
| full, `switch_lab_shrd` | as the first row | byte-identical | as the first row |
| smoke, both switches | 0 of either (one point) | byte-identical (`out_grd.ww3`, `out_pnt.ww3`, `restart.ww3`, NetCDF) | the same three byte-identical |

The CUDA build compiles (`--gpu --build-only`); whether it gives the CPU's bits is not known ⚠.
The regtest matrix did not run ⚠.

## 12. The resident spectrum on `ww3-gpu/kokkos-resident`

Two commits on `ww3-gpu/kokkos-prop3` (`fcdfabef`, section 11): `0b050a76` (unit 1) and `513954a6`
(unit 2), pushed to h0ffmann/WW3 on 2026-10-17. They are units 1 and 2 of
[WFIP-0005](../../../docs/WFIPs/WFIP-0005-device-resident-step.md): the spectrum
`VA(NSPEC,0:NSEALM)` gets a device copy that lives for the run, and the batched `W3SRCE` reads and
writes it there, so `VA` is no longer packed in Fortran and copied for the source terms. The C ABI
is `ww_kokkos` 0.5.0 (entry points added, `SameMajorVersion`), declared in
[`resident_c.hpp`](resident_c.hpp); the fork asks for `0.5`.

- **The context** ([`resident_shim.cpp`](resident_shim.cpp),
  [`resident_state.hpp`](../ww_kokkos/resident_state.hpp)): one per grid (`IGRID`), bound on the
  grid's first call to the address and shape of `VA` it passes, since `VA` is allocated
  (`W3DIMW`) after `W3KOKKOS_UNITS_SETUP`; a later call with another address or shape is refused
  (status 2), so `ww3_multi` is not supported. `d_va` is a `LayoutLeft` device View of the same
  shape, allocated on the first copy; the host side is WW3's own array, borrowed as an unmanaged
  View. A state says which copy holds the newer bits: `HOST` (the start, and after every host
  write), `DEVICE` (after a kernel wrote `d_va`), `BOTH`. The protocol is `Kokkos::DualView`'s
  (modify, sync), written out because the host array is the Fortran's; `DualView` itself is not
  used ⚠ (whether it takes an unmanaged host View was not tried). Copies are `deep_copy` of the
  whole array and compute nothing.
- **The calls.** `ww_va_host_read` (before a host read: `DEVICE` gives one device-to-host copy,
  state `BOTH`), `ww_va_host_write` (the same, then `HOST`), `ww_va_device` and its C++ form
  `ww::resident::device_va` (before a kernel reads `d_va`: `HOST` gives one host-to-device copy),
  `ww_va_report`. Diagnostics, read once, ignoring `WW_KOKKOS_ALL`: `WW_KOKKOS_VA_RESIDENT=1`
  makes every host call first push a `HOST` array to the device and then copy `d_va` back over it,
  so a host write of `VA` that has no `W3KOKKOS_VA_HOST_WRITE` before it is overwritten by stale
  bits and shows in the output; `WW_KOKKOS_VA_COUNT=1` prints at exit
  `ww_kokkos: Kokkos VA: resident on <backend>; H2D <n> (<MB>), D2H <n> (<MB>); host reads <n>,
  host writes <n>, device syncs <n>; spectrum <MB>` and one counter per line (stderr, as every
  `*_COUNT`; not `log.ww3` as the WFIP drew it). Timers: `va.host.call`, `va.h2d`, `va.d2h`.
- **`w3kokkos_residentmd.F90`** (a wiring unit, section 9; no part of its own): the `bind(C)`
  interfaces and `W3KOKKOS_VA_HOST_READ`, `W3KOKKOS_VA_HOST_WRITE`, which pass `VA`,
  `SIZE(VA,1)`, `UBOUND(VA,2)` and stop the model (`EXTCDE`) on a non-zero status or when called
  inside an OpenMP parallel region (`OMP_IN_PARALLEL`, compiled only with OpenMP).
- **The sites in `w3wavemd.F90`** (line numbers at `513954a6`), each under `#ifdef W3_KOKKOS`,
  outside every `!$OMP PARALLEL`; one call covers every host statement up to the next device call:

  | Line | Call | Before |
  |---|---|---|
  | `:1303` | write, when `FLIWND .AND. LOCAL` | `W3UINI (VA)` |
  | `:1389` | write | `W3UICE (VA)` |
  | `:1477` | write | `W3ULEV (VA, VA)` |
  | `:1876` | write | the `KOKKOS_KTP3` choice of section 11 and the `DO ITLOC` loop of `W3KTP3` part 1 (its parallel region is inside the loop) |
  | `:2024` | write | the `KOKKOS_XYP3` choice of section 11 and the `DO ISPEC` loop of `W3GATH`/`W3XYP3`/`W3SCAT` (and the SMC `ARCTC` block after it) |
  | `:2242` | write | the same for `W3KTP3` part 2 |
  | `:2375`, `:2378` | write, only when the Fortran loop runs (`.NOT. W3KOKKOS_SRCE_ON(IT)`; unconditional without `ST4 NL1` or with `PDLIB`) | the source-term loop |
  | `:2634` | read | the output block: `W3CPRT`, `W3OUTG`, `W3IOPE`, `W3IOTR`, `W3IORS ('HOT')`, `W3IOBC ('WRITE')`, `W3IOSF` |
  | `:3083` | read | the return of `W3WAVE`, so its caller never sees a stale `VA` |

  Not a site: `W3UBPT`, which the WFIP's table lists, writes the boundary spectra `BBPI0`,
  `BBPIN` and not `VA` (`w3updtmd.F90:1329-1545` at the pin has no `VA`, v); they reach `VA`
  through `W3XYP3`, after the `:2024` call.
- **`W3KOKKOS_SRCE_BATCH`** (`w3kokkosmd.F90:1875-1996`) no longer packs or unpacks the
  spectrum: it passes `VA`, its shape and `BJSEA(1:NPTS)` to `ww_srce_batch_va`
  (`w3kokkosmd.F90:1959`), which syncs `d_va`, copies the column list, and lets team *i* run
  `w3srce_team` in place on column `jsea[i]` of `d_va`; afterwards the state is `DEVICE`. The
  other packed arrays (`wn1`, `cg1`, `pin`, `psc`, `whitecap`, `taubbl`, `tauice`, `shave`) still
  cross per call. `ww_srce_batch` stays in the ABI, called by no fork branch at this pin.
- **Memory contract**: phase 2, step 5. `d_va` is `4 NSPEC (NSEALM+1)` bytes on the device for the
  run; the batch buffers no longer hold a spectrum per point. No Unified Memory.
- **Without `KOKKOS`**, no hunk is compiled: `gfortran -E -P` of `w3wavemd.F90`, `w3initmd.F90`
  and `w3srcemd.F90` with `switch_lab_shrd` and `switch_lab_ompg` is identical to `WW3-noaa/`'s,
  blank lines dropped (v, 2026-10-17). With `KOKKOS` and no kernel on `d_va` (every part but
  `SRCE`), the state stays `HOST` and no copy is made (the counters below).

The checkpoint on the CPU, 2026-10-17 (OpenMP backend; never `--gpu`; the table is in
`kokkos/port_report/W3SRCE_KOKKOS.md`, "Resident spectrum"): `--kokkos SRCE` smoke and full and
`--kokkos all` smoke, with `switch_lab_ompg` and `switch_lab_shrd`, each with and without
`WW_KOKKOS_VA_RESIDENT=1`, plus `--kokkos all` and `--kokkos XYP3,KTP3,SRCE` full on
`switch_lab_ompg`, byte-identical to the Fortran of the same executable and to `--ww3`,
`nccmp-tol` PASS both (v). The same set passed on unit 1 alone, before unit 2, at `caa5da36` plus
unit 1: there the counters read 0 copies in every run without the diagnostic, so no kernel had
`VA` and the state never left `HOST`. Traffic of `VA` per run (`WW_KOKKOS_VA_COUNT=1`):

| Run | Unit 1 only | Units 1 and 2 | Units 1 and 2, `WW_KOKKOS_VA_RESIDENT=1` |
|---|---|---|---|
| smoke (`ww3_ts1`, 2 161 steps, one point) | 0 / 0; host reads 2 166, writes 2 161 | H2D 1, D2H 2 161; reads 2 166, writes 0, device syncs 2 161 | H2D 1, D2H 2 166 |
| full (`bench_small`, 25 source-term steps, 9 204 active points) | 0 / 0; reads 14, writes 97 | H2D 25, D2H 25 (530.21 MB each way, 21.21 MB per step); reads 14, writes 72, device syncs 25 | H2D 73, D2H 86 |
| full with unit 1 only, `WW_KOKKOS_VA_RESIDENT=1` | H2D 98, D2H 111: each of the 111 calls copies `d_va` back (D2H), and the first call and each of the 97 that follow a write first push the host array (H2D), so a call after a write costs 2 copies and a call after a read 1 | | |

With unit 1 only, the batch still packs the spectra in Fortran and copies them inside
`srce.batch.h2d`/`.d2h` (9 204 x 2 304 B = 21.2 MB each way per step); with unit 2 the same bytes
cross as two `deep_copy` of `VA` and nothing is packed. `bash scripts/ww3-gpu.sh --ww3-gpu smoke
--gpu --build-only` compiled every unit for CUDA; nothing ran on the GPU, and whether CUDA gives
the CPU's bits is not known ⚠. The regtest matrix did not run ⚠. `ww3_multi`, `PDLIB`, `MPI` and
SMC are not covered (WFIP-0005 §8).

## 13. The propagation on the resident spectrum, `ww3-gpu/kokkos-prop3dev`

One commit on `ww3-gpu/kokkos-resident` (`513954a6`, section 12): `90631fad`, pushed to
h0ffmann/WW3 on 2026-10-17. It is unit 3b of
[WFIP-0005](../../../docs/WFIPs/WFIP-0005-device-resident-step.md) as far as this change takes
it: the batched `W3XYP3` and `W3KTP3` of section 11 read and write the device copy `d_va` of
section 12, so `VA` no longer crosses for the propagation, and on a device their read-only inputs
are copied only when their bits change. The ABI is `ww_kokkos` 0.6.0 (entry points added,
`SameMajorVersion`); the fork asks for `0.6` (`model/src/CMakeLists.txt:36-40`).

- **The entry points** ([`prop3_c.hpp`](prop3_c.hpp), [`prop3_shim.cpp`](prop3_shim.cpp)):
  `ww_xyp3_batch_va` and `ww_ktp3_batch_va` take `IGRID` first and, in place of `ncol`, `VA`,
  `SIZE(VA,1)`, `UBOUND(VA,2)` after it, as `ww_srce_batch_va` does. Both run the same checks and
  the same kernels as `ww_xyp3_batch` and `ww_ktp3_batch`: one template per routine
  (`xyp3_batch`, `ktp3_batch`) takes the column base of `VA` either from the host array (staged,
  the old entries) or from `d_va` (`ww::resident::device_va`, which syncs it first and refuses a
  padded View), and nothing else differs. The per-component and per-point lambdas, their work
  arrays, the gather through `MAPSF`, the scatter launch and the order of every operation are the
  ones of section 11, so the arithmetic is unchanged by construction; the index of `VA(ISPEC,JSEA)`
  is `JSEA*NSPEC + ISPEC-1` in both, since `d_va` is `LayoutLeft` of the Fortran's shape. After a
  launch the state is `DEVICE`. The `_va` entries refuse a call from inside an OpenMP region (the
  resident context is not locked; W3WAVE calls both outside one). `ww_xyp3_batch` and
  `ww_ktp3_batch` stay in the ABI, called by no fork branch at this pin.
- **`w3kokkos_prop3md.F90`**: `W3KOKKOS_XYP3` and `W3KOKKOS_KTP3` call the `_va` entries with
  `IGRID` (`W3GDATMD`), `VA`, `SIZE(VA,1)`, `UBOUND(VA,2)` (`:200`, `:208`, `:283` at `90631fad`);
  what they pack is unchanged.
- **`w3wavemd.F90`** (line numbers at `90631fad`), each under `#ifdef W3_KOKKOS`, the Fortran
  `ELSE` path unchanged:

  | Line | Before (section 12) | Now |
  |---|---|---|
  | `:1877` | `CALL W3KOKKOS_VA_HOST_WRITE` before `W3KTP3` part 1 | `IF ( .NOT. KOKKOS_KTP3 ) CALL W3KOKKOS_VA_HOST_WRITE` |
  | `:2029` | the same before the `DO ISPEC` loop | `IF ( .NOT. KOKKOS_XYP3 ) CALL W3KOKKOS_VA_HOST_WRITE` |
  | `:2151` | (covered by `:2024`) | under `W3_SMC`: `IF ( ARCTC .AND. KOKKOS_XYP3 ) CALL W3KOKKOS_VA_HOST_WRITE` before the SMC Arctic block, which reads and writes `VA(:,JSEA)` on the host |
  | `:2255` | `CALL W3KOKKOS_VA_HOST_WRITE` before `W3KTP3` part 2 | `IF ( .NOT. KOKKOS_KTP3 ) CALL W3KOKKOS_VA_HOST_WRITE` |
  | `:1974`, `:2240`, `:2348` | none | under `W3_DEBUGCOH`: `CALL W3KOKKOS_VA_HOST_READ` before the `ALL_VA_INTEGRAL_PRINT` that follows each part |

  With the part on Fortran the call is the one of section 12, so that path makes the same calls
  in the same order. With it on Kokkos, no host statement of W3WAVE touches `VA` between the
  propagation calls and the source-term batch: read in `w3wavemd.F90` from `:1877` to `:2400`, the
  only other uses of `VA` are the SMC Arctic block, the `W3_DEBUGCOH` integrals, and the `PDLIB`
  and SMC-grid branches, which `KOKKOS_XYP3` and `KOKKOS_KTP3` exclude (`W3KOKKOS_PROP3_SETUP`
  takes a regular or curvilinear grid only). `ARCTC` is read from the `PSMC` namelist, false by
  default (`w3gridmd.F90:2630`, `:2677`), so an `SMC` build could carry it on a grid the Kokkos
  `W3XYP3` takes; the `:2151` call covers that case. The `W3CFLXY` loop of section 3.6.1
  (`:1801`) reads `CG`, `CX`, `CY`, never `VA`.
  `W3_DEBUGCOH` and `W3_SMC` with `KOKKOS` were not compiled ⚠.
- **Inputs kept on the device** (CUDA only; on Serial and OpenMP the kernels read WW3's arrays in
  place and nothing is copied). Every read-only argument of both entries keeps a device buffer
  per argument slot between calls and a host shadow of the bits it last copied; a call compares
  the host array with the shadow (`memcmp`) and copies only when the length or a byte differs.
  The comparison is the guarantee, so a writer missed below cannot leave stale bits on the device;
  the list says how often each input should cross (read in the fork at `90631fad`):
  - written once, before the first step, so they cross once per run: `MAPSF`
    (`w3gridmd.F90:5182-5184`, `w3iogrmd.F90:1073`), `CLATS` (`w3gridmd.F90:5031-5035`,
    `:5193`), `SIG`, `DSIP` (`w3gridmd.F90:1320`), `ECOS`, `ESIN`, `EC2`, `ESC`, `ES2`
    (`w3gridmd.F90:1278-1307`), `MAPWN` (`w3gridmd.F90:5307`), `DPDX`, `DPDY`, `DQDX`, `DQDY`,
    `GSQRT` (grid geometry of `W3GDATMD`, read by `W3IOGR`): `ww3_grid` writes them and `W3INIT`
    reads them back; W3WAVE has no statement that writes them;
  - the maps, rewritten only in W3WAVE section 3.5 when `FLMAP` is set (`w3wavemd.F90:1499-1548`;
    `FLMAP` is true at the first step, `:755`, and after an ice or level update, `:1394`, `:1425`,
    `:1451`, `:1485`): `MAPX2`, `MAPY2`, `MAPAXY`, `MAPCXY`, `MAPTH2`, `MAPWN2` and the counts
    `NMX0`..`NCENT` (`W3MAP3`, `w3pro3md.F90:139-526`), `MAPTRN` (`W3MAPT`, `:527`, `:615`),
    `ATRNX`, `ATRNY` (`W3UTRN`, `w3updtmd.F90:2809`, `:2917-2949`);
  - the fields the host updates: `CG`, `WN` and `MAPSTA` (`W3ULEV`, after a level change), `CX`,
    `CY` (`W3UCUR`), `BBPI0`, `BBPIN` (`W3UBPT`), and the packets the Fortran wrappers fill per
    call (`ON`, `MTRN`, the point list and `PSCAL` of `W3KOKKOS_KTP3`), which cross when they
    change.

  `CFLTHMAX` and `CFLKMAX` are written by the kernel and still go in and out at every call on a
  device (`NSEALM` floats each). The counters `W3XYP3 device input copies`, `W3XYP3 device inputs
  unchanged, not copied` and the two of `W3KTP3` (`WW_KOKKOS_PROP3_COUNT=1`) count both; they
  read 0 on the CPU. Nothing of this ran on a GPU ⚠.
- **Memory contract**: phase 2. `VA` stays on the device from the first device call to the next
  host read; the read-only inputs are device buffers kept for the run, refreshed by content. No
  Unified Memory.
- **Without `KOKKOS`**, no hunk is compiled: `cpp -traditional-cpp -P` with the `-DW3_<switch>`
  of `switch_lab_shrd` and of `switch_lab_ompg`, blank lines dropped, gives the same text for
  `w3wavemd.F90`, `w3initmd.F90` and `w3srcemd.F90` of the fork at `90631fad` and of `WW3-noaa/`
  (v, 2026-10-17).

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, an
Intel Core i9-14900 with 32 threads, shared with other sessions; never `--gpu`). Every run below
gave `out_grd.ww3` and the NetCDF field (and in the smoke `out_pnt.ww3`, `restart.ww3`)
`cmp`-equal to the Fortran of the same executable, `out_grd.ww3` (and `out_pnt.ww3`,
`restart.ww3`) `cmp`-equal to `--ww3`, whose NetCDF differs only in `WAVEWATCH_III_switches`, and
`nccmp-tol` PASS both (v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos XYP3,KTP3,SRCE`
and `... smoke --cpu --kokkos XYP3,KTP3,SRCE` and `... smoke --cpu --kokkos all`, on
`switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`, each with and without
`WW_KOKKOS_VA_RESIDENT=1`; and full `--kokkos SRCE`, `KTP3,SRCE` and `XYP3,SRCE` with
`WW_KOKKOS_VA_RESIDENT=1` on `switch_lab_ompg`, the paths where one propagation part stays on the
Fortran and its sync runs. Traffic of `VA` per run (`WW_KOKKOS_VA_COUNT=1`, both switches give
the same counts):

| Run | Section 12 (`513954a6`) | This section (`90631fad`) | This section, `WW_KOKKOS_VA_RESIDENT=1` |
|---|---|---|---|
| full, `--kokkos XYP3,KTP3,SRCE` | H2D 25, D2H 25 (530.21 MB each way); host reads 14, writes 72 | H2D 1 (21.21 MB), D2H 13 (275.71 MB); reads 14, writes 0, device syncs 97 | H2D 1, D2H 14 |
| full, `--kokkos SRCE` (propagation on the Fortran) | H2D 25, D2H 25 | not run without the flag | H2D 73, D2H 86, as in section 12 |
| full, `--kokkos KTP3,SRCE` | | | H2D 25, D2H 38; writes 24 (the `W3XYP3` site) |
| full, `--kokkos XYP3,SRCE` | | | H2D 49, D2H 62; writes 48 (the two `W3KTP3` sites) |
| smoke, `XYP3,KTP3,SRCE` or `all` | H2D 1, D2H 2 161 | the same (one point: neither part is called) | H2D 1, D2H 2 166 |

In the full case the 13 D2H are the host reads of the output block (`:2650`) at the 12 output
times that follow a device write and the one at the return of `W3WAVE` (`:3099`); no step copies
`VA` otherwise, the target of the unit. The `W3XYP3` (13 824 spectra, 24 calls) and `W3KTP3`
(441 792 point calls, 48 calls) counters are unchanged, all on the resident `VA`
(`... batched calls on the resident VA: 24` and `48`). `--timers`, one fenced run each (not a
timing): `va.h2d` 25 calls 0.027 s and `va.d2h` 25 calls 0.028 s before, 1 call 0.001 s and 13
calls 0.014 s after; on the OpenMP backend `d_va` is host memory, so the copies saved are about
0.04 s of an 18 to 23 s run, and the gain this unit exists for is on a device ⚠ (never run). The
timing after the change, `--repeat 3`, is in the port reports of `W3XYP3` and `W3KTP3`.
`bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos XYP3,KTP3,SRCE --build-only` compiled the
shim and the fork for CUDA with no warning from `prop3_shim.cpp`; nothing ran on the GPU, and
whether CUDA gives the CPU's bits is not known ⚠. The regtest matrix did not run ⚠. Not covered:
`ww3_multi`, `PDLIB`, `MPI`, SMC; `W3_SMC` and `W3_DEBUGCOH` with `KOKKOS` were not compiled ⚠.

## 14. The source-term inputs kept on the device (WFIP-0005 unit 4)

No fork change: the `WW3/` pin stays at `ww3-gpu/kokkos-prop3dev` `90631fad` (section 13), and the
ABI stays `ww_kokkos` 0.6.0 (no entry point added). Unit 4 of
[WFIP-0005](../../../docs/WFIPs/WFIP-0005-device-resident-step.md) is a change of
[`srce_shim.cpp`](srce_shim.cpp) alone: `ww_srce_batch_va` (section 12) no longer copies its
per-step inputs at every call. `W3KOKKOS_SRCE_BATCH` packs them as before
(`w3kokkosmd.F90:1907-1950` at `90631fad`): `BJSEA`, `BWN1 = WN(1:NK,ISEA)`,
`BCG1 = CG(1:NK,ISEA)`, the forcing `BPIN` (`DW`, `U10`, `U10D`, `AS`, `ICE`, `RHOAIR`, `ASF`),
and the inout `BPSC` (`UST`, `USTDIR`, `TAUWX` .. `WNMEAN`), `BWC`, `BTB`, `BTI`.

- **On Serial and OpenMP** the kernel reads and writes those packed arrays in place: nothing is
  copied either way, where section 12 made a host `memcpy` of each into the batch buffers and back.
  Team *i* reads row *i* of each and writes rows *i* of `BPSC`, `BWC`, `BTB`, `BTI`, `BSH`, which
  no other team touches, so the bits are those of the buffers before.
- **On a device** each input keeps its batch buffer between calls and a host *shadow* of the bits
  the buffer holds; a call compares the packed array with the shadow (`memcmp`) and copies only
  when a byte or the length differs. For the inout ones the shadow is taken after the copy back,
  so an array the host repacked unchanged is not sent again. The comparison is the guarantee, the
  pattern of `prop3_shim.cpp` (section 13) extended to inout arrays: a writer of `WN`, `CG` (for
  example `W3ULEV` after a level change, `w3updtmd.F90:2378-2380` in `:2066-2614`), of the forcing (`W3UWND`,
  `W3UICE`, `W3URHO`) or of the packed state that this list misses cannot leave stale bits on the
  device. The shadows are kept only when the step runs in one launch (`WW_KOKKOS_SRCE_BATCH_MIB`
  large enough, the default on `bench_small`), and voided when the buffers are reallocated or the
  unpacked entry `ww_srce_batch` writes them. `BPSC`, `BWC`, `BTB`, `BTI` and `BSH` still come back
  at every call: `W3KOKKOS_SRCE_BATCH` unpacks them into `W3ADATMD` and `W3WDATMD`, which the output
  and the next step read on the host (`w3kokkosmd.F90:1967-1995`).
- **Diagnostic** `WW_KOKKOS_SRCE_STAGED=1`: on a host backend the shim stages and skips as on a
  device, into its own buffers, so a wrong skip would feed the kernel stale inputs and show in the
  output. **Counters** (`WW_KOKKOS_SRCE_COUNT=1`): input copies and bytes H2D, inputs unchanged and
  not copied, copies back D2H, and one line per input; on a host backend without the diagnostic
  they are labelled "projected for a device, none made", the same comparison run only to count.
- **Memory contract**: phase 2. The batch buffers live for the run and are refreshed by content;
  no Unified Memory.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, Intel
Core i9-14900, 32 threads, shared with other sessions; never `--gpu`). Each run below gave
`out_grd.ww3` and the `ww3_ounf` NetCDF (full) or `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3`
(smoke) `cmp`-equal to the Fortran of the same executable and to `--ww3`, whose NetCDF differs only
in `WAVEWATCH_III_switches`, `nccmp-tol` PASS both (v): `bash scripts/ww3-gpu.sh --ww3-gpu full
--cpu --kokkos resident` and `... smoke ... --kokkos resident`, each plain, with
`WW_KOKKOS_VA_RESIDENT=1` and with `WW_KOKKOS_SRCE_STAGED=1` (smoke staged on `switch_lab_ompg`
only), and `... smoke ... --kokkos all` plain and with `WW_KOKKOS_VA_RESIDENT=1`, on
`switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`: 15 runs. `VA` traffic is unchanged
(H2D 1, D2H 13 on the full; 1 / 2 161 on the smoke). The source-term inputs, from
`WW_KOKKOS_SRCE_COUNT=1` (the same on both switches and with every flag):

| Run | Section 13, per call on a device | This section | Copies back D2H |
|---|---|---|---|
| full (`bench_small`, 25 calls, 9 204 active points) | 8 inputs, 3 129 360 B per step (78.23 MB in the run) | 8 copies, 3 129 360 B, all at the first call; 192 of 192 later inputs unchanged | 1 104 480 B per step (27.61 MB), unchanged |
| smoke (`ww3_ts1`, 2 161 calls, one point, NK 36) | 8 inputs, 436 B per step (0.94 MB in the run) | 1 452 copies, 41 092 B: `BPIN` 1 441 times (the forcing changes), `BPSC` 5 times, the six others once | 120 B per step (0.26 MB), unchanged |

Per input in the full run: `BJSEA` 36 816 B, `BWN1` and `BCG1` 883 584 B each, `BPIN` 257 712 B,
`BPSC` 773 136 B, `BWC` 147 264 B, `BTB` and `BTI` 73 632 B each, each copied once and unchanged
24 times. `bench_small` has a homogeneous wind (`ww3_shel.nml`, `HOMOG_INPUT`) and no level, ice
or current input, so its forcing never changes after the first step; on a forcing that does, `BPIN`
crosses on each step that changed it, as in the smoke. The numbers on a host backend are the
projection described above; nothing of this ran on a GPU ⚠. `bash scripts/ww3-gpu.sh --ww3-gpu full
--gpu --kokkos resident --build-only` compiled the shim and the fork for CUDA; whether CUDA gives the
CPU's bits is not known ⚠. The regtest matrix did not run ⚠.

## 15. `W3OUTG` on the resident spectrum, `ww3-gpu/kokkos-outg`

One commit on `ww3-gpu/kokkos-prop3dev` (`90631fad`, section 13): `e0cc8ee9`, pushed to h0ffmann/WW3
on 2026-10-17. It is unit 5 of [WFIP-0005](../../../docs/WFIPs/WFIP-0005-device-resident-step.md):
`W3OUTG`'s integrals ([`iogo_w3outg.hpp`](../ww_kokkos/iogo_w3outg.hpp), translated 2026-10-16)
read the device copy `d_va` of section 12, so a field-output time copies no `VA` to the host for
them. The ABI is `ww_kokkos` 0.7.0 (one entry point added, `SameMajorVersion`); the fork asks for
`0.7` (`model/src/CMakeLists.txt`).

- **The fork** adds the unit file `model/src/w3kokkos_outgmd.F90` (`W3KOKKOS_OUTGMD`, found by the
  glob of section 9) and two hunks of `w3wavemd.F90`. `W3KOKKOS_OUTG_SETUP` sets `KOKKOS_OUTG`
  from `WW_KOKKOS_OUTG`, opt-in (`kOptInOnly` of [`part_flags.cpp`](part_flags.cpp): `WW_KOKKOS_ALL=1`
  does not turn it on), and refuses `WW_KOKKOS_OUTG=1` where the translation does not hold the
  statements: anything but `SHRD`, and `MPI`, `IG1`, `O8`, `O9`, `T`. `W3WAVE`'s `CALL W3OUTG (VA,
  FLPFLD, FLOUTG, FLOUTG2)` becomes `IF ( KOKKOS_OUTG ) CALL W3KOKKOS_OUTG (...) ELSE` the Fortran.
- **The fallback.** `W3KOKKOS_OUTG` builds `W3OUTG`'s `FLOLOC` (`w3iogomd.F90:1381-1386`) and calls
  the Fortran `W3OUTG` whole, after a host read of `VA`, when a field the translation leaves to it is
  requested: the partitions of section 6 (`FLPART`; they copy `W3CPRT`'s table, which `W3CPRT`
  builds on the host from `VA`), `CALC_U3STOKES` (`FLOLOC(6,8)`, `(6,12)`) and `CALC_WBT`
  (`FLOLOC(2,17)`), not translated, `SKEWNESS` (`FLOLOC(8,7:9)`, translated, not wired), or a
  requested 3-D field that `W3DIMA` did not allocate (`E3DF(1,k)`, `P2MSF(1)` zero). The shim refuses
  any of those flags too (`WW_KOKKOS_ERR_UNSUPPORTED`).
- **The entry point** ([`outg_c.hpp`](outg_c.hpp), [`outg_shim.cpp`](outg_shim.cpp)):
  `ww_outg_va(igrid, ipar, rpar, flogrd, flogr2, flogrr, va, nspec, nsealm, cg, wn, dw, sig, dden,
  dsii, ecos, esin, ec2, es2, esc, mapsf, mapsta, fields, nfields)`: 25 packed integers, 10 reals,
  the three flag tables as `INTEGER(C_INT)`, and the 46 output fields as a `TYPE(C_PTR)` array in
  the order of `WwOutgField` (40 of `(NSEALM)`, `EF` .. `STH2M` and `P2SMS` 2-D, null when not
  allocated). It calls `ww::iogo::w3outg` unchanged with `FLPART` false and no hook, on `A` = `d_va`
  (`ww::resident::device_va`, which copies `VA` in only when the host holds the newer bits; `W3OUTG`
  does not write `A`, so the state stays as it was).
- **Memory contract**: phase 2. On Serial and OpenMP every input and field is WW3's own array, read
  and written in place. On a device the inputs (`CG`, `WN`, `DW`, `SIG`, `DDEN`, `DSII`, the five
  direction tables, `MAPSF`, `MAPSTA`) are copied in per call, that is per output time, and the 46
  fields are device buffers for the run, of which only those a host reader needs come back: the
  `FLOGRD` (with `FLOUTG`) and `FLOGR2` (with `FLOUTG2`) entries `W3IOGO ('WRITE')` writes
  (`w3iogomd.F90:3158-3683`), and when `OARST` the `FLOGRR` entries `W3IORS ('HOT')` writes
  (`w3iorsmd.F90:1044-1120`: `HS`, `WLM`, `T0M1`, `T01`, `FP0`, `THM`, `BHD`, `TUSX`/`TUSY`,
  `USSX`/`USSY`, `UBA`/`UBD`). `CGE`, which `W3OUTG` writes at sea points only, is pushed to its
  buffer first. No Unified Memory.
- **Host readers of the fields** (the audit, at the pin, of the files `ww3_shel` compiles under the
  lab switches): every `USE W3ADATMD` that names a `W3OUTG` field is `W3IOGO`, `W3IORS` (above),
  `W3OUTG` itself and `W3MPIO` (`w3initmd.F90:2255-2259`, under `W3_MPI` only, where the unit is
  refused); `W3WDAS` (`ww3_shel.F90:2619`) reads none; the `wm*` files are `ww3_multi`'s; the
  coupling senders of `W3_OASIS` read the `FLOGR2` fields, which come back with `FLOUTG2`.
- **`VA` at an output time.** The one host read at the head of section 4.b (section 11) became one
  before each host reader of `VA`: `W3CPRT`, the Fortran `W3OUTG`, `W3IOPE`, `W3IOTR`, `W3IORS`
  (both restart streams), `W3IOBC ('WRITE')`, `W3IOSF` and the `W3_OASIS` senders, so a call with
  `W3OUTG` on Kokkos and no other reader copies nothing. Under `W3_MPI` the single read stays, since
  the requests that section 4.b starts send `VA`. A read when the host already holds the bits copies
  nothing, so the extra calls cost a function call each. Without `KOKKOS` nothing changes:
  `gfortran -E -P` of the fork's `w3wavemd.F90` equals `WW3-noaa`'s under `switch_lab_ompg` and
  `switch_lab_shrd`, blank lines aside (v, 2026-10-17).
- **The `switch_lab_ompg` object.** The translation had been checked against `switch_lab_shrd`'s
  object only. `W3OUTG` under `W3_OMPG` differs in two choices, which `W3OutgScalars::ompg` (set by
  the shim from `W3_OMPG`) now selects: no libmvec in the space-time loop (`thmp_sincos` takes the
  scalar `sincosf` for every point) and `AB` as the `maxss` destination of `MAX(1E-20,AB)` at
  `:1678` and `:1683` whatever `FLOLOC(3,5)`
  ([port report](../../port_report/W3OUTG_KOKKOS.md), "Arithmetic").
- **Counters** (`WW_KOKKOS_OUTG_COUNT=1`, a `COUNT` line of [`parts.d/outg.txt`](parts.d/outg.txt)):
  Kokkos calls, sea points, and the fields and bytes copied back (on a host backend labelled
  "projected for a device, none made"). **Timers**: `outg.call`, `outg.h2d`, `outg.kernel`,
  `outg.d2h`.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, Intel
Core i9-14900, 32 threads, shared with other sessions; never `--gpu`).
Each run gave every file it wrote `cmp`-equal to the Fortran of the same executable and to
`--ww3`, whose NetCDF differs only in `WAVEWATCH_III_switches`, `nccmp-tol` PASS both (v):
`out_grd.ww3` and the `ww3_ounf` NetCDF (full), with `out_pnt.ww3` and `restart.ww3` (smoke), with
`out_pnt.ww3`, `nest.ww3` and the boundary, current, level and ice NetCDF (coast). `bash
scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos SRCE,XYP3,KTP3,OUTG`, `... smoke ...` and `...
coast ...`, each plain and with `WW_KOKKOS_VA_RESIDENT=1`, and `... smoke ... --kokkos all` plain and
with `WW_KOKKOS_VA_RESIDENT=1`, on `switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`; `--kokkos
resident` (now with `OUTG`) on full and smoke, once more with `--timers` on full, and `--kokkos
SRCE,XYP3,KTP3` (the "before" below) on full, smoke and coast, under `switch_lab_ompg`: 23 runs. `full` asks for `HS` alone, so a
diagnostic beside the script ran `bench_small` with every field this unit computes (36 output
names, `E3D`, `TH1MF`, `STH1MF`, `TH2MF`, `STH2MF`, `P2SF` set, and the space-time extremes in each
of their three modes, `STDX`/`STDY`/`STDT`, and off): `out_grd.ww3` byte-identical to the Fortran of
the same executable on both switches, eight runs, 13 Kokkos calls and 44 fields each (v). The
smoke (`ww3_ts1`) asks for `WBT` and `SKW`, so every call there took the Fortran fallback (0 Kokkos
calls); its byte identity checks the split host reads, not the kernel.

`VA` traffic from `WW_KOKKOS_VA_COUNT=1` (the same on both switches):

| Run | Before (`SRCE,XYP3,KTP3`) | With `OUTG` | Why |
|---|---|---|---|
| full (13 field outputs, `HS`) | H2D 1, D2H 13, 14 host sync calls | H2D 1, D2H 1, 1 host sync call | the one D2H is the read at the return of `W3WAVE`; `bench_small` writes no restart |
| full, `WW_KOKKOS_VA_RESIDENT=1` | (both copies at every host call) | H2D 1, D2H 1 | one host call |
| smoke (`ww3_ts1`) | H2D 1, D2H 2 161 | H2D 1, D2H 2 161 | point output every 60 s reads `VA` (`W3IOPE`), and `W3OUTG` falls back |
| coast | H2D 13, D2H 36 | H2D 13, D2H 36 | point output every 1 800 s coincides with every field output, and the level and ice updates write `VA` |

The fields the shim would copy back on a device, projected on the host backend: 13 copies, 0.48 MB
in the full run (`HS`); 65, 0.96 MB in the coast run (`HS`, `FP0`, `THM`, `THP0`, `T0M1`). The
breakdown of a full run on `switch_lab_ompg` (`--kokkos resident --timers`, one run, fenced, a
breakdown and not a timing): `outg.call` 13 calls, 0.358 s of 30.20 s, all of it `outg.kernel`;
`va.d2h` 1 call, 0.001 s. Nothing of this ran on a GPU ⚠. `bash scripts/ww3-gpu.sh --ww3-gpu full
--gpu --kokkos SRCE,XYP3,KTP3,OUTG --build-only` compiled the shim and the fork for CUDA, with no
warning from `outg_shim.cpp`; whether CUDA gives the CPU's bits is not known ⚠. The device
copy-back path never ran ⚠. The regtest matrix did not run ⚠. The preset `--kokkos resident` of
`scripts/ww3-gpu.sh` now includes `OUTG`.

## 16. `W3XYP3` one team per component (`WW_KOKKOS_XYP3_TEAM`)

No fork change: the shim and the headers only. `ww_xyp3_batch` (and `_va`) ran one work item per
spectral component, the serial body of `W3XYP3` inside it: 576 work items for the whole grid
(`NK = NTH = 24`), each sweeping its own `FIELD` of about 0.6 MB, so a GPU could keep only a few
of its multiprocessors busy. The diagnostic `WW_KOKKOS_XYP3_TEAM` keeps one component per work
unit but gives each a Kokkos team whose threads share the loops inside it. Unset (or empty), the
per-component kernel runs as before; `AUTO` lets Kokkos pick the team size; a positive N asks for
N threads, refused (`WW_KOKKOS_ERR_UNSUPPORTED`) above the backend's `team_size_max`. A value that
does not parse is reported once and the per-component kernel runs. `WW_KOKKOS_PROP3_COUNT=1` adds
`W3XYP3 team launches`.

- **How.** [`prop_loops.hpp`](../ww_kokkos/prop_loops.hpp) has two ways of running a loop:
  `SerialLoops` (a plain `for` in index order) and `TeamLoops` (`TeamThreadRange`, then a team
  barrier; `one()` is `single(PerTeam)` then a barrier). `w3xyp3` and `w3qck3` became
  `w3xyp3_loops` and `w3qck3_loops`, templates on that choice; `w3xyp3` and `w3qck3` call them
  with `SerialLoops`, so `prop_xyp2.hpp`, `wave_w3wave.hpp` and the per-component kernel keep
  their calls. Every statement of every loop body is the one it was, in the same order; only the
  loop heads changed. FIELD's layout is unchanged.
- **Which loops are split, and why the bits cannot change.** A loop is split only when each
  iteration writes elements no other iteration of that loop writes or reads, so each element gets
  the same operations from whichever thread runs it; the barrier makes the next loop see all of
  them, as in the serial body. Read in the fork at `e0cc8ee9` (`w3pro3md.F90` and `w3uqckmd.F90`
  are byte-identical to `WW3-noaa/` at `03adda91`, `diff`, v):
  - `W3QCK3` has no row loop: its sweeps run over the lists `MAPBOU` (`MAPX2` or `MAPY2`) and
    `MAPACT` (`MAPAXY`). Sections 2 to 4 (`w3uqckmd.F90:1130-1172`, `:1187-1204`, `:1217-1234`)
    write `FLA(IXY)` and read only `Q`, `CFLL` (not written there); section 6 (`:1258-1283`)
    writes `Q(IXY)` from `Q(IXY)` itself, `TRANS` and `FLA`. The lists are injective: W3MAP3
    adds a cell to `MAPX2` under three mutually exclusive tests of `MAPSTA(IY,IX)` and
    `MAPSTA(IY,IX2)` (`w3pro3md.F90:270`, `:287`, `:304`), likewise `MAPY2` (`:334`, `:351`,
    `:368`), and each active cell once to `MAPAXY` (`:392`). The closure loops over `IY`
    (`:1101-1106`, `:1244-1246`) touch only indices congruent to `IY` modulo `MY`, so two rows
    never meet even when `NX < 3` makes columns alias. These are the loops W3_OMPH itself runs
    as `PARALLEL DO` (`:1098`, `:1123`, `:1255`).
  - In `W3XYP3`: the per-sea-point loops (the averaging stencil set-up `:983-1059`, the
    velocities `:1080-1137`, the straightening `:1301-1306` and back `:1368-1373`, the store
    `:1430-1438`) write through `MAPSF(ISEA,3)`, injective because `ww3_grid` numbers each sea
    cell once (`w3gridmd.F90:5020-5028`) and `MAPSF(:,3) = IY + (IX-1)*NY`
    (`w3iogrmd.F90:1073`); W3_OMPH runs these as `PARALLEL DO` too (`:978`, `:1077`, `:1106`,
    `:1126`, `:1298`, `:1365`, `:1427`). Also split: the zeroing and copies of `:944-956`, `:1149-1150`, `:1280-1282`, and the global fold `:1287-1290`, per `IY` (indices `IY` modulo `NY`).
  - **Kept on one thread:** the averaging scatter, central and near-coast (`:1154-1183`,
    `:1187-1276`), whose `+=` targets coincide, so the store order is binding (W3_OMPH leaves it
    serial too); the boundary update (`:1406-1414`), where a point listed twice takes the last
    write; the in-order `MINVAL`/`MAXVAL` of `:896-899`, broadcast to the team. Splitting the
    averaging would reorder its sums: an owner's decision and its own PR.
  - **W3GATH** in the team kernel: `FIELD = 0.` split over cells, a barrier, then the gather split
    over sea points (`MAPSF(:,3)` injective). The scatter back into `VA` is the existing launch.
- **`W3KTP3`** stays one point per work item (9 204 in `bench_small`). Its inner loops are over
  the 576 spectral bins of a point and could go to vector lanes, but its CFL maxima (`CFLTHMAX`,
  `CFLKMAX`) are in-order reductions whose NaN behaviour depends on the order; nothing there is as
  clear as `W3XYP3`'s case, so it was not changed.
- **CUDA, `ptxas -v`, `sm_89`** (the shim compiled alone with `-Xptxas -v` in a scratch build,
  `nvcc_wrapper`, `CMAKE_CUDA_ARCHITECTURES=89`; nothing ran):

  | Kernel | Before | After |
  |---|---|---|
  | `ww_xyp3_batch`, one work item per component | 96 registers, 0 spill, 0 stack | 96 registers, 0 spill, 0 stack (unchanged) |
  | `ww_xyp3_team`, one team per component | none | 80 registers, 0 spill, 0 stack, 1 barrier, 16 bytes shared memory |
  | `ww_xyp3_scatter` | 34 registers | 34 registers |
  | `ww_ktp3_batch` | 80 registers, 0 spill | 80 registers, 0 spill |

  At 96 registers an `sm_89` multiprocessor holds at most 21 warps (65 536 registers, 3 072 per
  warp), at 80 at most 25 of its 48: a register limit of 44 % and 52 % occupancy, computed, not
  measured ⚠. The point of the change is the launch size: 576 threads in 18 warps before, so at
  most 18 of the RTX 4090's 128 multiprocessors had work; 576 teams of up to `team_size_max`
  threads after, four or five teams per multiprocessor when they spread evenly. Which size `AUTO` picks on CUDA, and the
  time, are not known: nothing ran on a GPU ⚠. `bash scripts/ww3-gpu.sh --ww3-gpu full --gpu
  --kokkos resident --build-only` compiled the shim and the fork for CUDA.
- **On the CPU the team version is slower** (one barrier per loop on OpenMP threads that would
  otherwise each own a component); it exists for the device, and the default stays the
  per-component kernel.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, 32 threads, Kokkos 5.2.2, gfortran/g++
16.2.0, Intel Core i9-14900, shared with other sessions; never `--gpu`): `WW_KOKKOS_XYP3_TEAM=<t>
bash scripts/ww3-gpu.sh --ww3-gpu <case> --cpu --kokkos resident --timers`, for `<t>` unset,
`AUTO`, 1, 4 and 16, `<case>` full and coast, on `switch_lab_ompg` and
`WW3GPU_SWITCH=switch_lab_shrd`: 20 runs. In every one, every file the run wrote
(`out_grd.ww3` and the NetCDF field; in coast also `out_pnt.ww3`) is `cmp`-equal to the Fortran of
the same executable, `out_grd.ww3` (and `out_pnt.ww3`) `cmp`-equal to `--ww3`, and `nccmp-tol`
PASS both (v). `W3XYP3 team launches` read 0 with the flag unset and 24 (full) or 72 (coast), one
per call, with it set; the spectra are 13 824 and 41 472 in every run. `xyp3.kernel`, one fenced
run each, a breakdown and not a timing (the machine was shared, load average 9 to 15):

| `xyp3.kernel` (s) | unset | `AUTO` | 1 | 4 | 16 |
|---|---|---|---|---|---|
| full, `switch_lab_ompg` (24 calls) | 1.31 | 3.16 | 3.92 | 5.56 | 24.19 |
| coast, `switch_lab_ompg` (72 calls) | 1.23 | 1.57 | 1.30 | 5.98 | 32.59 |
| full, `switch_lab_shrd` | 1.58 | 1.53 | 1.70 | 3.11 | 15.95 |
| coast, `switch_lab_shrd` | 1.37 | 1.81 | 1.22 | 6.76 | 36.51 |

The team size `AUTO` resolves to on OpenMP was not printed ⚠. Also run: `bash scripts/ww3-gpu.sh
--self-test`, `just kokkos-test serial-debug` and `openmp-release`, `python3 scripts/port_score.py
--write` then `--check`. The regtest matrix did not run ⚠.

## 17. The point output's spectra alone, `ww3-gpu/kokkos-pnt`

One commit on `ww3-gpu/kokkos-outg` (`e0cc8ee9`, section 15): `2f35368a`, pushed to h0ffmann/WW3 on
2026-10-17. A follow-up of [WFIP-0005](../../../docs/WFIPs/WFIP-0005-device-resident-step.md) unit 5:
before `W3IOPE` (point output) only the spectra it reads come back from the device copy `d_va` of
section 12, not the whole `VA`. The ABI is `ww_kokkos` 0.8.0 (one entry point added,
`SameMajorVersion`); the fork asks for `0.8` (`model/src/CMakeLists.txt`).

- **What `W3IOPE` reads of `VA`.** Its dummy `A(NTH,NK,0:NSEAL)` (`w3iopomd.F90:1003`) is read in one
  statement, `SP(ITH,IK,J) = A(ITH,IK,IS(J))` (`:1214`, in the `W3_SHRD` loop `:1210-1217`), with
  `IS(J) = MAPFS(IY(J),IX(J))` (`:1069`) and `IX(:) = IPTINT(1,:,I)`, `IY(:) = IPTINT(2,:,I)`
  (`:1051-1052`), for the four interpolation corners `J` of each output point `I = 1..NOPTS`
  (`:1043`); no other line of `:873-1293` names `A`, and `W3IOPO ('WRITE')`, which follows, reads
  `W3ODATMD` and no `VA` (the module uses `W3WDATMD` only for `ICE`, `ICEH`, `ICEF`, `RHOAIR`,
  `ZETA_SETUP`, `TIME`, `W3SETW`, read at the pin, v). A land corner has `MAPFS = 0`, so column 0 is
  read too (its weight is 0, `:1083`). Under `W3_SHRD` `JSEA = ISEA` (`NAPROC = 1`,
  `w3parall.F90:1236`), so those `IS(J)` are the columns of `VA`. `IPTINT` is set once, by `W3IOPP`
  at set-up (`w3iopomd.F90:543-544`).
- **The fork** adds the unit file `model/src/w3kokkos_pntmd.F90` (`W3KOKKOS_PNTMD`).
  `W3KOKKOS_PNT_SETUP` sets `KOKKOS_IOPE` from `WW_KOKKOS_IOPE`, opt-in (`kOptInOnly` of
  [`part_flags.cpp`](part_flags.cpp)), and refuses it outside `SHRD` and under `MPI`, where the
  spectra reach `W3IOPE` through `W3MPIO`'s requests and section 4.b keeps its single host read.
  At the point-output site (`w3wavemd.F90:2899-2909` at `2f35368a`) `CALL W3KOKKOS_VA_HOST_READ`
  becomes `IF ( KOKKOS_IOPE ) CALL W3KOKKOS_IOPE_READ ELSE CALL W3KOKKOS_VA_HOST_READ`;
  `CALL W3IOPE ( VA )` after it is unchanged. `W3KOKKOS_IOPE_READ` lists the `4 NOPTS` values
  `MAPFS(IPTINT(2,J,I),IPTINT(1,J,I))` and calls `ww_va_host_read_cols`.
- **The entry point** ([`resident_c.hpp`](resident_c.hpp), [`resident_shim.cpp`](resident_shim.cpp)):
  `ww_va_host_read_cols(igrid, va, nspec, nsealm, ncol, cols)`. The columns are sorted and made
  unique, and a column outside `0..NSEALM` is refused (`WW_KOKKOS_ERR_BAD_SHAPE`). With the state
  `DEVICE` a kernel gathers `d_va(:,cols)` into a device buffer, one `deep_copy` brings it back,
  and each column is stored into WW3's `VA` at its place; with `HOST` or `BOTH` the host already
  holds the bits and nothing is copied. The list and its buffers are kept between calls, since
  `W3IOPE` asks for the same columns every time. Bits move and nothing is computed, so each column
  copied holds what a full `ww_va_host_read` would have put there.
- **Why the state does not change.** After the partial read only the listed columns of the host
  `VA` are current; the others are as stale as before. So the call leaves the state `DEVICE`: the
  next host reader (`W3WAVE`'s return, an update before `W3ULEV` or `W3UICE`, a restart, `W3CPRT`,
  the Fortran `W3OUTG`) still calls `ww_va_host_read` or `_write`, which copies the whole array, and
  the next kernel reads `d_va`, which the partial read did not touch. `W3IOPE` writes no `VA`
  (`INTENT(IN)`, `:1003`), so no host write needs a sync. Under `WW_KOKKOS_VA_RESIDENT=1` a `HOST`
  array is pushed first, as every host call does, and the gather then always runs, so the
  diagnostic exercises the copy at every point-output time; the rest of the host `VA` is not
  refreshed, so a column `W3IOPE` reads and the list missed would keep stale bits and show in
  `out_pnt.ww3`.
- **Counters** (`WW_KOKKOS_VA_COUNT=1`): `VA partial host reads`, `VA partial device-to-host
  copies`, `VA partial columns copied`, and the partial reads in the traffic line. **Timers**:
  `va.host.cols`, `va.d2h.cols`.
- **Not covered.** `W3IOTR` (track output) still reads the whole `VA`: the columns it reads come from
  the track positions it reads from its input file inside the routine (`w3iotrmd.F90:264`, `:819`),
  so listing them before the call means reproducing that reading; none of the cases writes tracks.
  Section 23 covers it, and `W3IOBC ('WRITE')`, from inside `W3IOTR` once the track is read.
  `ww3_multi` (`wmwavemd.F90:1367` calls `W3IOPE` with no sync) is not supported by the resident
  `VA` at all (section 12).
- **Memory contract**: phase 2. A device buffer of `NSPEC` floats per distinct column and a host one
  of the same size, for the run. No Unified Memory.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, Intel Core
i9-14900, 32 threads, shared with other sessions; never `--gpu`). `bash scripts/ww3-gpu.sh --ww3-gpu
smoke|full|coast --cpu --kokkos SRCE,XYP3,KTP3,OUTG,IOPE` and `... smoke ... --kokkos all`, on
`switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`, each plain and with `WW_KOKKOS_VA_RESIDENT=1`:
16 runs. In each, every file the run wrote, `out_grd.ww3`, the `ww3_ounf` NetCDF, and in the smoke
`out_pnt.ww3` and `restart.ww3`, in the coast `out_pnt.ww3`, was `cmp`-equal to the Fortran of the
same executable, and `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` to `--ww3`, whose NetCDF differs only
in `WAVEWATCH_III_switches` (`ncdump` diff); `nccmp-tol` PASS both, every `max|d|` 0 (v). `--kokkos
all` leaves `IOPE` off (opt-in), so its 4 runs check that the default path is unchanged.

`VA` traffic, `WW_KOKKOS_VA_COUNT=1` (the same on both switches; "before" is section 15):

| Run | Before (`SRCE,XYP3,KTP3,OUTG`) | With `IOPE` | With `IOPE`, `WW_KOKKOS_VA_RESIDENT=1` |
|---|---|---|---|
| smoke (`ww3_ts1`, one point, 2 161 point outputs) | H2D 1, D2H 2 161 | H2D 1, D2H 37; 2 161 partial reads, 2 124 of them copying 2 columns (the sea point and column 0) | H2D 1, D2H 42; 2 161 partial copies, 4 322 columns |
| full (`bench_small`, no point output) | H2D 1, D2H 1 | the same; 0 partial reads | the same |
| coast (5 points, every 1 800 s) | H2D 13, D2H 36 | H2D 13, D2H 24; 25 partial reads, 24 copying 20 columns each (480 in all, 46 kB each time) | H2D 27, D2H 39; 25 partial copies, 500 columns |

The D2H left are full host reads and writes that found the device newer: in the smoke 37 of 42
host sync calls (the field-output times, where `W3OUTG` falls back to the Fortran because the case
asks for `WBT` and `SKW`, section 15, the restart, and the returns of `W3WAVE`), in the coast case 24
of 39 (13 host reads, among them the returns of `W3WAVE`, and 26 host writes, the `W3UICE` and
`W3ULEV` updates, `WW_KOKKOS_VA_COUNT` of the diagnostic run); which site made each copy was not
counted ⚠. In the coast case a point-output time now copies 20 columns of 2 304 B (46 kB) where it
copied the whole `VA` (8.50 MB). Nothing of this ran on a GPU ⚠. `bash scripts/ww3-gpu.sh
--ww3-gpu full --gpu --kokkos resident --build-only` compiled the shim and the fork for CUDA; the
gather kernel never ran on a device ⚠. The regtest matrix did not run ⚠. The preset `--kokkos
resident` of `scripts/ww3-gpu.sh` now includes `IOPE`.

## 18. `W3UICE`'s write of `VA` on the device, `ww3-gpu/kokkos-pnt`

A second commit on `ww3-gpu/kokkos-pnt` (section 17): `769c44f0`, pushed to h0ffmann/WW3 on
2026-10-17. Before it, an ice update made `W3WAVE` call `W3KOKKOS_VA_HOST_WRITE`, so `VA` came back
from the device whole and went out again whole at the next kernel. The ABI is `ww_kokkos` 0.9.0 (two
entry points added, `SameMajorVersion`); the fork asks for `0.9`.

- **What `W3UICE` does to `VA`** (`w3updtmd.F90:1806-2052` at the pin). Under `W3_IC0` only, one
  statement form, `VA(:,JSEA) = 0.`: at `:1965`, a sea point whose concentration reaches `FICEN`
  and whose ice bit was 0 (`:1956`), and at `:2001`, an ice point whose concentration falls below
  `FICEN` (`:1987`) and whose `MAPST2` without the ice bit is 0 (`:1992`). It reads no `VA`. Without
  `W3_IC0` it writes no `VA`. That is a per-point operation with no arithmetic, so it is repeated on
  `d_va` as it stands.
- **Which columns.** `MAPICE = MOD(MAPST2,2)` and `MAPST2 - MAPICE` on entry (`:1938-1939`),
  `MAPST2 + MAPICE` on exit (`:2035`), and nothing else in the routine writes `MAPST2`. So the ice
  bit goes 0 → 1 exactly where `:1956` holds and 1 → 0 exactly where `:1987` holds; the two exclude
  each other (`ICEI >= FICEN` against `< FICEN`), and `MAPSF` visits each sea cell once. The
  columns zeroed are the sea points whose bit changed and whose `MAPST2` on exit is odd (`:1965`) or
  0 (`:2001`). Under `W3_SHRD` `JSEA = ISEA` (`NAPROC = 1`, `w3parall.F90:1236`), and `LOCAL` and
  `IAPROC .EQ. ISPROC` hold for every point.
- **The fork** adds the unit file `model/src/w3kokkos_updvmd.F90` (`W3KOKKOS_UPDVMD`).
  `W3KOKKOS_UPDV_SETUP` sets `KOKKOS_UPDV` from `WW_KOKKOS_UPDV`, opt-in (`kOptInOnly`), refused
  outside `SHRD` and under `MPI` or `PDLIB`. At the ice site (`w3wavemd.F90:1386-1399` at
  `769c44f0`) `CALL W3KOKKOS_VA_HOST_WRITE` and `CALL W3UICE ( VA )` become `IF ( KOKKOS_UPDV ) CALL
  W3KOKKOS_UICE ( VA ) ELSE` those two calls. `W3KOKKOS_UICE` keeps a copy of `MAPST2`, calls
  `ww_va_host_update`, calls the Fortran `W3UICE ( VA )` unchanged on the host `VA`, lists the
  columns by the rule above, and calls `ww_va_device_zero_cols`.
- **The entry points** ([`resident_c.hpp`](resident_c.hpp), [`resident_shim.cpp`](resident_shim.cpp)).
  `ww_va_host_update` copies nothing. `ww_va_device_zero_cols` stores `0.0f` in every listed column
  of `d_va` when the state is `DEVICE` or `BOTH`, and does nothing when it is `HOST`; the state is
  left as it was. The argument: `W3UICE` reads no `VA`, so running it on a stale host copy changes
  nothing it computes; with `DEVICE` the host columns it zeroed were stale anyway and the device
  gets the same zeros, so the device stays the newer copy; with `BOTH` both copies get the same zeros
  and stay equal; with `HOST` the host write is the real one and `d_va` is refreshed whole at the
  next kernel. `0.` is `+0.0` in both, so the bits agree.
- **Diagnostic.** Under `WW_KOKKOS_VA_RESIDENT=1`, `ww_va_host_update` is a full host read (the
  two copies equal, state `BOTH`), and after the kernel `ww_va_device_zero_cols` copies `d_va` back
  into a scratch array and compares every column with the host `VA` (`memcmp`); one column that
  differs stops the model (`WW_KOKKOS_ERR_KERNEL`, `EXTCDE`). So a column the host zeroed and the
  list missed, or a wrong kernel, cannot pass silently. The negative control: with the kernel's
  launch skipped, the diagnostic coast run without water levels (below) stopped at the first ice update that zeroed a
  column ("1 columns of VA differ"), 2026-10-17 (v).
- **Why the output alone does not show the kernel.** With the launch skipped and no diagnostic, the
  coast case was still byte-identical, `out_grd.ww3`, `out_pnt.ww3` and hourly restarts included,
  with and without water levels (v, four throwaway runs). With levels on, `W3ULEV`'s host write right
  after `W3UICE` makes the host `VA` the newer copy, so the device is refreshed whole; without them,
  a point that turns to ice becomes inactive (`MAPSTA < 0`, `:1957`) and its stale spectrum seems not
  to reach any output before it is overwritten (not traced ⚠). So the evidence for the
  kernel is the check above, not the byte identity.
- **`W3ULEV` is not covered.** Its writes of `VA` are not a per-point operation of that kind: besides
  zeroing the new dry and new wet points (`w3updtmd.F90:2395`, `:2419`), it re-interpolates the
  spectrum of every point whose wavenumber grid moved by `RDKMIN` or more onto the new grid
  (`:2464-2546`), reading `A(ITH,IK,JSEA)` before it writes, with `A` and `VA` the same array passed
  twice (`CALL W3ULEV ( VA, VA )`) and the depth, `WN`, `CG`, `MAPSTA` and `MAPST2` it updates on the
  host deciding which points. Moving that to `d_va` means running the translation
  (`kokkos/src/ww_kokkos/updt_w3ulev.hpp`, translated, never run) on the device with its inputs,
  a unit of its own; its site keeps `W3KOKKOS_VA_HOST_WRITE`.
- **Counters** (`WW_KOKKOS_VA_COUNT=1`): `VA host updates repeated on the device`, `VA device
  zeroing launches`, `VA device columns zeroed`, `VA zeroing checks run`. **Timer**: `va.zero.cols`.
- **Memory contract**: phase 2. A device list of the zeroed columns per call. No Unified Memory.

The checkpoint on the CPU, 2026-10-17 (the machine of section 17; never `--gpu`): `bash
scripts/ww3-gpu.sh --ww3-gpu smoke|full|coast --cpu --kokkos SRCE,XYP3,KTP3,OUTG,IOPE,UPDV` and `...
smoke ... --kokkos all`, on `switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`, each plain and with
`WW_KOKKOS_VA_RESIDENT=1`: 16 runs, every file cmp-equal as in section 17, `nccmp-tol` PASS, every
`max|d|` 0 (v). Only the coast case updates ice: 13 calls, 8 launches, 91 columns zeroed, and under
the diagnostic 13 checks passed, on both switches. Beside the script, the coast case with water levels
off (`INPUT%FORCING%WATER_LEVELS = 'F'`) and hourly restarts was byte-identical too, restarts
included, plain and with the diagnostic (v).

`VA` traffic, `WW_KOKKOS_VA_COUNT=1`:

| Run | Before (section 17, `IOPE` on) | With `UPDV` | Why |
|---|---|---|---|
| coast | H2D 13, D2H 24; 39 host sync calls | H2D 13, D2H 24; 26 host sync calls | `W3ULEV` runs at the same hours, right after `W3UICE`, and its host write makes the copies that `W3UICE`'s made before |
| coast, `WW_KOKKOS_VA_RESIDENT=1` | H2D 27, D2H 39 | H2D 14, D2H 39 | |
| coast without water levels (diagnostic) | H2D 13, D2H 24; 38 host sync calls | H2D 1, D2H 12; 25 host sync calls | no host write of `VA` left in the step |
| smoke, full | unchanged | unchanged | no ice input |

Nothing of this ran on a GPU ⚠. `bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident
--build-only` compiled the shim and the fork for CUDA; the zeroing kernel never ran on a device ⚠.
The regtest matrix did not run ⚠. The preset `--kokkos resident` of `scripts/ww3-gpu.sh` now
includes `UPDV`.

## 19. Device defaults: `W3XYP3` teams and `W3SRCE` chunks on CUDA

No fork change: two defaults of the shims that now depend on the backend, so that `--ww3-gpu
--gpu --kokkos resident` runs the launch shapes meant for a GPU without a flag, and a CPU build
runs exactly what it ran before.

- **`WW_KOKKOS_XYP3_TEAM` unset** now lets the execution space decide (`xyp3_team<E>()` in
  [`prop3_shim.cpp`](prop3_shim.cpp)): a space that reads host memory (Serial, OpenMP) runs the
  per-component kernel as before; a device (CUDA) runs the team kernel of section 16 with teams of
  `kXyp3DeviceTeam` = 128 threads. The new value `0` forces the per-component kernel on any
  backend; `AUTO` and a positive N are as in section 16. The default is clamped to the backend's
  `team_size_max`, where a size asked for is refused: the bits do not depend on it (section 16).
  Why 128, from `ptxas -v` (below) and the hardware limits, computed and not measured ⚠: the team
  kernel takes 80 registers for `sm_89` and 94 for `sm_90`, so a multiprocessor of 65 536 registers
  holds 25 or 21 warps. Teams of four warps put six (`sm_89`) or five (`sm_90`) teams on one, 768
  on the RTX 4090's 128 multiprocessors and 660 on an H100's 132, so the 576 components of
  `bench_small` (NK = NTH = 24) are all resident at once; teams of eight warps fit three or two per
  multiprocessor (384 or 264 at once) and would need two passes. Smaller teams also wait less at
  the barrier after each split loop. Before, the device ran 576 threads, at most 18 warps, so at
  most 18 of the 4090's multiprocessors had work.
- **`WW_KOKKOS_SRCE_BATCH_MIB` unset** is 1024 MiB on a backend that reads host memory, as
  before, and 128 MiB on a device ([`srce_shim.cpp`](srce_shim.cpp), `kBatchMibDefault`). At about
  56 KB per point (`bench_small`, section 10) 128 MiB holds about 2 400 points, so a step of its
  9 204 points runs in four launches instead of one. The reason is the display watchdog: the RTX
  4090 here also drives the screen, and the driver ends a kernel that runs too long
  (`cudaErrorLaunchTimeout`). The estimate, ⚠ (nothing ran on the GPU, and the watchdog's limit,
  a few seconds on Linux with a display, was not measured here): on the CPU (OpenMP, 32 threads)
  `srce.batch.kernel` took 18.6 s for 25 one-launch steps, 0.74 s per launch of 9 204 points, and
  17.3 s for 100 launches at 128 MiB, 0.17 s per launch of about 2 300. A device launch of 2 300
  points stays under 1 s as long as the GPU runs this kernel at no less than about a sixth of the
  speed of the 32 CPU threads; at 1024 MiB it would need three quarters of it. The price: the
  input shadows of section 14 are kept only when a step runs in one launch, so on the device the
  per-step inputs (340 bytes per point at NK = 24: `BJSEA`, `BWN1`, `BCG1`, `BPIN`, `BPSC`,
  `BWC`, `BTB`, `BTI`; 3.1 MB per step) are copied at every step again.
  A machine whose GPU drives no display can set `WW_KOKKOS_SRCE_BATCH_MIB=1024`.
- **Chunks do not change the bits**: each team integrates its own point, and a chunk only
  changes which launch it is in (section 10, `WW_KOKKOS_SRCE_BATCH_MIB=64`, and below).

**`ptxas -v`, CUDA 12.9 `nvcc_wrapper`**, the shims compiled alone in a scratch build of the
`cuda-release` preset with `-Xptxas -v` (`sm_89`), and `prop3_shim.cpp` again with `-arch=sm_90`
against the same ADA89 Kokkos headers (so its `sm_90` numbers are indicative ⚠), on `main` at
`d8905a3b` with these changes (after WFIP-0006's device libm; the resident-path numbers are those
of the tree before it; the `OUTG` rows, and the whole `sm_89` column, measured again on `main` at
`b38b5fc8` with these changes, after `W3OUTG` and its skewness moved to the device libm, where
only those two rows changed); nothing ran:

| Kernel (resident path) | Launch shape on `bench_small` | `sm_89` | `sm_90` |
|---|---|---|---|
| `ww_xyp3_team` (now the device default) | 576 teams of 128 | 80 registers, 0 spill, 1 barrier, 16 B shared | 94 registers, 0 spill |
| `ww_xyp3_batch` (`WW_KOKKOS_XYP3_TEAM=0`) | 576 threads, one per component | 96 registers, 0 spill | 96 registers, 0 spill |
| `ww_xyp3_scatter` | 9 204 threads, one per sea point, 576 stores each | 34 registers | 28 registers |
| `ww_ktp3_batch` | 9 204 threads, one per point, `NTIMES` sweeps of 576 bins each | 80 registers, 0 spill | 80 registers, 0 spill |
| `ww::iogo::w3outg` (`OUTG`) | 9 204 threads, one per point, at output times | 161 registers, 576 B stack, 0 spill, with the device libm of `b38b5fc8` (96 registers, 112 B stack before it) | not compiled |
| `w3outg` init, part, skewness kernels | one per sea point or `NSEALM` | 30 to 60 registers; skewness 66, 288 B stack (44 before `b38b5fc8`) | not compiled |
| `ww_va.gather_cols` (`IOPE`) | one thread per element of the columns read | 20 registers | not compiled |
| `ww_va.zero_cols` (`UPDV`) | one thread per element zeroed | 20 registers | not compiled |
| `W3GATH` / `W3SCAT` / `W3MAP3` / `W3MAPT` / `W3DZXY` / `W3UTRN` / `W3UWND` (`wave_support_shim.cpp`, not in `resident`) | one thread per cell or sea point | 11 to 56 registers, at most 344 B stack (`DISTAB`, with the device libm of WFIP-0006; the same at `b38b5fc8`) | not compiled |

**What else has the shape `W3XYP3` had** (one thread for a large item, a launch smaller than the
device), checked for the resident path and not changed, because none is bit-neutral by
construction or none matters:

- **`ww_ktp3_batch`**: 9 204 threads of 80 registers are 288 warps, about 2.3 per multiprocessor
  of the 4090, and each thread sweeps its point's 576 bins `NTIMES` times; on the CPU it took
  0.66 s of the 21.6 s full run. It is the next launch to give a team: the bins of a point would
  go to the team's threads, but `CFLTHMAX` and `CFLKMAX` are in-order maxima whose NaN behaviour
  depends on the order (section 16), so splitting it is an owner's decision and its own PR.
- **`ww::iogo::w3outg`**: one thread per point with in-order sums over the spectrum; 13 calls,
  0.24 s on the CPU. The same reductions keep it per point.
- **`ww_xyp3_scatter`**: 9 204 threads, each storing its point's 576 components into its own
  column of `VA`. A pure copy, so an element-per-thread version would be bit-neutral; its stores
  are 576 floats apart across a warp, but the whole scatter moves 21 MB per call, about 0.1 ms of
  the 4090's bandwidth even at a tenth of it, computed ⚠, so it was left as written.
- **`ww_va.gather_cols`, `ww_va.zero_cols`, `W3GATH`, `W3SCAT`** already run one thread per
  element.

The checkpoint on the CPU, 2026-10-17 (OpenMP backend, 32 threads, Kokkos 5.2.2, gfortran/g++
16.2.0, Intel Core i9-14900, shared with other sessions, load average 11 to 56; never `--gpu`):
`<env> bash scripts/ww3-gpu.sh --ww3-gpu <case> --cpu --kokkos resident --timers`, on
`switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`, for full and coast with `<env>` empty,
`WW_KOKKOS_XYP3_TEAM=0`, `AUTO`, 4 and 16, and `WW_KOKKOS_SRCE_BATCH_MIB=128` (the device default,
forced on the CPU), and smoke with `<env>` empty.
Run on `main` at `d8905a3b` with these changes, 26 runs. In every one, every file the run wrote
(`out_grd.ww3` and the NetCDF field; in coast also `out_pnt.ww3`; in smoke also `out_pnt.ww3` and
`restart.ww3`) is `cmp`-equal to the Fortran of the same executable, `out_grd.ww3` (and
`out_pnt.ww3`, `restart.ww3`) `cmp`-equal to `--ww3`, whose NetCDF differs only in the
`WAVEWATCH_III_switches` attribute (`diff` of the two `ncdump`, full and coast), and `nccmp-tol`
PASS both (v). `W3XYP3 team launches` read 0 with the flag unset and with `0`, 24 (full) or 72
(coast) with `AUTO`, 4 and 16; `W3SRCE batched launches` 25 (full) and 72 (coast) at the host
default, 100 and 144 at 128 MiB. The same set on `switch_lab_ompg` also passed before the rebase
onto `d8905a3b`, at `4fbffbe8`.

Mutations, on `switch_lab_ompg`, full, at `4fbffbe8` with these changes (scratch edits, restored,
never committed; the files they touch did not change in the rebase), each confirming that the CPU
parity run under the forced setting sees a broken kernel:

| Mutation | Run | Result |
|---|---|---|
| the team kernel's gather skips the last sea point (`lp.par(1, insea, ...)`) | `WW_KOKKOS_XYP3_TEAM=AUTO` | FAIL: `out_grd.ww3` and NetCDF differ, `nccmp-tol` FAIL on `hs`, `max|d|` 0.571 m |
| the same | flag unset (the per-component kernel) | byte-identical, PASS: the CPU default does not take the team kernel |
| every `TeamLoops::par` loop skips its last index | `WW_KOKKOS_XYP3_TEAM=AUTO` | FAIL, `hs` `max|d|` 1.742 m |
| every chunk after the first of `ww_srce_batch(_va)` one point short | `WW_KOKKOS_SRCE_BATCH_MIB=128` | FAIL, `hs` `max|d|` 0.556 m |
| the same | host default, one launch per step | byte-identical, PASS |

Also run: `bash scripts/ww3-gpu.sh --self-test`, `just kokkos-test serial-debug` and
`openmp-release`, `python3 scripts/port_score.py --write` then `--check`, and `bash
scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident --build-only` (compiled the shims and the
fork for CUDA, `sm_89`). Nothing ran on a GPU ⚠: whether the team kernel and the chunks give the
CPU's bits there, and how long a launch takes, are not known. The regtest matrix did not run ⚠.

## 21. Diagnostics from the mutation audit (no fork change)

No fork change and no ABI change: [`resident_shim.cpp`](resident_shim.cpp),
[`outg_shim.cpp`](outg_shim.cpp), `scripts/ww3-gpu.sh` and the coast fixture. Each piece of
sections 12 to 18 was broken on purpose in a scratch tree, and the checks that let a broken piece
pass were tightened ([the audit](../../../docs/log/2026-10-17-mutation-audit.md), 2026-10-17, with
the table of mutants). Every flag below is read once, ignores `WW_KOKKOS_ALL`, is off by default,
and leaves a correct run's bits unchanged (v: coast, smoke and full byte-identical with each).

- **`WW_KOKKOS_VA_CHECK=1`**: every sync call (`ww_va_host_read`, `_write`, `_read_cols`,
  `_update`, a kernel's `device_va`) first checks what the state claims, by `memcmp` against a host
  snapshot taken whenever the two copies were made equal and updated by the declared column
  writes (the partial read, the zeroed columns): unless the state is `HOST`, the host `VA` must
  still hold those bits, so a host write with no `W3KOKKOS_VA_HOST_WRITE` before it stops the model
  at the next call; with `BOTH`, `d_va` must too, so a kernel that wrote `d_va` without
  `device_written` stops it. Unlike `WW_KOKKOS_VA_RESIDENT=1` it adds no copy, which would hide a
  missing mark. Counter: `VA consistency checks run`.
- **`WW_KOKKOS_VA_POISON=1`**: after every `device_written` the host `VA`, stale from then on, is
  filled with a quiet NaN (`0x7FC0BAD0`), so a host read that lacks its sync, or a column the
  partial read misses, puts NaN into the output. The next full sync copies `d_va` back over it.
  Counter: `VA host copies poisoned`.
- **`WW_KOKKOS_OUTG_STAGED=1`**: on a host backend `ww_outg_va` takes the device's path: inputs
  copied into the shim's buffers, fields computed into buffers, and only the fields the selection
  of section 15 wants copied back into WW3's arrays, so a field the selection misses keeps its old
  bits and shows in `out_grd.ww3`.
- **`scripts/ww3-gpu.sh`**: every model file (`out_grd.*`, `out_pnt.*`, `restart*.ww3`, every
  NetCDF) is compared byte for byte with the Fortran run's, the binary ones with `--ww3`'s; on the
  CPU a difference fails the run, where before `nccmp-tol` judged one NetCDF within its tolerances.
  A run whose `VA` is still newer on the device at finalize fails. For `--kokkos resident` on the
  coast case the calls per part must match `kokkos/tests/fixtures/coast/counts_resident.txt`, so a
  part that falls back to the Fortran without a word fails.
- **The coast case** asks for every field the Kokkos `W3OUTG` computes and writes restarts at 05:30
  and 11:30, inside a `W3WAVE` call.

Not provable on the CPU by any of these: the device copies themselves, the prop3 input cache
(section 13; a staged diagnostic in `prop3_shim.cpp` would do what `WW_KOKKOS_SRCE_STAGED` does),
and every CUDA bit ⚠. The regtest matrix did not run ⚠.

## 22. `W3KOKKOS_SRCE_SETUP` on a switch without `LN1`, `BT1` or `DB1`, `ww3-gpu/kokkos-pnt`

A third commit on `ww3-gpu/kokkos-pnt`: `80644ba9`, pushed to h0ffmann/WW3 on 2026-10-18. No ABI
change. `W3GDATMD` declares `SLNC1`, `FSPM`, `FSHF` only under `W3_LN1`, `SBTC1` under `W3_BT1`
and `SDBC1`, `SDBC2`, `FDONLY` under `W3_DB1` (`model/src/w3gdatmd.F90:874-876`, `:975`,
`:993-994` at the pin). `W3KOKKOS_SRCE_SETUP` read all seven without a guard, so every `ST4`+`NL1`
switch with the KOKKOS switch and without one of those three words stopped in `w3kokkosmd.F90`
before linking (v, the `WW3GPU_KOKKOS=resident` matrix of 2026-10-18: `ww3_tp2.6` `ST4` (`LN0`),
`ww3_ts2`, `ww3_ts3` and `mww3_test_05` (`BT0 DB0`)). Each `USE` and `RPAR`/`IPAR` store now sits
under its switch; without it the packet keeps 0 and the switch gate (`IS_LAB_SWITCH`) leaves
`W3SRCE` to the Fortran, as before. With the guard, `ww3_tp2.6` `ST4` and `ww3_ts2` `ST4_PR3_UQ`
build, and their Kokkos runs stop at `WW_SRCE_INIT` code 5 because the preset asks for `SRCE`
on a switch it does not translate (v, same matrix, entries 1 and 7).

## 23. The boundary and track outputs read only their columns, `ww3-gpu/u8-out-readers`

Two commits on `80644ba9` (section 22), pushed to h0ffmann/WW3 on 2026-10-18: `31a09522` (a
compile guard) and `791e8c34`. No ABI change: both use `ww_va_host_read_cols` of section 17, and
the fork still asks for `0.8`. They apply the resident-spectrum rule of `AGENTS.md` to the host
readers of `VA` in the output block of `W3WAVE` under `SHRD` (`w3wavemd.F90:2930-2990` at
`791e8c34`). The partial reads stay behind `WW_KOKKOS_IOPE=1`, opt-in, the flag of section 17.

| Reader | What it reads of `VA` (at `80644ba9`) | Under `WW_KOKKOS_IOPE=1` now |
|---|---|---|
| `W3IOBC ('WRITE')`, boundary output | `VA(IS,ISEA)`, `ISEA = ISBPO(ISOUT)`, `ISOUT = 1..NBO2(NFBPO)`, in the `W3_SHRD` gather (`w3iobcmd.F90:660-668`); `ww3_grid` sets `ISBPO` to sea points only (`w3gridmd.F90:6005-6023`), read with the model definition | `W3KOKKOS_IOBC_READ` (`w3kokkos_pntmd.F90:90`) passes `ISBPO(1:NBO2(NFBPO))` in place of the full read (`w3wavemd.F90:2953-2958` at `791e8c34`) |
| `W3IOTR`, track output | `A(ITH,IK,JSEA)`, `JSEA = ISEA = MAPFS(IY,IX)`, for every point with `MASK1(IY,IX)` and `TSTSTR = 'SEA'`, which is `MAPSTA(IY,IX) > 0` (`w3iotrmd.F90:728-763`, `:774`, `:808-819`); its section 3 sets `MASK1` from the track file, inside the call (`:518-697`) | `W3WAVE` skips its read (`w3wavemd.F90:2936`); `W3IOTR` calls `W3KOKKOS_IOTR_READ` (`w3kokkos_pntmd.F90:107`) after section 3 and before section 4 (`w3iotrmd.F90:718-723` at `791e8c34`), with exactly those columns |
| `W3IOSF`, partitioned spectra | every `JSEA = 1..NSEAL` with `MAPSTA >= 0` (`w3iosfmd.F90:243-265`): every sea point that is not ice, dry or excluded | unchanged, the full read, which the rule allows for a reader of the whole field. A column read of the `MAPSTA >= 0` points would save only the ice and dry points; partitioning on the device belongs to the `W3CPRT` unit |
| The OASIS coupling (`W3_OASIS`, `J = 7`) | none: `SND_FIELDS_TO_ATMOS`, `_OCEAN` and `_ICE` read `W3ADATMD` fields and `ICEF`, and no `VA` (`w3agcmmd.F90:88-220`, `w3ogcmmd.F90:62-404`, `w3igcmmd.F90:76-167`, v by `grep`) | the sync is removed (`w3wavemd.F90:2978`); not compiled here, as there is no OASIS library ⚠ |

- **Why inside `W3IOTR`.** The columns follow the track points that `W3IOTR` reads from
  `track_i.ww3` in its section 3, with `BACKSPACE` and the `MASK2` of the previous call, so a list
  made before the call would repeat that reading. The hunk is one `CALL` under
  `W3_KOKKOS && W3_SHRD && !W3_MPI`; `W3KOKKOS_IOTR_READ` reads `MASK1` from `W3ODATMD`. It writes
  the module `VA` while `W3IOTR` holds the same array as its `INTENT(IN)` dummy `A`. The `-O3`
  code loads nothing of `A` before the call: in `gfortran -O3 -fdump-tree-optimized` of
  `w3iotrmd.F90` with the entry's switch the call is at line 4477 of the dump and every use of
  `a_1999(D)` is at lines 4847-4976 (v, 2026-10-18). The Fortran standard does not allow the actual
  argument of a dummy to change through another name during the call, so this rests on the code
  the compiler makes, not on the standard ⚠.
- **The resident state** stays `DEVICE` after each partial read, as in section 17: the next full
  host read or write still copies the whole array, and the next kernel reads `d_va`.
- **The shim** (`resident_shim.cpp`) keeps up to four column lists per grid with their buffers,
  the most recent first, in place of one: the point and the boundary outputs alternate at an output
  time, and a single list would allocate the device buffers again at every call.
- **`31a09522`**: `W3KOKKOS_MAP3` and `W3KOKKOS_MAPT` named `W3ADATMD` variables that exist only
  under `W3_PR3` (`w3adatmd.F90:520-525`, `:648-653`), so no KOKKOS switch without `PR3` compiled
  (`ww3_tr1`, `SHRD RTD PR1`: "Symbol 'nmx0' referenced at (1) not found in module 'w3adatmd'",
  v). The `USE` and the stores now sit under `W3KOKKOS_HAS_PR3`, as the calls did; without `PR3`
  both parts stay off.
- **Without `KOKKOS`**, `gfortran -E -P` of `w3wavemd.F90` and `w3iotrmd.F90` with the entry's
  switch is identical to the output from `WW3-noaa/`, blank lines dropped, apart from the include
  path in one comment (v, 2026-10-18). `w3iotrmd.F90` is a fork file of the pnt unit (its `FILE`
  line in `parts.d/pnt.txt`).

The checkpoint on the CPU, 2026-10-18 (OpenMP backend, Kokkos 5.2.2, gfortran 16.2.0, Intel Core
i9-14900 shared with other sessions, `OMP_NUM_THREADS=4`; never `--gpu`). Each entry ran with
`WW3GPU_LAB_SWITCH=ON`, `WW3GPU_STRICT=ON`, `WW_KOKKOS_VA_POISON=1` and `WW_KOKKOS_VA_COUNT=1`,
before (`80644ba9`; for `ww3_tr1`, plus `31a09522`) and after (`791e8c34`).
[The log](../../../docs/log/2026-10-18-resident-output-readers.md) has the commands.

| Entry, parts | Output it writes | test.comp, after | `VA` before | `VA` after |
|---|---|---|---|---|
| `ww3_tp2.2 -s PR3_UQ`, `resident` | track every 4 h (`track_o.ww3`, `ww3.202206_trck.nc`) | 0/24 files different (v) | D2H 4 (60.07 MB); host reads 8; partial reads 3, 2 of them copying 24 columns | D2H 4 (60.07 MB); host reads 4; partial reads 7, 5 copying 29 columns: the 3 track times after the first copy 5 columns, 4.4 kB |
| `ww3_tr1 -i input_std`, `SRCE,OUTG,IOPE,UPDV` | boundary every 1 h (`nest1.ww3` to `nest3.ww3`) | 0/23 files different (v) | D2H 7 (0.57 MB); host reads 8, writes 6; partial reads 7, 28 columns | D2H 7 (0.57 MB); host reads 1, writes 6; partial reads 14, 84 columns: 8 at each of the 7 boundary times, 70 kB in all |

The host reads before these outputs are gone, and with the host copy poisoned the outputs still
have the Fortran's bits, so no column a reader needs was missed. The whole-spectrum copies per run
did not drop in these two entries. In `ww3_tp2.2` the track times are the wind times, where
`W3WAVE` returns and its exit read (`w3wavemd.F90:3160` at `791e8c34`) copies `VA` anyway; in
`ww3_tr1` (`PR1`) the host write before the Fortran propagation copies it at every step. What each
reader copied is now its columns, and the count per run drops only where no other full sync
follows in the same step. `bash scripts/ww3-gpu.sh --ww3-gpu smoke|full --cpu --kokkos resident`:
byte-identical to the Fortran and to `--ww3`, `nccmp-tol` PASS, D2H 37 (smoke) and 1 (full), as in
section 17 (v). The matrix driver drops the empty options of a first-kept entry
(`regtests/cmake/ww3gpu_common.cmake:337-343`: a `list(APPEND)` of an empty string onto an empty
list adds nothing), so `ww3_tr1` cannot be configured there; it ran through
`regtests/cmake/run_entry.cmake` directly, with a copy of the configured `ww3gpu_config.cmake`
whose parts are `SRCE,OUTG,IOPE,UPDV` ⚠. Nothing ran on a GPU ⚠.

## 24. `WW_KOKKOS_STRICT`: a fallback to the Fortran stops or is counted, `ww3-gpu/u2-strict-preset`

Fork commit `f0cbe70d` on `ww3-gpu/u2-strict-preset`, branched from `80644ba9` and pushed to
h0ffmann/WW3 on 2026-10-18; not at the pin until the integrator merges it into
`ww3-gpu/kokkos-pnt`. Gap 6 of
[`PRODUCTION_READINESS_202610.md`](../../../docs/PRODUCTION_READINESS_202610.md). Three ABI
entries in [`part_flags.cpp`](part_flags.cpp) (`ww_kokkos_strict`, `ww_kokkos_fallback_slot`,
`ww_kokkos_fallback_add`); `w3kokkosmd.F90` +85 lines, `w3initmd.F90` +6 -1, `w3wavemd.F90` 2
changed.

Four places ran the Fortran for a part asked on Kokkos and said so only in the parts line:
`KOKKOS_PART` under `WW_KOKKOS_ALL=1` on a switch file a translation does not cover (`GATH`,
`SCAT`, `UWND`, `UTRN`, `MAP3`, `MAPT`; the opt-in parts are never on under the master),
`W3KOKKOS_SRCE_SETUP` on a switch file that is not the lab switch's source terms, `W3INIT`
without `ST4` and `NL1`, and the per-step `W3SRCE` gate (`IQTPE <= 0`, Liu's ice dispersion,
`TAUWBUG = 0` with `IT > 0`). Each now calls `KOKKOS_FALLBACK ( PART, REASON )`
(`w3kokkosmd.F90:1257-1269` at `237ca0de`):

- **`WW_KOKKOS_STRICT=1`**: `*** WAVEWATCH III ERROR IN W3KOKKOSMD : WW_KOKKOS_STRICT=1 AND PART
  <PART> WOULD RUN ON THE FORTRAN: <REASON>` on `NDSE`, then `EXTCDE(1)`, on every rank.
- **Unset (the default)**: the Fortran, as before, and the shim counts the call. At exit each
  process prints one line to `stderr`, as the `*_COUNT` lines do:
  `ww_kokkos: calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead): SRCE=0`. A set-up
  fallback counts 1 per `W3INIT`; `SRCE` counts the time steps whose batch ran the Fortran. The
  line appears only when some part could fall back.

**The hot path.** The per-step gate is a new function, `W3KOKKOS_SRCE_STEP ( IT )`
(`w3kokkosmd.F90:1912-1917`), which `W3WAVE` calls once per step in place of
`W3KOKKOS_SRCE_ON` before the batch (`w3wavemd.F90:2407`), outside the `!$OMP PARALLEL` loop over
sea points. It returns `W3KOKKOS_SRCE_ON ( IT )` and adds one test of two `LOGICAL`s; only on
the fallback branch does it call `SRCE_FALLBACK`, which under the default is one call and one
increment of a plain counter (`ww_kokkos_fallback_add`; no lock, the caller is serial). The
counter's slot is made once in `W3KOKKOS_SRCE_SETUP`. `WW_KOKKOS_STRICT` is read once by the shim
(a function-local static) and cached in `KOKKOS_STRICT` by `W3KOKKOS_SETUP`. The per-point
`W3KOKKOS_SRCE_ON` of the Fortran loop (`w3wavemd.F90:2485`) is unchanged: that loop runs only
when the step's gate was false, so counting the step is complete without a counter in the loop.

**The matrix.** `ww3gpu_kokkos_env` (`regtests/cmake/ww3gpu_common.cmake`) sets
`WW_KOKKOS_STRICT=1` in the Kokkos run of every pair and unsets it in the Fortran runs (`F`,
`G`), so a pair can no longer compare the Fortran with itself unseen: an entry whose Kokkos run
would fall back now fails at run time instead. This changes how a comparison is invoked and is
reviewed by a person before it merges (AGENTS.md, "Never cheat a regtest tool").

**What it was checked on** (2026-10-18, i9-14900, `OMP_NUM_THREADS=8`, the machine shared with
three other jobs, so every wall time is one run and ⚠). The runs re-ran `ww3_grid`, `ww3_strt`,
`ww3_shel` and `ww3_ounf` in a copy of the smoke case's Kokkos directory with every `WW_KOKKOS_*`
unset and only the variables named:

| Run | Result |
|---|---|
| `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident`, before (`80644ba9`) and after | every model file byte-identical to the Fortran's, and the after run's four files identical (`cmp`) to the before run's (v); `ww3_shel` with Kokkos 3.96 s before, 3.65 s and 10.93 s after (load average 17 during the second) ⚠ |
| the same smoke, `WW_KOKKOS_ALL=1` | `SRCE=0` in the line at exit (v) |
| smoke with `TAUWBUG = 0` in `&SIN4`, `WW_KOKKOS_ALL=1` | `ww_kokkos: calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead): SRCE=2160` (v) |
| the same, `WW_KOKKOS_STRICT=1` | stops: `... WW_KOKKOS_STRICT=1 AND PART SRCE WOULD RUN ON THE FORTRAN: TAUWBUG = 0 (SINTAILPAR(4) <= 0.5) WITH IT > 0, WHICH NO TRANSLATION CAN REPRODUCE` (v) |
| `switch_lab_ompg` with `WNT2` for `WNT1` (a scratch copy, not committed), `--kokkos all` | `UWND=1 SRCE=0` at exit; the pair byte-identical (v) |
| the same build, `WW_KOKKOS_ALL=1 WW_KOKKOS_STRICT=1` | stops: `... PART UWND WOULD RUN ON THE FORTRAN: WW_KOKKOS_ALL=1 ASKS FOR IT, BUT ITS KOKKOS TRANSLATION DOES NOT COVER THIS SWITCH FILE` (v) |

## 25. `WW_KOKKOS_PRESET`: the resident part list in one table, `ww3-gpu/u2-strict-preset`

Fork commit `237ca0de` on `ww3-gpu/u2-strict-preset` (the child of `f0cbe70d`); not at the pin
until the integrator merges it. Gap 7 of
[`PRODUCTION_READINESS_202610.md`](../../../docs/PRODUCTION_READINESS_202610.md), smallest form.
One ABI entry, `ww_kokkos_preset_status`; `w3kokkosmd.F90` +24 -1 lines.

[`presets.txt`](presets.txt) holds one line per preset, `PRESET <name> <PART,...>`; today only
`PRESET resident SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`, the list that was a line of
`scripts/ww3-gpu.sh`. Three readers, one file:

- **The shim.** `CMakeLists.txt` here turns the file into `ww_kokkos_presets.inc` at configure
  time, and `part_flags.cpp` includes it. `ww_kokkos_flag` reads `WW_KOKKOS_PRESET` once: a set
  `WW_KOKKOS_<PART>` still wins, then a part of the preset is on as if set to `1` (so a part the
  switch file does not cover stops the model, as under `--kokkos resident`), then the opt-in
  list and `WW_KOKKOS_ALL` decide as before.
- **`scripts/ww3-gpu.sh`**: `kokkos_preset` reads the `PRESET` lines, so `--kokkos resident`
  sets the same six `WW_KOKKOS_<PART>=1`. Both runs of a pair unset `WW_KOKKOS_PRESET`, and the
  Fortran run unsets `WW_KOKKOS_STRICT`.
- **The matrix**: `ww3gpu_kokkos_preset` reads it for `WW3GPU_KOKKOS=<name>`; `selftest.cmake`
  checks a fixture table and that the real `resident` names only parts of `parts.d`.

`W3KOKKOS_SETUP` (`w3kokkosmd.F90:1182-1188` at `237ca0de`) stops on a name the file lacks:
`*** WAVEWATCH III ERROR IN W3KOKKOSMD : WW_KOKKOS_PRESET=<name> IS NOT A PRESET OF
kokkos/src/fortran_iface/presets.txt`. `W3KOKKOS_REPORT` writes `  Kokkos preset
(WW_KOKKOS_PRESET): resident` on the line before the parts line, which stays the effective set,
once per `W3INIT`. No namelist: the next step of gap 7.

Checked the same way as section 24 (2026-10-18): `WW_KOKKOS_PRESET=resident` alone gives the
parts line of `--kokkos resident` character for character and the same four model files (`cmp`,
v); `WW_KOKKOS_PRESET=bogus` stops with the line above (v).
## 26. `ww3_multi` and the resident `VA`: column syncs, no read at `W3WAVE`'s return, `ww3-gpu/u1-multi-va`

Five commits on `80644ba9` (section 22), pushed to h0ffmann/WW3 on 2026-10-18: `3bbd1557`
(`W3KOKKOS_RESIDENTMD` and the ABI), `bb5c516e` (`wminiomd.F90`), `f4df761a` (`wmwavemd.F90`),
`5ef94512` (`w3wavemd.F90`) and `f330dc53` (the remark of gap 13 of
[`PRODUCTION_READINESS_202610.md`](../../../docs/PRODUCTION_READINESS_202610.md)). Unit U1 + U7
of [the production single-node plan](../../../docs/compound/plans/2026-10-18-002-production-single-node-plan.md),
under the resident-spectrum rule of `AGENTS.md`. The ABI is `ww_kokkos` 0.10.0 (one entry point
added, `ww_va_host_wrote_cols`); the fork asks for `0.10`.

**The defect.** `ww3_multi` reads and writes the `VA` of a grid on the host between two `W3WAVE`
calls of that grid. Before this change `W3WAVE` ended with a whole-array read
(`w3wavemd.F90:3151-3153` at `80644ba9`), which left both copies "equal"; the writes below then
changed the host copy with no sync, and the next kernel on the device copy (`W3XYP3`, `W3KTP3` or
the batched `W3SRCE`) read it without the nesting update. `WW_KOKKOS_VA_CHECK=1` stops such a run;
without it the outputs differ (the table below).

**Every host use of `VA` outside `W3WAVE`** in the drivers at `80644ba9`, read with
`grep -n "VA" ww3_shel.F90 wm*.F90` and every routine those files call that takes `VA` (v,
2026-10-18):

| Site (line at `80644ba9`) | Use | Under `W3_KOKKOS` now |
|---|---|---|
| `WMIOBS`, `wminiomd.F90:374` | reads `VA(:,JSEA)` at the boundary points it sends, `JSEA` from `NBI2S(IOFF+I,2)` (`SHRD`) or `INIT_GET_JSEA_ISPROC` (`DIST`, local points only) | `W3KOKKOS_VA_READ_COLS` with that list, per receiving grid (`wminiomd.F90:365-384` at `f330dc53`) |
| `WMIOHS`, `:1636`, `:1640` | reads `VA(:,JSEA)`, `JSEA = HGSTGE(J,IMOD)%ISEND(I,1)`, `I = 1..NR` | `W3KOKKOS_VA_READ_COLS` with `ISEND(1:NR,1)` (`:1651-1659`) |
| `WMIOHG`, `:2264` | writes every element of `VA(:,JSEA)`, `JSEA = HGSTGE(IMOD,J)%LJSEA(IS)`, `IS = 1..NTOT`, reads none | `W3KOKKOS_VA_WRITE_COLS` with `LJSEA(1:NTOT)` after the loop (`:2318-2327`) |
| `WMIOES`, `:2726`, `:2732`, `:2736` | reads `VA(:,JSEA)`, `JSEA = EQSTGE(J,IMOD)%SJS(I)`, `I = 1..NR` | `W3KOKKOS_VA_READ_COLS` with `SJS(1:NR)` (`:2772-2780`) |
| `WMIOEG`, `:3261`, `:3278`, `:3326`, `:3330` | reads and writes `VA(:,JSEA)`: `JSEA = EQSTGE(IMOD,IMOD)%JSEA(I)` (2.a) and `EQSTGE(IMOD,J)%JSEA(I)` for every other grid with `NREC > 0` (2.b, SMC included) | the union of those lists, read before 2.a and stored after 2.b (`:3341-3363`, `:3457-3463`) |
| `WMWAVE`, `wmwavemd.F90:1367` | `W3IOPE ( VA )`, the unified point output | as before `W3WAVE`'s own `W3IOPE`: `W3KOKKOS_IOPE_READ` under `WW_KOKKOS_IOPE=1`, otherwise `W3KOKKOS_VA_HOST_READ`; under `MPI` a host read before the `W3MPIP` requests start (`wmwavemd.F90:1365-1370`, `:1379-1387`) |
| `ww3_shel.F90` | none: `VA` is named only in commented-out lines (`:1950-2003`) | nothing |
| `WMINIT` (`wminitmd.F90:2993-3006`) | calls `WMIOBS` before any kernel ran | covered by the `WMIOBS` row |

`W3IOBC`, `W3IORS`, `W3IOSF`, `W3IOTR` and `W3OUTG` read `VA` only when `W3WAVE` calls them,
and the sites of sections 12, 15, 17 and 23 sync them. `wmupdtmd.F90:1407-1844` names a local
scalar `VA`, not the spectrum. `WMIOBG` writes `BBPI0`/`BBPIN`, not `VA`. `PDLIB_exchange2Dreal_zero
( VA )` after `WMIOHG` and `WMIOEG` runs only under `PDLIB`, where no resident part is allowed
(sections 13, 15, 17), so the state stays `HOST` there.

- **`W3KOKKOS_VA_READ_COLS ( N, COLS )`**, `W3KOKKOS_VA_WRITE_COLS ( N, COLS )`
  (`w3kokkos_residentmd.F90:86-132` at `f330dc53`): `COLS` are `JSEA`, `0..NSEALM`, of the
  current grid (`IGRID`, `VA` of `W3SETG`/`W3SETW`); `N < 1` does nothing; both stop the model
  inside an OpenMP parallel region, as every sync. The read is `ww_va_host_read_cols` (section 17).
- **`ww_va_host_wrote_cols`** (`resident_shim.cpp`, declared in `resident_c.hpp`): with the state
  `DEVICE` or `BOTH`, the listed host columns are packed into the list's host buffer, copied once
  to the device buffer and scattered into `d_va` by one `parallel_for` (`ww_va.scatter_cols`); with
  `HOST`, nothing. The state is not changed. It shares the column lists of the partial read
  (section 23), now up to eight per grid (`kMaxColSets`): `ww3_multi` uses one list per
  neighbouring grid and direction at every exchange. Bits move and nothing is computed. Under
  `WW_KOKKOS_VA_CHECK=1` the listed columns are taken as written and every other column must still
  hold the bits of the last sync; under `WW_KOKKOS_VA_RESIDENT=1` `d_va` is then copied back
  whole, so a column written and not listed shows in the output.
- **`W3WAVE`'s return** no longer reads `VA` back (section 12's `:3083` row, `:3151-3153` at
  `80644ba9`). A run of `ww3_shel` may end with the device copy newer; `resident_shim.cpp`
  still says so at finalize, and `scripts/ww3-gpu.sh` now prints that line instead of failing on
  it (the check of mutant A2 in section 21 guarded the read this change removes). The check that
  a host reader missed its sync is `WW_KOKKOS_VA_POISON=1`.
- **Why `wminiomd.F90` and `wmwavemd.F90` join the fork's diff.** The plan's first choice (B1)
  marked the host newer at `W3WAVE`'s return, inside the files the fork already changed; the owner
  rejected it on 2026-10-18, because on a GPU it costs a whole host-to-device copy of `VA` per
  grid per call. The syncs belong where `ww3_multi` reads and writes, and they move only the
  columns exchanged. Both files are `FILE` lines of `parts.d/resident.txt`, so
  `scripts/ww3-gpu.sh --ww3` still accepts the fork.
- **Without `KOKKOS`**, `gfortran -E -P` of `w3wavemd.F90`, `wminiomd.F90` and `wmwavemd.F90` with
  `switches/switch_lab_shrd` and with `mww3_test_08`'s `switch_ST4_PR3_UQ_MPI` is identical to
  `WW3-noaa/`'s, blank lines and the include path in one comment aside (v, 2026-10-18). With
  `KOKKOS` and that MPI switch, `ww3_multi` builds (v, `cmake -S <fork> -DSWITCH=<switch + KOKKOS>
  -DWW_KOKKOS=ON`, `cmake --build ... --target ww3_multi`); it was not run (ADR-0017).

The checkpoint on the CPU, 2026-10-18 (OpenMP backend, gfortran 16.2.0, Intel Core i9-14900
shared with other sessions, 2 matrix jobs of 4 threads; never `--gpu`), with
`WW3GPU_LAB_SWITCH=ON` and `WW3GPU_STRICT=ON`, before (`80644ba9`) and after (`f330dc53`'s tree).
[The log](../../../docs/log/2026-10-18-ww3-multi-resident-va.md) has the commands; the summaries
are [`ww3gpu_matrix_multi_va_2026-10-18.tsv`](../../../docs/data/ww3gpu_matrix_multi_va_2026-10-18.tsv).

| Entry, parts | Before, `VA_CHECK=1` | Before, plain: test.comp | After, poisoned and plain: test.comp | Whole `VA` D2H, before → after | Columns after (read; stored) |
|---|---|---|---|---|---|
| 33 `mww3_test_01` `PR3_UQ`, `resident` | runs | 0/22 | 0/22 | 24 (24.13 MB) → 12 (12.07 MB) | 1 440 (3.46 MB); 0 |
| 34 `mww3_test_02` `PR3_UQ` `grdset_a`, `resident` | stops: 2 columns of grid 1 changed | 5/23, `ww3_self` 0/23 | 0/23 | 384 (853.40 MB) → 8 (32.29 MB) | 28 032 (67.28 MB); 18 432 (44.24 MB) |
| 38 `mww3_test_03` `PR3_UQ` `grdset_a`, `resident` | stops: 110 columns of grid 1 | 10/34, `ww3_self` 0/34 | 0/34 | 216 (441.68 MB) → 216 (441.68 MB) | 100 224 (192.43 MB); 49 680 (95.39 MB) |
| 43 `mww3_test_04` `PR3_UQ` `grdset_a`, `resident` | stops: 240 columns of grid 2 | 4/25, `ww3_self` 0/25 | 0/25 | 864 (3 002.99 MB) → 0 | 384 372 (737.99 MB); 71 388 (137.07 MB) |
| 2, 8 `mww3_test_05` `ST1`/`ST4_PR3_UQ_OMP` (moving grid), `SRCE,OUTG,IOPE,UPDV` | stops: 138 columns of grid 2 | 0/27 | 0/27 | 156 (790.79 MB) → 156 (790.79 MB) | 66 120 (158.69 MB); 6 312 (15.15 MB) |

- Every after run is byte-identical with the host copy poisoned, so no host reader of `VA` outside
  `W3WAVE` on these entries reads a column it did not sync (v).
- The whole-array copies went down or stayed; none went up (v, `WW_KOKKOS_VA_COUNT=1`). In 38 the
  216 that stay are one per step of the `W3OUTG` fallback inside `W3WAVE` (`W3OUTG Kokkos calls: 0`
  in that run), and in `mww3_test_05` the 156 each way are the host write of the Fortran
  propagation (`XYP3` and `KTP3` refuse a moving grid): the output block and the Fortran
  propagation, not the exchanges.
- `mww3_test_05` passes before without the diagnostic because, with `XYP3` and `KTP3` off, a
  host write of the Fortran propagation (`w3wavemd.F90:1885` or `:2037`) comes before any kernel
  in each step and declares the host newer, as gap 2 predicted.
- `ww3_shel`: `bash scripts/ww3-gpu.sh --ww3-gpu full|smoke --cpu --kokkos resident`, plain and
  with `WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_CHECK=1`: every model file byte-identical to the
  Fortran's and every file but the NetCDF to `--ww3`'s, `nccmp-tol` PASS; whole-`VA` D2H on full
  1 → 0 (one copy of 21.21 MB per run), on smoke 37 → 37 (the output readers of section 23) (v).
  Wall time of full, one run each on the shared machine ⚠: 40.86 s before, 43.99 s and 32.35 s
  after; no difference can be read from these.
- `mww3_test_07` (`default`) was not compared: with the lab switch its `ww3_grid` stops in the
  Fortran run too (`ERROR IN READING SDS4`, the entry's namelist against `ST4`'s).

## 27. `W3CPRT` on the resident `VA`, and `W3OUTG`'s partition fields without it, `ww3-gpu/u9-cprt`

One commit on `80644ba9` (section 22), `4f126d85`, pushed to h0ffmann/WW3 on 2026-10-18; not at the
pin until the integrator merges it. ABI `ww_kokkos` 0.10 (three entry points added: `ww_cprt_va`,
`ww_cprt_fetch` of [`cprt_c.hpp`](cprt_c.hpp), `ww_outg_part` of [`outg_c.hpp`](outg_c.hpp)).
Unit U9 of the production plan: at every output time with partitioning on, `W3WAVE` read the whole
`VA` back from the device for `W3CPRT` (`w3wavemd.F90:2688-2693` at `80644ba9`), again before
`W3IOSF` (`:2963-2966`), and `W3KOKKOS_OUTG` fell back to the Fortran `W3OUTG`, with its own host
read, whenever a partition field was asked for (`FLPART`). GFS-Wave writes partitions.

- **The fork.** A unit file, `model/src/w3kokkos_cprtmd.F90` (`W3KOKKOS_CPRTMD`): the flag
  `KOKKOS_CPRT` from `WW_KOKKOS_CPRT` (opt-in, `kOptInOnly` of [`part_flags.cpp`](part_flags.cpp)),
  refused outside `SHRD` and under `MPI` or `T`; `W3KOKKOS_CPRT ( IMOD )` packs what `W3CPRT`
  reads, calls `ww_cprt_va`, allocates `ICPRT` and `DTPRT` as `W3CPRT` does (`w3iosfmd.F90:221-229`,
  `:320-326`), writes `PTMEAN`'s format 1000 the number of times the kernel counted, and calls
  `ww_cprt_fetch`. A sea point with a NaN depth, or a spectrum `PTMEAN` cannot take, makes it read
  `VA` to the host and call the Fortran `W3CPRT` whole instead (the first stops the model there,
  `:270-274`; the second reads out of bounds in the Fortran,
  [`PTMEAN_KOKKOS.md`](../../port_report/PTMEAN_KOKKOS.md)). In `w3wavemd.F90`, two hunks: at
  `:2688` the host read and `CALL W3CPRT` become `IF ( LOCAL .AND. FLPART .AND. KOKKOS_CPRT ) CALL
  W3KOKKOS_CPRT ( IMOD ) ELSE` the old lines; before `W3IOSF` the host read becomes
  `IF ( .NOT. KOKKOS_CPRT )`. `W3IOSF` reads `ICPRT`, `DTPRT`, `DW`, `U10`, `U10D`, `CX`, `CY`,
  `ASF`, `TIME` and the grid, and no `VA` (its `USE` lists, `w3iosfmd.F90:429-453`, v), so it needs
  no host copy once `W3CPRT` is on Kokkos. In `w3kokkos_outgmd.F90`, `FLPART` no longer forces the
  Fortran `W3OUTG`: after `ww_outg_va` (run with `FLPART` false, as before), `W3KOKKOS_OUTG` calls
  `ww_outg_part` for section 6, which reads the table and no `VA` (`w3iogomd.F90:2242-2340`) and
  whose fields no other section reads. That holds whichever of `W3CPRT` or `W3KOKKOS_CPRT` built
  the table. `model/src/CMakeLists.txt` asks for `ww_kokkos 0.10`.
- **The kernel** ([`../ww_kokkos/iosf_w3cprt.hpp`](../ww_kokkos/iosf_w3cprt.hpp), the shim
  [`cprt_shim.cpp`](cprt_shim.cpp)). The sea points go in chunks (`WW_KOKKOS_CPRT_CHUNK`, by default
  four per thread on a host backend, 1024 on a device), one work slot per point. Per chunk: a launch
  runs `W3CPRT` sections 2-4 and `W3PART` (unchanged,
  [`W3PART_KOKKOS.md`](../../port_report/W3PART_KOKKOS.md)) per point in the Fortran's serial
  order; an integer reduction counts format-1000 messages and no-peak points; an integer prefix sum
  over `NP+1` gives each point its first `DTPRT` column and `ICPRT(JSEA+1,2)`; a launch copies
  `XP(:,0:NP)` there. Integer sums are exact in any order, so the table is in the Fortran's `JSEA`
  order by construction. The static parity argument is in
  [`W3CPRT_KOKKOS.md`](../../port_report/W3CPRT_KOKKOS.md).
- **Memory contract**: phase 2. `VA` is read in `d_va` (`ww::resident::device_va`). On a device the
  inputs (`MAPSF`, `MAPSTA`, `U10`, `U10D`, `ASF`, `DW`, `CG`, `WN`, the frequency and direction
  tables, `EWN1`, `ECG1`) are copied in per call, `CG` and `WN` being most of it; `ICPRT` and the
  `FINSIZ` used columns of `DTPRT` come back. The work slots, 288 kB per point for `NK = NTH = 24`
  and `IHMAX = 50` (`w3cprt_nreal` and `w3cprt_nint`) (`PTMEAN`'s sums are sized for `NSPEC+1` partitions), live for the run. `ww_outg_part`
  copies `ICPRT` and `DTPRT` in on a device and only the partition fields `W3IOGO` writes back
  (`FLOGRD(4,k)` and `FLOGR2(4,k)`, `w3iogomd.F90:3283-3367`). No Unified Memory.
- **Counters**: `WW_KOKKOS_CPRT_COUNT=1` (calls, sea points, fallbacks, bytes in and back);
  `WW_KOKKOS_OUTG_COUNT=1` adds the partition-field calls and bytes. **Timers**: `cprt.call`,
  `cprt.h2d`, `cprt.kernel`, `cprt.d2h`, `outg.part`.
- **Not covered.** `W3_MPI` (the `DIST` runs keep the single host read of section 11); the
  `WW_KOKKOS_STRICT` count of section 24 (the two fallbacks above are not reported to it: this
  branch is on `80644ba9`, without `KOKKOS_FALLBACK`); the `resident` preset of section 25 does not
  list `CPRT`, so the runs below name the parts.

The checkpoint on the CPU, 2026-10-18 (OpenMP backend, Kokkos 5.2.2, gfortran/g++ 16.2.0, i9-14900
shared with other agents' jobs, `OMP_NUM_THREADS=4`; never `--gpu`); the commands, the two
throwaway `bench_small` variants with partitioning on and the numbers are in
[`docs/log/2026-10-18-cprt-resident.md`](../../../docs/log/2026-10-18-cprt-resident.md):

- `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos SRCE,XYP3,KTP3,OUTG,IOPE,UPDV[,CPRT]` with
  `WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_COUNT=1`, five runs on `switch_lab_shrd` and
  `switch_lab_ompg`: every model file and `partition.ww3` byte-identical to the Fortran of the
  same executable and to `--ww3` (v). `VA` device-to-host copies in 12 hours with 13 output times:
  13 before, 1 after (the read at `W3WAVE`'s return, which section 26 removes), with only the
  partition file and with all 17 partition fields (v). Per output time a device would move back
  1.14 MB of table and, with every partition field requested, 3.39 MB of fields, against the
  21.21 MB of `VA`, and in 2.21 MB of inputs (projected on the host backend).
- WW3's `ww3_tpt1.1`, the five partitioning methods (`-N -g PTM1` .. `PTM5`), through
  `regtests/cmake/run_entry.cmake` with `WW3GPU_KOKKOS=CPRT`: `test.comp` 0 of 22 files different
  and `nccmp-tol` PASS on each, 2 Kokkos calls on 18 sea points each (v), with `PR3 UQ` in the
  staged switch for its `PR0` ⚠, because the fork's `KOKKOS` build needs `PR3`. No `matrix.base`
  entry with partitioning builds with `KOKKOS` (`ww3_tnc1` is `PR0`), so the matrix proper did not
  run this part ⚠.

## 28. The integration fixes, `ww3-gpu/integ-fixes`

One fork commit, `711afb80`, on `b733066b` of `ww3-gpu/integ-abbb0456` (sections 23 to 27 merged
onto `80644ba9`), pushed to h0ffmann/WW3 on 2026-10-18; not at the pin until the integrator moves
it. No ABI change.

- **The `CPRT` fallbacks report to strict mode.** Section 27 left them out of section 24's count.
  `W3KOKKOS_CPRT` now calls `KOKKOS_FALLBACK ( 'CPRT', REASON )` on both branches that run the
  Fortran `W3CPRT` (`w3kokkos_cprtmd.F90:167-175` at `711afb80`): a sea point with a NaN depth, and
  a spectrum `PTMEAN` cannot take (the kernel's `FALLBACK` flag). With `WW_KOKKOS_STRICT=1` the
  model stops on `... PART CPRT WOULD RUN ON THE FORTRAN: <REASON>`; without it the call is counted,
  one per output time that fell back. `W3KOKKOS_CPRT_SETUP` makes the counter when the part is on
  (`:78-81`), so the line at exit shows `CPRT=0` when nothing fell back. The path that runs the
  kernel adds one `LOGICAL` store per call and nothing per point. Neither fallback was made to
  happen in a run: no case here has a NaN depth or a spectrum that trips `PTMEAN` ⚠.
- **`CPRT` joins the `resident` preset** ([`presets.txt`](presets.txt):
  `SRCE,XYP3,KTP3,OUTG,IOPE,UPDV,CPRT`). Its consumers changed with it: the shim compiles the file
  (`just kokkos-test serial-debug` and `openmp-release`), `scripts/ww3-gpu.sh --self-test` checks
  the seven flags, and `regtests/cmake/selftest.cmake` has `CPRT` in its fixture `parts.d` and in
  the expected list. The selection words stay `ST4;NL1;PR3;UQ;SHRD;!PDLIB;!MGP;!MGG`: under a set
  flag `KOKKOS_PART` stops on a switch the part does not cover, and `CPRT` refuses a switch without
  `SHRD` or with `MPI` or `T` (`w3kokkos_cprtmd.F90:70-76`), a subset of what `OUTG`, already in
  the preset, refuses (`SHRD` missing, `MPI`, `IG1`, `O8`, `O9`, `T`, `w3kokkos_outgmd.F90:74-80`).
  So the preset stops on no switch it ran before (v, by reading both set-ups).
- **The `KOKKOS` build without `PR3`.** The `PR3`-only `USE` at `w3kokkosmd.F90:3183` of
  `80644ba9` is the one `31a09522` (section 23) put under `W3KOKKOS_HAS_PR3`, and `b733066b`
  contains that commit (`git merge-base --is-ancestor`, v). Section 27's note that the fork's
  `KOKKOS` build needs `PR3` was written on `u9-cprt`, which branched before it. No fork change
  was needed: `ww3_tnc1` (`SHRD PR0`) builds with `KOKKOS` at `b733066b` and at `711afb80` (v,
  below).

`ww3_tnc1`, matrix entry 38, the serial `matrix.base` entry that writes partition fields (`PHS
PTP`, `FIELD%PARTITION = '0 1 2'`), on 2026-10-18 (OpenMP backend, gfortran 16.2.0, i9-14900
shared with other agents, `OMP_NUM_THREADS=4`; never `--gpu`), with `WW3GPU_LAB_SWITCH=ON`,
`WW3GPU_STRICT=ON`, `WW3GPU_SWITCH_WORDS=PR0;SHRD`, `WW_KOKKOS_VA_POISON=1` and
`WW_KOKKOS_VA_COUNT=1`:

| `WW3GPU_KOKKOS` | Result |
|---|---|
| `resident` | builds; `CPRT=K OUTG=K IOPE=K UPDV=K SRCE=K`, 3 `W3CPRT` Kokkos calls on 243 sea points and 3 partition-field calls of `W3OUTG`; test.comp 0/30 files different; the entry fails its parts-line check, `XYP3,KTP3` not on Kokkos (v) |
| `SRCE,OUTG,IOPE,UPDV,CPRT` | passes: test.comp 0/30, `CPRT` fallbacks 0, whole-`VA` D2H 0, `calls left on the Fortran ...: SRCE=0 CPRT=0` (v) |

The first row shows a gap of the preset that `CPRT` does not cause: without `PR3` and `UQ`,
`W3KOKKOS_PROP3_SETUP` compiles its body out (`w3kokkos_prop3md.F90:111-138`), so `XYP3` and `KTP3`
never reach `KOKKOS_PART`, the run neither stops nor counts them, and only the matrix's parts-line
check reports it. The selection words keep every `PR0` entry out of a `resident` matrix run.

Closed by a second fork commit, `a304018c` on `711afb80` (the integrator, 2026-10-18):
`W3KOKKOS_PROP3_SETUP` gains an `#else` branch for a switch without `PR3` and `UQ` that asks
`KOKKOS_PART ( 'XYP3', .FALSE. )` and `( 'KTP3', .FALSE. )` and registers both. A set flag or a
preset now stops, and `WW_KOKKOS_ALL=1` counts or, under `WW_KOKKOS_STRICT=1`, stops. On
`switch_lab_shrd` and `switch_lab_ompg` the preprocessed module (`gfortran -E -P`, `#ww3` shell) is
the same as before apart from blank lines (v). `ww3_tnc1` with `WW3GPU_KOKKOS=resident`, as above,
at `a304018c`: the Kokkos run stops in `W3INIT` with `WW_KOKKOS_XYP3=1, BUT ITS KOKKOS TRANSLATION
DOES NOT COVER THIS SWITCH FILE`, the row reads `ran fortran-only` (v), where at `711afb80` it ran
with both parts silently on the Fortran.

`bash scripts/ww3-gpu.sh --ww3-gpu smoke|full --cpu --kokkos resident` with `WW_KOKKOS_VA_POISON=1
WW_KOKKOS_VA_COUNT=1`, at `711afb80` (2026-10-18, same machine, one run each): every model file
byte-identical to the Fortran of the same executable and every file but the NetCDF to `--ww3`'s,
`nccmp-tol` PASS; whole-`VA` D2H 37 on smoke and 0 on full, as in section 26; `CPRT=K` with 0 calls,
since neither case partitions (v).

## 29. The ESMF cap reads the resident `VA` before its exports, `ww3-gpu/integ-fixes`

Fork commit `50a10730` on `a304018c`, 2026-10-18. Section 26 removed the read at `W3WAVE`'s return
and synced every host use of `VA` in `ww3_shel` and `ww3_multi`. `wmesmfmd.F90`, the ESMF cap of
`ww3_multi` (built only with `MULTI_ESMF`, `model/src/CMakeLists.txt:193-199`), reads the export
grid's `VA` on the host after `WMWAVE` in `SetExport` (`wmesmfmd.F90:2824-3046` at `a304018c`):
`W3OUTG` under `USE_W3OUTG_FOR_EXPORT`, `CalcCharnk` and `CalcRoughl` (`W3SPR3`/`W3SPR4` on
`va(:,jsea)`), `CalcStokes3D`, `CalcPStokes`, `CalcBotcur`, `CalcRadstr2D`. Every one reads the
whole field, so one `W3KOKKOS_VA_HOST_READ` after the grid's `w3set*` calls covers them; the
"Resident spectrum" rule allows a whole-field read before an output that needs every point. Its
`w3uini ( va )` at `:2180` runs in `InitializeP3` before the resident `VA` is bound (the C++ side
binds it on the first sync, `w3kokkos_residentmd.F90:68-70`), so it needs no sync.

- Without `KOKKOS`, `gfortran -E -P` with `switch_lab_shrd` gives `WW3-noaa/`'s file apart from the
  `__LINE__` values `ESMF_LOGERR_PASSTHRU` writes into error messages and the include-path comment
  (v); with `KOKKOS`, only the `USE` and the read are added (v).
- Not compiled and not run: there is no ESMF library here ⚠. No regtest of this repository builds
  `MULTI_ESMF`, so the matrix evidence of section 26 and of U3 holds at this commit.
- Not covered: the NUOPC cap of the UFS (`wav_comp_nuopc.F90`, `wav_import_export.F90`,
  `wav_restart_mod.F90`), which is not in the fork at `a304018c`; `production/GFS.v17` has it, with
  16 and 13 uses of `VA` in the last two (v, `git show`). The pin move to that branch
  ([plan](../../../docs/compound/plans/2026-10-18-004-pin-gfs-v17-plan.md)) lists and syncs them.

## 30. `W3XYPUG` and `W3KTP3` on an unstructured grid, `ww3-gpu/xypug-196-rebased`

Fork commits `e299fa92`, `43601904` (`find_package(ww_kokkos 0.11)`) and `aa382393` (the `MAPSTA`
size check) on `fac74069` (`ww3-gpu/strict-outg`), 2026-10-18, branch `ww3-gpu/xypug-196-rebased`; issue #196.
`ww3_tp2.6` (the LIMON unstructured grid, `EXPFSN = T`, `SHRD PR3 UQ`, no `PDLIB`) stopped under
the `resident` preset: `W3KOKKOS_PROP3_SETUP` gave `XYP3` and `KTP3` unsupported on `UNGTYPE`, so
`KOKKOS_PART ( 'XYP3', .FALSE. )` stopped the model in `W3INIT` (`w3kokkosmd.F90:1233-1251`). The
part keeps its name: `XYP3` is `W3WAVE` section 3.6.3's spatial step, which on `UNGTYPE` is
`W3XYPUG`. The C entry point is `ww_xypug_batch_va` of [`xypug_c.hpp`](xypug_c.hpp), a new entry
point, so the package version is `0.11.0` and the fork asks for `0.11`.

The hunks, at `e299fa92`:

- **The set-up** (`w3kokkos_prop3md.F90:132-180`). Inside the `PR3`-and-`UQ` branch, `OKU` is
  `GTYPE == UNGTYPE`, false under `PDLIB`, `MGP` or `MGG` as `OKG` is. `KTP3` is supported on
  `OKG .OR. OKU`, still refused under `REFRX`. `XYP3` on `UNGTYPE` (`OKXU`) needs, besides `OKU`,
  `SHRD`, no `REF1` (`W3XYPFSN2` has a `REF1` branch, `w3profsmd.F90:682-686`), and the N scheme:
  `FSN` true, `FSPSI`, `FSFCT`, `FSNIMP` false. Every other case gives `.FALSE.` as before, so a set
  flag or a preset stops and `WW_KOKKOS_ALL=1` falls back and counts.
- **`W3KOKKOS_XYPUG ( FACX, FACY, DTG, LCALC )`** (`w3kokkos_prop3md.F90:261-350` at `aa382393`) and its
  interface block (`:103-119`). It packs `ON(ISPEC) = IAPPRO(ISPEC) == IAPROC` as `W3KOKKOS_XYP3`
  does, calls `SETDEPTH` once, widens `IOBP` (`INTEGER*2`), `IOBPD`, `IOBDP`, `IOBPA` (`INTEGER*1`)
  into `SAVE`d `C_INT` arrays (resized when `NSPEC`, `NX` or `NTH` change), computes `RD1`, `RD2`
  as `W3XYPUG` does (`w3profsmd.F90:241-247`: `DSEC21 ( TBPI0, TIME )` and `DSEC21 ( TBPI0,
  TBPIN )` under `FLBPI`, else `1.` and `0.`, `REAL`), and passes `ITER(NK,NTH)` in and out and
  `VA` with `SIZE(VA,1)` and `UBOUND(VA,2)`, as the other `_va` forms. Without `FLBPI` it passes
  one-element dummies for `ISBPI`, `BBPI0`, `BBPIN` and `NBI = 0`, as `W3KOKKOS_XYP3` does: those
  arrays are not allocated then, and `W3XYPFSN2` reads them only under `FLBPI`
  (`w3profsmd.F90:694-720`). A non-zero status stops through `PROP3_CHECK` (`EXTCDE`). It
  re-checks at the call that the grid is `UNGTYPE` with `FSN`, for a later grid of a multi-grid run,
  and that `MAPSTA` holds `NX` nodes (`NY = 1`), which the shim relies on (`aa382393`).
- **The call site** (`w3wavemd.F90:2053-2060`). Under `KOKKOS_XYP3`, `GTYPE == UNGTYPE` calls
  `W3KOKKOS_XYPUG ( FACX, FACX, DTG, UGDTUPDATE )`, the arguments `W3WAVE` gives `W3XYPUG`
  (`:2097-2103`; `FACX` is `1./(DERA*RADIUS)` with `FLAGLL`, else `1.`, set at `:1994-1999` just
  before). Other grids call `W3KOKKOS_XYP3` as before; with `WW_KOKKOS_XYP3=0` the original
  `DO ISPEC` loop runs. Without `KOKKOS`, `gfortran -E -P` of the file is the same as at
  `fac74069`, for the switch words of `switch_lab_shrd` and of `ww3_tp2.6` (v, 2026-10-19).
- **`W3KOKKOS_KTP3`** (`w3kokkos_prop3md.F90:351-431` at `aa382393`). The stop for a grid that is not regular
  or curvilinear now lets `UNGTYPE` through, and on `UNGTYPE` a point with `IOBP(ISEA) /= 1` is
  skipped before the `MAPSTA(IY,IX) == 1` test (`:399-402`). Both sections do that on a grid
  without `PDLIB` (`w3wavemd.F90:1904-1914` for 3.6.2, `:2288-2298` for 3.6.4, at `e299fa92`);
  they differ only in where `DEPTH = MAX ( DMIN, DW(ISEA) )` is computed (inside the `MAPSTA` test
  in 3.6.2, `:1915`, before the `IOBP` test in 3.6.4, `:2286`), which gives the same value, since
  the loop writes neither `DW` nor `DMIN`. So one selection serves both call sites
  (`:1887-1888`, `:2270-2271`), each under `IF ( FLCTH .OR. FLCK )`. `IXrel = IX` without `PDLIB`,
  so `DDDX(IY,IX)` and the others are packed as before. The index of `IOBP` is `ISEA`, as in
  `W3WAVE`, although `IOBP` is dimensioned by node (`NX`).

**When `FSN` and the others are known.** They are per grid, `MPARS(IMOD)%SCHMS%FSN` and so on
(`w3gdatmd.F90:1039-1048`, pointers at `:2850-2854`), read from `mod_def.ww3` by `W3IOGR ( 'READ'
)` for `UNGTYPE` (`w3iogrmd.F90:1002-1004`). `W3INIT` calls `W3IOGR` at `w3initmd.F90:746` and
`W3KOKKOS_UNITS_SETUP` at `:852`, so the set-up reads them after they are set; it reads them
through `MPARS(IMOD)`, not the pointers, so the answer does not depend on which grid `W3SETG` last
selected (v, by reading).

**Why `SETDEPTH` once is enough.** `W3XYPUG` calls it once per component
(`w3profsmd.F90:202`). It writes only `IOBDP`: `IOBDP = 1`, then `IOBDP(IP) = 0` where `DW(IP) <
DMIN + DTHR` for `IP = 1, NX` (`:1672-1676`); it reads `DW` (`W3ADATMD`) and `DMIN`
(`W3GDATMD`) and nothing else (`:1650-1651`). In the loop it replaces, `W3GATH` writes only
`FIELD` under `SHRD` (`w3wavemd.F90:3427-3435`), `W3SCAT` only `VA` (`:3749`), and `W3XYPUG` with
`W3XYPFSN2` write `VQ`, `ITER` and `IOBDP` through `SETDEPTH` (`w3profsmd.F90:69-272`,
`:470-727`). None writes `DW` or `DMIN`, so every call after the first writes the same `IOBDP` (v,
by reading). The wrapper calls it only when some component is on this process, as `W3XYPUG` is
called only for those; under `SHRD` that is every component.

**Not covered and what to check.**

- Built, not run: the shim (`xypug_shim.cpp`) indexes `CLATS(1:NSEA)` from 0, as the wrapper
  passes it; `kokkos-test serial-debug` and `openmp-release` pass and `cuda-release` compiles. A
  fresh review on 2026-10-18 found no defect in the kernel, the shim or this caller (port report
  [W3XYPUG](../../port_report/W3XYPUG_KOKKOS.md)). The regtest verdict on `ww3_tp2.6` at WW3 `aa382393` / ww3-gpu `94b3118`: `ST4` `test.comp` 0 of 36 files different, `ST4_ASCII` 0 of 38, `nccmp-tol` PASS (v, [log](../../../docs/log/2026-10-19-xypug-tp2.6-rebased.md)).
- Other parts of the `resident` preset on `UNGTYPE`: none of the set-ups of `SRCE`, `OUTG`, `IOPE`,
  `UPDV`, `CPRT` tests the grid type (grep of `RLGTYPE`, `UNGTYPE` in `w3kokkos*.F90`, v). The
  `UNGTYPE` branches of `W3WAVE` outside 3.6.2 to 3.6.4 (`UG_GRADIENTS` for `DCXDX` and `DDDX`,
  `W3CFLUG`) read `CX`, `CY`, `DW`, `CG`, never `VA`, so they need no sync (v, by reading). `MAPSTA`
  is `MAPSTA(NY,NX)` with `NY = 1`, and `W3SCAT` indexes it as one dimension by `IXY` (`:3749`);
  the `XYP3` wrapper's `NX*NY` sizes hold for it. Whether `W3SRCE`, `W3OUTG` and `W3CPRT` on
  Kokkos give the Fortran's bits on an unstructured grid was not checked ⚠.

## 31. `W3OUTG`'s Fortran fallback reports to strict mode, `ww3-gpu/strict-outg`

One fork commit, `fac74069`, on `50a10730`, 2026-10-18; it touches only `w3kokkos_outgmd.F90`. No
ABI change and no new entry point: `KOKKOS_FALLBACK` and `ww_kokkos_fallback_slot` are section 24's.
Section 30 is held by the unstructured-propagation port (h0ffmann/ww3-gpu#196).

`W3KOKKOS_OUTG` runs the Fortran `W3OUTG` whole, after a host read of `VA`, when a requested field
is one its translation leaves to the Fortran: U3 Stokes (`FLOLOC(6,8)`, `(6,12)`,
`CALC_U3STOKES`), skewness (`FLOLOC(8,7:9)`, `SKEWNESS`), `WBT` (`FLOLOC(2,17)`, `CALC_WBT`), or a
requested 3-D or `P2SMS` field that is not allocated (`E3DF(1,K)` or `P2MSF(1)` `<= 0`). Section 24
left that branch out: under `WW_KOKKOS_STRICT=1` it neither stopped nor counted, and the parts line
showed `OUTG=K` while the Fortran wrote the fields (the code review of #191, finding 3).

- **The branch calls `KOKKOS_FALLBACK ( 'OUTG', REASON )`** before `W3KOKKOS_VA_HOST_READ`
  (`w3kokkos_outgmd.F90:138-157` at `fac74069`), with one reason per kind of field, in the order
  above: `A U3 STOKES FIELD IS REQUESTED ...`, `A SKEWNESS FIELD IS REQUESTED ...`, `THE WBT FIELD
  IS REQUESTED ...`, `A REQUESTED 3-D OR P2SMS FIELD IS NOT ALLOCATED ...`. With
  `WW_KOKKOS_STRICT=1` the model stops on `... PART OUTG WOULD RUN ON THE FORTRAN: <REASON>`;
  without it each output time that falls back is counted.
- **`W3KOKKOS_OUTG_SETUP` makes the `OUTG` counter** when the part is on (`:89-91`), as
  `W3KOKKOS_CPRT_SETUP` does, so the line at exit shows `OUTG=0` when nothing fell back.
- **Nothing changes on the kernel path or per point.** The condition `FORT` is the one before;
  only its `THEN` branch gained the call.
- **A build without `KOKKOS` is unchanged.** `gfortran -E -P` (16.2.0, the `#ww3` shell's
  compiler) of all 143 `model/src/*.F90` at `50a10730` and at `fac74069`, with `switch_lab_shrd`'s
  words as `-DW3_<word>`, blank lines dropped: 142 identical, and the one that differs is
  `w3kokkos_outgmd.F90`, which `model/src/CMakeLists.txt` compiles only under `WW_KOKKOS` (its
  glob of `w3kokkos_*md.F90`, `:25-85`) (v). With `-DW3_KOKKOS` that file gains 30 lines and loses
  only the four it rewrites: two lines of the header comment and the two `USE` lines that now
  import `C_NULL_CHAR`, `KOKKOS_FALLBACK` and `WW_KOKKOS_FALLBACK_SLOT` (v).

The stop, shown on a scratch copy of the `full` case's Kokkos run directory whose `ww3_shel.nml`
asks for `TYPE%FIELD%LIST = 'HS SKW'` (the fixture `kokkos/tests/fixtures/bench_small` is
unchanged), with the parts of the `resident` preset, `WW_KOKKOS_VA_POISON=1` and
`WW_KOKKOS_OUTG_COUNT=1`, 2026-10-18 (OpenMP backend, 8 threads, gfortran 16.2.0; never `--gpu`):

| Run | Result |
|---|---|
| `WW_KOKKOS_STRICT=1` | `ww3_shel` exit 1 at the first output time, `*** WAVEWATCH III ERROR IN W3KOKKOSMD : WW_KOKKOS_STRICT=1 AND PART OUTG WOULD RUN ON THE FORTRAN: A SKEWNESS FIELD IS REQUESTED (FLOLOC(8,7:9), SKEWNESS), WHICH THE TRANSLATION LEAVES TO THE FORTRAN W3OUTG` (v) |
| strict unset, the domain cut to `20200101 020000` (three output times; the Fortran `SKEWNESS` at every point made the twelve-hour run take over an hour on the shared machine, so it was stopped) | `ww3_shel` exit 0, `End of program`, `calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead): SRCE=0 CPRT=0 OUTG=3`, `W3OUTG Kokkos calls: 0` (v) |

The `smoke` case (`ww3_ts1`) asks for `WBT` and `SKW` among its fields, so its `OUTG` has always
taken this branch: `scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` at `fac74069` ends
with `calls left on the Fortran (...): SRCE=0 CPRT=0 OUTG=37`, one per output time, with every
model file byte-identical to the Fortran of the same executable and every file but the NetCDF to
`--ww3`'s (v). At `50a10730` no `OUTG` counter exists, so the line names `SRCE` and `CPRT` alone (v, by reading the set-up; that run was not repeated). The `full` case asks for `HS`
alone and ends with `OUTG=0` and 13 `W3OUTG` Kokkos calls (v).

## 32. The batched `W3SRCE` admits `SEED`, `FLD2` and no `LN1`, `ww3-gpu/gfs-srce-be5bdbea`

One fork commit, `5dbf6eba`, on the pin `aa382393`, 2026-10-19 (goal be5bdbea-3817); it touches
`model/src/w3kokkosmd.F90` and one line of `model/src/w3wavemd.F90`. The switch of WW3's GFS-Wave
regtests (`regtests/ww3_ufs1.2/input/switch_MPI_OMPH`: `... SEED ST4 STAB0 NL1 BT1 DB1 MLIM FLD2 ...`, no `LN1`)
sent every `W3SRCE` call to the Fortran, because the gate of `W3KOKKOS_SRCE_SETUP` required `LN1` and refused
`SEED`, `FLD1` and `FLD2`. The kernel translates the three branches (`srce_kernel.hpp`, `srce_fld2.hpp`); the
gate now lets them through.

**What the gate (`IS_LAB_SWITCH`) admits now.** `BT1 DB1 MLIM` (and `ST4 NL1`, the caller's) with `LN1`, or with
`LN0` or `SEED` in its place (`VSLN` is zeroed by `w3srcemd.F90:938-940` and `W3SLN1` is not called; with none of
the three `VSLN` is never set and the Fortran reads it, so that stays refused), with `SEED` and `FLD2` each in or
out. Still refused: `FLD1`, `FLX1` to `FLX5`, `BS1`, `BT4`, `BT8`, `BT9`, `IC1` to `IC5`, `IG1`, `IS1`, `IS2`, `NL0`,
`NL2` to `NL5`, `NLS`, `NNT`, `PDLIB`, `REF1`, `TR1`, `UOST`, `ST0` to `ST3`, `ST6`, `STAB3`, `S`, `T`, `T0`, `T1`,
`DEBUGSRC`, `DEBUGRUN`.

**ABI (`ww_kokkos_c.hpp` and `srce_params.hpp`, read in both).**

| Packet | Index (0-based) | Content |
|---|---|---|
| `RPAR` | `RS_FACSD` = `RS_SLNC1+6` | `FACSD` (seeding, `w3srcemd.F90:1972`) |
| `RPAR` | `RS_FTTR` = `RS_SLNC1+7` | `FTTR` (`CALC_FPI`) |
| `RPAR` | `RS_FLDALPHA`, `RS_TAIL_LEV`, `RS_TAIL_TRAN1`, `RS_TAIL_TRAN2` = `RS_SLNC1+8` to `+11` | `FLDALPHA`, `TAIL_LEV`, `TAIL_TRAN1`, `TAIL_TRAN2`; set only under `W3_FLD2` (W3GDATMD declares them only then) |
| `IPAR` | `IS_LN1`, `IS_SEED`, `IS_FLD2` = 8, 9, 10 | 1 under `W3_LN1`, `W3_SEED`, `W3_FLD2` |
| `IPAR` | `IS_TAIL_ID` = 11 | `TAIL_ID`, under `W3_FLD2` |
| `NRSPAR`, `NISPAR` | `RS_SLNC1+12`, 12 | were `RS_SLNC1+6`, 8 |
| `PSC` row | `SRCEB_S_FPI` = 21, `SRCEB_NPSC` = 22 | `FPIS(ISEA)`; unpacked to `FPIS(ISEA)` only under `W3_FLD2` (`CALC_FPI` writes it, and `W3IORSMD` writes `FPIS` to the restart) |
| `ww_srce` (per point) | new dummy `FPI` before `SHAVE` | `FPIS(ISEA)`; the call in `w3wavemd.F90` passes it (that call is reached by no build, section 10) |
| `SHAVE` word of the batch | bits 1 and up | `Fld2Status << 1` (1 bulk fail-safe, 2 stress not converged, 4 work array too short), read and cleared by the shim; the Fortran sees 0 or 1 as before |

**Package version.** The packet lengths and `ww_srce`'s dummy list changed, so the package is `0.12.0` and fork
commit `96892c95` (`find_package(ww_kokkos 0.12)`, on top of `5dbf6eba`) asks for it: the new fork refuses an
older install at configure time. An older fork against this install still configures (`SameMajorVersion`), and
`ww_srce_init` refuses its shorter `RPAR`/`IPAR` (`WW_KOKKOS_ERR_BAD_SHAPE`), which `SRCE_CHECK` turns into a stop at
setup, never a run on mismatched packets (v, `srce_shim.cpp`, `w3kokkosmd.F90` `SRCE_CHECK`). `kokkos/CMakeLists.txt`
says a changed entry point needs a major bump; this stays a minor one, for the owner to confirm ⚠.

**Caveat kept by a check.** `W3FLD2`'s `FIRST` is a threadprivate `SAVE`: `INFLD` copies `TAIL_ID`, `TAIL_LEV`,
`TAIL_TRAN1`, `TAIL_TRAN2` of the grid current at a thread's first call and every later grid uses them. The kernel
uses each grid's own. `W3KOKKOS_SRCE_SETUP` keeps the first grid's four values and, for a later grid that differs,
turns `KOKKOS_SRCE` off and calls `KOKKOS_FALLBACK ( 'SRCE', ... )` (a stop under `WW_KOKKOS_STRICT=1`).

**A build without `KOKKOS` is unchanged.** The commit is the two files; `w3kokkosmd.F90` is compiled only under
`WW_KOKKOS` (`model/src/CMakeLists.txt`) and the `w3wavemd.F90` line is inside the
`#if defined(W3_KOKKOS) && defined(W3_ST4) && defined(W3_NL1)` hunk of section 10 (v, read in the diff).

Checked 2026-10-19 on the CPU (Kokkos OpenMP backend, gfortran 16.2.0, never `--gpu`) (v):

- The fork with the `ww3_ufs1.2` switch and `KOKKOS` compiles and links.
- `WW3GPU_SWITCH=switch_lab_gfs` (`switch_lab_ompg` with `SEED FLD2` for `LN1`), `scripts/ww3-gpu.sh --ww3-gpu smoke`
  and `full --cpu --kokkos SRCE` with `WW_KOKKOS_STRICT=1`: every model file byte-identical to the Fortran of the same
  executable, the binary ones also to `--ww3`, `nccmp-tol` PASS, `SRCE=0` fallbacks. The smoke wind is 12 m/s, so `W3FLD2`
  runs (the output `CHA` is 0.016, not `FLDALPHA` = 0.0095).
- The unchanged `switch_lab_ompg` smoke is still byte-identical.
- `WW3GPU_KOKKOS=all WW3GPU_MPI_RANKS=1 WW3GPU_OMP_THREADS=1 WW_KOKKOS_STRICT=1 just ww3-gpu-matrix 1 mpi -R '^323:'`,
  the `ww3_ufs1.1` `grdset_c` entry with the GFS-Wave switch: `test.comp` finds 0 of 30 files different, `nccmp-tol`
  PASS, 33 286 spectra in 24 batched calls through the new path, no fallback (strict mode would have stopped).
  Before this section the same entry fell back for every step.
