# PTNGHB_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The nearest-neighbour table of the spectral grid used by the watershed partitioning. `W3PART`
calls it before the flooding (`w3partmd.F90:359`); `PT_FLD` reads it (`:946-947`, `:986-987`).
Each spectral point `N = I + (J-1)*NK` (`I` frequency, `J` direction) gets up to eight neighbours:
frequency neighbours stop at `I = 1` and `I = NK`, direction neighbours wrap from `J = 1` to
`J = NTH`. Integer only.

## What was ported

| | |
|---|---|
| WW3 routine | `PTNGHB`, `model/src/w3partmd.F90:630-821` at NOAA-EMC/WW3 `03adda91`, with the module state `MK`, `MTH`, `NEIGH` (`:88-92`); its only `#ifdef` is `W3_S` |
| Kokkos code | [`../src/ww_kokkos/part_ptnghb.hpp`](../src/ww_kokkos/part_ptnghb.hpp): `ww::part::ptnghb(nk, nth, neigh)`, the host builder, and `PtnghbTable`, the module state with the early return |
| Reader | `PT_FLD`, [`../src/ww_kokkos/part_fld.hpp`](../src/ww_kokkos/part_fld.hpp), which indexes `neigh[(N-1)*9 + K-1]` as this header lays it out |
| C shim | none yet |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16); `port_claims.tsv` and `PORT_TABLE.tsv` held only this PR's claim |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

Layout: `int neigh[9*NSPEC]`, column-major as `NEIGH(9,NSPEC)`. The values stay 1-based as the
Fortran stores them: slots 1 to `NEIGH(9,N)` hold spectral indices in `1..NSPEC`, slot 9 holds
the count, the other slots are 0 (`:704`).

## What WW3 does here at the pin

- The table is rebuilt only when `NK` or `NTH` differs from the last build (`:694`); `MK` and
  `MTH` start at -1 (`:88`, `:90`). It depends on nothing else, so in `ww3_multi` grids with the
  same `NK` and `NTH` share it, which is correct. `PtnghbTable::update` keeps the same rule.
- `NEIGH` is sized with `NSPEC` of `W3GDATMD` (`:697`); the C++ uses `NK*NTH`, which is how WW3
  sets `NSPEC` (`w3iogrmd.F90:664`, `w3gdatmd.F90:2127`).
- With `NTH = 1` both direction wraps (`:737-740`, `:751-754`) point at `N` itself, and with
  `NTH = 2` the wrapped and unwrapped direction neighbours coincide, so a point is listed twice.
  Translated as written; no real grid has `NTH` below 3 ⚠ (not checked against every regtest).
- The routine's own header documents arguments `IMI`, `IMD`, `IHMAX` (`:623-625`, `:652-656`) that it does not
  have: a documentation defect only.
- `NEIGH` is module state written without a lock: two threads calling `W3PART` with a new `NK` or
  `NTH` at once would race. The callers at the pin (`w3iosfmd.F90:278`, `ww3_outp.F90:1910`,
  `ww3_ounp.F90:2067`, and `W3CPRT` from `w3wavemd.F90:2488`) are not inside an OpenMP region
  (v, no `$OMP` directive in `w3iosfmd.F90`, `ww3_outp.F90`, `ww3_ounp.F90`, nor in
  `w3wavemd.F90:2300-2800`).

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_NCEP_st4`), identical to the CMake build's, read by the translator, 2026-10-16. Scripts
and dumps in the session scratchpad `xlate-ptnghb/` (`cfg.sh`, `dump.sh`, `tu.sh`).

- (v) Integer arithmetic only; every value is the Fortran's expression, in the Fortran's slot
  order, and exact while `9*NSPEC` fits an `int`. `J = (N-1)/NK + 1` truncates in both languages
  with a non-negative numerator; gfortran and g++ both emit the signed division of
  `(int)((unsigned)N - 1)` by `NK`.
- (v) `NEIGH = 0` is a `memset` of `36*NSPEC` bytes in both objects; slot 9 receives `K` last.
- (v) The early return compares `NK` with `MK` first, then `NTH` with `MTH`, as the C++ does.
- (v) The C++ compiles warning-free with the openmp-release flags (`-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -ffp-contract=off`).
- No floating point and no device code, so no CUDA build was made and the device-math caveat does
  not apply. A device `PT_FLD` gets the table by a `Kokkos::deep_copy` the shim owns.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
