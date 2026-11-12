# W3UICE_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Ice concentration `ICE` and iceberg damping `BERG` of each sea point from the input fields and,
under `IC0`, the masking of the points that freeze (`MAPSTA < 0`, `VA(:,JSEA) = 0.`,
`CHARN = AALPHA`) and the unmasking of those that thaw (`UST = 0.05`, and the same reset when
`MAPST2` allows it).

## What was ported

| | |
|---|---|
| WW3 routine | `W3UICE`, `model/src/w3updtmd.F90:1806-2052` at h0ffmann/WW3 `96892c95` (247 lines), byte-identical to NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`IC0`, `ST4`, no `PDLIB`) |
| Kokkos code | [`../src/ww_kokkos/updt_uice.hpp`](../src/ww_kokkos/updt_uice.hpp): the launcher `ww::updt::w3uice<ExecSpace>`, three passes (map preparation, sea points, map restore) |
| C shim | none yet; the fork already repeats the `VA(:,JSEA) = 0.` columns on a resident `VA` under `WW_KOKKOS_UPDV=1` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 18), with the rest of `W3UICE` in Fortran |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1392-1398` at the pin (`W3KOKKOS_UICE` at `:1392`, `W3UICE` at `:1395`, `:1398`) |

Not compiled under the lab switch and not ported: `CHARN = 0.` (the non-`ST3`/`ST4` branches,
`:1969`, `:2005`), `T` (prints), `S`. `INIT_GET_JSEA_ISPROC` is its round-robin branch
(`w3parall.F90:1236-1237`, `parall_init_get_jsea_isproc.hpp`). `TICE = TIN` (`:1932-1933`) is
integer time on the host.

`VA` is `VA(NSPEC,0:NSEALM)` (`w3wdatmd.F90:491`, the dummy at `w3updtmd.F90:1902`); `AALPHA` is the `ST4` pointer of `w3gdatmd.F90:1328`; the port
takes it as a device View and writes only the columns the Fortran zeroes, so a resident `VA` needs
no copy (`AGENTS.md`, "Resident spectrum").

## Arithmetic

None in floating point. Reference and method as in [`W3UCUR_KOKKOS.md`](W3UCUR_KOKKOS.md).

- (v) `:1956` `ICEI >= FICEN` and `:1987` `ICEI < FICEN` are ordered compares in the GIMPLE: a
  NaN `ICEI` takes neither branch, as C++ `>=` and `<` do.
- (v) `:1938` `MAPICE = MOD(MAPST2,2)` is the routine's one vectorised loop (integer, truncating
  remainder, as C++ `%`); `:1965`, `:2001` are `memset` (+0.0); `:1990` stores the `REAL` 0.05,
  `5.00000007450580596923828125e-2`, which `static_cast<float>(0.05)` gives.
- (v) The second test (`:1987`) reads `MAPICE` after the first may have set it, in both.
- Parallel order: on a regular grid `MAPSF(ISEA,1:2)` is set once per sea cell of the loop over
  cells (`w3gridmd.F90:5010-5028`; `:5182-5184` is the `SMC` branch, not compiled here), so
  `ISEA` to `(IX,IY)` is one-to-one, and `(JSEA, ISPROC)` determines `ISEA`, so no two points write the same element (v, read
  from the source; not exercised).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested, postponed to the validation phase (ADR-0016). Not wired: no shim, no caller hunk.
Compiled with no warning under the `openmp-release` flags and at `-O0 -g
-fsanitize=address,undefined` (v, 2026-11-12); nothing was run.

## WW3 defects found

None.
