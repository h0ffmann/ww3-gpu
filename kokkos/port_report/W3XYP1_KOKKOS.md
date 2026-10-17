# W3XYP1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Spatial propagation of one spectral component on a regular grid with the first-order scheme (`PR1`): the sub-steps, the upwind fluxes between cells, the longitude and tripole closures and the boundary input.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYP1`, `model/src/w3pro1md.F90:301-873` at NOAA-EMC/WW3 `03adda91` (identical at the fork pin `357f575c`), as `switches/switch_lab_shrd` with `PR3 UQ` replaced by `PR1` compiles it |
| Kokkos code | [`../src/ww_kokkos/prop_xyp1.hpp`](../src/ww_kokkos/prop_xyp1.hpp), a header-only device function `w3xyp1`, serial, in the Fortran's structure and order; [`../src/ww_kokkos/prop_params1.hpp`](../src/ww_kokkos/prop_params1.hpp) adds `FACVX`/`FACVY` to the `Params`, `Grid` and `Step` of `prop_params.hpp` |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, its disassembly identical to the CMake build's object), read as GIMPLE and as
assembly by the translator, 2026-10-16.

- (v) No libm call: the two `SQRT`s of `:500-502` are `sqrtss`; no FMA; the only vectorised loops are the zero-fills, the closure copies and the `GSQRT` multiply and divide (`:622`, `:802`), elementwise. The `MAXVAL`s as gfortran inlines them (NaN-skip scan, in-order reduction, `-HUGE` when empty); `CGN` as the two `maxss` the object emits (`MAX(CC, CGA)`, then the `0.001*CG0` term), times `0.9999`; `0.575*GRAV` and `DERA*RADIUS` folded.
- (v) Every `MAX(VCB,0.)`/`MIN(VCB,0.)` of the flux loops and the `MIN(1.,MAX(0.,RD1/RD2))` of the boundary in the operand order of the emitted `maxss`/`minss`: `-0.0` and NaN give `+0.0`.
- (v) The divide-then-multiply order of `:566`, `:568` and `:792`, the left-to-right sum of `:772-776`, the boundary value stored in physical space without the `/CG*CLATS` of `W3XYP3`, and the tripole branches (`:642-648`, `:709-727`), which `W3XYP1` handles and `W3XYP3` refuses.
- For an active point on row 1 the Fortran reads `VFLY2D(IY-1,IX)` out of its first dimension (`:761`). What is known (v), from the object: for `IX > 1` it reads the neighbouring column's `VFLY2D(NY,IX-1)`, in bounds, which is the `0.` of `:545` except on a tripole grid, where `:709-727` wrote it; for `IX = 1` it reads the 4 bytes just before the `malloc` block of `VFLY2D` (GIMPLE bb 152, element offset `-1`; the automatic arrays are `malloc`ed, bb 2). The port keeps both reads in the same layout; the second lands on a pad set to `+0.0`. What is not known ⚠: the value of those 4 bytes in a WW3 run. On glibc's `malloc` on x86-64 they should be the upper half of the chunk's size field, 0 for a block under 4 GiB, but that comes from glibc's documented chunk layout, not from a run or a read of glibc's source at a version; another allocator linked into the executable can hold anything there, and a positive value would set `JYN = -1` and add `ATRNY(IXY,-1)` times it to the sum. The `+0.0` is a model of the allocator, not a reproduction of the Fortran. When `ww3_grid` reads the map in parts (`FROM = 'PART'`, `w3gridmd.F90:4565`) it turns active points on rows 1 and `NY` into excluded ones (`NSTAT = -1`, `:4584`, `:4773-4776`), so such a grid never reaches the read (v, read in the source). A map read from file (`:4793` on, the only way for a tripole grid, `:4570-4575`), a run-time change of `MAPSTA`, and whether any regtest grid has an active point at `(1,1)` were not checked ⚠.
- ⚠ When both terms of a sum are NaN, the payload x86 returns depends on the operand order of `addss`, which C++ source does not fix; values are otherwise the same bits. The Fortran object adds commuted at `:696` (`VCY2D(IY+1,IX)` first), at the flux sums of `:664-665`, `:697-698` and `:719-720` (the `MIN` term first) and at `:772` (the first product, then `FLD2D`).
- ⚠ `FACVX`/`FACVY` come from `W3MAP1`, which is not translated: the shim must copy them from the Fortran.
- ⚠ The `-O3` GIMPLE of the C++ was not compared operation by operation (ADR-0011). The review's `-O3` object of a scratch TU (no `-march`, `-ffp-contract=off`) has no FMA, no libm call but `sqrtf`'s errno fallback beside each `sqrtss` (glibc's `sqrtf` should return the same bits for a negative input, not checked), and the `maxss` operand order of the two `MAXVAL` scans and of `CGN` that the Fortran object has.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch translation unit including
the header and calling `w3xyp1` from an external function compiled with `prop.cpp`'s
`serial-debug` command (`g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
-ffp-contract=off -fsanitize=address,undefined -fopenmp`), with no warning, and the function's
code was generated (`nm` shows `w3xyp1`). The `openmp-release` and `cuda-release` presets were not
built with it ⚠.

## Review

2026-10-16, the fresh reviewer (single pass, static only, nothing run). Read `W3XYP1` at `w3pro1md.F90:301-873` (identical at `03adda91` and at the fork pin `357f575c`, checked with `diff`) against the header line by line, and against the `-O3` GIMPLE and the disassembly of the translator's object, whose `.text` equals the CMake build's `w3pro1md.F90.o` (checked with `cmp`; `W3_PR1` defined, no `W3_MGP`, `W3_PDLIB`, `W3_OMPH`, `W3_T*`). Checked: `ITH`/`IK`; `CG0`, `CGL` and the folded constants; both `MAXVAL`s (empty section `-HUGE`, the `x >= -Inf` scan, the in-order `maxss` with the new element as destination); the operand order of both `maxss` of `CGN` with and without `FLCUR`; `NTLOC` (`cvttss2si`), `DTLOC`, `DTRAD`; the bounds and base of every work array and the zero-fills; the operand grouping of `:566`, `:568-569`, `:590-591`, `:600-601`, `:611-612`, `:622`, `:663`, `:696`, `:718`, `:772-776`, `:792`, `:802`, `:807-812`, `:824`; every `minss`/`maxss` (the variable as destination, `0.` or `1.` the source); `NYMAX` and both closures, in the Fortran's order; the `ATRNX`/`ATRNY` and `BBPI0`/`BBPIN` index formulas; the `FIELD` offset; and the citations of the header and of this report. No defect found in the arithmetic or the control flow. Fixed: the `:761` paragraph of the header and of this report said `ww3_grid` "builds the map from the depths"; the exclusion of rows 1 and `NY` happens only when the map is read in parts (`FROM = 'PART'`, `w3gridmd.F90:4565`), which a tripole grid cannot use (`:4570-4575`). That paragraph now separates what the object shows (v) from the value of the bytes before `VFLY2D` (⚠, an allocator assumption), the NaN-payload `⚠` names the commuted additions, and the review's `-O3` C++ object check is recorded above. The header compiles with no warning with `g++ -DKOKKOS_DEPENDENCE -DWW_DETERMINISTIC=1 -fsanitize=address,undefined -fno-omit-frame-pointer -g -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -ffp-contract=off -fopenmp -c` in the `picasso/labs/pratico#ww3` shell (g++ 16.2.0), and `nm` shows `w3xyp1` generated.
