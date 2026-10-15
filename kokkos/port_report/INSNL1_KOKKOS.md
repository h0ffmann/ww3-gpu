# INSNL1_KOKKOS

**Status:** L2 (inside WW3, on the CPU), with `W3SNL1`.

The tables of the DIA, `INSNL1`, built once at start-up for the Kokkos `W3SNL1`.

## What was ported

| | |
|---|---|
| WW3 routine | `INSNL1`, `model/src/w3snl1md.F90:483-786` at NOAA-EMC/WW3 `03adda91` ([`../ports.tsv`](../ports.tsv)) |
| Kokkos code | [`../src/ww_kokkos/snl1_tables.cpp`](../src/ww_kokkos/snl1_tables.cpp); C entry `ww_snl1_init` in [`../src/fortran_iface/snl1_shim.cpp`](../src/fortran_iface/snl1_shim.cpp) |
| Called from | `W3INIT`, through `WW_SNL1_INIT` in WW3's `w3kokkosmd.F90` (branch `hoff/kokkos-snl1`) |

## Benchmarks

None. `INSNL1` runs once when the model starts, and its time is not measured apart: the gprof
profile of `bench_small` does not list it among the routines that take time
([`../PORT_STATUS.md`](../PORT_STATUS.md), "Port score").

<!-- bench:start -->
| step | date | what | backend | value | baseline | unit | source |
|---|---|---|---|---|---|---|---|
<!-- bench:end -->

## How it was regtested

| Level | What | Result | Where |
|---|---|---|---|
| L1 (history) | the tables against `INSNL1`'s, three presets | all 32 address tables exactly; the weights and `AF11` within 1e-6 relative | `L1_test_snl1_tables`: removed with the tests, 2026-10-14 |
| L2 | every `--ww3-gpu --cpu` run builds the tables through `WW_SNL1_INIT`; the DIA that reads them passes | PASS, as in [`W3SNL1_KOKKOS.md`](W3SNL1_KOKKOS.md) | `just smoke-ww3-gpu`, `just full-ww3-gpu`, 2026-10-14 |
| L2, matrix | `just ww3-gpu-matrix`, after `pow11()` was given the chain gfortran uses at `-O3` for `AF11 = (SIG*TPIINV)**11` (`w3snl1md.F90:741`, `:750`): `x^5*x^6`, not the `-O0` `x^3*x^8` the `-g` fixture had validated | `test.comp` 0 differing files on all 100 entries; `nccmp-tol` 71 PASS, 29 n/a, 0 FAIL (v) | [`docs/data/hoff_matrix_2026-10-15.tsv`](../../docs/data/hoff_matrix_2026-10-15.tsv), [`docs/log/2026-10-14-snl1-bitwise-af11.md`](../../docs/log/2026-10-14-snl1-bitwise-af11.md) |

## GoogleTest, the WW4 way

None. `L1_test_snl1_tables.cpp` was a GoogleTest, run by CTest on every preset, until it was
removed on 2026-10-14 under the owner's rule: "kokkos code should NOT have its own tests, hard
rule. All tests should come [from] WW3 regtests." The tables are judged through the DIA's WW3
regtest replay (`scripts/ww3-gpu.sh --ww3-gpu --cpu`, pair to pair with `--ww3`).
