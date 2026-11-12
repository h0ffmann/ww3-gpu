# W3UNO2R_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its sibling [`W3UNO2`](W3UNO2_KOKKOS.md), batch 4 unit U3.

`W3UNO2r`: the UNO2 propagation of [`W3UNO2`](W3UNO2_KOKKOS.md) reduced to a regular grid: the
Courant number `CFLL` replaces `VELO*DT/DX1` and the limiter works on the differences of `Q` alone.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UNO2r`, `model/src/w3uno2md.F90:472-805` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (v, `git -C WW3 diff` empty), as `mww3_test_01`'s `switch_PR3_UNO` compiles it |
| Kokkos code | [`../src/ww_kokkos/uno2_w3uno2.hpp`](../src/ww_kokkos/uno2_w3uno2.hpp): the launcher `ww::uno2::w3uno2r<ExecSpace>` |
| C shim | none yet |
| Callers at the pin | `W3KTP2` (`w3pro2md.F90:1640`, `:1656`) |

Not compiled and not ported: `W3_S`, `W3_T`, `W3_T0`, `W3_T1`, `W3_T2`. `NDSE`, `NDST` and `MX`
as in `W3UNO2`.

## Arithmetic

The reference and the method are [`W3UNO2`](W3UNO2_KOKKOS.md)'s (one object, `-O3`, no FMA).

- (v) `CFL = (CFLL(IXY)+CFLL(IXY+INC))*0.5` (`:649`); `QB = Q(IXYC) + ((SIGN(0.5,dQ)*(1.0-ABS(CFL)))
  * MIN(ABS(Q(IXYC+INC)-Q(IXYC)), ABS(Q(IXYC)-Q(IXYC-INC))))` (`:652`): `MIN` is `minss` with the
  forward difference first. `FLA = CFL*QB` (`:659`); the one-sided fluxes are `CFL*Q(IXYC)`
  (`:690`, `:716`).
- (v) Last loop (`:752`): `MAX(0., (Q + FLA(IXY-INC)) - FLA(IXY))`, `addss`, `subss`, `maxss` with
  `X` first.
- ⚠ Operand order of the commutative products and of the `addss` differs from the Fortran's for
  a NaN pair only, as in `W3UNO2`.
- (v) The throwaway differential of `W3UNO2` covered this routine too (4 000 random cases, 0 bits
  different, NaN injected). ⚠ Not kept; no command in the repository reproduces it.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk.

## WW3 defects found

None. (⚠ `W3_T1` prints in `W3UNO2` use `XCFL`, never set; `W3UNO2r` is not affected, and nothing of it is compiled.)
