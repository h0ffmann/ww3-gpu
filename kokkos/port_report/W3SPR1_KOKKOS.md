# W3SPR1_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1098, :2212` behind `WW_KOKKOS_SPR1` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Mean wave parameters of the WAM-3 source terms (switch `ST1`): the spectral energy
`EMEAN`, the mean frequency `FMEAN`, the mean wavenumber `WNMEAN` and the largest action
density `AMAX`, with the parametric tail added through `FTE`, `FTF`, `FTWN`. `W3SRCE` calls it
per sea point (`w3srcemd.F90:1038`, `:1866`); the point post-processors call it too
(`ww3_outp.F90:1958`, `:2132`, `ww3_ounp.F90:2116`, `:2293`, `gx_outp.F90:1061`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SPR1`, `model/src/w3src1md.F90:87-240` at NOAA-EMC/WW3 `03adda91`; compiled by 57 regtest switches (`ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tc1`, `ww3_tr1`, `mww3_test_05`; `git grep -lw ST1 -- '*switch*'`) and by `model/bin/switch_USACE_1`, by no NCEP or NWS operational switch. Its only `#ifdef` blocks are `W3_S`/`W3_T` output: no variant changes the arithmetic |
| Kokkos code | [`../src/ww_kokkos/srce_st1_spr1.hpp`](../src/ww_kokkos/srce_st1_spr1.hpp), `ww::srce::st1::w3spr1_point`; the automatic array `EB(NK)` (`:181`, a `malloc` per call in the object) is a caller-provided work array |
| C shim | none yet; it passes `SIG(1:NK)`, `DDEN(1:NK)`, `FTE`, `FTF`, `FTWN` of the current grid |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src1md.F90` as WW3's CMake Release build compiles it
(gfortran 16.2, `switch_lab_shrd` with `ST1 FLX1`), recompiled from the build's `flags.make` and
checked identical to the CMake object, against the C++ under the `openmp-release` flags plus
`-ffp-contract=off`, both read operation by operation, 2026-10-16.

- (v) Section 1 (`:196-202`): `EB(IK)` is summed in `ITH` order on every path (the vector loop
  extracts the four lanes and adds them one by one). `AMAX` is not a scalar chain when
  `NTH >= 8`: `maxps` keeps four lanes, lane `j` holding `ITH = 4g+j+1`, each started from
  `AMAX`; the lanes fold as `movhlps`/`maxps`/`shufps $0x55`/`maxps`, i.e.
  `max(max(l3,l1), max(l2,l0))` with the `maxss` destination rule, then the `NTH mod 4` tail one
  `maxss` at a time. For `NTH < 8` it is one scalar chain `AMAX = AMAX > A ? AMAX : A`. The C++
  writes both paths; the order only shows in a NaN or a `+0/-0` tie, where it decides the bits.
- (v) Section 2 (`:206-211`): vectorised in the object (and versioned for aliasing), but the
  three sums are added lane by lane in `IK` order; products, quotients and `sqrtps` are
  correctly rounded per element, so a scalar loop gives the same bits. `SQRT` is `sqrtss`/`sqrtps`;
  the C++ `Kokkos::sqrt` is `vsqrtss` with a `sqrtf` fallback for a negative argument, the same
  default NaN.
- (v) Sections 3 and 4 (`:216-224`): `EBAND = EB(NK)/DDEN(NK)`; `TPIINV` and `1.E-7` are folded
  into `.rodata.cst4` (`0x3e22f983`, `0x33d6bf95`), written as hex literals with a `static_assert`
  of how they are built; `MAX(1.E-7, x)` is `maxss 1.E-7,x` (a NaN sum gives `1.E-7`) at both
  sites; `**2` is `t*t`.
- (v) No FMA, no libm or libmvec call (only the inlined square root), and no reduction other than
  the in-order sums and the `AMAX` lanes above in either object.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, Kokkos 5.2.0 CUDA), never run. The SASS holds
  `FFMA` inside the IEEE division and square root sequences (`FCHK`, `MUFU.RCP`, `MUFU.RSQ`); they
  were attributed by count, not read line by line. On a device the `AMAX` lanes run as written,
  so the device takes the same path as the host for each `NTH`.
- ⚠ NaN payloads: some operands are commuted between the objects, which is exact for every
  non-NaN input, signed zeros included. In the `NTH >= 8` loop of section 1 g++ adds the first
  lane as `A + EB` where the Fortran adds `EB + A`, and in section 2 it multiplies `EB*DDEN` where
  the Fortran multiplies `DDEN*EB`. Only when both operands are NaN with different payloads can
  the payload stored in `EB`, and so in `EMEAN`, `FMEAN`, `WNMEAN`, differ.
- (v) Fresh review 2026-10-16: no defect. The `w3src1md.F90` object was recompiled from
  `build_st1`'s `flags.make` and found identical to the CMake object; the C++ unit was compiled
  under the `openmp-release` flags, warning-free, no FMA. Compared instruction by instruction:
  the `NTH >= 8` threshold (`lea -1; cmp $6; jbe` against `cmp $7; jle`), every `maxss`/`maxps`
  destination in the four lanes, their fold and the tail, the scalar chain for `NTH < 8`, the
  in-order sums from `+0`, the section 3 and 4 operand order, both `MAX(1.E-7, x)` sites and the
  `.rodata.cst4` bits of `TPIINV` (`.LC7`) and `1.E-7` (`.LC8`); the only `#ifdef` blocks are
  `W3_S`/`W3_T`; argument order, the `SIG(1:NK)` base and the column-major `A` match. Nothing was
  changed; the commuted operands above were added as ⚠.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
