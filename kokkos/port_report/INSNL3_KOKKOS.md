# INSNL3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Initialisation of `NL3`, the Generalized Multiple DIA: for each relative depth and each quadruplet
realization it builds the spectral offsets and weights `QST1` to `QST6` that `W3SNL3` reads. A host
function, run once per grid on each mod_def read (`w3iogrmd.F90:1809`).

## What was ported

| | |
|---|---|
| WW3 routine | `INSNL3`, `model/src/w3snl3md.F90:788-1649` at NOAA-EMC/WW3 `03adda91`, with its internal `MINLAM` (`:1525-1578`) and `MAXLAM` (`:1593-1645`); `NL3` is in `switch_NCEP_glwu`, `switch_UoM_nl3`, `switch_UoM_nl3s` and the `ww3_ts1` regtest switch `switch_ST4_GMD` |
| Kokkos code | [`../src/ww_kokkos/snl3_tables.hpp`](../src/ww_kokkos/snl3_tables.hpp): `insnl3()` fills `Snl3Tables`, the struct `W3SNL3` will read, documented at the top of the file |
| Not ported | `W3_S`, `W3_T`, `W3_T1`, `W3_T2`, `W3_T3` (tracing and test output), not compiled in the reference switch; the `WRITE` before each `EXTCDE` (the shim prints it) |
| C shim | none yet; it passes `NK`, `NTH`, `XFR`, `DTH`, `SIG(1:NK)`, `SNLNQ`, `SNLL`, `SNLM`, `SNLT`, `SNLCD`, `SNLCS` of the current grid and whether `W3_ST4` is in the switch, keeps one `Snl3Tables` per grid (`MPARS(IGRID)%SNLPS`), and turns a non-zero return into `EXTCDE` with the same code (1000, 1001, 1002, 1010, 1011) |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch C, issue #42 |

## What WW3 does here at the pin

- (v) `INSNL3` uses `W3GDATMD` without `ONLY` (`w3snl3md.F90:872`), so its `NKD` is `W3GDATMD`'s
  `NKD = 1300` whenever `W3_ST4` is on (`w3gdatmd.F90:636-637`), which hides the module's own
  `NKD = 275` (`w3snl3md.F90:130`). `W3SNL3` imports with `ONLY` (`:332-336`) and clamps the depth index
  to 275 (`:425`). Under `ST4` (both `switch_NCEP_glwu` and `switch_ST4_GMD`) the tables have 1300
  depth levels spaced by `XSIT = 1.0040009` (folded in the object), and `W3SNL3` never reads past
  level 275, whose nondimensional frequency `SITMIN*XSIT**274` is about 0.0747: every point with
  σ·sqrt(d/g) above that uses the layout of a very shallow depth. Without `ST4` (`switch_UoM_nl3`, `ST6`)
  `NKD` is 275 everywhere. The port keeps both (`Snl3Input::st4`); this is a WW3 defect, not fixed.
- (v) Section 3's `SELECT CASE` flips the signs of all `JTH` in its default case (`:1389-1391`),
  section 2's does nothing there (`:1104`); translated as written.
- ⚠ `FRQ`, `XSI` and the six tables are `ALLOCATE`d on every call without a `DEALLOCATE`
  (`:1164`, `:1177`, `:1230`): a second mod_def read of the same grid leaks the first tables.
- ⚠ With `SNLNQ = 0`, `NQA` is 0 and `W3SNL3`'s final loop reads `IF2MIN`/`IF2MAX` uninitialised
  (`:625`); not reachable from `INSNL3`'s checks.

## Arithmetic

Reference: the `-O3` object of `w3snl3md.F90` from WW3-noaa's CMake Release (Ninja) build,
gfortran 16.2, `switch_NCEP_glwu` with `NCEP2` replaced by `NOGRB` and `SCRIP SCRIPNC` dropped
(neither reaches this file; the preprocessed source has no `W3_PDLIB`, `W3_S` or `W3_T*`), identical
to the CMake object, read by the translator, 2026-10-16. C++: g++ 15.2 `-O3 -march=x86-64-v3
-ffp-contract=off`, warning-clean.

- (v) Folded constants with the object's bits: `SITMIN` 0x1.998eaep-6, `XSIT` 0x1.010634p+0 (NKD 1300)
  and 0x1.04e46cp+0 (NKD 275, from the same folding of a two-line module), `SQRT(GRAV)`, `DERA`,
  `ATAN(1.)`; `S0`'s start `SITMIN*SQRT(GRAV)/XSIT` rounded op by op.
- (v) The trap of this routine: gfortran SLP-vectorises the four `LOG(AUXFR(J))` (`:1046`, `:1273`)
  into one `_ZGVbN4v_logf` and stores `JFR`, `JFR1` (the frequency offsets, `IFRMIN`, `IFRMAX` and
  section 3's `IFR`) from those lanes, while the `__powisf2` exponents in `WFR` come from scalar `logf`
  of the same values. libmvec and scalar `logf` can differ in the last bits, so `freq_indices()` computes
  both, as the object does. ⚠ On a host without glibc's libmvec it falls back to scalar `logf`.
- (v) libm calls matched by relocation: `logf` 3 and `_ZGVbN4v_logf` 2 (the object: 9 and 2, its
  scalar copies unrolled), `acosf` 8, `__powisf2` (5 sites, the object's 17 unrolled); `cosf` 3 against
  5 (the object calls `MAXLAM` twice, once for the `WRITE`); `WAVNU2` with `EPS = 1.E-6`, `NMAX = 25`
  through `disp_distab.hpp`, which is the out-of-line `__w3dispmd_MOD_wavnu2` the object calls.
- (v) Every `MIN`/`MAX` of `REAL` (`:1018`, `:1029-1037`, `:1327-1346`, `:1565-1570`, `:1633-1637`) with the
  value in `minss`/`maxss`'s destination and the constant as source, read from the object.
- (v) `SIGN(1.,DELTH(2))` and `SIGN(1.,DELTH(4))` at `:1067` are folded to 1 by the object (an `ACOS`
  is never negative); the port does the same, which differs from `copysignf` only for a NaN.
- (v) `INT()` through `fint`; integer negation and `ABS` wrap at `INT_MIN` as `negl`/`pabsd` do.
- (v) The C++ vectorises only elementwise code; no FMA, no reciprocal, no reduction.
- (v) CUDA build-only (`sm_89`, `--fmad=false`): the header compiles in a CUDA translation unit; the
  routine is host-only and nothing runs on a device.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
