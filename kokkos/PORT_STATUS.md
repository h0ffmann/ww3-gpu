# `PORT_STATUS.md`: the WAVEWATCH III → Kokkos port ledger

One row per WW3 routine that is being ported, with where it came from, how far it
has got, and what it costs. A row is only allowed to claim a number that a command
in this repository reproduces: each timing links the commands below, and the timing
table is generated from its record in [`bench/results/`](../bench/results/)
(`python3 scripts/results.py table`).

Routines are ported in descending order of measured wall time
(`CONTRIBUTING.md`, "Port order is wall time"). The ranking below is measured: gprof on
`bench_small`, a 2-D case with propagation (`just profile-case full`), not the operational
case, which is not in this repository. The older `ww3_ts1` table in
`docs/data/ww3_ts1_gprof_202610.md` stays ⚠ (transcribed from #45). Every routine the committed
profiles time in compute is now translated; what they still rank is setup and I/O. Beyond them
the order follows what NOAA's operational switches compile (`WW3-noaa/model/bin/switch_NCEP_*`,
`switch_NWS_*`), then regtest coverage (PRs #152 and #157).

## Port score

How much of WW3 runs on the Kokkos backend, and what is next. Written by
`scripts/port_score.py --write` from [`ports.tsv`](ports.tsv) and the profile; `just port-score`
prints it, and CI checks that this block is current. How the port was assessed on 2026-10-14:
[`docs/PORT_ASSESSMENT_202610.md`](../docs/PORT_ASSESSMENT_202610.md).

<!-- port-score:start -->
Profile: full (bench_small, 120 x 80, 9 520 sea points, 12 h), Release -pg, switches/switch_lab_shrd, 1 thread, ww3_shel 146 s under -pg, WW3@6d1bcdfe, 2026-10-14 (`just profile-case full`). Self time; a routine's callees are not in its row.

| # | routine | self s | self % | cumulative % | port status |
|---|---|---|---|---|---|
| 1 | `w3snl1md:w3snl1` | 63.53 | 48.4 | 48.4 | L2 |
| 2 | `w3src4md:w3sds4` | 30.52 | 23.2 | 71.6 | ported |
| 3 | `w3src4md:w3sin4` | 16.63 | 12.7 | 84.2 | ported |
| 4 | `w3srcemd:w3srce` | 7.36 | 5.6 | 89.8 | ported |
| 5 | `w3src4md:w3spr4` | 4.10 | 3.1 | 93.0 | ported |
| 6 | `w3pro3md:w3xyp3` | 3.92 | 3.0 | 95.9 | ported |
| 7 | `w3uqckmd:w3qck3` | 1.50 | 1.1 | 97.1 | ported |
| 8 | `w3uqckmd:w3qck1` | 1.19 | 0.9 | 98.0 | ported |
| 9 | `init` | 0.86 | 0.7 | 98.6 | Fortran |
| 10 | `w3sdb1md:w3sdb1` | 0.51 | 0.4 | 99.0 | ported |
| 11 | `w3pro3md:w3ktp3` | 0.40 | 0.3 | 99.3 | ported |
| 12 | `w3sbt1md:w3sbt1` | 0.28 | 0.2 | 99.6 | ported |
| 13 | `w3wavemd:w3scat` | 0.21 | 0.2 | 99.7 | ported |
| 14 | `w3sln1md:w3sln1` | 0.17 | 0.1 | 99.8 | ported |
| 15 | `w3wavemd:w3gath` | 0.15 | 0.1 | 100.0 | ported |

| level | run time | lines | routines | files | what it counts (WW3@03adda91) |
|---|---|---|---|---|---|
| translated | **99.3 %** | 67699 of 263375 (25.70 %) | 310 of 1023 (30.30 %) | 63 of 147 | a Kokkos translation exists in kokkos/src |
| ported | **94.0 %** | 13411 of 263375 (5.09 %) | 54 of 1023 (5.28 %) | 21 of 147 | WW3 at the WW3/ pin calls the Kokkos code, behind its run-time flag |
| regtested | **48.6 %** | 2677 of 263375 (1.02 %) | 10 of 1023 (0.98 %) | 5 of 147 | WW3's regtests ran with it on Kokkos and agreed with the Fortran |

The port itself: 55129 lines in 336 files, the C++ under kokkos/src (every .cpp and .hpp line).

On a GPU: none yet. The CUDA build links; the RTX 4090 run, `just smoke-ww3-gpu gpu`, is the owner's, supervised.
<!-- port-score:end -->

SPDX-License-Identifier: MIT

## The ledger

| Routine | WW3 file:lines | Phase | Shim | L1 parity | L2 replay | Serial ms | OpenMP ms | CUDA ms | Notes |
|---|---|---|---|---|---|---|---|---|---|
| `W3SNL1` + `INSNL1` | `model/src/w3snl1md.F90:115-473`, `:483-786` | 1 (copy-in / kernel / copy-out) | `ww_snl1_init`, `ww_snl1` | **bit-identical** on all three presets, 2026-09-15 (CPU presets re-run 2026-10-14); removed with the tests, 2026-10-14 (see [L1 / L2](#l1--l2)) | **PASS on the CPU** inside WW3 (h0ffmann/WW3 `hoff/kokkos-snl1`): `just smoke-ww3-gpu`, `just full-ww3-gpu`, `just ww3-gpu-matrix` (100 entries): **byte-identical** to the Fortran DIA, 2026-10-15 ([`port_report/W3SNL1_KOKKOS.md`](port_report/W3SNL1_KOKKOS.md)) | [25.88](#where-the-numbers-come-from) | [5.95](#where-the-numbers-come-from) | [0.75](#where-the-numbers-come-from) | 1 000 points/call, end-to-end through the shim. Kernel alone: 24.96 / 5.02 / **0.047** ms. Timed with `-ffp-contract=off`; see below. Level-0 team scratch per point = (NSPECY+NTH) + 8(NSPECX+NTH) + NSPEC floats (`snl1_dia.cpp`): 28 608 B = 27.9 KiB at NK=25/NTH=24 (XFR 1.1, λ 0.25); 52 992 B ≈ 52 KiB at NK=32/NTH=36. When the request exceeds `TeamPolicy::scratch_size_max(0)` (for the Kokkos 5.2 CUDA backend the device's opt-in shared-memory limit minus ~24.6 KB, ≈75 KB on the RTX 4090; on OpenMP less than the 39 248 B that `ww3_ts1`'s NK=36 NTH=24 needs), the kernel falls back to level-1 scratch (2026-10-14, `docs/PORT_ASSESSMENT_202610.md` §3); it throws (→ `WW_KOKKOS_ERR_KERNEL`) only when level 1 is too small too. WW4's default grid (NK=50/NTH=36, XFR 1.07) is one of them on the 4090: see [Level-0 scratch at WW4's default grid](#level-0-scratch-at-ww4s-default-grid). |
| ST4: `W3SDS4`, `W3SIN4`, `W3SPR4`, `CALC_USTAR`, `INSIN4`, `TABU_*` | `model/src/w3src4md.F90` | 1 | `ww_sds4`, `ww_sin4`, `ww_spr4` (`st4_shim.cpp`), wired at the `WW3/` pin | — | ADR-0011 checkpoint under way: `just smoke-ww3-gpu` and `just full-ww3-gpu` byte-identical to the Fortran with each flag and all together, 2026-10-15 ([`docs/log/2026-10-15-st4-checkpoint.md`](../docs/log/2026-10-15-st4-checkpoint.md)); the regtest matrix is not recorded | — | — | — | No timing: no timing without parity. |
| `W3SRCE` with `W3SLN1`, `W3SBT1`, `W3SDB1` | `model/src/w3srcemd.F90` and callees | 1 | `ww_srce` (`srce_shim.cpp`), on the fork branch `ww3-gpu/kokkos-srce`, not the pin | — | waiting for its checkpoint | — | — | — | |
| Propagation (`PR3`, `UQ`), `W3WAVE` support, dispersion | `w3pro3md`, `w3uqckmd`, `w3wavemd`, `w3updtmd`, `w3dispmd` | translated | none yet | — | — | — | — | — | Header-only; static parity by object comparison, one fresh review per routine ([`docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md)). |
| NOAA operational batches A to C: `PDLIB`, `IC4`, `REF1`, `TR1`, `BT4`, `IS1`, `IC1`, partitioning, `NL3` | `w3profsmd_pdlib`, `w3sic4md`, `w3ref1md`, … `w3snl3md` | translated | none yet | — | — | — | — | — | Header-only; static parity, fresh reviews ([`docs/NOAA_OPS_KOKKOS_PLAN_202610.md`](../docs/NOAA_OPS_KOKKOS_PLAN_202610.md)). |
| `ST0`, `ST1`, `ST2` (with `INPTAB`), `ST3` (with its tables), `ST6` (with `LFACTOR`, `TAU_WAVE_ATMOS`), `FLX1`-`FLX5`, `SWL` (32 routines) | `w3src{0,1,2,3,6}md`, `w3flx{1..5}md`, `w3swldmd` | translated | written and compiled for six units, not committed (PR #157) | — | — | — | — | — | Header-only in the PR; static parity against eight reference builds and a fresh review per routine; `ST2` with `FLX3` is the physics of `switch_NCEP_st2`; nothing run ([`docs/log/2026-10-16-st-flx-swl-kokkos-translation.md`](../docs/log/2026-10-16-st-flx-swl-kokkos-translation.md)). |
| `W3SNL2`, `W3SNL4`, `W3SNL5`, `W3SNLS` | `model/src/w3snl{2,4,5,s}md.F90` | not started | — | — | — | — | — | — | No operational switch uses them; the `IQTPE <= 0` branch (`W3SNLGQM`) is not replaced either. |

Per routine, with each one's evidence and regtest record: [`ports.tsv`](ports.tsv) and the
generated [`PORT_TABLE.tsv`](PORT_TABLE.tsv).

**Phase** is the memory contract, not the completeness of the physics:
*phase 1* copies the spectrum host→device and back around every call; *phase 2*
keeps `VA` resident on the device and the copies disappear. Every routine starts
at phase 1 because phase 1 is what can be validated against the Fortran one call
at a time.

## Where the numbers come from

```bash
just kokkos-build openmp-release
nix develop ./picasso/labs/pratico#ww3 --command \
  env OMP_PROC_BIND=false OMP_NUM_THREADS=1 \
      kokkos/build/openmp-release/tests/ww_bench_snl1          # the Serial column
nix develop ./picasso/labs/pratico#ww3 --command \
  env OMP_PROC_BIND=spread OMP_PLACES=cores OMP_NUM_THREADS=32 \
      kokkos/build/openmp-release/tests/ww_bench_snl1          # the OpenMP column

just kokkos-cuda-test                                          # builds ww_bench_snl1
nix develop ./picasso/labs/pratico#cuda --command \
  env OMP_PROC_BIND=false kokkos/build/cuda-release/tests/ww_bench_snl1
```

`kokkos/tests/bench_snl1.cpp`: 1 000 sea points (the three fixture points tiled),
20 timed calls after 3 warm-up calls, on the committed `nk=25, nth=24` grid
(`nspec = 600`). It is not a CTest case: it asserts nothing and its timings are
not reproducible enough to gate a build on.

**Machine** (all nine runs, 15 Sep 2026): Intel Core i9-14900 (32 hardware
threads) + NVIDIA GeForce RTX 4090 (Ada, `CMAKE_CUDA_ARCHITECTURES=89`), driver
595.84, the owner's workstation, *not* the CI runner, whose numbers would be
several times worse and are not recorded here. Kokkos 5.2.0, GCC 15.3.0.
Median of three runs; the spread was under 2 % on the CPU rows and under 4 % on
the GPU row.

<!-- results:snl1-backends:start -->
| backend | shim ms/call | kernel ms/call | points/s (shim) | speed-up vs. Serial |
|---|---|---|---|---|
| Serial (OpenMP backend, 1 thread) | 25.88 | 24.96 | 3.9e4 | 1.0x |
| OpenMP, 32 threads | 5.95 | 5.02 | 1.7e5 | 4.4x |
| CUDA, RTX 4090 | 0.75 | 0.047 | 1.3e6 | 34x |

<sub>Generated by `scripts/results.py` from [`../bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`](../bench/results/snl1-i9-14900-rtx4090-2026-09-15.json); edit the record, not this table.</sub>
<!-- results:snl1-backends:end -->

### Two things these numbers are not

**They are not the fastest this kernel can go.** `ww_kokkos` is compiled with
`-ffp-contract=off` (and `--fmad=false` on CUDA) because the port's contract is
*the Fortran's arithmetic*: with GCC's default `-ffp-contract=fast`,
`-O3 -march=x86-64-v3` fuses `AWG1*UE(..) + AWG2*UE(..)` into an FMA (one
rounding where WW3 does two), and the `openmp-release` build drifted 1.1e-5
relative from the committed fixture while `serial-debug` stayed bit-identical.
With contraction off, all three presets reproduced the Fortran bit for bit, which
is what the "bit-identical" in the L1 column means (a historical result: the L1
tests were removed on 2026-10-14). Every row in the timing table is therefore the
cost of the **parity build**. Turning contraction back on is a physics change, not
a tuning knob, and it would invalidate the L2 replay verdicts.

**They are not what WW3 would see today.** `PATCH.md`'s phase-1 call site passes
`NPTS = 1`, one sea point per launch, because that is the shape `W3SRCE` has. The
CUDA row is measured at `NPTS = 1000`. The gap between the shim column (0.75 ms)
and the kernel column (0.047 ms) on CUDA is the phase-1 host↔device transfer:
94 % of the call. That gap is the entire argument for phase 2, and it is why the
CUDA row must not be read as "the model will be 34x faster".

## Level-0 scratch at WW4's default grid

WW4's commit `bdf3761` on `develop` (merged 2026-09-25) set WW4's default spectral space to
36 directions and 50 frequencies, XFR 1.07, first frequency 0.035 Hz `(v)` (`templates/ww4_run_config.yaml`
in NOAA-EMC/WW4; ww3-gpu issue #49). The DIA's per-point level-0 scratch grows with the
extended spectrum, and that grid crosses the RTX 4090's limit:

| grid | NSPEC | scratch floats × 4 B | fits ≈75 KB (RTX 4090)? |
|---|---|---|---|
| NK=25, NTH=24, XFR 1.1 (committed fixture) | 600 | 28 608 B | yes `(v)`, measured above |
| NK=32, NTH=36, XFR 1.1 | 1 152 | 52 992 B | yes ⚠ not run |
| NK=50, NTH=36, XFR 1.07 (WW4 default) | 1 800 | **80 352 B** | **no** ⚠ not run |

The byte counts are the float count of the formula in `snl1_dia.cpp:22` times
`sizeof(Real) = 4`, with NSPECX and NSPECY from `snl1_tables.cpp:126-129` and λ = 0.25;
`ScratchReal::shmem_size` adds per-view alignment padding on top, so they are lower
bounds. They reproduce the two figures in the table above:

```bash
python3 -c "
import math
def s(nk,nth,xfr,lam=0.25):
  l=math.log(xfr); ifrp=int(math.log(1+lam)/l); ifrm=int(math.log(1-lam)/l)
  nh=nk+ifrp+1-(ifrm-1); nc=nk-(ifrm-1); return 4*((nh*nth+nth)+8*(nc*nth+nth)+nk*nth)
print(s(25,24,1.1), s(32,36,1.1), s(50,36,1.07))"   # 28608 52992 80352
```

What happens on such a grid today: `ww::snl1::snl1` checks the request against
`TeamPolicy::scratch_size_max(0)` before launching and throws; the shim turns that into
`WW_KOKKOS_ERR_KERNEL`, and the `PATCH.md` call site stops the run with
`KOKKOS W3SNL1 FAILED, CODE ...` and `EXTCDE(1)` on the first source-term call. So the
failure is loud and immediate, never a silent wrong answer. ⚠ On the H100 (Hopper allows
up to 227 KB of shared memory per block) the WW4 grid should fit with room to spare; that
is inferred from the hardware limit, not run.

The fix for the 4090, not done: move the eight `SA*`/`DA*` work arrays, 16 128 of the
20 088 floats (80 %) at the WW4 grid, to level-1 scratch (global memory). That changes
the kernel's memory traffic, so it needs the WW3 regtest replay rerun and new timings before
this table can say "yes".

## L1 / L2

The port has no tests of its own. The owner's rule, 2026-10-14: "kokkos code
should NOT have its own tests, hard rule. All tests should come [from] WW3
regtests."

**L1 (history, removed with the tests, 2026-10-14)** was a unit test against the
Fortran: a committed binary fixture captured from a standalone copy of
`W3SNL1`/`INSNL1`, or that copy called in the same process.

| test | what it covered |
|---|---|
| `L1_test_snl1_tables` | `make_tables` vs. `INSNL1`: all 32 address tables exactly, weights and `AF11` to 1e-6 relative |
| `L1_test_snl1_dia` | `snl1` vs. `W3SNL1` to 1e-5 relative; zero-in/zero-out; cubic scaling; launch-to-launch bit-reproducibility |
| `L1_test_snl1_shim` | the same parity *through the C ABI*, plus the error paths: call before init, negative `npts`, null pointers, buffer growth, idempotent init |
| `shim_roundtrip` | a Fortran program calling `W3KOKKOSMD` → shim → kernel and comparing with `W3SNL1_REF` in the same process |

All four reported a max relative error of exactly **0.0** on all three presets
(2026-09-15; the two CPU presets re-run 2026-10-14). The tests, the standalone
reference and the fixture generator were removed on 2026-10-14; the fixture
itself stays as the benchmark's frozen input
([`tests/fixtures/README.md`](tests/fixtures/README.md)).

**L2** is the validation now: a WW3 regtest replay. `scripts/ww3-gpu.sh --ww3-gpu --cpu`
runs WW3 from one executable with `WW_KOKKOS_SNL1` flipped, compares the NetCDF
output field by field with `kokkos/tools/nccmp-tol`, then compares the Kokkos run
pair to pair with `--ww3` (WW3 built without the KOKKOS switch). The caller patch
lives on the h0ffmann/WW3 branch `ww3-gpu/kokkos-st4` (`src/fortran_iface/PATCH.md`);
nothing in this repository modifies `WW3/`. The WW3 regtest matrix of WFIP-0003
(`just ww3-gpu-matrix`, CTest in `regtests/` since 2026-10-15) extends the replay to WW3's own
regtests: byte-identical on the 100 serial and the 25 shared-memory OpenMP `NL1` entries (v,
`kokkos/port_report/W3SNL1_KOKKOS.md`, "How it was regtested").
