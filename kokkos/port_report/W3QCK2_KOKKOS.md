# W3QCK2_KOKKOS

**Status:** ported, phase 1, wired inside the Kokkos `W3KTP3` (`WW_KOKKOS_KTP3=1`) (2026-10-17). Never run: no case run so far has a
current-induced wavenumber shift (`FLCK`); not regtested.

UQ advection on a variable grid: the wavenumber pass of `W3KTP3`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3QCK2`, `model/src/w3uqckmd.F90:517-891` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_qck2.hpp`](../src/ww_kokkos/prop_qck2.hpp), a header-only device function `w3qck2`, serial, in the Fortran's structure and order |
| C shim | [`../src/fortran_iface/prop3_shim.cpp`](../src/fortran_iface/prop3_shim.cpp) (`ww_xyp3_batch`, `ww_ktp3_batch`), bound by `w3kokkos_prop3md.F90` on h0ffmann/WW3 `ww3-gpu/kokkos-prop3` (`fcdfabef`); [PATCH.md section 11](../src/fortran_iface/PATCH.md#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3) |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) As `W3QCK1`, with the grid-spacing terms: `CFL = (DT*VEL)/DX2`, the curvature factor `((DX2*DX2)/DX1)*(1-CFL*CFL)` then `/6`, and the propagation `Q + (DT/DX1)*(FLA(-INC)-FLA)`, quotient first.
- (v) The boundary loops run in the lab build (`NB0 = NSPEC-NTH`, `NB1 = NSPEC`, `NB2 = NSPEC+NTH` from `W3KTP3`); the lab caller passes `CFLK` with `MY*MX` reals and `CLOSE` false, so column `MX+1` is never touched.
- (v) A fresh reviewer found it bit-identical to the object for every input, statically.

## Benchmarks

None of its own: it runs inside `W3KTP3`, whose report has the timings of 2026-10-17 ([W3KTP3_KOKKOS.md](W3KTP3_KOKKOS.md)).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.

Update 2026-10-17, the wiring (v): `W3KTP3` calls it only when `FLCK` is true. The full case
(`bench_small`) has `FLCK` false and the smoke case is one point, so `W3KTP3`'s runs there
(byte-identical, see its report) never reached this routine. It compiles for CUDA inside the shim
(`--gpu --build-only`).

Update 2026-10-17, the coast case (v): `bash scripts/ww3-gpu.sh --ww3-gpu coast --cpu --kokkos
resident` (and `KTP3,SRCE` under `WW_KOKKOS_VA_RESIDENT=1`) has `FLCK` and `FLCUR` on, so the Kokkos
`W3KTP3` calls this routine for the wavenumber shift inside its 517 660 point calls
(`prop_ktp3.hpp:324-344`, `WW_KOKKOS_PROP3_COUNT=1`). `out_grd.ww3`, `out_pnt.ww3` and the NetCDF field
are `cmp`-equal to the Fortran of the same executable and `out_grd.ww3` and `out_pnt.ww3` to
`--ww3`, on `switch_lab_ompg` and `switch_lab_shrd`, with and without `WW_KOKKOS_VA_RESIDENT=1`;
`nccmp-tol` PASS. With `RUN%FLCK = F` the same case gives an `hs` up to 0.073 m different, so the
shift reaches the compared output. The rows and the case are in [W3KTP3_KOKKOS.md](W3KTP3_KOKKOS.md),
"The coast case". There is no counter of this routine's own calls ⚠; the count above is of
`W3KTP3`'s. Never run on the GPU ⚠; the regtest matrix did not run ⚠.
