# W3SNL3MD_EXPAND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`EXPAND` of the Generalized Multiple DIA `W3SNL3` (switch `NL3`): it writes the action spectrum
`A(NTH,NFR)` as the energy density `A*TPI*XSI/XCG` into `SPEC(1-NTHMAX:NTH+NTHMAX,NFRMIN:NFRMAX)`,
with zero columns for `IFR <= 0`, a tail scaled by `FACHFE` for `IFR > NFR`, and `NTHMAX`
periodic directions on each side, so that the quadruplet loops of `W3SNL3` address it without
wrapping. It is contained in `W3SNL3` and reads `A`, `XCG`, `XSI`, `FACHFE` and the dimensions by
host association. The report is named with its module because `W3SNLS` has its own `EXPAND`
([`W3SNLSMD_EXPAND_KOKKOS.md`](W3SNLSMD_EXPAND_KOKKOS.md)).

## What was ported

| | |
|---|---|
| WW3 routine | `EXPAND` inside `W3SNL3`, `model/src/w3snl3md.F90:644-703` at NOAA-EMC/WW3 `03adda91`; called once, `CALL EXPAND ( UE )` (`:406`), so `SPEC` is `UE(NSPMIN:NSPMAX)`. No `#ifdef` inside `EXPAND` |
| Switches that compile it | `NL3`: `model/bin/switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the regtest switch `regtests/ww3_ts1/input/switch_ST4_GMD` (`git grep -lw NL3 -- '*switch*'`) |
| Kokkos code | [`../src/ww_kokkos/snl3_gmd.hpp`](../src/ww_kokkos/snl3_gmd.hpp), `ww::snl3::expand`; `KOKKOS_INLINE_FUNCTION`, no allocation, the host-associated `A`, `XCG`, `FACHFE` and the `Snl3Tables` passed as arguments |
| Relation to `W3SNL3` | `w3snl3_point` (step 1.b) keeps its own inline copy of the same loops, translated with `W3SNL3` ([`W3SNL3_KOKKOS.md`](W3SNL3_KOKKOS.md)). Replacing that copy by a call to `expand` was tried and changed the `w3snl3_point` object (register allocation and addressing; the floating-point instructions were the same), so the caller was left as it was and `expand` stands beside it |
| C shim | none yet; `W3SNL3` is its only caller |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: `w3snl3md.F90` recompiled with the flags of WW3's CMake Release build under
`switch_lab_shrd` with `NL3` in place of `NL1` (gfortran 16.2.0, pinned shell); the object is
identical to the CMake object (`objdump -d` compared). `EXPAND` has no symbol of its own: it is
inlined into `W3SNL3`. Compared with a throwaway C++ unit built with the `openmp-release` flags
(g++ 16.2.0, `-ffp-contract=off`), 2026-10-16.

- (v) The floating-point operations are `A*TPI` (`:684`, `TPI` folded to `6.2831854820251465`,
  `snl3_detail::kTpi` = `0x1.921fb6p+2`), `(SPEC*XSI(IFR))/XCG(IFR)` (`:687`, a multiply then a
  true division, no reciprocal) and `SPEC*FACHFE` (`:691`); the rest is a `memset` (`:682`) and
  copies (`:695-696`). The C++ object of `expand` has three `vmulss`, one `vdivss`, no FMA, no call.
- (v) gfortran vectorises `:684`, `:687` and `:691` (16-byte vectors with an 8-byte epilogue) and
  g++ does not; every operation is elementwise, so the lanes give the same bits. The scalar
  epilogue of `:687` multiplies as `XSI*SPEC` (operands swapped); IEEE multiplication is
  commutative in value, and `XSI` is a positive finite table value, so no NaN payload can differ.
- (v) The directional wrap (`:694-697`) is kept in source order, row `NTH+ITH` then row `1-ITH` for
  each `ITH`; the order matters when `NTHMAX > NTH`, where a row reads one written earlier.
- (v) Layout: `UE(NSPMIN:NSPMAX)` as `SPEC(1-NTHMAX:NTH+NTHMAX,NFRMIN:NFRMAX)` puts `SPEC(ITH,IFR)`
  at `UE(ITH + (IFR-1)*NTHEXP)`, with `NTHEXP = NTH+2*NTHMAX`, `NSPMIN = 1+(NFRMIN-1)*NTHEXP-NTHMAX`,
  `NSPMAX = NFRMAX*NTHEXP-NTHMAX` (`:1153-1156`); every element of `SPEC` is written, as
  `INTENT(OUT)` requires.
- (v) No reduction, no MIN/MAX, no libm or libmvec call, no integer conversion.
- (v) `w3snl3_point` compiled in a throwaway unit before and after `expand` was added: objects
  identical (`objdump -d` compared).
- (v) Fresh review 2026-10-16: no defect. The inlined `:682-697` GIMPLE of `W3SNL3` (`A*TPI`, `(SPEC*XSI)/XCG`, `SPEC*FACHFE`, `memset`, copies) was read against a new C++ unit's `expand`: 3 `vmulss`, 1 `vdivss`, no FMA, no call. `w3snl3_point` compiled from the `HEAD` header and from this one gives identical disassembly. Nothing changed.
- ⚠ Not compiled for CUDA.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The `ww3_ts1` regtest with `switch_ST4_GMD` compiles `NL3` and would
exercise `EXPAND` through `W3SNL3` once that is wired.
