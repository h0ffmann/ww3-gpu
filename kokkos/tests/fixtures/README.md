# `kokkos/tests/fixtures/` — captured Fortran answers

The L1 tests do not compare the C++ port against numbers a human typed. They
compare it against what WAVEWATCH III's own Fortran produced, captured once into
a committed binary file.

| file | what it is |
|---|---|
| `snl1_ref.F90` | standalone, **verbatim** copy of `W3SNL1`/`INSNL1` from `WW3/model/src/w3snl1md.F90` (WW3 7.14). LGPL-3.0-or-later, like its source. |
| `gen_snl1_fixture.F90` | the driver: builds three sea states, runs the reference, streams the record out. |
| `gen_snl1_ww3lib.F90` | optional cross-check driver, linked against a real `libww3.a` (see `kokkos/README.md`). |
| `snl1_nk25_nth24.bin` | the committed fixture: `NK=25`, `NTH=24`, `NPTS=3`, 107 892 bytes. |

## Regenerating

```bash
just snl1-fixtures        # builds gen_snl1_fixture in serial-debug and runs it
git diff --stat kokkos/tests/fixtures/snl1_nk25_nth24.bin
```

The generator is deterministic — no random numbers, nothing read from the
environment — so a byte-level diff means the reference or the sea state changed,
and that change needs a reason in the commit message.

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
them**, and may be ≤ 0 — `INSNL1` clamps `IF3..IF6` to zero, which puts addresses
in `1-NTH .. 0`. `ww::fixture::load()` subtracts one from every index; the C++
kernel then reads them at scratch slot `index + nth`.

Reader: `kokkos/src/ww_kokkos/fixture_io.{hpp,cpp}` — the one place that knows
this layout.

SPDX-License-Identifier: MIT
