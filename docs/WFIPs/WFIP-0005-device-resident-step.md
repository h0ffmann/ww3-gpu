# WFIP-0005: The device-resident time step: `VA` lives on the GPU, only forcing in and output out

| | |
|---|---|
| **Status** | Draft |
| **Author** | M. Hoffmann (asked 2026-10-17 in the project thread: the long-term solution after the batched `W3SRCE` shim) |
| **Created** | 2026-10-17 |
| **Deliverable** | D5 |
| **Related** | issue #42 (the port planner); [`H100_OPTIMIZATION_STRATEGY_202610.md`](../H100_OPTIMIZATION_STRATEGY_202610.md) steps 3 to 6; [`CPU_GPU_SPLIT_202610.md`](../CPU_GPU_SPLIT_202610.md); [`KOKKOS_H100_PLAN_202609.md`](../KOKKOS_H100_PLAN_202609.md) §5 and §6; [`PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md) §7 to §9; [ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md), [ADR-0013](../ADRs/ADR-0013-operational-parts-default-to-kokkos.md), [ADR-0014](../ADRs/ADR-0014-gpu-runs-judged-by-tolerance.md); the batched `W3SRCE` (PR #173); [WFIP-0003](WFIP-0003-kokkos-regtest-matrix.md) |
| **Effort** | L: one new shim and Fortran unit, three kernels re-pointed at a resident View, five CPU checkpoints and one supervised GPU run; no new physics |
| **Gain** | `proposal` (D5: the Kokkos kernels measured on the H100 and the operation decision); `science` (whether a whole WW3 step on one GPU beats 32 OpenMP threads, with the Fortran's answer within `tolerances.txt`) |
| **Effort vs Gain** | do when the batched `W3SRCE` shim lands: without it every unit here re-does its packing; with it, unit 1 can start the same day |
| **Depends on** | the batched shim of PR #173 (its device buffers become the resident `VA`); the propagation lane's per-call wiring of `W3XYP3`, `W3KTP3`, `W3QCK3`, `W3CFLXY` (§5.3, unit 3a); ADR-0014 accepted for the GPU verdict; the owner at the terminal for every GPU run |
| **Blocked by** | none |
| **Risk** | the GPU run passes `nccmp-tol` but is not faster than `--ww3` on 32 threads, because the `W3SRCE` body runs on one thread per team (`srce_kernel.hpp:176,267,277,396`) and the H100 wins only on points, not inside a point |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | [`WFIP-0005.tasks.md`](WFIP-0005.tasks.md) |
| **Tests** | none of the port's own (ADR-0010): `just smoke-ww3-gpu`, `just full-ww3-gpu`, `just ww3-gpu-matrix <jobs> all` and `just perf-ww3-gpu full 3` are the tests; a sync counter in `log.ww3` is a diagnostic |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

Keep the spectrum `VA(NSPEC,0:NSEALM)` in a device View for the whole time step of `W3WAVE`, so
that the source terms, the propagation and the output integrals read and write it there, and the
only host-device traffic of a step is the forcing that changed (in) and the output fields at output
times (out). The Fortran stays the owner of the state and stays correct with every part off: a
small sync protocol, called at exactly the statements of `W3WAVE` that read or write `VA` on the
host, copies only when the other side holds the newer bits. Six units, each its own PR with a CPU
bit-for-bit gate, built on the batched `W3SRCE` shim.

## 2. Motivation

Phase 1 copies one sea point per call and is slower than the Fortran on the CPU: `bench_small`
(120 x 80, 9 520 sea points, NK=NTH=24, NSPEC 576, 24 global steps) runs in 20.73 s with `--ww3`
on `switches/switch_lab_ompg` and 32 threads, and in 248.53 s with the Kokkos DIA on the OpenMP
backend, one point per call behind `!$OMP CRITICAL`, one run each (v, the owner's workstation,
Intel Core i9-14900, 2026-10-17; not yet a `bench/results/` record ⚠). On a GPU each of those calls
would be about ten PCIe copies and one launch for one point's work (ADR-0013, "What crosses
between host and device"), so a phase 1 GPU run is expected slower than `--ww3`, not faster ⚠.

The batched shim removes the per-call latency but not the bytes: it still moves the whole
spectrum to the device and back once per step, and `W3GATH`/`W3SCAT`, which the Fortran runs
between two source-term steps, need `VA` on the host. Every GPU port in the plan's survey reached
its speed only once the state was device-resident for the whole step; FESOM2 was 3.8x slower until
its halo data stayed on the device (v, `KOKKOS_H100_PLAN_202609.md` §5, read 2026-10-17).
`CPU_GPU_SPLIT_202610.md` puts the cost in numbers: two spectrum copies per step cost as much as
sixty to a hundred passes over it in HBM (illustration ⚠).

## 3. What changes for the reader

Before, the `Kokkos parts` line and the counters of `log.ww3` (`PATCH.md` §7) say which routine
ran where. After, `log.ww3` also carries a traffic line, printed by the resident unit at exit:

```text
  Kokkos VA: resident on OpenMP; H2D 1 (before step 1), D2H 13 (12 output times, 1 restart),
             forcing in 25 x 0.27 MB, fields out 12 x 0.04 MB; spectrum 21.94 MB
```

and `just perf-ww3-gpu full 3`, run by the owner, prints the median of three `ww3_shel` wall
times for `--ww3` (32 threads) and `--ww3-gpu --gpu`, then the `nccmp-tol` table of every field.
The line above is the shape, not a result ⚠; its numbers come from §5.5 and are checked by §7.

## 4. Sources and dependencies reviewed

### `W3WAVE` at the pin (`WW3/` at `4a94a8f6`, h0ffmann/WW3 `ww3-gpu/kokkos-wire-base`)

`VA` is `WDATAS(IMOD)%VA(NSPEC,0:NSEALM)`, column 0 a sink (v, `model/src/w3wdatmd.F90:491`);
under `SHRD`, `NSEAL = NSEALM = NSEA` (`w3initmd.F90:918-919`, `w3parall.F90:1030`). The
statements of one time step of `W3WAVE` that touch it on the host, outside `PDLIB`, `SMC`, `MPI`
and debug switches (v, `model/src/w3wavemd.F90`, read 2026-10-17):

| Line | Statement | `VA` | Inside `!$OMP PARALLEL` (`OMPG`)? |
|---|---|---|---|
| `:1298` | `W3UINI (VA)`, first step with `FLIWND` | write (all) | no |
| `:1337` | `W3UBPT` after `W3IOBC ('READ')`, boundary input | write (boundary points) | no |
| `:1380` | `W3UICE (VA)`, ice update | write (zeros under ice) | no |
| `:1464` | `W3ULEV (VA, VA)`, level change | read and write | no |
| `:1922`, `:2263` | `W3KTP3 (..., VA(:,JSEA), ...)`, intra-spectral parts 1 and 2 | read and write | yes, `:1862-1938`, `:2204-2279` |
| `:2024`, `:2082` | `W3GATH`, `W3SCAT` per `ISPEC`, around `W3XYP3` `:2064` | read row, write row | no (`DO ISPEC`, serial) |
| `:2397`, `:2411` | `W3KOKKOS_SRCE` / `W3SRCE (srce_direct, ..., VA(:,JSEA), ...)` | read and write | yes, `:2323-2457` |
| `:2591` | `W3CPRT (IMOD)` when `FLPART` | read (`w3iosfmd.F90:265`) | no |
| `:2593` | `W3OUTG (VA, ...)` when `FLOUTG` | read | no |
| `:2793`, `:2809` | `W3IOPE (VA)`, `W3IOTR (..., VA, ...)` | read | no |
| `:2811`, `:2898` | `W3IORS ('HOT')` | read (`w3iorsmd.F90:685,696`) | no |
| `:2818` | `W3IOBC ('WRITE')` | read (`w3iobcmd.F90:668`) | no |

`VA` is allocated by `W3DIMW` (`w3initmd.F90:954`) and filled by `W3IORS ('READ')` (`:1092`),
both after `W3KOKKOS_UNITS_SETUP` (`:847`), so a unit's set-up cannot bind it (§5.2).

The batched caller of `goal/42-srce-batch` (fork `ww3-gpu/kokkos-srce-batch`, `W3KOKKOS_SRCE_BATCH`
in `w3kokkosmd.F90`) sits before the `:2323` parallel region, packs `VA(:,JSEA)` of every point with
`MAPSTA = 1` and `FLAGST` into `BSPEC(NSPEC,NSEAL)`, and calls `ww_srce_batch` once (v, PR #173, fork
commit `caa5da36`, read 2026-10-17). The shim keeps per-grid device
buffers `b_spec`, `b_wn1`, `b_cg1`, `b_pin`, `b_psc`, ... in a `GridCtx` and chunks by
`WW_KOKKOS_SRCE_BATCH_MIB` (default 1 024 MiB) (v, `srce_shim.cpp:55-62,102-153`). `ww_kokkos` is
0.4.0 there; its compatibility mode is the batch PR's to set ⚠.

### What is translated and not wired (v, `kokkos/ports.tsv`, `PATCH.md` §8, 2026-10-17)

`W3XYP3`, `W3KTP3`, `W3QCK1`, `W3QCK3` are header-only device functions, compiled on three
presets, never run; `W3GATH`/`W3SCAT` are wired for `SHRD` only and are pure copies (`movss`, no
arithmetic); `W3OUTG` is translated (`iogo_w3outg.hpp`), not wired; the `W3SRCE` body runs under
`Kokkos::single` per team (`srce_kernel.hpp:176,267,277,396`) while the DIA inside it is
team-parallel; its work is about 56 KB per point ⚠ (a coordinator's count, not re-measured).

### Kokkos

`Kokkos::DualView` is the modify/sync protocol this WFIP needs: `modify<Space>()` marks a side,
`sync<Space>()` copies only when the other side was marked, and the constructor
`DualView(const t_dev&, const t_host&)` builds one from existing device and host Views (v,
[kokkos.org, Containers, DualView](https://kokkos.org/kokkos-core-wiki/API/containers/DualView.html),
read 2026-10-17). Whether it accepts an unmanaged host View over Fortran's storage is not shown
there ⚠, so §5.2 states the protocol itself and names `DualView` as the implementation to try first.

### Kokkos' Fortran Language Compatibility Layer (FLCL)

`kokkos/kokkos-fortran-interop`, BSD-3, read at `bf667c2` (2026-03-12) in a scratch clone on
2026-10-17 (v). Two routes to a Fortran array as a View: `view_from_ndarray` wraps an existing
array as an unmanaged `LayoutStride` View whose memory space under CUDA is `CudaUVMSpace`
(`src/flcl-cxx.hpp:59-65,185-216`, "host-gpu codes with UVM as the default memory space"); and
`kokkos_allocate_dualview` lets Fortran allocate an array as a `DualView`, handing back a
`c_f_pointer` over the host mirror with a 1-based shape (`src/flcl-dualview-f.f90:346,1270`),
while `modify`/`sync` are still called from C++ (`examples/02-axpy-dualview/axpy-dualview-cxx.cc:51-65`).
Its README asks for Kokkos 4.x (`README.md:13`); the pin here is 5.2.2, compatibility not checked ⚠.

**Pick:** the batch's `GridCtx` buffers become the resident `VA`; the protocol is DualView's; the
Fortran keeps ownership of the storage and of every statement in the table above. FLCL is weighed
in §9.

## 5. Design

### 5.1 Resident state

One C++ context per grid (`IGRID`, as every shim keys its `GridCtx`; `ww3_multi` runs several), in
`kokkos/src/fortran_iface/resident_shim.cpp` with `kokkos/src/ww_kokkos/resident_state.hpp`:

| View | Shape, layout | Mirrors | Lives |
|---|---|---|---|
| `d_va` | `View<float**, LayoutLeft, DeviceSpace>(NSPEC, NSEALM+1)` | `VA(NSPEC,0:NSEALM)`, spectrum contiguous per point, as WW3 has it | the run |
| `h_va` | unmanaged `View<float**, LayoutLeft, HostSpace>` over `VA`'s storage | the Fortran array itself | the run |
| `d_wn`, `d_cg` | `(NK, NSEA)` | `WN(1:NK,ISEA)`, `CG(1:NK,ISEA)` | the run; rewritten after `W3ULEV` |
| `d_force` | `(7, NSEA)` | `DW, U10, U10D, AS, ICE, RHOAIR, ASF` (the batch's `pin` rows) | one step |
| `d_psc` | `(21, NSEA)` | `UST, USTDIR, TAUWX, ..., WNMEAN` (the batch's `psc` rows) | the run from unit 4 |
| `d_field` | `(NSPEC_chunk, (NX+2)*(NY+2))` | `FIELD(1-NY:NY*(NX+2))` per `ISPEC` (`w3wavemd.F90:774`) | unit 3b |

`LayoutLeft` keeps `VA(ISPEC,JSEA)` at `d_va(ISPEC-1, JSEA)`: the source terms read one point
contiguously, the device transpose of unit 3b reads a strided row, as `W3GATH` does on the host.
The layout question of `H100_OPTIMIZATION_STRATEGY_202610.md` ("Propagation") is measured in
unit 6, not decided here: the Fortran's layout is the only one until a timing says otherwise.
Memory, in bytes: `d_va` is `4 NSPEC (NSEA+1)`; the propagation field at most the same again; the
source-term work is the batch's, about 56 KB per point ⚠, bounded by its chunk cap.

### 5.2 Ownership protocol

Three states per grid: `HOST` (the Fortran array has the newer bits), `DEVICE` (`d_va` has them),
`BOTH` (equal). Four calls, `extern "C"` in the shim and wrapped in `W3KOKKOS_RESIDENTMD`
(`model/src/w3kokkos_residentmd.F90`, a unit module of `PATCH.md` §9). Every wrapper passes `VA`
and its shape; the shim binds on the first call per grid and refuses a later call whose address
or shape differs (`ww3_multi` re-points `VA` through `W3SETW`), since set-up runs before `VA` exists.

| Call | When the Fortran calls it | Does |
|---|---|---|
| `ww_va_host_read (igrid, va, nspec, nsealm)` | before every host read of the table in §4 | `DEVICE` → D2H, state `BOTH`; else nothing |
| `ww_va_host_write (...)` | before every host write of that table | `ww_va_host_read`, then state `HOST` |
| `ww_va_device (igrid, write)` | inside a shim, before its kernel reads `d_va` | `HOST` → H2D, state `BOTH`; `write` sets `DEVICE` after the launch |
| `ww_va_report ()` | `W3KOKKOS_REPORT` at exit, under `WW_KOKKOS_VA_COUNT=1` | prints the traffic line of §3 |

With every part off, no shim ever calls `ww_va_device`, the state never leaves `HOST`, and the
host calls are no-ops: the Fortran is untouched, which is what makes the gate of each unit a
byte comparison. With a part on, a host statement always sees the newer bits, whichever side
wrote them, so turning one part off at a time (bisecting a difference, ADR-0011) stays correct.
The wrapper refuses a call from inside an OpenMP parallel region (`omp_in_parallel()`, `EXTCDE`):
every site of the table is outside one, and the two `W3KTP3` sites get their call before the
region opens. A diagnostic flag `WW_KOKKOS_VA_RESIDENT=1` makes `ww_va_host_read` copy even in
state `BOTH` and `ww_va_host_write` push to the device after the statement, so the placement of
every call is exercised on the CPU, where both copies are `memcpy` and the bits cannot change.

### 5.3 Units, owners, gates

Each unit is one PR, on a fork branch from `ww3-gpu/kokkos-wire-base` (`PATCH.md` §9) and a
`goal/42-...` branch here. Its CPU gate is `scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos all`
and `--ww3-gpu full --cpu --kokkos all`, each with `WW3GPU_SWITCH=switch_lab_ompg` (the default)
and `WW3GPU_SWITCH=switch_lab_shrd`, with `cmp` finding `out_grd.ww3`, `out_pnt.ww3`,
`restart.ww3` and the `ww3_ounf` file byte-identical to the Fortran of the same executable and to
`--ww3`, as the 2026-10-16 checkpoint did (`PATCH.md` §8); ADR-0011 adds the matrix at the pin
move. "Lane" names the sibling checkouts of this machine (`git -C <checkout> branch
--show-current` over the owner's sibling checkouts, 2026-10-17): the propagation lane is `ww3-gpu-prop`
(`claude/42-port-propagation`), `ww3-gpu-prop3` (detached at `898635f`, with an untracked
`cflxy_shim.cpp`) and `ww3-gpu-units` (`goal/42-wire-units`); the batch lane is `ww3-gpu-3-batch`
(`goal/42-srce-batch`).

| Unit | What | Owner | Starts when |
|---|---|---|---|
| 1 | `resident_shim.cpp`, `w3kokkos_residentmd.F90`, a call at every host site of §4's table, the counter; no kernel reads `d_va` yet | this goal | now |
| 2 | `ww_srce_batch` reads and writes `d_va` through the point list `BJSEA` (a device gather); `BSPEC` and its two copies go; `wn1`, `cg1`, `pin`, `psc` still cross | this goal, after the batch PR merges | batch merged |
| 3a | per-call shims and `W3WAVE` hunks for `W3XYP3`, `W3KTP3`, `W3QCK3`, `W3CFLXY`, phase 1, CPU checkpoint | the propagation lane (already started) | theirs |
| 3b | `W3KTP3` as one team per point on `d_va`; `W3GATH`/`W3SCAT` as a device transpose into `d_field`; `W3XYP3`+`W3QCK3` one launch per `ISPEC` chunk, the Fortran's sweep order inside each component | the propagation lane by default; this goal only if that lane hands it over (a row in `kokkos/port_claims.tsv` and a comment on #42, before any file is touched) | 3a merged |
| 4 | `d_wn`, `d_cg`, `d_psc` resident; `d_force` filled only on steps where `W3UWND`/`W3UCUR`/`W3UICE`/`W3ULEV` ran (their flags `FLFRST`, `IDACT`) | this goal | 2 merged |
| 5 | `W3OUTG`'s integrals on `d_va` (`iogo_w3outg.hpp`, one team per point, sums in the Fortran's order), the requested fields D2H; `VA` D2H only for `W3IORS`, `W3IOPE`, `W3IOTR`, `W3IOBC ('WRITE')`, `W3CPRT` | this goal | 3b merged |
| 6 | the CUDA build, the owner's supervised `just perf-ww3-gpu full 3`, the layout timing, the record | the owner at the terminal; this goal prepares | 5 merged, ADR-0014 accepted |

No unit edits `w3kokkosmd.F90`, `ww_kokkos_c.hpp`, `scripts/ww3-gpu.sh` or the propagation lane's
`prop_*.hpp` and `cflxy_*` files (`PATCH.md` §9, "Never edit"); unit 2 edits `srce_shim.cpp`
only after the batch PR is on `main`. Everything runs on the CPU (Kokkos OpenMP) except unit 6.

After unit 5, the host keeps `W3INIT`, the forcing readers, `W3UWND`/`W3UCUR`/`W3UICE`/`W3ULEV`
(host code; their outputs cross as `d_force`), every I/O routine, `W3CPRT` and the time arithmetic
of `W3WAVE`; the device runs `W3SRCE` with every source term, `W3KTP3`, `W3GATH`/`W3XYP3`/`W3SCAT`
and `W3OUTG`'s integrals. On the CPU the result is the Fortran's bits (the gate); on CUDA it is
deterministic run to run (no atomics, no tree reduction, the Fortran's order per point) and judged
by tolerance (ADR-0014).

### 5.4 Traffic per step

Bytes per global step, from `NSEA`, `NSPEC`, `NK`; `B_VA = 4 NSPEC (NSEA+1)`. The `bench_small`
column is NSEA 9 520, NSPEC 576, NK 24 (`B_VA` = 21.94 MB), field output every 3 600 s, so every
second step (`scripts/ww3-gpu.sh:457`), one restart at the end. Copy counts are read from the
shims' structure ⚠; GPU figures are ⚠ until unit 6.

| After | In per step | Out per step | Copies per step | `bench_small` per step |
|---|---|---|---|---|
| phase 1 (today) | `B_VA + 4 NSEA (2 NK + 36)` | `B_VA + 4 NSEA 30` | about 14 `NSEA` ⚠ | 25.1 + 23.1 MB in about 133 000 copies |
| batch | the same | the same | 8 + 6 | 48.2 MB |
| unit 1 | the same | the same | the same | the same, plus `2 B_VA` per host site under `WW_KOKKOS_VA_RESIDENT=1` |
| unit 2 | the same | the same | 2 + 5 | 48.2 MB (the same bytes, fewer copies) |
| unit 3b | `4 NSEA (2 NK + 36)` | `4 NSEA 30`, plus `B_VA` at output times | 5 + 1, plus 1 at output times | 3.2 + 1.1 MB, plus 21.9 MB every second step |
| unit 4 | `4 NSEA 7` on forcing steps | `B_VA` at output times | 1, plus 1 at output times | 0.27 MB on each of the 25 forcing updates ⚠, plus 21.9 MB every second step |
| unit 5 | `4 NSEA 7` on forcing steps | `4 NSEA` per output field at output times; `B_VA` once (restart) | 1, plus the fields | 0.27 MB; 0.04 MB per field every second step; 21.9 MB once |

The forcing-step count is the number of `W3UWND` calls, 25 in the full case (v, `PATCH.md` §8
counters); the others ran once. Point and track output of `bench_small` are not checked ⚠; each
would add a `B_VA` copy at its times until a device gather of the output points exists (§11).

## 6. Parity and physics impact

The arithmetic does not change: every kernel keeps the translation that is wired today, and reads
its spectrum from `d_va` instead of a copy of it. On the CPU the gate is bytes (§5.3, `test.comp`,
`WW3GPU_STRICT=ON`), and a difference is a defect of the unit until a Fortran/Fortran run shows WW3
does it too. On CUDA the verdict is `nccmp-tol` with `tolerances.txt` (`hs`, `fp`, `dir`, `dp`,
`t0m1`), never widened (ADR-0014); the smoke judges one field of 26 and the full case `hs` of
five ⚠, so unit 6 asks the owner whether to list more fields before the run, as a PR of its own.
The regtest entries that judge the units are those whose switch has `NL1 ST4 PR3 UQ` and no
`MPI`: `just ww3-gpu-matrix <jobs> serial` and `omp` (WFIP-0003), with `WW3GPU_ROUTINES=SRCE` and
the propagation parts once 3a names them. Reductions keep the Fortran's order per point; a sum
over points (none in these kernels) would be asked about first.

## 7. Verification plan and definition of done

- [ ] Unit 1 merged: every host site of §4's table has its call, smoke and full byte-identical with `--kokkos all` under `switch_lab_ompg` and `switch_lab_shrd`, with and without `WW_KOKKOS_VA_RESIDENT=1`; the traffic line counts 2 copies per site under the flag: `WW3GPU_SWITCH=switch_lab_shrd WW_KOKKOS_VA_RESIDENT=1 WW_KOKKOS_VA_COUNT=1 scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos all`
- [ ] Unit 2 merged: `ww_srce_batch` reads `d_va`, `WW_KOKKOS_SRCE_COUNT` equals the Fortran loop's point count, smoke and full byte-identical on both switches: `just smoke-ww3-gpu && just full-ww3-gpu`
- [ ] Unit 3b merged (by its owner): `W3GATH`/`W3SCAT` counters 0 and the device transpose's counter `NSPEC` per step, full byte-identical, the `PR3 UQ` entries of the matrix byte-identical: `WW3GPU_ROUTINES=SRCE,XYP3,KTP3 just ww3-gpu-matrix 8 serial` and `omp`
- [ ] Unit 4 merged: the traffic line shows no `B_VA` copy outside output times in the full case: `WW_KOKKOS_VA_COUNT=1 just full-ww3-gpu`
- [ ] Unit 5 merged: `W3OUTG` on `d_va`, the `ww3_ounf` file byte-identical on the CPU, one `B_VA` D2H in the full run (the restart): `WW_KOKKOS_VA_COUNT=1 just full-ww3-gpu`
- [ ] The CUDA build compiles every unit and the GPU smoke passes `nccmp-tol` under the owner's supervision: `just smoke-ww3-gpu gpu`
- [ ] A dated `docs/log/` entry records each checkpoint, its wall time and any difference bisected by flag: `ls docs/log/2026-*-resident-*.md`
- [ ] The owner's supervised `just perf-ww3-gpu full 3` ran, `nccmp-tol` passed on every listed field, the GPU median is lower than `--ww3`'s on OpenMP with 32 threads, and the record says "GPU: judged by tolerance, not bits": `just perf-ww3-gpu full 3 && python3 scripts/results.py record && python3 scripts/results.py check`

## 8. Risks, limitations, and honest caveats

- **Bits on the GPU** (ADR-0014). libdevice's `expf`, `logf`, `tanhf`, `sincosf`, `powf` and the
  five libmvec loops of ST4 differ from glibc by a few ULP per step; how far that grows over 24
  steps is unknown until the first supervised run ⚠; a field outside `tolerances.txt` is
  reported, not judged.
- **Speed is not promised.** `W3SRCE`'s body is serial per team; 9 520 teams of one active thread
  may not fill an H100 ⚠ (not measured). If unit 6 is not faster than 32 threads, the next step is
  inside the point (the `TeamThreadRange` loops of `KOKKOS_H100_PLAN_202609.md` §6), a reordering
  that is its own PR with the owner's leave, not part of this WFIP.
- **OMPG.** The Fortran path inside the parallel regions keeps its `!$OMP CRITICAL` and stays the
  bisection path; the resident path never runs a kernel from inside a region (§5.2). The 25 `OMPG`
  entries of the matrix (`ww3_ts3`) judge it.
- **Memory.** `bench_small` needs 22 MB for `d_va` and about 0.5 GB of source-term work ⚠; a
  grid of 100 000 points at NSPEC 1 152 needs 461 MB and about 5.6 GB ⚠; a GFS-Wave-sized grid
  (NSPEC 1 800, sea-point count not read ⚠) needs a few GB for `d_va` and tens of GB of work,
  which the batch's chunk cap bounds on a 24 GB RTX 4090 and which fits whole on an 80 GB H100 ⚠.
  The 56 KB per point is the number to shrink if memory, not time, is the limit.
- **Deferred.** `ww3_multi` (one context per grid exists; the mosaic's `WMUPDT` moves spectra
  between grids on the host and would need a `host_read` per grid, not designed here); `DIST MPI`
  (the `MPI` branches of `W3GATH`/`W3SCAT` and `W3MPIO` are Fortran; one device per rank is a
  later WFIP, `H100_OPTIMIZATION_STRATEGY_202610.md` step 8); `PDLIB` (its `VAOLD` copy and
  exchanges, `w3wavemd.F90:1141,1352`); SMC and unstructured grids; point output gathered on
  the device.
- **One supervisor.** Every GPU run needs the owner at the terminal (AGENTS.md); units 1 to 5
  need none, which is why they come first.

## 9. Alternatives considered

- **Do nothing beyond the batch.** Keeps `2 B_VA` per step crossing PCIe; the plan's survey says
  that is where GPU ports stall.
- **Unified Memory** (`CudaUVMSpace`), by hand or through FLCL's `view_from_ndarray`, whose CUDA
  space is `CudaUVMSpace` (v, §4). Hides the copies instead of removing them; AGENTS.md forbids it
  as a way to avoid thinking about transfers, and it makes the traffic line meaningless.
- **`VA` allocated as a `DualView` through FLCL's `kokkos_allocate_dualview`**, in `W3DIMW`
  under the `KOKKOS` switch, against (a) the context-owned device View of §5.1 with the sync calls
  of §5.2 at the fork's `VA` statements. What FLCL buys is the pairing of the two Views at
  allocation; what it costs: `W3DIMW` is `W3WDATMD`'s allocation (`w3wdatmd.F90:491`), not a shim
  or a caller hunk, so the edit goes past the patch AGENTS.md allows and is the owner's decision;
  `VA`'s lower bound 0 needs a pointer remap after FLCL's 1-based `c_f_pointer` (v, §4) ⚠ not tried;
  every `modify`/`sync` is still a call at the same host statements, from C++ (v, its example), so
  the protocol of §5.2 is not saved, only re-spelt; a 4.x library against Kokkos 5.2.2 ⚠; and a
  `DualView` host mirror is Kokkos' allocation, so with every part off the Fortran no longer owns
  its own array, which weakens the "every part off is the Fortran" gate. **Recommended: (a).** It
  stays inside the shim-and-caller patch, keeps WW3's allocation untouched, and
  `DualView(const t_dev&, const t_host&)` gives the same pairing from the C++ side (v, §4). FLCL is
  worth revisiting if the owner opens `W3WDATMD` to the patch, or for the host driver of
  `W3WAVE_KOKKOS_PLAN_202610.md`, where C++ owns the state anyway.
- **Translate `W3WAVE` whole into C++ first** (`W3WAVE_KOKKOS_PLAN_202610.md`, the host driver
  drafted in a sibling checkout, not on `main` ⚠).
  It moves ownership of the state to C++ and loses the "every part off is the Fortran" gate; the
  sync protocol gives the resident step without it, and the driver can adopt the same Views later.
- **`[NSPEC][NSEA]` layout now** for propagation. A guess until timed; the Fortran's layout keeps
  every wired kernel's indexing and the host mirror trivial.
- **Split by sea points between CPU and GPU** (`CPU_GPU_SPLIT_202610.md`, "Next"). A tuning after
  this WFIP, when there is a resident step to split.

## 11. Open questions

- Does `Kokkos::DualView` accept an unmanaged host View over Fortran's storage? **Default:**
  unit 1 tries it and falls back to the two-View struct of §5.2; the implementer decides.
- Should the propagation lane also own unit 3b? **Default:** yes, by the hand-over rule of §5.3;
  the owner decides on #42.
- Which fields does `tolerances.txt` list for unit 6? **Default:** the five it has; the owner
  decides in a PR of its own before the supervised run (ADR-0014, point 3).
- Point and track output on `bench_small`: gather the output points on the device, or pay a
  `B_VA` copy at their times? **Default:** pay the copy and count it; revisit if the traffic line
  shows it dominates.
- What is the baseline unit 6 must beat? **Default:** `--ww3` on `switch_lab_ompg` with every
  thread, median of three, from the same `perf-ww3-gpu` run and recorded by
  `scripts/results.py record`; the owner's 20.73 s of 2026-10-17 is the expected size ⚠.

## Appendix

### Checked live

- `WW3/model/src/w3wavemd.F90` at `4a94a8f6`, 2026-10-17: every `VA` reference (`grep -n '\bVA\b'`),
  the `!$OMP` directives (`:1616` is `OMP0`; `:1761`, `:1862-1938`, `:2204-2279`, `:2323-2457`,
  `:3855-3880` are `OMPG`), the calls at the lines of §4; `w3wdatmd.F90:491`; `w3initmd.F90:847,918-919,954,1092`;
  `w3parall.F90:1030`; `w3updtmd.F90:1063,1329,1806,1965,2001,2066,2395-2538`; `w3iosfmd.F90:180,265`;
  `w3iorsmd.F90:685,696`; `w3iobcmd.F90:668`; `w3iogomd.F90:1201,1514-1516`.
- PR #173, 2026-10-17: fork `ww3-gpu/kokkos-srce-batch` at `caa5da36` (one commit on `4a94a8f6`),
  hunks in `w3wavemd.F90` and `w3kokkosmd.F90` (`W3KOKKOS_SRCE_BATCH`); `kokkos/src/fortran_iface/srce_shim.cpp:55-62,102-153,223-300`;
  `ww_kokkos_c.hpp:258-270`; `kokkos/CMakeLists.txt:12,81` (0.4.0).
- `kokkos/ports.tsv`, `kokkos/src/fortran_iface/PATCH.md` §7 to §9, `kokkos/PORT_STATUS.md`
  (level-0 scratch), ADR-0013, `docs/H100_OPTIMIZATION_STRATEGY_202610.md`,
  `docs/CPU_GPU_SPLIT_202610.md`, `docs/KOKKOS_H100_PLAN_202609.md` §5 and §6, `scripts/ww3-gpu.sh:244,457`,
  `kokkos/tests/fixtures/bench_small/ww3_grid.nml:5-6,16-19,31-32`, `kokkos/tools/nccmp-tol/tolerances.txt`,
  `kokkos/src/ww_kokkos/srce_kernel.hpp:176`: read 2026-10-17 in this worktree.
- ADR-0014: Accepted, on `main` since PR #170 (merged 2026-10-17).
- Kokkos DualView documentation, the URL of §4, 2026-10-17: `modify()`, `sync()`, the two-View constructor.
- FLCL, `kokkos/kokkos-fortran-interop` at `bf667c2`, a scratch clone, 2026-10-17: `LICENSE.md:13`
  (BSD-3), `README.md:13` (Kokkos 4.x), `src/flcl-cxx.hpp:59-65,185-216`,
  `src/flcl-dualview-f.f90:47-48,346,1270`, `examples/02-axpy-dualview/axpy-dualview-cxx.cc:51-65`.
- Sibling checkouts and their branches, 2026-10-17: the `for d in ...` loop of §5.3.

### Not checked

- The 20.73 s and 248.53 s of §2 (one run each, the owner's session, no record yet) ⚠.
- The 56 KB of source-term work per point, the number of copies per phase 1 call, and every GPU
  memory and time figure of §5.4 and §8 ⚠.
- Whether `Kokkos::DualView` takes an unmanaged host View ⚠; the batch PR's final ABI compatibility mode ⚠.
- Whether FLCL builds against Kokkos 5.2.2, and whether its 1-based host pointer can be remapped to `VA(:,0:)` ⚠.
- `bench_small`'s point and track output, and GFS-Wave's sea-point count ⚠.
