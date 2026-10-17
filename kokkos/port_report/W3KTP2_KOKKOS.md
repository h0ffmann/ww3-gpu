# W3KTP2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Propagation in spectral space for one sea point under `PR2`: refraction in direction, with the depth-refraction slope filter, and the wavenumber shift from depth and current gradients, through `W3QCK1` and `W3QCK2`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3KTP2`, `model/src/w3pro2md.F90:1306-1710` at NOAA-EMC/WW3 `03adda91` (identical at the fork pin `357f575c`), as `switches/switch_lab_shrd` with `PR3` replaced by `PR2` compiles it (`PR2`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_ktp2.hpp`](../src/ww_kokkos/prop_ktp2.hpp), a header-only device function `w3ktp2`, serial, in the Fortran's structure and order; it calls the translated `w3qck1` and `w3qck2` |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags from
`flags.make`, gfortran 16.2, `-march=x86-64`; the object rebuilt with `-fdump-tree-optimized`
disassembles identically to the CMake build's), read as GIMPLE and as assembly by the translator,
2026-10-16, and again by the fresh reviewer the same day (see Review).

- (v) Every floating-point statement in the object's order; no libm call, no FMA in the Fortran object or in the C++ `-O3 -ffp-contract=off` object of a scratch unit that instantiates `w3ktp2`.
- (v) `FDDMAX` (`:1510-1512`) is the one reduction. -O3 vectorises it when `NTH/2 >= 4`: four lanes from +0.0, each `maxps |v|,lane`, folded `max(max(l3,l1), max(l2,l0))`, then one scalar `maxss` per remaining direction; with `NTH/2 <= 3` only the scalar form runs. The port has the same grouping, so a NaN propagates as in the Fortran. The C++ `-O3` GIMPLE keeps every max as a compare and select in that grouping, with no `MAX_EXPR` and no `REDUC_MAX`.
- (v) `MAX(1., FRK*FDDMAX/CTMAX)` (`:1516`) is `(x > 1) ? x : 1` at all four sites (vector, two-lane, two scalar epilogues), so a NaN quotient leaves `FRK` undivided; `MAX(0., ...)` in `DSDD` (`:1480`) is `(t > 0) ? t : 0`.
- ⚠ Where both operands of a multiply or an add are NaN with different payloads, which payload propagates depends on the destination operand of the `mulss`/`addss`, which register allocation chooses after GCC has canonicalised the commutative operands; neither the Fortran nor the C++ source order pins it. The C++ writes the operands in the object's GIMPLE order where it differs from the source (`CTHG0*FACTH`, `FDDMAX*FRK`, `DCYDX*FACTH`), and the `DSDD` loop's instructions have the same operand order in both objects (`DEPTH*WN`, `WN*CG`), but the other sites were not compared instruction by instruction.
- (v) The k and theta passes in `MOD(ITSTEP,2)` order with the exact actual arguments of `W3QCK2` (`:1618`, `:1668`) and `W3QCK1` (`:1634`, `:1650`); the object tests `ITSTEP & 1`, which agrees with `MOD` for negative values.

## Quirks kept

- (v) `VQ` is zeroed only when `FLCK` (`:1467`) and `VCFLT` never; `W3KTP3` zeroes both (`w3pro3md.F90:1689-1690`). With `FLCTH` and not `FLCK`, rows 1 and `NK+2` of `VQ` and its columns 0, `NTH+1`, `NTH+2` hold whatever the buffer held; rows 1 and `NK+2` and column `NTH+1` of `VCFLT` too. `W3QCK1` with `INC = NK2` and `CLOSE` true reads them only through its closure copies and a stencil that stays in rows 2..`NK+1` (`w3uqckmd.F90:283-333`), and the store reads rows 2..`NK+1`, so no garbage reaches `VA`: the closure copies row `IY` to row `IY` (`w3uqckmd.F90:292-297`), every stencil offset is a multiple of `INC = NK2` (`:308-402`), and the update reads `FLA(IXY-INC)` and `FLA(IXY)` of `MAPACT` points only (`:420-431`). `W3QCK2` runs only with `FLCK`, after the memset. The port does not zero them either. Checked by the fresh reviewer against those lines, not executed.
- (v) `CFLK(NK2,NTH)` is passed to `W3QCK2`'s `VELO(MY*(MX+1))`, one column larger; with `CLOSE` false the largest index read is `NK2*NTH`. The port allocates the extra column and never touches it. `DB(:,NTH+1)`, `DM(:,0)` and `DM(:,NTH+1)` are never written and never read: with `INC = 1` and `CLOSE` false, `W3QCK2` reads `VELO` and `DX2` at rows 1..`NK+1` and `DX1` at rows 2..`NK+1` of columns 1..`NTH` only (`w3uqckmd.F90:710-810`, `MAPWN2` from `w3pro2md.F90:435-463`). Checked by the fresh reviewer, not executed.
- (v) `ISEA`, `DCDX`, `DCDY` and `ITIME` are never read in this build (`W3_T`, `W3_REFRX`); `DEPTH` is used as the caller passes it, `MAX(DMIN, DW(ISEA))` (`w3wavemd.F90:1819`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch translation unit that
includes the header and instantiates `w3ktp2` (so its code, `w3qck1`'s and `w3qck2`'s are
generated) compiled with the `serial-debug` flags of `prop.cpp` (`-fsanitize=address,undefined -g
-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -ffp-contract=off -fopenmp`) and at
`-O3`, with no warning. The `openmp-release` and `cuda-release` presets were not compiled with it ⚠;
nothing in the library includes the header yet.

## Review

2026-10-16, the fresh reviewer, static only (nothing run). Read `w3pro2md.F90:1306-1710` at
`03adda91` (identical at the fork pin `357f575c`, checked with `diff`) line by line against the
header and against the translator's `-O3` GIMPLE and disassembly of `w3pro2md.o`, whose
disassembly was confirmed identical to the CMake build's object. Checked: every expression's
grouping and float type; `DEPTH*WN .LT. 5.` as `comiss`/`jbe` (NaN takes the zero branch);
`MAX(0.,t)` as `maxss` 0 into `t` and `MAX(1.,x)` as `maxps`/`maxss` 1.0 into `x` at all four
sites; the `FDDMAX` vector path (entered when `NTH/2 >= 4`, lanes `ITH..ITH+3` from +0.0,
`maxps v,lane`, fold `movhlps`/`maxps` then `shufps 0x55`/`maxps`, giving
`max(max(l3,l1), max(l2,l0))`) and the shared scalar tail used alone for `NTH/2 <= 3`; the C++
`-O3` GIMPLE keeps the same compare-and-select order with no `MAX_EXPR`, no FMA and no libm call;
loop bounds and index bases of every array, `VQ`'s offset `NK+1 = NK2-1` matching `Q(1-MY:)`, `DM`'s
matching `DX2(1-MY:)`; statement order, the `MOD(ITSTEP,2)` test (`& 1`), and the actual
arguments of both `W3QCK1` and `W3QCK2` calls. One defect, in a comment: `prop_ktp2.hpp:94` cited
W3QCK1's `FLA` at `w3uqckmd.F90:171`, which is a comment line; it is `:232`, now fixed (the same
wrong citation in `prop_qck1.hpp:27` and `prop_ktp3.hpp:87` was fixed in the same change).
The translator's two ⚠ on uninitialised `VQ`/`VCFLT` entries and the unwritten `DB`/`DM`/`CFLK`
columns were settled from the `W3QCK1`/`W3QCK2` index ranges (above); the NaN-payload ⚠ stays. The
scratch unit recompiled with the `serial-debug` flags (ASan, UBSan, `-Wshadow -Wconversion`) and at
`-O3`, with no warning.
