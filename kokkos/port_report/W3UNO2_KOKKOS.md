# W3UNO2_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

One-dimensional second-order UNO2 propagation of a quantity `Q` along the lines of a 2-D field
for any distance between points: the flux `FLA` at each cell face (3rd-order central scheme with
a minmod-type limiter for the interior points, 1st order next to a boundary), then
`Q = MAX(0., Q + (FLA(IXY-INC)-FLA(IXY))/DX1(IXY))`. Batch 4, unit U3. The two regular-grid
variants [`W3UNO2R`](W3UNO2R_KOKKOS.md) and [`W3UNO2S`](W3UNO2S_KOKKOS.md) are in the same header.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UNO2`, `model/src/w3uno2md.F90:102-446` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3uno2md.F90` is empty, v), as `mww3_test_01`'s `switch_PR3_UNO` and `switch_PR2_UNO` compile it |
| Kokkos code | [`../src/ww_kokkos/uno2_w3uno2.hpp`](../src/ww_kokkos/uno2_w3uno2.hpp): the launcher `ww::uno2::w3uno2<ExecSpace>`, one `parallel_for` for each loop of the Fortran, in its order, on one execution space instance |
| C shim | none yet |
| Callers at the pin | `W3KTP2` (`w3pro2md.F90:1625`, `:1675`) |

Not compiled and not ported: `W3_S` (`STRACE`), `W3_T`, `W3_T0`, `W3_T1`, `W3_T2` (test prints and
their locals). `NDSE` and `NDST` are used only by those prints and are not in the argument list;
`MX` is used only for the size of `FLA`. `FLA(1-MY:MY*MX)` is the caller's work View, `MY*MX+MY`
long, zeroed by the launcher. Index offsets for the arrays with a lower bound other than 1 are in
the header.

## Arithmetic

The reference is the `-O3` object of `w3uno2md.F90` (WW3's CMake Release flags, gfortran 16.2.0,
`-ffp-contract=off`), recompiled with `-fdump-tree-optimized-lineno`; its disassembly equals the
CMake `switch_PR2_UNO` object's, and the object under `switch_PR3_UNO` is the same (`objdump -d`
compared, v, 2026-11-12). Read as GIMPLE and assembly:

- (v) `SIGN(1.1,X)` is `copysignf` of the `REAL` 1.1; `INT(MIN(0.,SIGN(1.1,X)))` is `-1` for a
  negative sign bit and `0` otherwise, `INT(SIGN(1.1,X))` is `+1` or `-1`. The C++ compiles the
  first as a compare and a conditional subtract, not `minss`; both give the same integer, as the
  argument is never NaN.
- (v) `MIN(A,B)` of the two limiter terms is `minss` with `A` (the forward difference over
  `DX2(IXYC)`) first, so a NaN or a tie gives `B`; the C++ has the same operand order.
  `MAX(0.,X)` is `maxss` with `X` first: a NaN or `-0.` gives `+0.`.
- (v) `QB = Q(IXYC) + ((SIGN(0.5,dQ)*(DX1(IXYC)-ABS(CFL))) * MIN(...))`: two `divss`, one `subss`,
  two `mulss`, one `addss`; the same multiset and order in the C++ object. `VEL = (V1+V2)*0.5`,
  `CFL = DT*VEL`, `FLA = CFL*QB`; the one-sided fluxes are `(VEL*DT)*Q`.
- (v) The last loop is `(FLA(IXY-INC)-FLA(IXY))/DX1(IXY)`, then `+Q`, then `MAX`. No FMA in either
  object (`vfmadd`: 0, also with `-march=x86-64-v3`).
- ⚠ g++ puts a memory operand of a commutative product second, so `CFL*Q` and the final
  `lim * (sign*(DX1-|CFL|))` have the other operand order than the Fortran's. Only a product of
  two NaNs can differ, in its payload.
- (v) The C++ unit with the `openmp-release` flags (`-O3 -march=x86-64-v3 -std=c++20 -fopenmp
  -ffp-contract=off -Wall -Wextra -Wpedantic -Wshadow -Wconversion`) and with `-O0 -g
  -fsanitize=address,undefined` compiles with no warning, for the Serial and OpenMP spaces.
- (v) A throwaway differential program linked the Fortran object of `w3uno2md.F90` and compared
  its outputs with the C++ Serial launcher on 4 000 random cases (grid size 3 to 8, `BCLOSE`
  on and off, `INC = 1` and `INC = MY`, one case in seven with a NaN in `Q`, `VELO` or `CFLL`):
  1 299 647 REALs (`Q`, `VELO`, `DX1`, `DX2` or `CFLL` for the three routines together), 0 bits
  different. ⚠ Not kept (ADR-0010): no command in the repository reproduces these counts. `MAPACT` had
  no repeated entry, `MAPBOU` could.

## Parallel order

Each loop over `IP` reads arrays it does not write and writes `FLA(IXY)` of its own entry, or
`Q(IXY)` of its own entry in the last loop, so the launches in the Fortran's order give the serial
bits. ⚠ Two preconditions the Fortran does not state and the C++ does not check: the entries of
`MAPACT` are all different (`W3MAP2` builds `MAPAXY` once for each cell; a repeated entry applies
the update twice in the Fortran and races here), and the entries of one `MAPBOU` list are different
or write the same value. The closure loop over `IY` keeps the statement order for each `IY`, and
different `IY` touch different elements (also for `NX = 1`, where `Q(IY+IADN1)` is read by the
next statement of the same `IY`). Not run on OpenMP with more than one thread ⚠.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: WW3's regtests are postponed to the validation phase (ADR-0016). Not wired: no shim
and no caller hunk. Not run on a GPU.

## WW3 defects found

None in the compiled code. ⚠ Read, not compiled: `W3_T1` prints `XCFL` (`:335`, `:362`), declared at `:200`, which no statement sets;
`DX2(IXYC)` and `DX2(IXYC-INC)` are divided by with no zero test (`:286-288`), so a zero band width
gives an infinite or NaN flux, which the C++ reproduces.
