# W3DZXY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Gradients `DZZDX`, `DZZDY` of a sea-point field `ZZ` on the rectilinear `(IY,IX)` grid, by
centred differences in the `(p,q)` index space, one-sided next to land, mapped to `(x,y)` by
`DPDX`, `DPDY`, `DQDX`, `DQDY`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3DZXY`, `model/src/w3updtmd.F90:3211-3529` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/updt_dzxy.hpp`](../src/ww_kokkos/updt_dzxy.hpp), a header-only device function |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` GIMPLE of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), 2026-10-16. Per point and
per output: `tx = (ZZ(a) - ZZ(b)) * STX`, `ty = (ZZ(c) - ZZ(d)) * STY`,
`DZZDX = ((tx * DPDX) + (ty * DQDX)) * DFAC`, `DZZDY = ((tx * DPDY) + (ty * DQDY)) * DFAC`.

- (v) Fortran's left-to-right `a*b*c = (a*b)*c`; `tx` and `ty` shared between the two outputs
  (CSE, no bit change); no FMA (contraction off); the intermediate store of `DZZDX` before the
  `*DFAC` folded away (no bit change); neither loop nest vectorised.
- (v) A fresh reviewer found no bit change for finite inputs; the author line and the strided
  caller at `w3wavemd.F90:1507` (under `W3_REFRX`, which the lab switch does not set, passing
  `DCDX(IK,:,:)`, which a `LayoutLeft` View cannot alias: that caller must copy) were corrected
  after the review.
- ⚠ Three products are commuted in the assembly (`gy*DFAC` against `DFAC*gy`) whichever way the
  C++ source writes them; a product gives the same bits either way except which NaN payload
  survives when both operands are NaN, which needs a NaN grid metric `DPDY`/`DQDY` with a NaN
  `tx`/`ty`.
- `W3_S` (STRACE) and `W3_T` (the PRTBLK test print, `w3updtmd.F90:3486-3513`) are not compiled
  under the lab switch and not ported; `ZUNIT`, read only by that print, is not an argument.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA. Since then
`wave_support.cpp` instantiates its launcher on the default execution space in the
`serial-debug`, `openmp-release` and `cuda-release` builds (the CUDA one built, never run).
