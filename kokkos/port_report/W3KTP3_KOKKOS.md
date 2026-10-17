# W3KTP3_KOKKOS

**Status:** ported, phase 1, wired, opt-in (`WW_KOKKOS_KTP3=1`), batched: every sea point of an intra-spectral part in one call (2026-10-17). Run byte-identical inside WW3 on the CPU in
the full case; not regtested: the regtest matrix has not run.

Propagation in spectral space for one sea point: refraction in direction and the wavenumber shift from depth and current gradients, through `W3QCK1` and `W3QCK2`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3KTP3`, `model/src/w3pro3md.F90:1519-1961` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_ktp3.hpp`](../src/ww_kokkos/prop_ktp3.hpp), a header-only device function `w3ktp3`, serial, in the Fortran's structure and order |
| C shim | [`../src/fortran_iface/prop3_shim.cpp`](../src/fortran_iface/prop3_shim.cpp) (`ww_xyp3_batch`, `ww_ktp3_batch`, and on the resident spectrum `ww_xyp3_batch_va`, `ww_ktp3_batch_va`), bound by `w3kokkos_prop3md.F90` on h0ffmann/WW3 `ww3-gpu/kokkos-prop3` (`fcdfabef`) and `ww3-gpu/kokkos-prop3dev` (`90631fad`, which calls the `_va` forms); [PATCH.md sections 11](../src/fortran_iface/PATCH.md#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3) and [13](../src/fortran_iface/PATCH.md#13-the-propagation-on-the-resident-spectrum-ww3-gpukokkos-prop3dev) |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) Every floating-point statement in the object's order; no libm call, no FMA; the dead `FDDMAX` loop is not translated (-O3 removed it); the two CFL caps take their `MIN` operands in opposite orders, as the object does.
- (v) The `CFLKMAX` maximum is vectorised by -O3 (four lanes from the running value, a pairwise fold, a two-lane block, a scalar tail); the port follows the same grouping, so a NaN survives where the Fortran's does. The grouping was read from the object by the reviewer and corrected by a second one: after the four lanes, the two-lane block starts from the two partial folds, not from their maximum.
- ⚠ With both `WN(IK)` and `FKC(ITH)` NaN, the Fortran's multiply puts `FKC` first in the four-lane loop and `WN` first elsewhere, which decides which NaN payload propagates into `CFLK` and `CFLKMAX`; the C++ operand order there was not compared.
- (v) The k and theta passes in `MOD(ITSTEP,2)` order with the exact actual arguments of `W3QCK2` and `W3QCK1`; zero fills only where the Fortran zeroes.

## Benchmarks

CPU measurements of the wired `W3XYP3` and `W3KTP3`, each after its replay passed on the same tree
(2026-10-17, v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos <parts> --repeat 3`,
`switch_lab_ompg`, `OMP_NUM_THREADS` unset (32 threads), an Intel Core i9-14900, `ww3_shel` wall
time. The machine was shared with other sessions (load average 40 to 50 on 32 threads, `uptime`),
so the spread is wide and the medians are not a clean benchmark ⚠.

| `--kokkos` | `--ww3` three runs (s), median | `--ww3-gpu` three runs (s), median | `--ww3` / `--ww3-gpu` |
|---|---|---|---|
| `SRCE` (before this change) | 39.94, 37.74, 40.46; 39.94 | 66.50, 62.74, 66.36; 66.36 | 0.60x |
| `XYP3,KTP3,SRCE` | 41.49, 36.81, 41.40; 41.40 | 46.46, 54.43, 48.88; 48.88 | 0.85x |
| `XYP3,KTP3` (the source terms Fortran) | 44.12, 46.15, 43.57; 44.12 | 37.42, 27.32, 21.14; 27.32 | 1.61x |
| `XYP3,KTP3,SRCE` on the resident `VA` (`90631fad`, WFIP-0005 unit 3b; load average 9 to 19) | 23.06, 23.08, 23.21; 23.08 | 18.58, 18.50, 18.54; 18.54 | 1.24x |

Where the time goes, one fenced run each (`--timers`, not a timing): with `--kokkos SRCE` the run
spent 19.35 s outside Kokkos, propagation included; with `XYP3,KTP3,SRCE`, 2.89 s, while
`xyp3.call` took 1.80 s (24 calls) and `ktp3.call` 0.60 s (48 calls); with `XYP3,KTP3` alone,
`xyp3.call` 1.33 s and `ktp3.call` 0.44 s of a 22.41 s run. The copies (`.h2d`, `.d2h`) are 0 on
the OpenMP backend: the kernels read WW3's arrays in place. The gain comes from running the
spectral components of `W3XYP3` in parallel, where WW3's `DO ISPEC` loop is serial under this
switch, not from any change of arithmetic.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.

Update 2026-10-17, the wiring (v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos
XYP3,KTP3,SRCE`, with `switch_lab_ompg`, with `WW3GPU_SWITCH=switch_lab_shrd`, and with
`WW_KOKKOS_PROP3_BATCH_MIB=64` (six and three launches per call instead of one): `out_grd.ww3` and
the NetCDF field byte-identical (`cmp`) to the Fortran of the same executable, `out_grd.ww3`
byte-identical to `--ww3` (the NetCDF differs from it only in the `WAVEWATCH_III_switches`
attribute), `nccmp-tol` PASS. The counters: `W3XYP3` 13 824 spectra in 24 calls, `W3KTP3` 441 792
point calls in 48 calls. The smoke case (one point) makes no call of either. Not exercised by
`bench_small` ⚠: currents (`FLCUR`), the wavenumber shift (`FLCK`, so `W3QCK2`), index closure
and boundary input. The CUDA build compiles (`--gpu --build-only`), never run. The regtest matrix
did not run.

## Resident spectrum (WFIP-0005 unit 3b)

Since `90631fad` (`ww3-gpu/kokkos-prop3dev`, PATCH.md section 13) the fork calls `ww_ktp3_batch_va`: the
same kernels read and write the resident device copy of `VA` instead of WW3's array, so W3WAVE no
longer syncs `VA` to the host around the call when `WW_KOKKOS_KTP3=1` (the sync stays on the
Fortran path). Only the base pointer of the `VA` columns changed; the lambdas, the work arrays and
the order of every operation are those above. On a device the read-only inputs are copied only when
their bits change (a host shadow compared with `memcmp`); on the CPU nothing is copied either way.

Checked 2026-10-17 on the CPU (Kokkos OpenMP backend, the machine above, shared with other
sessions), never on the GPU (v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos
XYP3,KTP3,SRCE` on `switch_lab_ompg` and `WW3GPU_SWITCH=switch_lab_shrd`, each with and without
`WW_KOKKOS_VA_RESIDENT=1`, and on `switch_lab_ompg` `--kokkos SRCE`, `KTP3,SRCE` and `XYP3,SRCE`
under `WW_KOKKOS_VA_RESIDENT=1` (the mixed paths): `out_grd.ww3` and the NetCDF field `cmp`-equal
to the Fortran of the same executable, `out_grd.ww3` `cmp`-equal to `--ww3`, `nccmp-tol` PASS
both. The traffic of `VA` in the full case with `XYP3,KTP3,SRCE` (`WW_KOKKOS_VA_COUNT=1`) went
from H2D 25 / D2H 25 (one copy each way per step) to H2D 1 / D2H 13, the D2H only at the host reads
of the output block and of the return of `W3WAVE`. The `bench_small` branches listed above as not
exercised are still not exercised ⚠. The same command with `--repeat 3` after the replay, on the same commit,
gave the row "on the resident `VA`" of the table above, a quieter machine than the earlier rows,
so the rows are not a before/after comparison ⚠. The CUDA build compiles (`--gpu --build-only`),
never run ⚠. The regtest matrix did not run ⚠.
