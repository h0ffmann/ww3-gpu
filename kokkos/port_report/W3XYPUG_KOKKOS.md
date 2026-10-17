# W3XYPUG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the four
scheme hooks, a shim and the `W3WAVE` wiring first. Reviewed statically by a fresh reviewer on
2026-10-16: no arithmetic defect, one citation fixed (see Review).

Spatial propagation of one spectral component on an unstructured grid without `PDLIB`: it turns
`VQ` into the action `AC`, builds the advection velocities `C` (group velocity, plus the current
on nodes with `IOBP = 1`), calls the selected fluctuation-splitting scheme and stores the result
back into `VQ`. The `PDLIB` sibling is [PDLIB_W3XYPUG](PDLIB_W3XYPUG_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYPUG`, `model/src/w3profsmd.F90:69-272` at NOAA-EMC/WW3 `03adda91` (byte-identical in `WW3/` at the fork pin, `cmp`), called from `w3wavemd.F90:1970`, `:1973`, `:1976` (one per `PR1`/`PR2`/`PR3`), as the switch of `ww3_tp2.6` compiles it (`SHRD PR3 UQ ... REF0`) |
| Kokkos code | [`../src/ww_kokkos/prop_xypug.hpp`](../src/ww_kokkos/prop_xypug.hpp), a header-only host function `w3xypug`, serial, in the Fortran's structure and order; it calls the ported `SETDEPTH` ([SETDEPTH](SETDEPTH_KOKKOS.md)) directly |
| Hooks | `XypugHooks`: `dsec21` and the four schemes `W3XYPFSN2`, `W3XYPFSPSI2`, `W3XYPFSFCT2`, `W3XYPFSNIMP` (`w3profsmd.F90:470`, `:730`, `:1290`, `:988`), which have no Kokkos translation. A hook the flags reach and that is missing aborts |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
switch of `ww3_tp2.6`, objdump-identical to the CMake build's), read as GIMPLE and as assembly by
the translator, 2026-10-16.

- (v) `VQ = (VQ/CG)*CLATS`, `VLCFLX = (CG*CCOS)/CLATS`, `VLCFLY = CG*CSIN`, the current terms
  `(FACX*CX)/CLATS` and `FACY*CY` added (`:207-229`): `divss`, `mulss`, `addss` in that order, no
  FMA, no libm call. The products commute, the bits do not change.
- (v) `IOBP` is `INTEGER*2` and compared with 1 (`cmpw $0x1`); `IOBDP` is `INTEGER*1`.
  `AC = DBLE(VQ)*DBLE(IOBDP)` (`cvtss2sd`, `cvtsi2sd`, `mulsd`) and `C = VLCFL*REAL(IOBDP)`
  (`:234-236`); the vectorised loops are element-wise, so the lanes cannot change a bit.
- (v) `DSEC21(TBPI0,TIME)` is called before `DSEC21(TBPI0,TBPIN)` (`:242-243`); without `FLBPI`,
  `RD1 = 1`, `RD2 = 0`. The first true flag of `FSN`, `FSPSI`, `FSFCT`, `FSNIMP` selects the scheme
  (`:251-259`); with none set no scheme runs and `VQ` is still written back.
- (v) `VQ(IX) = REAL(AC(IX))` for every node, sea or not (`cvtsd2ss`); `ISEA = MAPFS(1,IX)` is a
  dead store (only `DEBUG` in the GIMPLE) and is left out. The store
  `VQ = MAX(0., CG/CLATS*VQ)` (`:270`) is `divss`, `mulss`, `maxss %xmm1(+0),%xmm0`, so NaN and
  −0 store +0; the C++ object at `-O3` emits the same `maxss` operand order.
- (v) `MAPSF` has leading extent `MSEA` (`w3gdatmd.F90:1736`), which the state carries apart from
  `NSEA`. `VGX`, `VGY` are read only under `W3_MGP` (`:212-215`), not ported; `W3WAVE` passes
  `FACX` as `FACY`, both are kept.
- ⚠ The shim must pass `C(NX,2)` and `AC(NX)` contiguous to the Fortran schemes, whose dummies
  are assumed shape (`:547-548`); not checked against a shim, which does not exist yet.
- (v) A fresh reviewer read the translation against the Fortran, the GIMPLE and the object,
  2026-10-16 (Review below). Static only: no WW3 regtest has compared the two ⚠.

## Review

2026-10-16, the fresh reviewer, static only (nothing was run). Read `prop_xypug.hpp` line by line
against `w3profsmd.F90:69-272` (identical in `WW3/` and `WW3-noaa/`, `cmp`), the translator's
`-O3` GIMPLE and the disassembly of `__w3profsmd_MOD_w3xypug` in the CMake build's
`w3profsmd.F90.o` (gfortran 16.2.0, `-O3 ... -fno-second-underscore`, no fast-math; the
translator's object disassembles identically). (v) Checked: the `VQ(1-NY:)` offset, the `MAPSF`,
`CLATS`, `CG`, `CX`, `CY`, `DW` bases and every declaration line cited; `ITH`/`IK` (`idivl`);
the operand order of every `divss`/`mulss`/`addss` in section 2 and the current loop, with
`FACX`/`FACY` as `CCURX`/`CCURY`; the `INTEGER*2` test `cmpw $0x1`; `AC = DBLE(VQ)*DBLE(IOBDP)`
and `C = VLCFL*REAL(IOBDP)` (integer to floating conversions exact, products element-wise);
`DSEC21(TBPI0,TIME)` before `DSEC21(TBPI0,TBPIN)`; the scheme selection and the contiguous
`C(NX,2)`, `AC(NX)` descriptors the Fortran builds for the schemes; `REAL(AC)` for every node
(`cvtsd2ss`, `cvtpd2ps`, both round to nearest); the final `maxss %xmm1,%xmm0` with `%xmm1` from
`pxor`, which is `x > 0 ? x : +0`, the C++ expression. The ported `SETDEPTH` is called with
`IOBDP(1:MX)` and `DW(0:)`. The header compiles with the `serial-debug` flags (ASan, UBSan,
`-Wconversion`, `-ffp-contract=off`) without a warning; at `-O3` its object has no FMA and the same
`maxss` operand order. One defect, a citation: the `DMIN` comment cited `w3profsmd.F90:1667-1674`,
now `:1650` (the `USE`) and `:1674` (the comparison). No arithmetic defect.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch translation unit that
calls `w3xypug` compiled with `prop.cpp`'s `serial-debug` flags (`-std=c++20 -Wall -Wextra
-Wpedantic -Wshadow -Wconversion -ffp-contract=off`, ASan and UBSan) with no warning, and its code
was generated; the same unit at `-O3` was read in assembly. `prop.cpp` does not include the
header yet, so no preset builds it; CUDA was not compiled ⚠.
