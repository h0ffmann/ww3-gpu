# W3UNO2S_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12). Translated in the header
of its sibling [`W3UNO2`](W3UNO2_KOKKOS.md), batch 4 unit U3.

`W3UNO2s`: [`W3UNO2R`](W3UNO2R_KOKKOS.md) for a regular grid with sub-grid obstructions: the two
fluxes of the update are weighted by the transparencies `TRANS(IXY,JN)` and `TRANS(IXY,JP)`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UNO2s`, `model/src/w3uno2md.F90:836-1224` at `WW3@96892c95` (h0ffmann/WW3), byte-identical to NOAA-EMC/WW3 `03adda91` (v, `git -C WW3 diff` empty), as `mww3_test_01`'s `switch_PR3_UNO` compiles it |
| Kokkos code | [`../src/ww_kokkos/uno2_w3uno2.hpp`](../src/ww_kokkos/uno2_w3uno2.hpp): the launcher `ww::uno2::w3uno2s<ExecSpace>`; `TRANS(MY*MX,-1:1)` is the flat View `trans((IXY-1)+(J+1)*MY*MX)` |
| C shim | none yet |
| Callers at the pin | `W3XYP2` (`w3pro2md.F90:1020`, `:1024`, `:1044`, `:1048`) |

Not compiled and not ported: `W3_S`, `W3_T`, `W3_T0`, `W3_T1`, `W3_T2`, and the empty `W3_OMPH`
blocks (`:998`, `:1009`, `:1023`, `:1065`, `:1138`, `:1173`: no statement). `NDSE`, `NDST` and
`MX` as in `W3UNO2`.

## Arithmetic

The reference and the method are [`W3UNO2`](W3UNO2_KOKKOS.md)'s. The central and boundary loops are
`W3UNO2R`'s (`:1034-1045`, `:1080`, `:1106`).

- (v) Last loop (`:1146-1166`): `JN = -1` if `FLA(IXY-INC) > 0.` else `0`; `JP = 1` if
  `FLA(IXY) < 0.` else `0` (`comiss`, ordered: a NaN gives `0`); `Q = MAX(0., (Q +
  TRANS(IXY,JN)*FLA(IXY-INC)) - TRANS(IXY,JP)*FLA(IXY))`: `mulss`, `addss`, `mulss`, `subss`,
  `maxss` in the object, the same in the C++. The index of `TRANS(IXY,J)` is `(IXY-1) + (J+1)*MY*MX`
  (the object's `size`/`offset` arithmetic).
- (v) No FMA (`vfmadd`: 0, also with `-march=x86-64-v3`).
- (v) The throwaway differential of `W3UNO2` covered this routine too (4 000 random cases with
  `TRANS` in 0..1, 0 bits different, NaN injected). ⚠ Not kept; no command in the repository
  reproduces it.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk.

## WW3 defects found

None.
