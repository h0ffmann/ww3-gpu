# W3UTRN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Cell-boundary transparencies `ATRNX`/`ATRNY` for the propagation, from the sub-grid
obstructions `TRNX`/`TRNY`, the ice concentration (`IC0`) and icebergs.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UTRN`, `model/src/w3updtmd.F90:2809-3194` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`IC0` in, `W3_S` and `W3_T` out); called by `W3WAVE` at `w3wavemd.F90:1465` |
| Kokkos code | [`../src/ww_kokkos/updt_utrn.hpp`](../src/ww_kokkos/updt_utrn.hpp), a header-only device function; instantiated by `wave_support.cpp` in every preset |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2),
rebuilt by the reviewer inside the pinned `ww3` shell and byte-identical to the CMake build's
object, read as GIMPLE and assembly, 2026-10-16.

- (v) `:3002-3005`, `:3169-3175`: `((1+T(IXY))*T(N))/(1+T(N))`, a true `divss`, `1+T(IXY)`
  shared; `:3075-3076` `(DX*RADIUS)*DERA`; `:3083-3084` `expf(-((ICE*DX)/FICEL))`; `:3091,3105`
  `(LICEN-ICE*DX)/(LICEN-LICE0)`; `:3118-3119` `TRIX*expf(-(((BERG*FFACBERG)*DX)*1.E-4))`.
- (v) Constants with the object's bits: `RADIUS` `0x4ac247eb`, `DERA` `0x3c8efa35`, `1.E-6`
  `0x358637bd`, `0.0001` `0x38d1b717`.
- (v) `:3110-3111`: `minss` with 1 then `maxss` with 0, the same operand order in both objects
  (a NaN `T` gives 1).
- (v) No FMA, no reciprocal multiply, every `expf` a scalar libm call, no libmvec, in both
  objects. Four single products are commuted between the objects (`FFACBERG*BERG`, `exp*TRIX`,
  `A*(1+T)`, `DX*FICE0`): the same bits unless both operands are NaN.
- (v) Control flow: the neighbour indices with `NX == 1` taking the `IX == 1` branch, every
  `TRFLAG` value, the `MAPSTA == 2` sub-cases, `ICE(0:NSEA)` and `BERG(NSEA)` bases, the ice
  clamp inside `ICE > 0`, the iceberg branch outside the `IC0` block.
- (v) Parallel passes give the Fortran's bits: `MAPSF(:,3)` is one cell per sea point
  (`w3gridmd.F90:5184`), each pass writes its own cell, the one cross-point read (`TRIX`/`TRIY`
  of neighbours, `:3169-3175`) follows a fence after the pass that writes them; no reduction or
  atomic.
- ⚠ `:2954` reads `TRNY(IXY-1)`, which is `TRNY(0)` (out of bounds) when cell 1 is a sea point
  and `TRFLAG` is 1, 3 or 5. The header takes that `REAL` as `trny_before`; it is right only if
  the future shim reads the 4 bytes before the `TRNY` W3UTRN receives, and a packed temporary
  would move them.
- ⚠ With `NX == 1` or `NY == 1` and `TRFLAG` not 0, 1, 3, 5, the Fortran reads one element past
  `TRIX`/`TRIY`; the port reads the same index and cannot reproduce heap bytes. Whether WW3
  accepts such a grid was not checked.
- ⚠ CUDA's `expf` bits are not glibc's (`AGENTS.md`, "Bit for bit").

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a fresh reviewer compared the C++
object (`openmp-release` flags) with the Fortran object and found no defect; `wave_support.cpp`
instantiates its launcher on the default execution space in the `serial-debug`,
`openmp-release` and `cuda-release` builds (the CUDA one built, never run). Its CUDA SASS has 48
fused multiply-adds despite `--fmad=false`, taken to be CUDA's `expf` and not traced instruction
by instruction ⚠.
