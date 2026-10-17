# W3SRCE_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the checkpoint of the W3SRCE
unit (ADR-0011), itself after the ST4 checkpoint. Selected by `WW_KOKKOS_SRCE=1`; the flag
defaults to 0, so WW3 runs the Fortran. Wired: WW3 at the `WW3/` pin (`513954a6`,
`ww3-gpu/kokkos-resident`) calls it for every sea point of a time step in one call at
`w3wavemd.F90:2383-2384` (`W3KOKKOS_SRCE_BATCH`, set-up `w3initmd.F90:825`) behind
`WW_KOKKOS_SRCE` (default off), on the resident spectrum; see
[Batched path](#batched-path-phase-2-step-4), [Resident spectrum](#resident-spectrum-wfip-0005-units-1-and-2)
and [Inputs kept on the device](#inputs-kept-on-the-device-wfip-0005-unit-4).

The source-term integration of one sea point: the semi-implicit dynamic time step with its
limiter, the MLIM shallow-water cap, the high-frequency tail, and the flux diagnostics, with every
source term it calls on the device.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SRCE`, `model/src/w3srcemd.F90:190-2412` at NOAA-EMC/WW3 `03adda91`, the `srce_direct` path as `switches/switch_lab_shrd` compiles it (ST4 NL1 LN1 BT1 DB1 MLIM) |
| Kokkos code | [`../src/ww_kokkos/srce_kernel.hpp`](../src/ww_kokkos/srce_kernel.hpp), a team function `w3srce_team`, one team per point; it calls the ST4 point functions, the DIA as a team ([`snl1_team.hpp`](../src/ww_kokkos/snl1_team.hpp)), [`W3SLN1`](W3SLN1_KOKKOS.md), [`W3SBT1`](W3SBT1_KOKKOS.md), [`W3SDB1`](W3SDB1_KOKKOS.md) |
| C shim | [`../src/fortran_iface/srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp) (`ww_srce_init`, `ww_srce`, `ww_srce_batch`); parameters in [`srce_params.hpp`](../src/ww_kokkos/srce_params.hpp); the DIA and ST4 tables of the same grid through [`shim_state.hpp`](../src/fortran_iface/shim_state.hpp) |
| WW3 side | `w3kokkosmd.F90` (interfaces, `W3KOKKOS_SRCE_SETUP`, `W3KOKKOS_SRCE_ON`, `W3KOKKOS_SRCE`), `w3initmd.F90` (init and log line), `w3wavemd.F90` (the `srce_direct` call) on the h0ffmann/WW3 branch `ww3-gpu/kokkos-srce` (`e062ff0b`), off `ww3-gpu/kokkos-st4`; the `WW3/` pin is not moved |
| Phase | `ww_srce`: 1, copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_SRCE)`; Kokkos::Serial inside an OpenMP region. `ww_srce_batch`: every active point of a time step per call, one copy in, one launch, one copy out per chunk, called outside W3WAVE's OpenMP region |
| Plan | [`W3SRCE_KOKKOS_PLAN_202610.md`](../../docs/W3SRCE_KOKKOS_PLAN_202610.md), issue #42 |

Left to the Fortran `W3SRCE`, call by call (`W3KOKKOS_SRCE_ON`): `IQTPE <= 0` (GQM,
`:1259`); `INFLAGS2(4)` with `IICEDISP` (Liu's dispersion, `:2124-2126`); `TAUWBUG = 0`
(`SINTAILPAR(4) <= 0.5`) with `IT > 0`, where the `W3SPR4` call at `:1119` reads the local `LLWS`
before anything sets it (`w3src4md.F90:302`): undefined in the Fortran, so not translatable. Any
other source-term switch makes `ww_srce_init` refuse (error 5).

## Arithmetic

The reference is the `-O3` object of `w3srcemd.F90` (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`), read as GIMPLE and as assembly.

- (v) No libmvec call, no power call, no FMA in W3SRCE; `logf` (`:1427`, `:1937`), `tanhf`
  (`:1932`, `:1958`) and one `sincosf` for `COS`/`SIN(USTDIR)` (`:2099-2100`), all called the
  same way here. `WN1**3` is `w*(w*w)`; `OFFSET*x` is folded to `x`; `/16.` is `*0.0625`;
  `FFXFM + FAGE` keeps the build's `+ 0.0` at `:1178` and the tail keeps `+ 0.` at `:1995`.
- (v) Every sum is serial and in source order in the build except the time-step `MIN` over `IS`
  (`:1532`, `.REDUC_MIN`, 4 lanes). Every candidate is positive or `+inf`, so a serial `MIN`
  gives the same bits; ⚠ only a NaN candidate could differ.
- (v) Each `MAX`/`MIN` takes its operands in the order of the object's `maxss`/`minss`/`maxps`,
  which is not always the GIMPLE order: `:951`, `:956`, `:1427`, `:1436`, `:1523`, `:1549`,
  `:1937`, `:1958` were reordered after the review. They matter for a NaN or a `+0`/`-0` tie only.
- ⚠ `:2350`: the final `MAX(0., SPEC)` is a 4-wide `maxps` with a scalar remainder whose third
  element has the other operand order; the kernel reproduces it for `NSPEC mod 4 = 3`. It is tied
  to the build's vector width.

## Quirks kept

The `DT = MAX(0.5, DT)` floor (`:1539`); the `TAUWX`/`TAUWY` reset that "keeps the bug as an
option" (`:1069`); `W3SDB1`'s early return leaving `VSDB`/`VDDB` of the previous sub-step;
`ECOS(ITH)` (not `ECOS(IS)`) in the momentum sums (`:2073`); the ice block with `ATT = 1.`, kept for
its signed zeros. Dead under this switch and not ported: `PHINL`, `HSTOT`, `COSI`, `TAUSCX/Y`,
`FHIGI`, `FIRST`, `TAUNUX/Y`.

## Static parity

Two fresh reviewers compared the C++ object (`openmp-release` flags, g++ 16.2) with the Fortran
dump and assembly section by section, 2026-10-15: the initialisation, the time-step limiter, the
spectrum updates, the flux sums, `FAGE`/`FHIGH`/`NKH`, MLIM, the tail, and the post-loop sums and
stresses match statement by statement for finite inputs; the `MAX`/`MIN` orders they found were
fixed. Static parity is still ⚠: only the regtests prove it.

## Batched path (phase 2, step 4)

Step 4 of [`H100_OPTIMIZATION_STRATEGY_202610.md`](../../docs/H100_OPTIMIZATION_STRATEGY_202610.md):
the source terms of a whole time step reach Kokkos in one call, without the per-point copies and
the `!$OMP CRITICAL`. The fork branch `ww3-gpu/kokkos-srce-batch` (`caa5da36`, from
`4a94a8f6`) adds `W3KOKKOS_SRCE_BATCH` to `w3kokkosmd.F90` and one hunk before the
`!$OMP PARALLEL` of W3WAVE's source-term loop (`w3wavemd.F90:2325-2331`, and its `END IF` at `:2469-2471`
after the loop), under `KOKKOS`, `ST4`, `NL1` and not `PDLIB`: when `W3KOKKOS_SRCE_ON(IT)`, it packs
the points with `MAPSTA(IY,IX) == 1` and `FLAGST(ISEA)`, calls `ww_srce_batch` once, unpacks,
and writes `UNDEF` to `UST`, `USTDIR`, `DTDYN`, `FCUT` of the other points as the loop's `ELSE`
does; otherwise the loop runs as before (v, read in the fork). The C ABI is `ww_kokkos` 0.4.

- **The arithmetic is the kernel's.** Team *i* of a `TeamPolicy(npts)` runs the same
  `w3srce_team` on point *i*, with its own `PointScalars`, `State` and slice of the work buffers.
  Point order cannot change a bit: a point reads and writes only its own `JSEA`/`ISEA` entries,
  which W3WAVE's own `!$OMP DO SCHEDULE (DYNAMIC,1)` already relies on (`w3wavemd.F90:2333-2335` at `caa5da36`).
- **Nothing shared is written** (v, read 2026-10-17): the teams share `Grid`, `st4::Params`,
  `st4::Fields`, the DIA `Config`/`Tables` and `SIG`. `w3spr4_point`, `w3sin4_point`,
  `w3sds4_point`, `calc_ustar` (`st4_*.hpp`), `w3sln1_point`, `w3sbt1_point`, `w3sdb1_point`
  (`srce_*.hpp`) and `snl1_team` write only their pointer arguments, which are the point's
  spectrum, outputs and work slice, and `snl1_team` its team scratch; none writes a table or
  holds a `static`.
- **Memory.** Per point the batch keeps about 56 KB on the device for `bench_small` (NK = NTH =
  24: the 12 624-float work of `srce_work_reals`, the spectrum, the inputs and outputs). The
  buffers hold at most `WW_KOKKOS_SRCE_BATCH_MIB` MiB (default 1024, about 19 000 such points); a
  step with more points runs in chunks, one launch each. They are allocated outside the kernel,
  kept between calls and grown on demand. The Fortran pack buffers are `SAVE` and sized `NSEAL`.
- **Counters.** `WW_KOKKOS_SRCE_COUNT=1` prints, besides the spectra, `W3SRCE batched calls`,
  `launches` and `points`.

Checked 2026-10-17 on the CPU (Kokkos OpenMP backend; Intel Core i9-14900, 32 threads, the pinned
`ww3` shell), never on the GPU (v):

| Run | Switch | Batched calls / launches / points | Against the Fortran, same executable | Against `--ww3` |
|---|---|---|---|---|
| `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos SRCE` | `switch_lab_ompg` | 2 161 / 2 161 / 2 161 | byte-identical, `nccmp-tol` PASS | `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` equal (`cmp`), `nccmp-tol` PASS |
| `... --ww3-gpu full --cpu --kokkos SRCE` | `switch_lab_ompg` | 25 / 25 / 230 100 | byte-identical, PASS | `out_grd.ww3` equal, PASS |
| the same with `WW_KOKKOS_SRCE_BATCH_MIB=64` | `switch_lab_ompg` | 25 / 200 / 230 100 | byte-identical, PASS | `out_grd.ww3` equal, PASS |
| `WW3GPU_SWITCH=switch_lab_shrd ... smoke ...` | `switch_lab_shrd` | 2 161 / 2 161 / 2 161 | byte-identical, PASS | PASS |
| `WW3GPU_SWITCH=switch_lab_shrd ... full ...` | `switch_lab_shrd` | 25 / 25 / 230 100 | byte-identical, PASS | PASS |

`bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --build-only` compiled the shim for CUDA (Ada 8.9);
nothing ran on the GPU. ⚠ The batch on CUDA is not known to give the bits of the CPU (the libdevice
`logf`, `tanhf`, `sincosf` of the [arithmetic](#arithmetic) section). Since #178 a CUDA build with `WW_KOKKOS_LIBM` (its default) calls `ww::dm`'s copies of glibc's functions there instead (`../src/ww_kokkos/libm_device.hpp`, WFIP-0006); that they give glibc's bits on a device is ⚠ until `scripts/libm_bits.sh --device` runs.

## Resident spectrum (WFIP-0005, units 1 and 2)

Since `513954a6` (`ww3-gpu/kokkos-resident`, PATCH.md section 12) the batch no longer packs the
spectrum: `W3KOKKOS_SRCE_BATCH` passes `VA` and the list of active `JSEA`, and
`ww_srce_batch_va` runs team *i* on column `jsea[i]` of the resident device copy of `VA`, in
place. The arithmetic is the same `w3srce_team` on the same bits; only where the spectrum lives
changed. W3WAVE's sync calls copy `VA` back before the next host statement that touches it: in the
full case one host-to-device and one device-to-host copy of the whole `VA` (21.21 MB) per time
step, 25 each, where the batch of `caa5da36` packed the 9 204 active spectra (21.2 MB) in Fortran
and copied them both ways per step (v, `WW_KOKKOS_VA_COUNT=1`, below).

Checked 2026-10-17 on the CPU (Kokkos OpenMP backend; the machine above), never on the GPU (v):
every run byte-identical to the Fortran of the same executable (`ww3_ounf` NetCDF, `cmp`),
`out_grd.ww3` (and in the smoke `out_pnt.ww3`, `restart.ww3`) `cmp`-equal to the Fortran and to
`--ww3`, `nccmp-tol` PASS both:

| Run (`bash scripts/ww3-gpu.sh --ww3-gpu ... --cpu`) | Switches | `WW_KOKKOS_VA_RESIDENT` | VA copies H2D / D2H |
|---|---|---|---|
| `smoke --kokkos SRCE` | `switch_lab_ompg`, `switch_lab_shrd` | unset, `1` | 1 / 2 161; 1 / 2 166 |
| `full --kokkos SRCE` | both | unset, `1` | 25 / 25; 73 / 86 |
| `smoke --kokkos all` | both | unset, `1` | 1 / 2 161; 1 / 2 166 |
| `full --kokkos all` | `switch_lab_ompg` | unset, `1` | 25 / 25; 73 / 86 |
| `full --kokkos XYP3,KTP3,SRCE` | `switch_lab_ompg` | unset, `1` | 25 / 25; 73 / 86 (`W3XYP3` 13 824 spectra, `W3KTP3` 441 792 point calls) |

`WW_KOKKOS_SRCE_COUNT`: 2 161 batched calls and points in the smoke, 25 calls and 230 100 points
in the full, the counts of the Fortran loop before. The smoke's 2 161 device-to-host copies are
one per `W3WAVE` call (`ww3_ts1` returns from `W3WAVE` every step, and the return syncs `VA`).

`--timers`, full, `switch_lab_ompg`, one fenced run each, the machine shared with other sessions
(load average 45 for the first, 35 for the second), so the shares, not the seconds, are the reading ⚠:

| Region | Batch packing `VA` (unit 1 only) | Resident `VA` (unit 2) |
|---|---|---|
| `ww3_shel` | 70.39 s | 44.89 s |
| `srce.batch.kernel` | 46.124 s | 30.685 s |
| `srce.batch.h2d` / `.d2h` | 0.160 / 0.151 s | 0.064 / 0.032 s |
| `va.h2d` / `va.d2h` / `va.host.call` | none / none / 0.000 s | 0.032 / 0.031 / 0.031 s |
| outside Kokkos | 23.955 s (34.0 %) | 13.946 s (31.1 %) |


Since `2f35368a` (`ww3-gpu/kokkos-pnt`, PATCH.md section 17), with `WW_KOKKOS_IOPE=1` (in
`--kokkos resident`) the point output copies back from the resident `VA` only the columns `W3IOPE`
interpolates, and the state stays `DEVICE`. The batch is unchanged; what changes is how often its
output crosses: `VA` D2H 2 161 → 37 in the smoke and 36 → 24 in the coast case, every output
byte-identical to the Fortran and to `--ww3` on both switches, with and without
`WW_KOKKOS_VA_RESIDENT=1` (v, 2026-10-17, the table there).

## Inputs kept on the device (WFIP-0005 unit 4)

Since this change (`srce_shim.cpp` only; the fork pin `90631fad` and the ABI 0.6 unchanged,
`PATCH.md` section 14), `ww_srce_batch_va` reads and writes the arrays `W3KOKKOS_SRCE_BATCH`
packs (`BJSEA`, `BWN1`, `BCG1`, `BPIN`, `BPSC`, `BWC`, `BTB`, `BTI`, `BSH`) in place on Serial and
OpenMP, and on a device copies an input only when its bits differ from a host shadow of its
buffer. The kernel and its arithmetic are unchanged; the values it reads are the same bits.

Checked 2026-10-17 on the CPU (the machine above, never the GPU) (v): 15 runs of
`bash scripts/ww3-gpu.sh --ww3-gpu full|smoke --cpu --kokkos resident` and `smoke --kokkos all`, on
`switch_lab_ompg` and `switch_lab_shrd`, plain, with `WW_KOKKOS_VA_RESIDENT=1` and with
`WW_KOKKOS_SRCE_STAGED=1` (the device's copy-and-skip run on the host), each byte-identical to the
Fortran of the same executable and to `--ww3` (`ww3_ounf` NetCDF but its switches attribute,
`out_grd.ww3`, and in the smoke `out_pnt.ww3`, `restart.ww3`). Source-term input traffic from
`WW_KOKKOS_SRCE_COUNT=1`, per run (on the CPU, projected for a device ⚠):

| Run | H2D before (every call) | H2D after | D2H (both) |
|---|---|---|---|
| full, 25 calls | 78.23 MB (3.13 MB per step) | 3.13 MB, all at the first call | 27.61 MB (1.10 MB per step) |
| smoke, 2 161 calls | 0.94 MB (436 B per step) | 41 092 B (`BPIN` changes 1 441 times, `BPSC` 5) | 0.26 MB |

`--timers`, full, `switch_lab_ompg`, one fenced run each on the same tree with and without the
change, the machine shared (not a timing) ⚠:

| Region | Before (`5eca265`) | After |
|---|---|---|
| `ww3_shel` | 27.86 s | 24.66 s |
| `srce.batch.kernel` | 23.452 s | 21.295 s |
| `srce.batch.h2d` / `.d2h` | 0.035 / 0.024 s | 0.010 / 0.000 s |
| outside Kokkos | 1.974 s | 1.368 s |

The copies removed are about 0.05 s of host `memcpy` per run on this CPU; the kernel and wall time
differences are the machine's load, not the change. What the change exists for is a device, where
3.13 MB per step no longer crosses PCIe ⚠ (never run).

## Team-parallel body on a device

**Why.** The batched launch ran `w3srce_team`, which does almost all of W3SRCE inside
`Kokkos::single(PerTeam)` and spreads only the DIA (`snl1_team`) over its team. On the OpenMP
backend that is right: the 9 520 sea points of the full case already give each of the 32 threads
its own points. On CUDA it leaves one thread per point working outside the DIA: ptxas gives that
kernel 168 registers, and the DIA's team scratch (about 27 KiB at NK = NTH = 24, in shared memory)
lets three teams share a streaming multiprocessor (SM) of the RTX 4090 (sm_89), so about three
threads per SM ran the non-DIA part ⚠ (an estimate from ptxas and the hardware limits, never
measured).

**Two bodies, one arithmetic.** `w3srce_split`
([`srce_kernel_team.hpp`](../src/ww_kokkos/srce_kernel_team.hpp)) and the team forms of its
callees (`st4_sds4_team.hpp`, `st4_sin4_team.hpp`, `st4_spr4_team.hpp`, `srce_ln1_team.hpp`,
`srce_bt1_team.hpp`, `srce_db1_team.hpp`) split every loop whose iterations are independent over
the team (`TeamExec` in [`srce_team.hpp`](../src/ww_kokkos/srce_team.hpp): `for_` a
`TeamThreadRange`, `one` a `Kokkos::single`, `sync` a `team_barrier`). The design is that of
`6ba626c` (branch `goal/42-srce-kernel`), re-applied to this code with one difference: there
`w3srce_team` and the `*_point` functions became the team bodies run on one thread, here they are
left as they were, and the team bodies are new code beside them. So every host run by default, the
per-point ST4 shims and the PDLIB block compile the code they compiled before. `run_batch` picks
the body at compile time (`run_batch_as<ExecSpace, Split>`, `w3srce_body<Split>`):

| `WW_KOKKOS_SRCE_TEAM` | Host backend (Serial, OpenMP) | Device (CUDA) |
|---|---|---|
| unset, empty, `AUTO` | `w3srce_team`, `TeamPolicy(n, AUTO)` as before | `w3srce_split`, 96 threads per point (32 with `WW_KOKKOS_SRCE_SCRATCH=1`) |
| `0` | `w3srce_team` | `w3srce_team`, `TeamPolicy(n, AUTO)`: the old kernel, for an A/B |
| `N`, 1 to 1024 | `w3srce_split`, teams of `N` capped at `team_size_max` | the same |

`WW_KOKKOS_SRCE_SCRATCH` (`AUTO`, `0`, `1`) puts the DIA's scratch at that level for both bodies;
it moves no bit (each point owns its scratch) and exists for a supervised A/B on the GPU. Any
other value of either flag stops the run in `ww_srce_init` (status 3). `WW_KOKKOS_SRCE_COUNT=1`
prints the split launches, the threads per point of the last one and the scratch level, so a log
says which body ran. `w3srce_split` needs three more arrays of NSPEC and the 14 shared reals of
`W3SIN4` per point; the work is sized for it only when a launch will run it.

**The rules** (the owner's, 2026-10-17). Only a loop whose iterations are independent (each
writes its own elements and reads nothing another writes) is split, each element with the same
operations in the same order. Every sum, `MIN`/`MAX`/`MAXVAL`, accumulation and running value
stays on one thread in the Fortran's order: no `parallel_reduce`, no tree, no atomic, the
`MIN`/`MAX` operand orders unchanged. Where a reduction's terms are element-wise, the terms are
computed over the team into a work array and summed in order after a barrier. A per-`IK` sum over
`ITH` is computed whole by the one thread that owns that row, in `ITH` order. The libmvec
emulation keeps its groups of four: a split loop calls `mvec_powf` once per row on one thread (the
grouping starts at `ITH = 1` of each row, as in the build) or stays on one thread. A value one
thread computes reaches the others through `State`, `PointScalars` or a work array, after a
`team_barrier`. Contraction stays off, `float` as before.

**Split over the team** (v, read in the code at this commit):

| Where | Loop | Unit of work |
|---|---|---|
| `srce_kernel_team.hpp` `w3srce_split` | zeroing of the 13 `V*` arrays and `BRLAMBDA`; `DAM`/`WN2` (two loops, a barrier between); `LLWS = .TRUE.` at `IT = 0`; `SPECINIT`; the ice scaling (`:1446-1453`); `VS`, `VD` and the time-step candidate of `:1523-1532`; the two `SPEC` updates of `:1761-1808`, the second after the first on each element; the `PHIAW`/`PHIBBL` terms of `:1827-1831`; the MLIM rescaling; the tail (`:1985-1997`) row by row, a barrier after each row; `SPEC = SPECINIT` at `IT = 0`; the final `MAX` of `:2350` | `IS`, or `ITH` within a row |
| `st4_sds4_team.hpp` `w3sds4_team` | section 0 zeroing; `SDSBCHOICE = 1`: `BTH = 0`, the saturation (`BTH` row, `BTH0(IK)`, its `MAXVAL` on the row's thread), the 2.a.2 diagonal (one `mvec_powf` per row), `PB`/`BRLAMBDA`/`SRHS`; the cumulative term with `SSDSC(3) < 0` (the default, `O(NK^2 NTH^2)`) per (`IK`, `ITH`), each element's `IK2`, `J` sums its own; with `SSDSC(3) >= 0` per `IK` | `IS`, `IK`, (`IK`, `ITH`) |
| `st4_sin4_team.hpp` `w3sin4_team` | `DSTAB = 0`; `DSTAB`/`LLWS` of each `ITH` of a row (`:740-783`), the row's scalars recomputed by every thread from the shared stress; `D`, `S = D*A` | `IS`, `ITH` |
| `st4_spr4_team.hpp` `w3spr4_team` | `EB(IK)`, `EB2(IK)`, summed in locals as `w3spr4_point` does | `IK` |
| `srce_ln1_team.hpp` `w3sln1_team` | `DIRF`, `WNF`, `S` | `ITH`, `IK`, `IS` |
| `srce_bt1_team.hpp` `w3sbt1_team` | `D`, `S = D*A` | `IS` |
| `srce_db1_team.hpp` `w3sdb1_team` | `EB(IK)` (REAL*8, dead under `IWB = 1` and kept); `D`, `S = D*A` | `IK`, `IS` |

**Kept on one thread**, and why: in W3SRCE the scalars of the initialisation and of each
sub-step, the time-step `MIN` over `IS` (`:1532`), the `PHIAW`/`PHIBBL`/`WHITECAP(3)` sums
(`:1817-1846`), the stresses, `FAGE`/`FHIGH`/`NKH`, the MLIM test, and after the loop the energy
and momentum sums (`:2066-2081`) and the ice block; in `W3SDS4` the `MSSSUM` prefix sums,
`SDSBCHOICE = 2` (running counts and sums over scales) and `= 3` (`BTH(1:NTH)` and `LMODULATION`
are scratch reused by every `IK`), the whitecap recurrences; in `W3SIN4` the `UORB`/`AORB` sums,
the stress of each row after its `DSTAB` (the same `TEMP2` as at `:785`, accumulated in `ITH`
order; the next row reads it), and the tail (`:847-976`); in `W3SPR4` `ELCS`, `ELSN`, `AMAX`, the
two libmvec sites over `IK` and everything after them, `CALC_USTAR`; in `W3SBT1` the `CBETA` loop
that stops at the first `IK` with `WN*DEPTH > 6`; in `W3SDB1` `ETOT` and `FMEAN2`.

**The host code did not change** (v, 2026-10-17). `srce_shim.cpp` compiled with the
`openmp-release` command of `kokkos/build/openmp-release` (g++ 16.2, `-O3 -march=x86-64-v3
-ffp-contract=off -fopenmp`) at `origin/main` (`d8905a3b`) and at this commit, then each function
of the default host path disassembled (`objdump -d --no-show-raw-insn --disassemble=<symbol>`) and
its floating-point instructions counted by mnemonic (every `v*ss`, `v*ps`, `v*sd`, `v*pd`, compare,
convert, min, max, sqrt on `xmm`/`ymm`):

| Function (host instantiation) | FP instructions, by mnemonic | Whole instruction stream |
|---|---|---|
| `w3srce_team<HostThreadTeamMember<OpenMP>>`, its three `single` bodies | equal | equal, 4 268 instructions each |
| `w3srce_team<...OpenMP>` itself | equal | 1 031 against 946: one `team_barrier` inlined before is a call now, registers renamed |
| `w3srce_team<...Serial>` | equal | |
| `ParallelFor` of `run_batch<OpenMP>` (`run_batch_as<OpenMP, false>`) | equal | equal, 785 each, but the `State` stride (`imul $0x68` against `$0x74`: three fields appended) |
| `snl1_team<...OpenMP>`, `<...Serial>` | equal | `team_barrier` called instead of inlined (fewer `lock`/`pause`), registers |

So a host run by default does the same floating-point operations in the same order; what moved is
where GCC put a barrier (with one thread per team it returns at once) and the size of `State`.

**ptxas** (v, 2026-10-17; `nix develop ./picasso/labs/pratico#cuda`, `cmake -S kokkos -B <dir>
--preset cuda-release -DCMAKE_CXX_FLAGS='-Xptxas -v' -DWW_W3KOKKOSMD=$PWD/WW3/model/src/w3kokkosmd.F90`,
`ninja -C <dir> CMakeFiles/ww_kokkos.dir/src/fortran_iface/srce_shim.cpp.o`; sm_89, `--fmad=false`,
`libm_device.hpp` on), the `ww_srce_batch` kernel, the same for the static and the dynamic schedule:

| Kernel | Registers | Spill stores / loads | Stack frame | Static smem | Barriers |
|---|---|---|---|---|---|
| `w3srce_team`, `origin/main` | 168 | 0 / 0 B | 3 568 B | 16 B | 1 |
| `w3srce_team`, this commit (`WW_KOKKOS_SRCE_TEAM=0` on a device) | 168 | 0 / 0 B | 3 568 B | 16 B | 1 |
| `w3srce_split`, this commit (the device default) | 203 | 0 / 0 B | 2 640 B | 16 B | 1 |

Before `libm_device.hpp` (base `4fbffbe8`) the two were 178 and 146 registers, with 544 and 632 B
of stack: the glibc-bit routines cost registers and stack, and `w3srce_split` more.

**Occupancy on sm_89** ⚠ (an estimate from the table above and the hardware limits: 64 Ki
registers, 48 warps, 24 blocks and 100 KiB of shared memory per SM, registers allocated per warp
in units of 256; not measured, and the GPU never ran). The DIA's team scratch for the full case is
`snl1_team_scratch_bytes` = (768 + 8 x 696 + 576) floats, about 27 KiB per team, plus 1 KiB the
driver keeps per block, so three teams fit in shared memory at level 0. 203 registers are 6 656
per warp, so 9 warps per SM; 168 are 5 632, 11 warps.

| Body, scratch level, threads per point | Teams per SM | Warps per SM | Threads working outside the DIA, per SM |
|---|---|---|---|
| `w3srce_team`, level 0, `AUTO` (before) | 3 | at most 11 | 3 (one per team, the rest wait in `single`) |
| `w3srce_split`, level 0, 32 | 3 | 3 | 96 in the split loops, 3 in `one()` |
| `w3srce_split`, level 0, 96 (the default) | 3 | 9 | 288 in the split loops, 3 in `one()` |
| `w3srce_split`, level 0, 128 | 2 | 8 | 256, 2 |
| `w3srce_split`, level 1 (`WW_KOKKOS_SRCE_SCRATCH=1`), 32 (that level's default) | 9 | 9 | 288, 9 |

The default trades the most points per SM for the most threads in the split loops at level 0;
level 1 runs three times as many points at once, with the DIA's scratch in global memory (L1 and
L2 cached). Which is faster, and by how much against WW3 on 32 OpenMP threads, is for the
supervised GPU run: `WW_KOKKOS_SRCE_TEAM=0` against unset, and `WW_KOKKOS_SRCE_SCRATCH=1`, are its
A/B, and `WW_KOKKOS_SRCE_COUNT=1` prints which body ran.

**Parity on the CPU** (v, 2026-10-17, Kokkos OpenMP backend, Intel Core i9-14900, 32 threads,
the pinned `ww3` shell; never on the GPU). `bash scripts/ww3-gpu.sh --ww3-gpu <case> --cpu
--kokkos resident` with the variables shown; every row: the Kokkos run "byte-identical" to the
Fortran of the same executable, and `nccmp-tol` PASS against that Fortran and against `--ww3`.
"Split launches" is the counter `W3SRCE batched launches split over a team`.

| Case | Switch | Variables | Split launches, threads per point, scratch level | Result |
|---|---|---|---|---|
| smoke | `switch_lab_ompg` | none | 0, -, 1 | byte-identical, PASS, PASS |
| full | `switch_lab_ompg` | none (`--timers`) | 0, -, 0 | byte-identical, PASS, PASS |
| coast | `switch_lab_ompg` | none | 0, -, 0 | byte-identical, PASS, PASS |
| full | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=4` | 25, 4, 0 | byte-identical, PASS, PASS |
| coast | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=4` | 72, 4, 0 | byte-identical, PASS, PASS |
| full | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=16` | 25, 16, 0 | byte-identical, PASS, PASS |
| coast | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=16` | 72, 16, 0 | byte-identical, PASS, PASS |
| smoke | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=4 WW_KOKKOS_SRCE_SCRATCH=1` | 2 161, 4, 1 | byte-identical, PASS, PASS |
| full | `switch_lab_ompg` | `WW_KOKKOS_SRCE_TEAM=4 WW_KOKKOS_SRCE_SCRATCH=1` | 25, 4, 1 | byte-identical, PASS, PASS |
| smoke | `switch_lab_shrd` | `WW3GPU_SWITCH=switch_lab_shrd` | 0, -, 1 | byte-identical, PASS, PASS |
| full | `switch_lab_shrd` | `WW3GPU_SWITCH=switch_lab_shrd` | 0, -, 0 | byte-identical, PASS, PASS |
| coast | `switch_lab_shrd` | `WW3GPU_SWITCH=switch_lab_shrd` | 0, -, 0 | byte-identical, PASS, PASS |
| full | `switch_lab_shrd` | `... WW_KOKKOS_SRCE_TEAM=4` | 25, 4, 0 | byte-identical, PASS, PASS |
| coast | `switch_lab_shrd` | `... WW_KOKKOS_SRCE_TEAM=4` | 72, 4, 0 | byte-identical, PASS, PASS |
<!-- T16-SHRD -->

`WW_KOKKOS_SRCE_TEAM=bogus` and `WW_KOKKOS_SRCE_SCRATCH=2` stop the smoke in `ww_srce_init` with
"expected AUTO, 0 or a team size from 1 to 1024 (code 3)" and "expected AUTO, 0 or 1 (code 3)"
(v): the variables reach the run. `bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident
--build-only` built `ww_kokkos` (cuda-release) and WW3 for CUDA; nothing ran on the GPU. ⚠ The
`SDSBCHOICE = 2, 3`, `SSDSC(3) >= 0` and `W3_STAB3` branches of the team bodies did not run (the
cases take `SDSBCHOICE = 1`, `SDSCUM < 0`), so no output shows them.

**Mutation check** (v, 2026-10-17; scratch edits of `w3srce_split`, reverted, never committed).
Identical outputs prove only what reaches the outputs, so the forced-team run was shown to catch a
broken team loop. `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident` with
`WW_KOKKOS_SRCE_TEAM=4`, `switch_lab_ompg`:

<!-- MUTATIONS -->

**CPU time** (`--timers`, one fenced run each, `switch_lab_ompg`, the machine shared with other
sessions ⚠): `srce.batch.kernel` 22.66 s before (`4fbffbe8`, load average about 16) and 24.91 s
after (load about 40); `ww3_shel` 26.95 s and 30.09 s with Kokkos, against `--ww3` 50.48 s and
45.78 s. The load, not the change, sets that difference: the object code above is the evidence
that the default host path is unchanged. The forced team is a check, not a CPU mode: with
`WW_KOKKOS_SRCE_TEAM=4` the full took 143.52 s (Kokkos) against 54.49 s for the Fortran, the team
barriers costing more than they save when the points already fill the threads; with 16 the full
took 2 499.54 s and the coast 686.40 s (spinning team barriers on a machine shared by other
sessions).

## Benchmarks

One CPU measurement of the batched path, after the replay above passed on the same tree
(2026-10-17, v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos SRCE --repeat 3`,
`switch_lab_ompg`, the machine above, `OMP_NUM_THREADS` unset (32 threads), `ww3_shel` wall time.

| Run | Three runs (s) | Median (s) |
|---|---|---|
| `--ww3` (pure NOAA WW3, OpenMP) | 25.26, 25.86, 24.26 | 25.26 |
| `--ww3-gpu`, Kokkos `W3SRCE` batched, OpenMP backend | 31.20, 31.71, 31.87 | 31.71 |

On this CPU the batch is 0.80x of WW3's own OpenMP loop. It is a CPU number on the OpenMP
backend, not a GPU one and not a projection of one. Why it is slower here was not measured ⚠ (one
guess: most of `w3srce_team` runs in `Kokkos::single`, so every thread of a team past the first
waits). With `switch_lab_shrd`, where WW3 has one thread, the full ran 34.71 s with the batch
against 154.70 s for `--ww3` (one run each, not a median).

On the resident spectrum (`513954a6`), the same command after the replay of
[Resident spectrum](#resident-spectrum-wfip-0005-units-1-and-2) passed on the same tree,
2026-10-17, with the machine shared with other sessions (load average about 35) ⚠:

| Run | Three runs (s) | Median (s) |
|---|---|---|
| `--ww3` (pure NOAA WW3, OpenMP) | 46.46, 29.33, 45.48 | 45.48 |
| `--ww3-gpu`, Kokkos `W3SRCE` on the resident `VA`, OpenMP backend | 67.35, 47.84, 67.43 | 67.35 |

0.68x; the spread of `--ww3` itself (29 to 46 s) says the load, not the change, sets these
numbers, so they are not a speed-up or slow-down of unit 2 ⚠. A quiet machine is needed.

## How it was regtested

The resident path: the runs of [Resident spectrum](#resident-spectrum-wfip-0005-units-1-and-2), pair to pair; not the regtest matrix. The batched path: smoke and full, pair to pair, above; not the regtest matrix. Before it: What ran, 2026-10-15 (v): `openmp-release` and `serial-debug` built with no warning, ctest
10/10 each (the intro programs' and tools' tests; the port has none, ADR-0010); `cuda-release`
compiled, never run; the fork branch built and linked `ww3_shel` with `KOKKOS`, not run. The
checkpoint (plan §7) adds `SRCE` to `WW3GPU_ROUTINES` and runs `just smoke-ww3-gpu`,
`just full-ww3-gpu` and `just ww3-gpu-matrix` on the CPU.
