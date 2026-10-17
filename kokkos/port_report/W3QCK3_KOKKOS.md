# W3QCK3_KOKKOS

**Status:** ported, phase 1, wired inside the Kokkos `W3XYP3` (`WW_KOKKOS_XYP3=1`) (2026-10-17). Run byte-identical inside WW3 on the CPU in
the full case; not regtested: the regtest matrix has not run.

UQ advection in one spatial direction with obstruction (transparency): the two sweeps of `W3XYP3`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3QCK3`, `model/src/w3uqckmd.F90:925-1336` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_qck3.hpp`](../src/ww_kokkos/prop_qck3.hpp), a header-only device function `w3qck3`, serial, in the Fortran's structure and order |
| C shim | [`../src/fortran_iface/prop3_shim.cpp`](../src/fortran_iface/prop3_shim.cpp) (`ww_xyp3_batch`, `ww_ktp3_batch`), bound by `w3kokkos_prop3md.F90` on h0ffmann/WW3 `ww3-gpu/kokkos-prop3` (`fcdfabef`); [PATCH.md section 11](../src/fortran_iface/PATCH.md#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3) |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) No libm or libmvec call, no FMA, no reduction; the vectorised central loop is per element.
- (v) Every `MIN`/`MAX` in the object's `minss`/`maxss` operand order in all five code paths; the final `MAX(0.,.)` turns -0.0 and NaN into +0.0; `TRANS(IXY,-1:1)` with column 0 for a NaN flux (`comiss`).
- (v) The closure copies (three ghost columns of `Q`, one of `CFLL`) in source order and persisting into the caller's arrays. The `TRANS` index is computed in 64-bit, as the Fortran does.
- (v) A fresh reviewer found it bit-identical for every non-NaN input, signed zeros included; a NaN can change only a payload that the final `MAX` removes before `Q` is stored.

## Benchmarks

None of its own: it runs inside `W3XYP3`, whose report has the timings of 2026-10-17 ([W3XYP3_KOKKOS.md](W3XYP3_KOKKOS.md)).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.

Update 2026-10-17, the wiring (v): `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos
XYP3,KTP3,SRCE`, with `switch_lab_ompg`, with `WW3GPU_SWITCH=switch_lab_shrd`, and with
`WW_KOKKOS_PROP3_BATCH_MIB=64` (six and three launches per call instead of one): `out_grd.ww3` and
the NetCDF field byte-identical (`cmp`) to the Fortran of the same executable, `out_grd.ww3`
byte-identical to `--ww3` (the NetCDF differs from it only in the `WAVEWATCH_III_switches`
attribute), `nccmp-tol` PASS. The counters: `W3XYP3` 13 824 spectra in 24 calls, `W3KTP3` 441 792
point calls in 48 calls. The smoke case (one point) makes no call of either. Not exercised by
`bench_small` ⚠: currents (`FLCUR`), the wavenumber shift (`FLCK`, so `W3QCK2`), index closure
and boundary input. The CUDA build compiles (`--gpu --build-only`), never run. The regtest matrix
did not run.
