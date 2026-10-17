# W3XYP2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Spatial propagation of one spectral component on a regular grid with `PR2` and UQ: the sub-steps, the two `W3QCK3` sweeps, the boundary input and the swell-age diffusion correction for linear dispersion.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYP2`, `model/src/w3pro2md.F90:508-1264` at NOAA-EMC/WW3 `03adda91` (the whole file identical at the `WW3/` fork pins `357f575c` and `4a94a8f6`), as `switches/switch_lab_shrd` with `PR3` replaced by `PR2` compiles it (`PR2`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_xyp2.hpp`](../src/ww_kokkos/prop_xyp2.hpp), a header-only device function `w3xyp2`, serial, in the Fortran's structure and order; [`../src/ww_kokkos/prop_params2.hpp`](../src/ww_kokkos/prop_params2.hpp) for what it reads beyond `prop_params.hpp` (`DTME`, `CLATMN`, `HPFAC`, `HQFAC`, `MAPXY`, `NMXY`, `U10`, `WN`) |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`), recompiled with `-fdump-tree-optimized` and the flags of `flags.make`; its
disassembly is identical to the CMake build's object. It was read as GIMPLE and as assembly by the
translator, 2026-10-16.

- (v) No libm call but `lroundf` (`NINT`, `:827`; the C++ uses `roundf` with the same parity for every input), no FMA, no vectorised arithmetic (the only vector instructions are zero fills); `MINVAL`/`MAXVAL` as gfortran inlines them (NaN-skip scan, in-order reduction, `HUGE` when empty), reused from `prop_xyp3.hpp`; the `CGN` maximum as the tree `-O3` emits, whose last `maxss` takes its operands in the other order from `W3XYP3`'s; `0.575*GRAV`, `0.5*GRAV`, `DERA*RADIUS` and `DFAC = 1./RFAC**2` folded to the constants the GIMPLE shows (`static_assert`s).
- (v) Every `MIN`/`MAX` in the operand order of the emitted `minss`/`maxss`: the four transparencies (`:951-952`), `HPFAC`/`HQFAC` (`:953`), `XWIND` (`:956`), `TFAC` (`:963`), the boundary weights (`:1078`), the store (`:1230`). The 16-term `QXY` sum, `QXX`, `QYY` and `DVQ` left to right as written, each product one multiply; `VQ_OLD(IXY)` read once and reused, as the object does.
- (v) Store order: velocities, then diffusion coefficients, then per sub-step the `GSQRT` scaling, the `W3QCK3` sweeps in `YFIRST` order, the boundary input (before the diffusion, unlike `W3XYP3`'s averaging), the diffusion from `VQ_OLD`, and the store back skipping `MAPSTA <= 0`.
- (v) `VFDIFX`, `VFDIFY`, `VFDIFC` (`:732-733`) are zeroed at `:860-862` and never read: not allocated. `CGD`, `DSSD`, `DNND` are read only when `DTME /= 0`, as in the Fortran.
- ⚠ The cross-term stencil reads `VQ_OLD(IXY-NY-1)` and `VFDIFC_FAC(IXY-NY-1)`, one element below their lower bound for `IXY = 1`. `W3MAP2` lists only rows `IY = 2..NY-1` in `MAPAXY` (`w3pro2md.F90:380-388`), so with its maps every index read is in bounds; the port keeps the stencil as written. The maps (`MAPX2`, `MAPY2`, `MAPAXY`, `MAPXY`) and the transparencies are to be copied from the Fortran, not rebuilt.
- ⚠ Unlike `W3XYP3`, `W3XYP2` never clears `VQ`'s ghost columns: without closure the sweeps read them as `W3WAVE` left `FIELD`. The port reads the caller's buffer the same way; what `W3WAVE` leaves there is the shim's concern.
- (v) Reviewed once by a fresh reviewer, 2026-10-16 (Review, below). Static parity only: nothing was run ⚠.

## Review

2026-10-16, the fresh reviewer, single pass, static. Read `prop_xyp2.hpp` and `prop_params2.hpp` line by line against `w3pro2md.F90:508-1264` at `03adda91` and against the translator's `-O3` GIMPLE and a fresh `objdump -dr -l` of the same `w3pro2md.o`. Checked: the constants (`0.575*GRAV`, `0.5*GRAV`, `DERA*RADIUS`, `DFAC`, `TRNMIN`, `0.001`, `0.9999`, `3.3`, `2.3`) against the GIMPLE literals; every `maxss`/`minss` operand order, the `CGN` tree on both the `FLCUR` path (object 0x4d84-0x4db3, joining 0x235f) and the other one, `CXC`/`CYC`, the four transparencies, `HPFAC`/`HQFAC`, `XWIND`, `TFAC`, the boundary weights and the store; `MINVAL`/`MAXVAL` (empty: `HUGE`, all NaN: NaN, then the in-order scan) against bb 9-41; `NINT` as `lroundf` with the parity taken from the low bit (`not; and $1`), which `roundf` and the conversion reproduce, out of range and NaN both even; `DSSD`, `DNND`, `DCELL`, `DSS`, `DNN`, `VDXX`, `VDYY`, `VDXY`, the 16-term `QXY`, `QXX`, `QYY` and `DVQ` operation by operation, commuted products only; array bases and strides (`ATRNX(NY*NX,-1:1)`, `BBPI0(NSPEC,0:NBI)`, `WN` and `CG` `(0:NK+1,0:NSEA)`, `CX(0:NSEA)`, `U10(NSEA)`, `HPFAC(MY,MX)`, the `FAC` arrays and `VQ_OLD` offsets); the `W3QCK3` calls (argument order, `YFIRST` order, `GLOBAL` and `NY` for X, `.FALSE.` and 1 for Y). `VFDIFX`, `VFDIFY`, `VFDIFC` are written only at `:860-862` and never read in the routine (v). `MAPAXY` holds only `IY = 2..NY-1` (`w3pro2md.F90:380-388`), so `IXY-NY-1 >= 1-NY` for every listed cell (v). No defect in the code. One defect in this report, fixed: the fork pin is now `4a94a8f6` (the file is identical at it). The scratch translation unit compiled with the `serial-debug` flags (sanitizers, `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, `-ffp-contract=off`) with no warning, and at `-O3` with no FMA, no packed arithmetic and no libm call but `roundf` (v). Residual ⚠: for the four transparencies the `MIN_EXPR` operand order was read from the GIMPLE and matched to the `minss` registers by inference, which matters only for a NaN transparency; what `W3WAVE` leaves in `VQ`'s ghost columns; nothing was run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch translation unit that
includes the header and calls `w3xyp2` from a non-inline function, so its code is generated,
compiled with the `serial-debug` flags of `prop.cpp` (sanitizers, `-Wall -Wextra -Wpedantic
-Wshadow -Wconversion`, `-ffp-contract=off`) with no warning, and at `-O3` with no FMA and no libm
call but `roundf`. `openmp-release` and `cuda-release` were not compiled with it ⚠.
