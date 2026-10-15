# `kokkos/tests/fixtures/`: the benchmark's inputs

Two inputs live here, both frozen: `snl1_nk25_nth24.bin`, the sea state
`ww_bench_snl1` times the DIA on (and `proof/snl1_cons` reads), and `bench_small/`,
the small benchmark case `scripts/hoff.sh` runs and `L1_test_bench_case` compares
against.

| file | what it is |
|---|---|
| `snl1_nk25_nth24.bin` | `NK=25`, `NTH=24`, `NPTS=3`, 107 892 bytes: a sea state and the Fortran `W3SNL1`/`INSNL1` answers for it. |
| `bench_small/` | the six files of the small benchmark case plus `ww3_ounf.nml` (below). |

## The `.bin` is frozen

The `.bin` was written by a standalone copy of `W3SNL1`/`INSNL1` and a Fortran
generator, and the L1 parity tests compared the C++ port against it. Generator,
reference and tests were removed on 2026-10-14 under the owner's rule that the
port has no tests of its own and all its tests come from WW3's regtests. The file
is kept, unchanged, as the benchmark's input. Nothing in this repository can
regenerate it any more, so it is not edited: a new benchmark input is a new file.
The port is validated by `scripts/hoff.sh --hoff --cpu` against WW3 itself, not
against this file.

## What is in the fixture

`SETUP_REF(25, 24, 1.1, 0.04118)` (WW3's ST4/NL1 defaults: `LAMBDA = 0.25`,
`NLPROP = 2.50E7`, `KDCONV = 0.75`, `KDMIN = 0.50`, `SNLCS1..3 = 5.5, 0.833,
-1.25`, `FACHF = 5`), then a JONSWAP (U10 = 10 m/s, fetch = 100 km, γ = 3.3) ×
cos² sea state in action form `A = E(σ,θ)/σ`, at three depths:

| point | depth | `KDMEAN` | exercises |
|---|---|---|---|
| 0 | 1000 m | 158.2 | deep water: `EXP(X2)` underflows, `CONS = SNLC1` |
| 1 | 50 m | 7.91 | the shallow-water correction, small |
| 2 | 10 m | 1.73 | the shallow-water correction, ≈ −7 % on `CONS` |

`CG` comes from the linear dispersion relation at that depth (20 Newton steps on
σ² = g k tanh kd); `KDMEAN` is the band-energy-weighted mean of kd.

## Binary layout

Little-endian, no record markers (Fortran `ACCESS='STREAM'`), `int32` and
`float32` throughout, in this order:

```
header    int32   magic = 0x534E4C31 ('SNL1'), nk, nth, npts
          float32 xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, snls3, fachfe
          float32 sig(nk)

tables    int32   nfr, nfrhgh, nfrchg, nspecx, nspecy
          float32 dal1, dal2, dal3
          float32 awg(8), swg(8)
          int32   ip11, ip12, ip13, ip14, im11, im12, im13, im14,
                  ip21, ip22, ip23, ip24, im21, im22, im23, im24   (nspecx each)
          int32   ic11, ic21, ic31, ic41, ic51, ic61, ic71, ic81,
                  ic12, ic22, ic32, ic42, ic52, ic62, ic72, ic82   (nspec each)
          float32 af11(nspecx)

per point float32 kdmean, cg(nk), a(nspec), s(nspec), d(nspec)
```

`nspec = nk*nth`. The index tables are stored **1-based, exactly as Fortran holds
them**, and may be ≤ 0: `INSNL1` clamps `IF3..IF6` to zero, which puts addresses
in `1-NTH .. 0`. `ww::fixture::load()` subtracts one from every index; the C++
kernel then reads them at scratch slot `index + nth`.

Reader: `kokkos/src/ww_kokkos/fixture_io.{hpp,cpp}`, the one place that knows
this layout.

## `bench_small/`: the small benchmark case

`kokkos/tools/bench_case/` replaced `bench/make_bench_case.py`. Before the Python
was deleted it was run once, `python3 bench/make_bench_case.py --size small -o
kokkos/tests/fixtures/bench_small`, and the six files it wrote (`depth.inp`,
`mask.inp`, `namelists.nml`, `ww3_grid.nml`, `ww3_shel.nml`, `case.json`) are
committed unchanged. `L1_test_bench_case` regenerates the small case into a
temporary directory and compares every file byte for byte, so the C++ generator
cannot drift from the numbers the Python produced.

The seventh file, `ww3_ounf.nml`, is **not** from the Python. It never wrote one.
It was produced by the C++ generator and is pinned here so that it, too, cannot
change silently. The `.gitignore` rules for `depth.inp`/`mask.inp` are lifted for
this directory only.

There is nothing to regenerate: the fixture is a historical capture, and the test
is what keeps the generator honest. If the case format has to change on purpose,
the new files replace these in the same commit as the generator change.

SPDX-License-Identifier: MIT
