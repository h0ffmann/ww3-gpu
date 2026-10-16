# W3SNL3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The Generalized Multiple DIA (switch `NL3`): the nonlinear quadruplet source term `S` and its
diagonal `D` at one sea point, from the action spectrum `A`, `CG`, `WN` and the depth, with several
quadruplet layouts and a depth-dependent lookup table (the tables of `INSNL3`). `W3SRCE` calls it
per sea point inside its sub-step loop (`w3srcemd.F90:1269`); `ww3_outp.F90:2263`,
`ww3_ounp.F90:2435` and `gx_outp.F90:1188` call it too. No profile with `NL3` exists in this
repository, so its share of the run time is not known ⚠.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SNL3`, `model/src/w3snl3md.F90:181-777` at NOAA-EMC/WW3 `03adda91`, with its internal `EXPAND` (`:644-703`) and `EXPND2` (`:717-773`). `NL3` is in `switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the `ww3_ts1` regtest switch `switch_ST4_GMD`; none has `PDLIB`, so the `WAVNU1` branch (`:400`) is the one translated and the `WAVNU3` branch (`:398`) is not |
| Kokkos code | [`../src/ww_kokkos/snl3_gmd.hpp`](../src/ww_kokkos/snl3_gmd.hpp): `w3snl3_point()`, a `KOKKOS_INLINE_FUNCTION` that reads `Snl3Tables` ([`snl3_tables.hpp`](../src/ww_kokkos/snl3_tables.hpp), `INSNL3`), calls `WAVNU1` of [`prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp) and the libmvec grouping of [`st4_libmvec.hpp`](../src/ww_kokkos/st4_libmvec.hpp) |
| Not ported | `W3_S` (tracing), not compiled in the reference switch |
| C shim | none yet; see the contract below |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch C, issue #42 |

The shim passes the grid's `Snl3Tables` (one per grid, `MPARS(IGRID)%SNLPS`), the `W3GDATMD`
scalars `FACHFE`, `FACTI1`, `FACTI2`, `SNLMSC`, `SNLNSC`, `SNLSFD`, `SNLSFS` of the same grid
(`Snl3Params`), `WAVNU1`'s `DISTAB` table as `W3IOGR` left it, and per point the work arrays whose
sizes `Snl3Scratch` documents (`XWN`, `XCG` of `NFRMAX`; `SCALE1`, `SCALE2`, `JKD` of `NFRCUT`; `UE` of
`NSPMAX-NSPMIN+1`; `DSB`, `DD1` to `DD4` of `NSPMX2-NSPMIN+1`; `EXPND2`'s `TEMP` of `NTH*NFRCUT`).
`A`, `S`, `D` are `(NTH,NK)` in Fortran order; `S` and `D` are overwritten.

## What WW3 does here at the pin

- (v) The depth index is clamped to the module's `NKD = 275` (`w3snl3md.F90:130`, `:425`) while
  `INSNL3` builds 1300 depth levels when `W3_ST4` is on (`INSNL3_KOKKOS.md`): with `ST4`, every point
  with σ·sqrt(d/g) above about 0.0747 uses level 275. Read in the object (`MIN_EXPR <ikd, 275>`);
  translated as written. A WW3 defect.
- (v) Section 5 converts back to action only over `IF2MIN:IF2MAX` of the last quadruplet realization
  (`:625-627`): when that realization raised `IF2MIN` (`:455`), the lowest frequencies of `S` stay in
  the pseudo-spectrum form while the other realizations did add to them. A WW3 defect, translated.
- (v) With `NQA = 0` `IF2MIN` and `IF2MAX` would be read uninitialised at `:625` (the object's PHI
  of undefined values), and the Fortran's behaviour is undefined there. The port takes an empty loop.
  Not reachable: `W3GRID` raises `NQDEF = 0` to 1 (`w3gridmd.F90:2035-2037`) and `INSNL3` adds 2 or 4
  realizations per quadruplet (`w3snl3md.F90:1083-1107`), so `NQA >= 2`.
- (v) `EXPND2(DSB(1:NTH*NFRCUT), DSB)` (`:548-552`) passes overlapping actual arguments to two
  dummies, which Fortran does not allow; it works because `TEMP = ARIN` (`:758`) is copied first. The
  object keeps `EXPND2` out of line and does the `memcpy` into `TEMP` before any store to `AROUT`; the
  port copies first too.
- (v) `DSB`, `DD1` to `DD4` are not initialised (`:385-389` are commented out); every element section 4
  reads is written by section 3, the zeroing of `:456-460`, `:472-476` or `EXPND2` of the same realization.

## Arithmetic

Reference: the `-O3` object of `w3snl3md.F90` from WW3-noaa's CMake Release (Ninja) build, gfortran
16.2, `switch_NCEP_glwu` with `NCEP2` replaced by `NOGRB` and `SCRIP SCRIPNC` dropped (the preprocessed
source has no `W3_PDLIB`), identical to the CMake object (re-checked 2026-10-16). C++: g++ 15.2
`-O3 -march=x86-64-v3 -ffp-contract=off -fopenmp`, warning-clean with `-Wall -Wextra -Wpedantic
-Wshadow -Wconversion`. Compared statement by statement in the two `-fdump-tree-optimized` dumps by the
translator, 2026-10-16. A fresh review rebuilt both objects in the `ww3` shell (each identical to its
CMake object) and found the machine code of `W3SNL3` and `EXPND2` identical under `switch_NCEP_glwu`
and `switch_UoM_nl3` (v, 2026-10-16).

- (v) Folded constants with the object's bits: `TPI**11` = 0x1.1f4f06p+29, `GRAV**2/TPI**11` =
  0x1.56b76ap-23 (`:410-411`), `TPI`, `TPIINV`; `GRAV**(4.-SNLMSC)` is a run-time `powf(9.806, ·)`.
  `LOG(XSIT)` and `LOG(SITMIN)` are run-time `logf` of the module values (`Snl3Tables::xsit`, `sitmin`).
- (v) The trap of this routine: the `SCALE1`/`SCALE2` loop (`:413-418`) is vectorised with three
  `_ZGVbN4vv_powf` per group of four from `IFR = 1` (`XWN**(4+SNLMSC)`, `XSI**(13-2*SNLMSC)`,
  `(DEPTH*XWN)**SNLNSC`) when `NFRCUT >= 4`, and scalar `powf` for the last `NFRCUT mod 4`; the port
  calls `mvec_powf` three times with the same grouping. `XWN**11` is the object's power tree
  (x², x³, x⁶·x⁵). Operation order `((AUX1*p1)*p2)/(XCG*XCG)` and `((x¹¹*AUX2)*p3)/XCG` in both.
- (v) `NINT` (`:424`, `:450`, `:466`) is `lroundf` in the object (3 relocations), truncated to 32 bits;
  the port uses `ww::st4::nint` (`roundf` and a 64-bit conversion, 0 for NaN as `LONG_MIN` truncates).
  `SQRT(DEPTH/GRAV)` is a `divss` then `sqrtss`, hoisted out of the loop in both.
- (v) Section 3: the four `FQ` sums left to right then times `FACT`, `FACT(1:4)*XCG/(XWN*XSI)`, `FPROP`,
  `DSB`, `DD1` to `DD4` with `FPROP*FACT(k)` first, as in the object (which runs it two `ITH` at a
  time, elementwise). Section 4: `S + t1 + ... + t16` and the same for `D`, scalar left to right in
  both. Section 5: `((S/XSI)*XCG)*TPIINV`. `EXPAND`: `(A*TPI*XSI)/XCG`, then the `FACHFE` tail and the
  directional wrap in source order (it matters when `NTHMAX > NTH`).
- (v) Integer `MIN`/`MAX` only (`:425`, `:454-455`, `:470`); no `REAL` `MIN`/`MAX` in this routine.
- (v) libm calls: the object's `powf` 13 (1 for `GRAV**(4.-SNLMSC)`, then 3 per element in the three peeled
  tail iterations and in the scalar loop taken when `NFRCUT < 4`), `logf` 5
  sites, `lroundf` 3, `_ZGVbN4vv_powf` 3; the C++ `powf` 4, `_ZGVbN4vv_powf` 3, `roundf` 3, `logf` 7
  (duplicated by loop versioning). The C++ vectorises only copies and elementwise loops; no FMA
  (`vfmadd` 0), no reciprocal.
- ⚠ CUDA build-only (`sm_89`, `--fmad=false`, `nix develop picasso/labs/pratico#cuda`): compiles,
  never run. The SASS has 377 `FFMA`, expected inside CUDA's correctly rounded division (42
  `MUFU.RCP`) and its `powf`/`logf`, not checked one by one; a device has no libmvec, so the `SCALE`
  loop cannot be bit for bit there, and CUDA's `logf`, `powf`, `sqrtf` are not shown to return glibc's
  bits.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
