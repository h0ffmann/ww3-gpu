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
| L1 | the tables against `INSNL1`'s, three presets | all 32 address tables exactly; the weights and `AF11` within 1e-6 relative | `L1_test_snl1_tables` ([`../tests/`](../tests/)), CI job `kokkos` |
| L2 | every `--hoff --cpu` run builds the tables through `WW_SNL1_INIT`; the DIA that reads them passes | PASS, as in [`W3SNL1_KOKKOS.md`](W3SNL1_KOKKOS.md) | `just smoke-hoff`, `just full-hoff`, 2026-10-14 |

## GoogleTest, the WW4 way

`L1_test_snl1_tables.cpp` is a GoogleTest, run by CTest on every preset, as WW4 tests its code.
The tables have no L2 test of their own: they are judged through the DIA's.
