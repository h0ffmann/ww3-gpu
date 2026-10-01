# 10 — Modern Fortran refactoring: the step before any port

Step 3 of the ladder. The profile from [lesson 09](09-benchmark-profile-compile-run.md)
names the routines; this lesson rewrites them in place (same algorithm, same arithmetic,
standard Fortran 2008/2018) and proves per routine that nothing changed. It is worth
doing on its own (the gain is measured on the model the lab runs), and it is what makes a
routine portable at all: a routine with an explicit interface and contiguous data can be
replaced by a kernel; one that reads module state through five `USE` lines cannot.

## What a profile-top routine looks like

`W3SNL1` (`WW3/model/src/w3snl1md.F90`, lines 115–473 in 7.14 (v)) is the DIA source
term and the first thing the lab ported. Its interface is already good:

```fortran
    REAL, INTENT(IN)        :: A(NSPEC), CG(NK), KDMEAN
    REAL, INTENT(OUT)       :: S(NSPEC), D(NSPEC)
```

Its workspace is not. Every call declares, sized at run time from module variables (v,
`kokkos/tests/fixtures/snl1_ref.F90` lines 335–339, verbatim from WW3 lines 324–328):

```fortran
    REAL               ::  UE  (1-NTH:NSPECY), SA1 (1-NTH:NSPECX),  &
         SA2 (1-NTH:NSPECX), DA1C(1-NTH:NSPECX),  &
         DA1P(1-NTH:NSPECX), DA1M(1-NTH:NSPECX),  &
         DA2C(1-NTH:NSPECX), DA2P(1-NTH:NSPECX),  &
         DA2M(1-NTH:NSPECX), CON (      NSPEC )
```

Ten **automatic arrays**, one of them with a lower bound of `1-NTH` so that clamped
quadruplet addresses can read zeros. `NK`, `NTH`, `SIG`, the 32 address tables
`IP11..IC82`, the weights `AWG1..8` and `AF11` all arrive through `USE W3GDATMD` /
`USE W3ADATMD` (v). Test output and tracing sit behind `#ifdef W3_T` / `W3_S` (v). This is
the shape of most of WW3's inner routines, and `W3SRCE` is the same shape with dozens of
locals and 239 preprocessor guards in its file (`docs/KOKKOS_H100_PLAN_202609.md`, ⚠ counted
there, not here).

The properties that matter: per point, no I/O, reads shared tables, writes two arrays.
Everything in this lesson is about making those properties *visible in the interface*.

## The checklist

| Construct | What it buys | Where `W3SNL1` stands |
|---|---|---|
| Module procedure (explicit interface) | the compiler checks every call; a kernel can replace the body | already: `MODULE W3SNL1MD` (v) |
| `INTENT(IN/OUT/INOUT)` on every argument | the data-flow contract a shim needs | already (v) |
| `CONTIGUOUS` on assumed-shape dummies | no copy-in on the call, a pointer that C can take | not used; WW3 passes explicit-shape `A(NSPEC)` (v), which is contiguous anyway |
| `PURE` | no hidden I/O or global writes; required for `DO CONCURRENT` bodies | blocked by the `#ifdef W3_T` `WRITE` (v) |
| Workspace by argument or per-grid allocation | no automatic arrays, no per-call stack traffic | the ten arrays above |
| Tables passed in, not `USE`d | the routine can be called from a test with captured inputs | `snl1_ref.F90` does this for the reference copy (v) |

Do them in that order. The first two cost nothing in WW3 because the code is already
modular; the last two are the work.

## `DO CONCURRENT`

`gpu/02_do_concurrent.f90` is the example: the dispersion relation solved by Newton at
every (point, frequency), in standard Fortran with no directives (v):

```fortran
  do concurrent (ip = 1:npt, ik = 1:nk) local(x, kd, f, fp, t, it)
```

One source, three targets: `nvfortran -stdpar=gpu -gpu=cc89`, `nvfortran
-stdpar=multicore`, plain `gfortran -O3` (v, header of that file). The file's own
caveats (v): `LOCAL()` is Fortran 2018 locality, and if a compiler rejects it you may drop
the clause, but then every scalar assigned inside the loop is one the compiler has to
prove private on its own, and `-Minfo=stdpar` must be read to see whether it did or
whether the loop went serial. Also `-stdpar=gpu` moves allocatables to CUDA managed
memory, so data movement becomes implicit: convenient, and exactly how a naive port
ends up thrashing PCIe without noticing ⚠ (the file says so; not measured here).

The construct is honest about one thing the directives are not: the loop body must be
independent per iteration and free of side effects. Writing `W3SNL1`'s section 3 as a
`DO CONCURRENT` over `ISP` is legal; writing its section 2 tail loop that way is not,
because row `IFR` reads row `IFR-1` (v, `snl1_ref.F90` `UE(ISP) = UE(ISP-NTH) * FACHFE`).
The port in lesson 12 keeps that loop sequential for the same reason.

## Killing automatic arrays

Three ways, in increasing distance from the original:

1. **Allocate once per grid.** WW3 already does this for the tables: `INSNL1` section 6
   calls `W3DMNL`, which allocates `IP11..AF11` in `W3ADATMD` once (v, `snl1_ref.F90`
   comment on section 6). Do the same for `UE`, `SA1`…: module-level `ALLOCATABLE`
   workspace sized `1-NTH:NSPECY` at grid setup. Cost: thread-private copies under OpenMP.
2. **Pass workspace in.** Add a `WORK` derived type or explicit dummies. The routine
   becomes testable in isolation and the caller decides who owns the memory.
3. **Team scratch on the device.** The Kokkos version gives each sea point its own
   scratch and offsets every index by `+NTH` so the `1-NTH` lower bound becomes slot 0
   (v, `kokkos/src/ww_kokkos/snl1_dia.cpp`). Lesson 12.

Whichever you pick, the arithmetic inside the loops does not change. That is the rule the
parity test enforces.

## The runtime switch: one binary, two paths

The proposal's method for step 3 (`pubs/proposal/pt/07-methodology.md`): the refactored
routine lives next to the original in a fork, and a switch read once at start-up picks
one. The regression matrix and `nccmp-tol` then run both paths without a rebuild.

The lab's Kokkos interface module shows the pattern (v, `kokkos/src/fortran_iface/w3kokkosmd.F90`):

```fortran
  LOGICAL :: KOKKOS_SNL1 = .FALSE.
  ...
  SUBROUTINE W3KOKKOS_SETUP
    KOKKOS_SNL1 = ( WW_SNL1_ENABLED() == 1_C_INT )
  END SUBROUTINE W3KOKKOS_SETUP
```

`WW_SNL1_ENABLED()` reports whether `WW_KOKKOS_SNL1=1` was in the environment when the
runtime started (v, `ww_kokkos_c.hpp`). The model reads a `LOGICAL` in its inner loop, not
an environment variable, and not a C call per sea point. A Fortran-only refactor uses the
identical shape: `W3SNL1_NEW` beside `W3SNL1`, one logical, `IF (…) THEN CALL … ELSE
CALL … END IF` at the single call site. Per-call branches inside kernels are the
anti-pattern (`docs/AGENTS_KOKKOS_202609.md` §1.6).

## The per-routine parity test

The matrix compares whole runs. Step 3 needs something finer: run the original and the
rewrite on the *same captured inputs* and compare the outputs.

`exercises/solutions/ex11_refactor.F90` holds a 40-line WW3-style routine (automatic
array, implicit interface) and its modern twin; `exercises/solutions/ex11_refactor_test.F90`
drives both on random input and stops with a non-zero status above 1e-6, built with plain
`gfortran`. That is the small version.

The full-size version is the fixture generator the port uses (v,
`kokkos/tests/fixtures/gen_snl1_fixture.F90`): call the verbatim reference on a
deterministic sea state, stream inputs, tables and outputs into a little-endian file with
`ACCESS='STREAM'`, refuse to write if the default `REAL` is not 32-bit, commit the file.
Any rewrite, Fortran or C++, is then tested against the same bytes. Same arithmetic
should give the same bits; 1e-6 relative is the room you leave for a compiler that
reassociates a sum. If you need more room than that, you changed the algorithm, and that
is a separate change with its own evidence.

## The contrast: directives

`gpu/00_hello_acc.f90` is the smallest OpenACC program that proves the toolchain (v):
`!$acc data create(a, b)`, two `!$acc parallel loop`s, a `reduction(+:s)`, and the advice
that if `NVCOMPILER_ACC_NOTIFY=1` prints no launch line you built a CPU binary. Reading
`-Minfo=accel` is most of OpenACC development.

Ikuyajolu et al. (2023) did this to `W3SRCEMD`: OpenACC on the source-term driver, MPI
for the rest, V100 nodes with NVLink. Result: about 1.3× against 42 CPU cores, and the
paper is explicit about why (lesson 09's aside): the state lives in Fortran module arrays
on the host, so every call ships the spectrum across the bus, and a routine with that many
locals starves the GPU of occupancy. Directives change *where a loop runs*; they do not
change *who owns the data*. That ownership question is the whole content of lesson 11: a
`View` names its memory space, a kernel can only touch what it was handed, and the
residency ladder in lesson 13 is the plan for moving the state, one array at a time.

## Checklist for one routine

1. Profile says it matters (lesson 09). If not, stop.
2. Capture its inputs and outputs with a generator like `gen_snl1_fixture.F90`.
3. Explicit interface, `INTENT`, tables as arguments, workspace out of the body.
4. Runtime switch, both paths in the binary.
5. Parity test on the capture; then the matrix on the closest regtest; then `nccmp-tol`
   on the operational case.
6. Re-profile. The residual is what lesson 12 is allowed to port.

→ [`11-kokkos-and-modern-cpp.md`](11-kokkos-and-modern-cpp.md)
