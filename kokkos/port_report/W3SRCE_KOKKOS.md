# W3SRCE_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the checkpoint of the W3SRCE
unit (ADR-0011), itself after the ST4 checkpoint. Selected by `WW_KOKKOS_SRCE=1`; the flag
defaults to 0, so WW3 runs the Fortran. Wired: WW3 at the `WW3/` pin (`caa5da36`,
`ww3-gpu/kokkos-srce-batch`) calls it for every sea point of a time step in one call at
`w3wavemd.F90:2328-2329` (`W3KOKKOS_SRCE_BATCH`, set-up `w3initmd.F90:825`) behind
`WW_KOKKOS_SRCE` (default off); see [Batched path](#batched-path-phase-2-step-4).

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
`logf`, `tanhf`, `sincosf` of the [arithmetic](#arithmetic) section).

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

## How it was regtested

The batched path: smoke and full, pair to pair, above; not the regtest matrix. Before it: What ran, 2026-10-15 (v): `openmp-release` and `serial-debug` built with no warning, ctest
10/10 each (the intro programs' and tools' tests; the port has none, ADR-0010); `cuda-release`
compiled, never run; the fork branch built and linked `ww3_shel` with `KOKKOS`, not run. The
checkpoint (plan §7) adds `SRCE` to `WW3GPU_ROUTINES` and runs `just smoke-ww3-gpu`,
`just full-ww3-gpu` and `just ww3-gpu-matrix` on the CPU.
