# W3ULEV_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Water level at the new time, then the depth `DW = MAX(0, WLV-ZB)`, and at every sea point
shallower than `k·d = 4` at the lowest frequency: new wavenumbers and group velocities from
`WAVNU1`, the dry and newly wet points (`MAPSTA`, `MAPST2`, `UST`, a zeroed spectrum), and the
action density redistributed, linearly and conservatively, from the old wavenumber grid onto the
new one where the lowest wavenumber moved by at least `RDKMIN = 0.05` of its band width.

## What was ported

| | |
|---|---|
| WW3 routine | `W3ULEV`, `model/src/w3updtmd.F90:2066-2614` at the `WW3/` pin `8e433f99`, as `switches/switch_lab_shrd` compiles it; its one call, in `W3WAVE`, is `W3ULEV(VA, VA)` (`w3wavemd.F90:1437`; `:1434` at NOAA `03adda91`) |
| Kokkos code | [`../src/ww_kokkos/updt_w3ulev.hpp`](../src/ww_kokkos/updt_w3ulev.hpp), header only: a launcher, three range kernels and one serial functor |
| Callees | `WAVNU1` as `ww::prop::wavnu1` ([`WAVNU1_KOKKOS.md`](WAVNU1_KOKKOS.md)), `GET_JSEA_IBELONG` as `ww::parall::get_jsea_ibelong` ([`GET_JSEA_IBELONG_KOKKOS.md`](GET_JSEA_IBELONG_KOKKOS.md)); `WRITE (NDSE,1000)` and `EXTCDE(1)` of `:2239-2242` through the hook `UlevHooks::nk_error` |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` from the `WW3/` pin, built by gfortran 16.2
inside the pinned `ww3` shell with the defines and flags of WW3's CMake Release build under the
lab switch, and read as GIMPLE (`-fdump-tree-optimized-lineno`) and assembly, 2026-10-16. Its
`.text` is byte-identical to the `w3updtmd.F90.o` of a CMake build of the fork under the same
switch (v); which fork commit that build was of was not recorded ⚠, though the two `.text`
sections being byte-identical means the compiled code is the same.

- (v) `MAX` operand order, from the object's `maxss`: `:2336` `MAX(0.,WLVeff-ZB)` is
  `maxss 0,d`; `:2359` `MAX(DMIN,-ZB)` and `:2370` `MAX(DMIN,DW)` are `maxss DMIN,x`. A NaN depth
  gives `0` or `DMIN`. The C++ `fmax2(a,b) = b>a?b:a` compiles to `vmaxss` with the same operand
  order at all three.
- (v) Constants: `KDMAX = 4.` and `RDKMIN = 0.05` (`:2207`) are folded (`4.0`,
  `5.00000007450580596923828125e-2`), as are `UST = 0.05` (`:2422`) and `0.95` (`:2543`,
  `9.49999988079071044921875e-1`); the C++ GIMPLE has the same literals.
- (v) `ABS` of a `REAL` (`:2438`) is `andps` with the sign mask, `-ZB` (`:2359`) is `xorps`;
  `MAPSTA = ±ABS(MAPSTA)` (`:2398`, `:2424`) is an integer `ABS_EXPR`. `MAPDRY =
  MOD(MAPST2/2,2)` (`:2256`) is integer `/` and `%`, which truncate the same way in C++.
- (v) `KDCHCK = WNbat*DEPTHbat` (`:2365`); `OWN`, `DWN` (`:2385-2386`) are two `divss`; `RDK`
  (`:2438`) is `subss`, `andps`, `divss` by `DWN(1)`; `RD1` (`:2512`, `:2515`) one `divss` of the
  parenthesised differences; `A + RD1*TA`, `A + RD2*TA` (`:2522`, `:2528`) a `mulss` then an
  `addss`; `VA/DWN(MAPWN)` (`:2538`) a `divss`; the tail (`:2546`) `FACHFA*A(IK-1)`. No FMA in
  either object, no libm call in `W3ULEV`.
- (v) gfortran vectorises the loops at `:2256`, `:2384`, `:2462`, `:2521`, `:2527`, `:2537` and
  `:2545` (`-fopt-info-vec-optimized`). Each applies one elementwise integer operation, a
  division, a product, or a product and a sum per element, with no reduction, so packed and
  scalar give the same bits; the C++ loops in Fortran order.
- (v) `A` and `VA` are one array in the call (`w3wavemd.F90:1437`). The object keeps the
  Fortran's statement order across them: `TA` is read from `A` (`:2466`), `VA(:,JSEA)` cleared by
  `memset` (`:2470`), then `A` reloaded inside each `:2522`, `:2528` loop, the `VA` division
  (`:2538`) done before the tail reads `A` (`:2546`). The port takes `VA` once and reads
  `A(ITH,IK,JSEA)` as `VA(ITH+(IK-1)*NTH, JSEA)`.
- (v) `RD2` is carried from one sea point to the next: the GIMPLE has `rd2_928 = PHI <rd2_567
  (back edge), rd2_714(D) (undefined)>` at the head of the `:2347` loop. `:2543` reads it with
  `I2 <= NK` at a point whose `:2502` loop never assigned it only when `COND` is false (`:2487`):
  otherwise `IK0 <= NK`, and a point where every `IK` cycles at `:2509` leaves `I2 = NK+1`. It
  then takes the `RD2` of the last point that did. The port runs `:2347-2570` as one serial loop
  in `ISEA` order for that reason; `:2256-2257`, `:2282-2338` and `:2574` touch only their own
  element and run as parallel range kernels.
- (v) The carried `RD2` cannot change a bit. At a point with `COND` false, `A(:,:,JSEA)` is all
  `+0` after the `memset` of `:2470` and the division of `:2538`, so the tail of `:2546` writes
  `FACHFA*(+0) = +0` (`FACHFA = XFR**(-FACHF-2) > 0`, `w3gridmd.F90:3564`) or is skipped, with
  the same bits either way, as long as `DWN` is finite and non-zero. The Fortran's `RD2` before
  the first point that assigns it is an undefined stack slot; the port's `0` is therefore as good
  as any value, and a per-point parallel loop with a private `RD2` would give the same bits too
  (the serial loop is kept as the conservative translation). ⚠ Whether `COND` is ever false on a
  real grid was not measured: it needs `WN(1)` at the new depth above every old `WN(IK)`,
  `IK <= NK+1`, a depth change of orders of magnitude in one level update.
- With `LPDLIB` true in a build without `W3_PDLIB`, `GET_JSEA_IBELONG` leaves `JSEA` and
  `IBELONG` unassigned (`w3parall.F90:1353-1380`) and the Fortran would use undefined values;
  the port uses `0`. (v) That does not happen in a model run under the lab switch: `W3INIT`
  sets `LPDLIB = .FALSE.` and only `#ifdef W3_PDLIB` sets it true (`w3initmd.F90:652-655`);
  `ww3_shel.F90:440-442` and `w3triamd.F90:235-237` are guarded the same way. `w3gridmd.F90:3736`
  sets it true without a guard, in the grid preprocessor, before `W3INIT` runs.
- Not compiled under the lab switch and not ported: `W3_PDLIB` (`WAVNU3` at `:2361`, `:2378`;
  `SET_IOBDP_PDLIB` at `:2580`), `W3_REF1` (`W3SETREF` at `:2584`), `W3_TIDE` (the blocks between
  `:2261` and `:2335`), `W3_SETUP` (`:2327-2332`), `W3_S`, `W3_T`, `W3_T2`, `W3_T3`. The
  `GTYPE.EQ.UNGTYPE` block (`:2578-2586`) is empty under the lab switch and the object does not
  load `GTYPE` there. `DWO` (`:2285`) is written and never read, and is not translated.
  `TLEV = TLN` (`:2249`) is a host copy of two integers.
- On CUDA the serial loop runs on one thread. (v) The header compiles for CUDA (see the fresh
  review below) and its PTX has `sqrt.rn.f32` and `div.rn.f32`, no `fma.rn.f32`; the `FFMA` in
  the SASS are the correctly rounded `div.rn`/`sqrt.rn` sequences. ⚠ One `MAX` became PTX
  `max.f32`, whose result for `+0` against `-0` was not checked against `maxss`; ⚠ that CUDA's
  `sqrtf` matches glibc's bits was not run.
- (v) Fresh review 2026-10-16: an independent `-O3` build of `w3updtmd.F90` at the pin under
  `switch_lab_shrd` (gfortran 16.2, modules built from the pin, GIMPLE and `objdump -dr` in
  `/tmp/claude-1000/w3ulev-review/`) re-checked every compiled statement, the `maxss` operand
  order at `:2336`, `:2359`, `:2370`, the folded constants, the vectorised loops, the A/VA
  statement order (`TA` loads before the `memset`, the tail's loads after the `:2538` stores),
  the `RD2` PHI, that `DWO` is stored and never loaded, that `W3ULEV` has one call
  (`w3wavemd.F90:1437`, `:1434` at NOAA `03adda91`), and every cited line. One defect, in the
  text only: the claim that `IK0 = NK+1` also leaves `RD2` unassigned is wrong (`IK0 <= NK`
  whenever `COND` is true); fixed here and in the header, with the argument that the carried
  `RD2` cannot change a bit. No defect in the code. Compiled with `g++ -O3 -march=x86-64-v3
  -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp`, instantiated on OpenMP and
  Serial: no warning, no FMA; with `nvcc_wrapper -arch=sm_89 --fmad=false` in the `#cuda`
  shell: compiles, the only warnings are `-Wpedantic` on nvcc's own line directives. Nothing
  was run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the Fortran object was built and
disassembled, and a throwaway translation unit in `/tmp/claude-1000/w3ulev-scratch/` that
explicitly instantiates `ww::updt::w3ulev<Kokkos::DefaultExecutionSpace>` (OpenMP) was compiled,
inside `nix develop "path:picasso/labs/pratico#ww3"`, with

```
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c tu.cpp -o tu.o
```

with no warning (also none with `-Wshadow -Wconversion` added), and no FMA instruction in
`objdump -d tu.o`. Nothing was linked or run.
