# W3FLGRDUPDT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Flag logic run once by `W3INIT`: an output field that needs a frequency range (`EF`, `TH1M`,
`STH1M`, `TH2M`, `STH2M` of group 3; `USF`, `USP`, `P2L` of group 6) is switched off in
`FLGRD` and `FLGR2` when the mod_def file left its bounds unset (`E3DF`, `US3DF`, `USSPF`,
`P2MSF`), with a warning when it had been requested; then the group flags `FLGD(3)`, `FLG2(3)`,
`FLGD(6)`, `FLG2(6)` are set to `ANY` of their group's fields. No floating-point arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLGRDUPDT`, `model/src/w3iogomd.F90:179-318` at NOAA-EMC/WW3 `03adda91`, author F. Ardhuin. `w3iogomd` is compiled by every switch; the only `#ifdef` is `W3_S` (`STRACE`, tracing only, not translated) |
| Callers | `W3INIT` only, unconditionally (`w3initmd.F90:780`), so every regtest runs it once per grid |
| Kokkos code | [`../src/ww_kokkos/iogo_w3flgrdupdt.hpp`](../src/ww_kokkos/iogo_w3flgrdupdt.hpp), `ww::iogo::w3flgrdupdt` (`KOKKOS_INLINE_FUNCTION`) |
| Structural change | The four `WRITE(NDSEN,1008)` sites (`:274`, `:282`, `:289`, `:296`) become bits of the returned mask (`flgrdupdt_detail::kWarn*`, bits 0-4 for the five `E3DF` fields); the shim writes format 1008 (`:311-314`) once per set bit, in bit order, which is the Fortran's order. `NDSO` is unused in the Fortran and dropped. Module state `E3DF(3,5)`, `P2MSF(3)`, `US3DF(3)`, `USSPF(2)` (`w3gdatmd.F90:715`) are arguments; `NOGRP = 10`, `NGRPP = 20` are the `PARAMETER`s of `w3odatmd.F90:330-331` |
| C shim | none yet; it passes `FLGRD`, `FLGR2`, `FLGD`, `FLG2` as `LOGICAL(4)` storage and does the `WRITE`s |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3iogomd.F90` recompiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), disassembly identical to the CMake
object of `build_lab`; compared with a throwaway C++ unit compiled warning-free with the
`openmp-release` flags (g++ 16.2.0), 2026-10-16.

- (v) No floating-point operation in either object. Integer tests the same: `E3DF(1,I) <= 0`,
  then `E3DF(3,I) < E3DF(2,I)`, and likewise for `US3DF`, `P2MSF`; `USSPF(1) <= 0`, then
  `USSPF(2) <= 0`.
- (v) `LOGICAL(4)` read as the whole 32-bit word against zero (`test %eax,%eax`) and written as
  `0` or `1` (`movl $0x0` / `movl $0x1`) in both objects, at the same byte offsets: `FLGRD(3,I)`
  at `8 + 40(I-1)`, `(6,8)` at `0x12c`, `(6,9)` at `0x154`, `(6,12)` at `0x1cc`.
- (v) `ANY(FLGRD(3,:))` etc. is a loop of stride 40 bytes over 20 elements that stops at the first
  non-zero word, in both, after the flag was set to `0`; groups 3 then 6, `FLGRD` before `FLGR2`.
- (v) Both objects short-circuit `FLGRD(IG,IP) .OR. FLGR2(IG,IP)`: `FLGR2` is read only when
  `FLGRD` is zero; the C++ computes the warning bit with `setne` where the Fortran branches to the
  `WRITE`. Reads and stores of the flags come in the same order in both.
- (v) In the Fortran, `E3DF`, `P2MSF`, `US3DF`, `USSPF` are pointer descriptors into
  `GRIDS(IMOD)` (`w3gdatmd.F90:1101`, `:2309-2312`), read through their strides; the C++ takes
  them contiguous and column-major, which the fixed-size components of `w3gdatmd.F90:715` are.
  The shim passes their base addresses.
- CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the header. The Fortran object recompiled under `build_lab` (identical to the CMake object) and a new C++ unit were compared block by block in GIMPLE and assembly: the five `E3DF` tests and the `US3DF`, `USSPF`, `P2MSF` tests (operands and order), the byte offsets of every flag read and store, the order of the `FLGD`/`FLG2` stores and the four `ANY` loops; the only `#ifdef` (`W3_S`) is tracing. Fixed in this report: the short-circuit bullet, which said the C++ read `FLGR2` before testing `FLGRD`; the object does not.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
