# `PATCH.md`: wiring the Kokkos DIA into WAVEWATCH III

## The fork today, 2026-10-16

Read with `git -C WW3 fetch origin && git -C WW3 branch -r` and `git rev-list --count` (v,
2026-10-16):

| h0ffmann/WW3 branch | Head | Against the `WW3/` pin | What it wires |
|---|---|---|---|
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
- **Propagation** (`W3XYP3`, `W3KTP3`, `W3QCK1-3`): nothing on the fork yet; the `W3WAVE`
  support routines are wired (section 8). Each needs a `bind(C)` interface in `w3kokkosmd.F90`, a C shim in `kokkos/src/fortran_iface/`,
  and a hunk in `w3wavemd.F90` that keeps the Fortran call selectable at run time, on a fork
  branch from `ww3-gpu/kokkos-wire-base` (section 9). The shared-memory
  `W3GATH`/`W3SCAT` path is the only one ported; NOAA's operational switches are `DIST MPI`
  (`WW3-noaa/model/bin/switch_NCEP_st4`), whose branches are not.
- The fork's `model/src/CMakeLists.txt` already links `ww_kokkos` under `KOKKOS`, and a new shim
  is one more source in this repository's `fortran_iface` target. What has to move with each new
  set of entry points is the ABI version: since `357f575c` the fork asks for
  `find_package(ww_kokkos 0.3)` and `kokkos/CMakeLists.txt` declares `0.3.0` (`SameMinorVersion`),
  so an install from before `ww_kokkos_flag` and `ww_wave_*` is refused at configure time. The
  registry of `ww3-gpu/kokkos-wire-base` is Fortran only and leaves the C ABI at `0.3`; the
  integrator raises both sides to `0.4` once, when the wiring units are merged (section 9).
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
rows differ: `*_COUNT` are diagnostics and ignore the master; `*_TABLES` follow it.

`W3INIT` writes one line to `log.ww3` after every set-up, the parts this build can run and where
each runs (v, smoke 2026-10-16):

```text
  Kokkos parts (K Kokkos, F Fortran): SNL1=K SDS4=K SIN4=K SPR4=K ST4_TABLES=K SRCE=K GATH=K SCAT=K UWND=K UTRN=K DZXY=K MAP3=K MAPT=K DISTAB=K (14 of 14 on Kokkos)
```

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
| ST4_TABLES | `WW_KOKKOS_ST4_TABLES` | `ST4` | the ST4 tables read from `mod_def.ww3` and `ST4TABUHF2.bin`, rebuilt in C++ | (prints the count of differing values) | measured: "0 values differ from the Fortran's" in the smoke 2026-10-16 (v) |
| SRCE | `WW_KOKKOS_SRCE` | `ST4 NL1` and exactly the lab switch's source terms | `W3SRCE(srce_direct)` in `W3WAVE`, with ST4, `W3SNL1`, `W3SLN1`, `W3SBT1`, `W3SDB1` inside | `WW_KOKKOS_SRCE_COUNT` | measured, CPU: byte-identical in the smoke 2026-10-16 with `WW_KOKKOS_ALL=1` (2 161 spectra, all of them; the per-routine ST4 and DIA counters were 0 there) (v), and in the full  (230 100 spectra, v); GPU ⚠ (`logf`, `tanhf`, `sincosf`) |
| SPR0 | `WW_KOKKOS_SPR0` | `ST0` | `W3SPR0` | `WW_KOKKOS_ST0_COUNT` | judgement ⚠: likely on any backend, `sqrtf` only (IEEE), sums kept in the Fortran's order (`port_report/W3SPR0_KOKKOS.md`); never run |
| SPR1, SIN1, SDS1 | `WW_KOKKOS_SPR1`, `_SIN1`, `_SDS1` | `ST1` | the three in `W3SRCE` | `WW_KOKKOS_ST1_COUNT` | judgement ⚠: CPU likely (`sincosf`, libmvec loops reproduced); GPU unlikely (libdevice `sincosf`); never run |
| SPR2, SIN2, SDS2 | `WW_KOKKOS_SPR2`, `_SIN2`, `_SDS2` | `ST2` | the three in `W3SRCE` | `WW_KOKKOS_ST2_COUNT` | judgement ⚠: CPU likely (`logf`, `sincosf`, `powf`, the x87 `fprem` of `MOD`, libmvec); GPU unlikely; never run |
| SPR3, SIN3, SDS3 | `WW_KOKKOS_SPR3`, `_SIN3`, `_SDS3` | `ST3` | the three in `W3SRCE` | `WW_KOKKOS_ST3_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `powf`, `sincosf`, libmvec); GPU unlikely; never run |
| SPR6, SIN6, SDS6 | `WW_KOKKOS_SPR6`, `_SIN6`, `_SDS6` | `ST6` | the three in `W3SRCE` | `WW_KOKKOS_ST6_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `tanhf`, `powf`, `__powisf2` kept, libmvec); GPU unlikely; never run |
| SWL6 | `WW_KOKKOS_SWL6` | `ST6` | `W3SWL6` | `WW_KOKKOS_SWL_COUNT` | judgement ⚠: likely on CPU (`sqrtf`, one libmvec loop reproduced); never run |
| FLX1 .. FLX5 | `WW_KOKKOS_FLX1` .. `_FLX5` | the matching `FLXn` | `W3FLXn` in `W3SRCE` | `WW_KOKKOS_FLX_COUNT` | judgement ⚠: CPU likely (`expf`, `logf`, `powf`, `tanhf`, `__powisf2` kept, libmvec); GPU unlikely; never run |
| FLX_TABLES | `WW_KOKKOS_FLX_TABLES` | `FLX2` or `FLX3` | `W3DISPMD`'s `EWN1`, `DSIE`, `N1MAX` as the FLX shim reads them, rebuilt in C++ | (prints the count of differing values) | judgement ⚠: the same `distab` as DISTAB below, so as DISTAB; never run |
| GATH, SCAT | `WW_KOKKOS_GATH`, `_SCAT` | `SHRD` | `W3GATH`, `W3SCAT` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16, alone with the other support parts and under `WW_KOKKOS_ALL=1` (13 824 calls each) (v); 0 calls in the smoke (one point, no spatial propagation). Pure copies (`movss`, no arithmetic, `port_report/W3GATH_KOKKOS.md`), so any backend gives the bits; GPU ⚠ never run. Only the `SHRD` branch exists |
| UWND | `WW_KOKKOS_UWND` | `WNT1` (with or without `RWND`), none of `WNT0 WNT2 OASACM MGW SMC WCOR STAB2` | `W3UWND` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the smoke 2026-10-16, alone with the other support parts (2 161 calls) and under `WW_KOKKOS_ALL=1` (v) and in the full (25 calls) (v); GPU ⚠ (`atan2f`, `fmodf`, `sincosf` from libdevice, `port_report/W3UWND_KOKKOS.md`) |
| UTRN | `WW_KOKKOS_UTRN` | `IC0` | `W3UTRN` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | judgement ⚠: likely on CPU (`expf`, `minss`/`maxss` operand order read from the object); the smoke and the full ran only `TRFLAG = 0` (`ATRNX = ATRNY = 1`), byte-identical (v); GPU ⚠ (`expf`); the `NX` or `NY` = 1 edge reads past the array go past the shim's copy instead of WW3's memory ⚠ |
| DZXY | `WW_KOKKOS_DZXY` | any (the `REFRX` call keeps the Fortran) | `W3DZXY` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16 (1 call, the depth gradient; the case has no current) (v); 0 calls in the smoke. No libm call; three products commuted in the assembly change only which NaN payload survives when two NaN meet (`port_report/W3DZXY_KOKKOS.md`) ⚠; GPU ⚠ never run, needs `--fmad=false` |
| MAP3, MAPT | `WW_KOKKOS_MAP3`, `_MAPT` | `PR3` (`MAP3` only where `GTYPE < 3`) | `W3MAP3`, `W3MAPT` in `W3WAVE` | `WW_KOKKOS_WAVE_COUNT` | measured, CPU: byte-identical in the full 2026-10-16 (1 call each on the 120 x 80 grid; the smoke grid is one cell) (v). `W3MAP3` is integer only, `W3MAPT` three `minss` and a compare with their operand order kept (port reports); GPU ⚠ never run. `W3MAP3`'s out-of-bounds read at `IX = 2, IY = 1` or `IX = NX-1, IY = NY` would read past the shim's copy, not WW3's memory ⚠ |
| DISTAB | `WW_KOKKOS_DISTAB` | any | `DISTAB` (with `WAVNU2`), run by `W3IOGR('READ')`: the Kokkos tables overwrite the Fortran's in `W3INIT` | `WW_KOKKOS_WAVE_COUNT`, and the count of differing values | measured, CPU: "0 of EWN1, ECG1(0:121), DSIE, N1MAX differ from the Fortran's" and byte-identical output in the smoke 2026-10-16 (v); GPU ⚠ (`tanhf`, `coshf`, `sinhf` from libdevice, `port_report/DISTAB_KOKKOS.md`) |

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
