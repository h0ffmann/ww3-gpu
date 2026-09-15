# `PATCH.md` — wiring the Kokkos DIA into WAVEWATCH III

> **Not applied in this repository.** `WW3/` here is an unmodified upstream
> checkout and stays that way; this file is the recipe for the fork branch
> (`just src-pr` / a `WW3` fork), written so that it can be applied and reviewed
> hunk by hunk. Everything below was read off the tree at
> `WW3/model/src` (WAVEWATCH III 7.14, `develop`) — the line numbers are real and
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

The shim holds exactly one set of tables and one set of device buffers in a
file-static context, and nothing in it is locked. Three limits follow, and the
patch below has to respect all three — each one is a wrong answer or an
out-of-bounds read, not a slowdown:

| limit | why | where the patch handles it |
|---|---|---|
| **One spectral grid per process** | `ww_snl1_init` *replaces* the tables; it does not add a grid. A `ww3_multi` run whose grids differ in `NK*NTH` would read past the end of the later grids' spectra. | hunk 2 aborts with `EXTCDE` when `IMOD > 1` |
| **One caller at a time** | Two threads in `ww_snl1` share the context, and one may be reallocating the device buffers while the other copies into them. | hunk 1c wraps the call in `!$OMP CRITICAL` |
| **`SIG` must start at bin 1** | the C dummy is assumed-size, so the actual argument's lower bound is lost; `W3GDATMD` allocates `SIG(0:MK+1)` | hunk 2 passes `SIG(1)` |

---

## 1. The call site — `model/src/w3srcemd.F90`

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
  at 1** — exactly the `NPTS = 1` case of the shim's batch layout, so they can be
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

### Hunk 1a — the `USE` (line 578)

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
`W3SRCE` does import `NDSE` — but at line 652, *inside* `#ifdef W3_NNT`:

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
`W3_KOKKOS` guard so that neither is a duplicate when `W3_NNT` is on — `ONLY`
imports of the same entity from the same module are allowed to repeat.

### Hunk 1b — a one-element `KDMEAN` (near the locals at line 712)

```diff
@@ -712,6 +712,10 @@
     REAL :: SPECINIT(NSPEC), SPEC2(NSPEC)
+#ifdef W3_KOKKOS
+    ! The shim takes KDMEAN as an array of NPTS; here NPTS is 1.
+    REAL    :: KDM_K(1)
+    INTEGER :: IERR_K
+#endif
```

### Hunk 1c — the branch (lines 1258–1264)

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

### Threading — why the `!$OMP CRITICAL` is there

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
over a device allocation — a segfault or silent corruption, not a rounding
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

## 2. Set-up — `model/src/w3initmd.F90`

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
added to the existing `USE W3GDATMD, ONLY: ...` list — `NK`, `NTH` and `SIG` are
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
bare name `SIG` and the shim reads `SIG(0:NK-1)` — every quadruplet built one
frequency bin low, every `AF11` scaled by the wrong `f**11`, and a source term
that is smooth, plausible and wrong. `SIG(1)` is WW3's own sequence-association
idiom for exactly this and costs nothing.

The lab's reference cannot catch this: `snl1_ref.F90:86` allocates `SIG(NK)`, so
`shim_roundtrip` passes either way. The check belongs on the fork branch, in the
`WW_KOKKOS_SNL1=0` vs `=1` comparison of section 5.

### Multi-grid

`ww3_multi` calls `W3INIT` once per grid. The guard above turns that into a
clean abort with a message rather than corruption, which is the honest phase-1
behaviour: the shim keeps one `Config`, one `Tables` and one set of device
buffers, and a second `WW_SNL1_INIT` releases the first grid's. Supporting
several grids means giving the shim a handle per grid — a real API change, and
deliberately out of scope here.

---

## 3. The switch — `model/src/cmake/switches.json`

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
`kokkos/src/fortran_iface/w3kokkosmd.F90` into `model/src/` unchanged — it has
no dependency on anything else in this lab, which is why it is built as its own
target here (`ww_kokkos_f`) rather than folded into the C++ library.

---

## 4. The link — `model/src/CMakeLists.txt`

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
same mechanism that lets `kokkos/tests/fixtures/shim_driver` — a Fortran
program linking `ww_kokkos_f` — link here today.

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
two answers that must agree to the tolerance the L1 tests already hold the
kernel to. It is the L2 row of `kokkos/PORT_STATUS.md`, and it is what this
patch is for.
