# W3KTP1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Propagation in spectral space for one sea point with the first-order upwind scheme: refraction in direction and the wavenumber shift from depth and current gradients, both fluxes taken from the spectrum as it entered.

## What was ported

| | |
|---|---|
| WW3 routine | `W3KTP1`, `model/src/w3pro1md.F90:900-1230` at NOAA-EMC/WW3 `03adda91` (identical at the `WW3/` fork pin `357f575c`), as `switches/switch_lab_shrd` with `PR3 UQ` replaced by `PR1` compiles it |
| Kokkos code | [`../src/ww_kokkos/prop_ktp1.hpp`](../src/ww_kokkos/prop_ktp1.hpp), a header-only device function `w3ktp1`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, `.text` byte-identical to the CMake build's), read as GIMPLE and as assembly by
the translator, 2026-10-16.

- (v) Every floating-point statement in the object's order; no libm call, no FMA (no `vfmadd` in the object, contraction off in the C++); the ten automatic arrays are the object's ten `malloc`/`free` pairs, carved here from one caller buffer.
- (v) Every `MIN`/`MAX` is a `minss`/`maxss` (or packed) with the constant second, `(t > c) ? t : c` and `(t < c) ? t : c`, so a NaN velocity gives zero flux and a NaN slope ratio the divisor 1; the C++ uses the same ternaries in the same operand order.
- (v) The `FDDMAX` maximum (`:1081-1084`) is vectorised by -O3 only for `NTH >= 8`: four lanes from 0, the fold `max(l2,l0)`, `max(l3,l1)`, then `max(t1,t0)`, then a scalar tail of at most three directions, with no two-lane block; `NTH < 8` is a sequential loop. The port follows that grouping, so a NaN survives where the Fortran's does.
- ⚠ Where both operands of a commutative `+` or `*` can be NaN (`VCTH(ISP) + VCTH(IS2(ISP))`, the two terms of `VFLTH`, `FKD0 + WN*FKC`), the operand order decides which NaN payload propagates. This cannot be settled by a fixed C++ order: the Fortran object itself puts the `MAX` term of `VFLTH` in the destination register in its vector body (`addps` at `0x1a36`) and in two scalar tails (`0x1afe`) but the `MIN` term in the third (`0x1bb8`), and computes `VCWN` as `(FKC*WN) + FKD0` (`0x16cf-0x16d6`). It can only change a bit when two NaNs of different payload meet, and every NaN an SSE instruction creates is the same default quiet NaN, so only two different NaN payloads coming in through the arguments would show it (fresh reviewer, from the object, 2026-10-16). On a GPU the payload rules differ again.
- (v) The quirks are reproduced, not fixed: `DEPTH` is used as passed (the caller applies `DMIN`, `w3wavemd.F90:1819, 2131`); the `FDDMAX` slope filter reads `ESIN`/`ECOS` at `ITH = 1..NTH` only; `CTMAX = 0` divides by zero as the Fortran does; `VAA(1-NTH:0)` and `VAA(NSPEC+1:)` are written only under `FLCK` and read only there.
- (v) The C++ at `-O3` (g++ 16.2, `-march=x86-64 -ffp-contract=off`) keeps the `FDDMAX` grouping: four scalar accumulators from 0, each `maxss |v|,lane`, folded `maxss l1,l3`, `maxss l0,l2`, then `maxss t0,t1`, the same operands as the Fortran's `maxps`/`movhlps`/`shufps $0x55` fold (fresh reviewer, 2026-10-16).
- ⚠ The rest of the C++ object was not compared with the Fortran's instruction by instruction; a C `a > b ? a : b` keeps the IEEE meaning of `maxss` whatever GCC emits for it, which is the argument for the other `MIN`/`MAX` sites.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch TU that includes the
header and instantiates `w3ktp1` in a non-inline function compiled with the `serial-debug` command
of `prop.cpp` (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, ASan and UBSan,
`-ffp-contract=off`) with no warning, and `nm` shows the function's code was generated. The
`openmp-release` and `cuda-release` presets were not compiled with it ⚠.

## Review

2026-10-16, the fresh reviewer (single pass, static only, nothing run). Read the Fortran at `w3pro1md.F90:900-1230` (identical at `03adda91` and at the fork pin `357f575c`, checked with `diff`) against the header line by line, and against the `-O3` GIMPLE and the disassembly of the CMake build's object (`-O3 -g`, `W3_PR1` defined, `W3_REFRX` not; `.text` of the scratch object and of the CMake object compared equal with `cmp`). Checked: every operand grouping (`DSDD`, `FRK`, `VCTH` with and without `FLCUR`, `FKC`, `FKD`, `FKD0`, `VFLTH`, `VFLWN`, both `VA` updates); the operand order of every `minss`/`maxss`/`minps`/`maxps` (the variable first, the constant second, so a NaN gives the constant); the `comiss` branch of `DSDD`; the `FDDMAX` reduction (vector only when `NTH-1 > 6` unsigned, lanes from `pxor`, lane `k` taking `ITH = 4m+k+1`, the fold, the scalar tail, the scalar loop for `NTH < 8`); the loop bounds and the index offsets of the arrays based at `1-NTH` and `0`, including the `VAA` halo written under `FLCK` (reads `NSPEC-NTH+1..NSPEC`, writes `NSPEC+1..NSPEC+NTH` and `1-NTH..0`, disjoint for `NK >= 1`); the size of the work buffer; and the citations of the header and of this report. No defect found in the code. The report's NaN-payload `⚠` was narrowed from the object and stays `⚠`; the `-O0`-only `⚠` was replaced by the `-O3` check of the `FDDMAX` fold. The header compiles with no warning with `g++ -DKOKKOS_DEPENDENCE -DWW_DETERMINISTIC=1 -fsanitize=address,undefined -g -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -ffp-contract=off -fopenmp -c` in the `picasso/labs/pratico#ww3` shell, and `nm` shows `w3ktp1` generated.
