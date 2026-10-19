# W3XYPUG_KOKKOS

**Status:** ported and wired, 2026-10-18 (issue #196): a batched device kernel with the `FSN`
scheme `W3XYPFSN2`, on the resident `VA`, called under `WW_KOKKOS_XYP3=1` on an unstructured grid
without `PDLIB` (PATCH.md section 30). Regtested on `ww3_tp2.6` `ST4` and `ST4_ASCII`, 2 of 2 entries byte-identical (below). The serial host translation of 2026-10-16
(`prop_xypug.hpp`, below "Arithmetic" and "Review") is kept and is not called.

Spatial propagation of one spectral component on an unstructured grid without `PDLIB`: it turns
`VQ` into the action `AC`, builds the advection velocities `C` (group velocity, plus the current
on nodes with `IOBP = 1`), calls the selected fluctuation-splitting scheme and stores the result
back into `VQ`. The `PDLIB` sibling is [PDLIB_W3XYPUG](PDLIB_W3XYPUG_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYPUG`, `model/src/w3profsmd.F90:69-272` at NOAA-EMC/WW3 `03adda91` (byte-identical in `WW3/` at the fork pin, `cmp`), called from `w3wavemd.F90:1970`, `:1973`, `:1976` (one per `PR1`/`PR2`/`PR3`), as the switch of `ww3_tp2.6` compiles it (`SHRD PR3 UQ ... REF0`) |
| Kokkos code | [`../src/ww_kokkos/prop_xypug.hpp`](../src/ww_kokkos/prop_xypug.hpp), a header-only host function `w3xypug`, serial, in the Fortran's structure and order; the header (the host path, not called by WW3) calls the ported `SETDEPTH` ([SETDEPTH](SETDEPTH_KOKKOS.md)) directly; the batched shim does not: it takes `IOBDP` from the Fortran `SETDEPTH`, called once per step (`xypug_c.hpp:11-13`) |
| Hooks | `XypugHooks` (the host path): `dsec21` and the four schemes `W3XYPFSN2`, `W3XYPFSPSI2`, `W3XYPFSFCT2`, `W3XYPFSNIMP` (`w3profsmd.F90:470`, `:730`, `:1290`, `:988`). A hook the flags reach and that is missing aborts. The batched kernel translates `W3XYPFSN2` only ([W3XYPFSN2](W3XYPFSN2_KOKKOS.md)); the other three stay on the Fortran and the Kokkos part refuses them at start-up |
| C shim | [`../src/fortran_iface/xypug_shim.cpp`](../src/fortran_iface/xypug_shim.cpp), entry `ww_xypug_batch_va` with its argument table in [`xypug_c.hpp`](../src/fortran_iface/xypug_c.hpp), the batched kernel [`../src/ww_kokkos/ug_xypug.hpp`](../src/ww_kokkos/ug_xypug.hpp) on the resident `VA`; called from the fork's `W3KOKKOS_XYPUG` (PATCH.md section 30) |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
switch of `ww3_tp2.6`, objdump-identical to the CMake build's), read as GIMPLE and as assembly by
the translator, 2026-10-16.

- (v) `VQ = (VQ/CG)*CLATS`, `VLCFLX = (CG*CCOS)/CLATS`, `VLCFLY = CG*CSIN`, the current terms
  `(FACX*CX)/CLATS` and `FACY*CY` added (`:207-229`): `divss`, `mulss`, `addss` in that order, no
  FMA, no libm call. The products commute, the bits do not change.
- (v) `IOBP` is `INTEGER*2` and compared with 1 (`cmpw $0x1`); `IOBDP` is `INTEGER*1`.
  `AC = DBLE(VQ)*DBLE(IOBDP)` (`cvtss2sd`, `cvtsi2sd`, `mulsd`) and `C = VLCFL*REAL(IOBDP)`
  (`:234-236`); the vectorised loops are element-wise, so the lanes cannot change a bit.
- (v) `DSEC21(TBPI0,TIME)` is called before `DSEC21(TBPI0,TBPIN)` (`:242-243`); without `FLBPI`,
  `RD1 = 1`, `RD2 = 0`. The first true flag of `FSN`, `FSPSI`, `FSFCT`, `FSNIMP` selects the scheme
  (`:251-259`); with none set no scheme runs and `VQ` is still written back.
- (v) `VQ(IX) = REAL(AC(IX))` for every node, sea or not (`cvtsd2ss`); `ISEA = MAPFS(1,IX)` is a
  dead store (only `DEBUG` in the GIMPLE) and is left out. The store
  `VQ = MAX(0., CG/CLATS*VQ)` (`:270`) is `divss`, `mulss`, `maxss %xmm1(+0),%xmm0`, so NaN and
  −0 store +0; the C++ object at `-O3` emits the same `maxss` operand order.
- (v) `MAPSF` has leading extent `MSEA` (`w3gdatmd.F90:1736`), which the state carries apart from
  `NSEA`. `VGX`, `VGY` are read only under `W3_MGP` (`:212-215`), not ported; `W3WAVE` passes
  `FACX` as `FACY`, both are kept.
- ⚠ The shim must pass `C(NX,2)` and `AC(NX)` contiguous to the Fortran schemes, whose dummies
  are assumed shape (`:547-548`); not checked against a shim, which does not exist yet.
- (v) A fresh reviewer read the translation against the Fortran, the GIMPLE and the object,
  2026-10-16 (Review below). Static only: no WW3 regtest has compared the two ⚠.

## Review

2026-10-16, the fresh reviewer, static only (nothing was run). Read `prop_xypug.hpp` line by line
against `w3profsmd.F90:69-272` (identical in `WW3/` and `WW3-noaa/`, `cmp`), the translator's
`-O3` GIMPLE and the disassembly of `__w3profsmd_MOD_w3xypug` in the CMake build's
`w3profsmd.F90.o` (gfortran 16.2.0, `-O3 ... -fno-second-underscore`, no fast-math; the
translator's object disassembles identically). (v) Checked: the `VQ(1-NY:)` offset, the `MAPSF`,
`CLATS`, `CG`, `CX`, `CY`, `DW` bases and every declaration line cited; `ITH`/`IK` (`idivl`);
the operand order of every `divss`/`mulss`/`addss` in section 2 and the current loop, with
`FACX`/`FACY` as `CCURX`/`CCURY`; the `INTEGER*2` test `cmpw $0x1`; `AC = DBLE(VQ)*DBLE(IOBDP)`
and `C = VLCFL*REAL(IOBDP)` (integer to floating conversions exact, products element-wise);
`DSEC21(TBPI0,TIME)` before `DSEC21(TBPI0,TBPIN)`; the scheme selection and the contiguous
`C(NX,2)`, `AC(NX)` descriptors the Fortran builds for the schemes; `REAL(AC)` for every node
(`cvtsd2ss`, `cvtpd2ps`, both round to nearest); the final `maxss %xmm1,%xmm0` with `%xmm1` from
`pxor`, which is `x > 0 ? x : +0`, the C++ expression. In the header, the ported `SETDEPTH` is called with
`IOBDP(1:MX)` and `DW(0:)`. The header compiles with the `serial-debug` flags (ASan, UBSan,
`-Wconversion`, `-ffp-contract=off`) without a warning; at `-O3` its object has no FMA and the same
`maxss` operand order. One defect, a citation: the `DMIN` comment cited `w3profsmd.F90:1667-1674`,
now `:1650` (the `USE`) and `:1674` (the comparison). No arithmetic defect.

## The batched kernel with W3XYPFSN2 (issue #196), 2026-10-18

The owner asked for this port out of the wall-time order (ADR-0002) so that `ww3_tp2.6`, an
unstructured grid (`LIMON`, `EXPFSN = T`, `regtests/ww3_tp2.6/input/namelists_LIMON.nml:6`), gets a
parity verdict under the resident preset instead of the start-up stop
`WW_KOKKOS_XYP3=1, BUT ITS KOKKOS TRANSLATION DOES NOT COVER THIS SWITCH FILE`.

| | |
|---|---|
| WW3 code replaced | `W3WAVE` section 3.6.3, the `ISPEC` loop of `W3GATH`, `W3XYPUG`, `W3SCAT` (`w3wavemd.F90:2050-2135` at the fork pin), on `GTYPE = UNGTYPE` |
| Routines translated | `W3XYPUG` (`w3profsmd.F90:69-272`), `W3XYPFSN2` (`:470-727`, see [W3XYPFSN2](W3XYPFSN2_KOKKOS.md)), `W3GATH` and `W3SCAT` under `W3_SHRD` |
| Not translated | `SETDEPTH` (`:1601-1678`): the Fortran wrapper calls it once per step instead of once per component. It writes only `IOBDP` (`:1672-1676`) from `DW` and `DMIN` (`:1650-1651`), and the replaced loop writes neither, so `IOBDP` has the same bits (v, read 2026-10-18). `W3CFLUG` (`:274-467`): called only for the CFL output field (`w3wavemd.F90:1776` is the guard `IF ( FLOGRD(9,3).AND. UGDTUPDATE )`, the call is at `:1800`), and it does not read `VA` |
| Kokkos code | [`ug_xypug.hpp`](../src/ww_kokkos/ug_xypug.hpp), [`xypug_shim.cpp`](../src/fortran_iface/xypug_shim.cpp), [`xypug_c.hpp`](../src/fortran_iface/xypug_c.hpp) (argument table), package `ww_kokkos` 0.11.0 |
| Fortran caller | fork branch `ww3-gpu/xypug-196-rebased`: `W3KOKKOS_XYPUG` and the set-up in `w3kokkos_prop3md.F90`, the call-site hunk in `w3wavemd.F90`; the Fortran loop stays selectable with `WW_KOKKOS_XYP3=0` |
| Covered | `SHRD`, `PR3` with `UQ`, scheme `FSN` alone (`FSPSI`, `FSFCT`, `FSNIMP` false), no `PDLIB`, `MGP`, `MGG`, `REF1`. Every other case keeps the start-up stop under an explicit flag |

**Parallel structure.** One team per spectral component, as the batched `W3XYP3`. Inside a
component, the per-triangle quantities (`KELEM`, `NM`, `FLALL`, `UTILDE`) are independent per
triangle. The Fortran's scatter sums `KKSUM(NI) = KKSUM(NI) + KELEM(:,IE)` and
`ST(NI) = ST(NI) + KELEM(:,IE)*(U(NI)-UTILDE)` (`:630-634`, `:683-688`) become per-node gathers
over a list of (triangle, corner) pairs built once per grid in ascending `IE`, then corner. Each
node receives the same additions in the same order, starting from `0.d0`, so no sum is reordered
and no atomic is used. The CFL minimum and `ITER` (`:638-659`) and the boundary loop under `FLBPI`
(`:704-721`, the last `IBI` wins on a repeated node) run on one thread of the team in the
Fortran's order. Nothing depends on the team size or the backend.

**Arithmetic (v, translator, 2026-10-18).** gfortran 16.2 `-O3` GIMPLE and assembly of
`w3profsmd.F90` under the switch of `ww3_tp2.6`, compared with the `openmp-release` GIMPLE of the
kernel (`-ffp-contract=off`, no `vfmadd` in the object): the same operations, types and order,
including `LAMBDA = DBLE((C1+C2)+C3)*ONESIXTH` summed in `REAL`, `DTMAXGL = (double)1e11f`,
`MAX(..., (double)1e-9f)`, `SUM(MIN(0,KTMP))` as `((m1+0.0)+m2)+m3`, `DOT_PRODUCT` as
`((f1*u1+0.0)+f2*u2)+f3*u3`, and the `minsd`/`maxsd`/`maxss` operand order with the constant as
source. `NINT` is `lround`; the port computes `round` cast to `long` with a guard beyond 2^63, so
no device `round` is relied on there. ⚠ Device `/`, `fmod` and `round` on CUDA were not run.

**Fresh review (v, 2026-10-18).** An independent reviewer re-read the Fortran, the kernel, the
shim and the fork's caller against the same gfortran objects and the kernel's GIMPLE, and
confirmed all ten claims: the per-node gathers give each node the Fortran's additions in the
Fortran's order (the scatters compile to load, add, store per corner after a `memset` to 0), the
mixed `REAL`/`REAL(8)`/`INTEGER*1` types, the `MIN`/`MAX` operand order, `ITER`, the boundary
update, `W3GATH`/`W3SCAT` (only `VA(ISPEC, ISEA)` with `MAPSTA /= 0` is written), no result that
depends on the team size or the backend, the indexing of every array against its bounds and the
wrapper's packing, the set-up's refusals, and `SETDEPTH` once per step. No defect. It also
cross-checked the 2026-10-16 serial translation, which agrees on every `W3XYPUG` operation. Its
smaller findings were fixed: line cites of `W3XYPFSN2` and `W3XYPUG` in the kernel's comments, the
`w3wavemd.F90` cites at the new pin, a glibc 2.44 note, a misleading comment, and a `MAPSTA` size
check in the fork's wrapper (`aa382393`).

**Builds (v, 2026-10-18).** `just kokkos-test serial-debug` and `just kokkos-test openmp-release`:
no warning, 11/11 (the tools' and lessons' tests; the port has none, ADR-0010). The `cuda-release`
preset configures and builds (`cmake --preset cuda-release`, `cmake --build`); nothing ran on a
GPU. The fork's `ww3_lib` compiles for the `ww3_tp2.6` switch and `switch_lab_shrd`, each with
`KOKKOS`. Without `KOKKOS`, the preprocessed `w3wavemd.F90` is identical to the pin's.

## Benchmarks

Regtest pending ⚠ (ADR-0016): parity of the two `ww3_tp2.6` entries is checked (0/36 and 0/38 files
different, `nccmp-tol` PASS, at `106263d`), the rest of the matrix is not.

Four bit-neutral speed-ups, each with a switch (`WW_KOKKOS_XYPUG_FUSE_UT`, `_SKIP_H2D`, `_LAYOUT`,
`_CFL_PAR`, default on, `0` is the earlier code). Which operations moved and why the bits stay is in
[the log](../../docs/log/2026-10-19-xypug-speedups.md). `xypug.kernel` of the `ww3_tp2.6` `ST4`
entry, resident preset, 600 calls, OpenMP backend, Intel Core i9-14900 (32 threads), median of three
alternated runs, load below 2 at the start of each; raw rows in
[`docs/data/xypug_speedups_tp2.6_2026-10-19.tsv`](../../docs/data/xypug_speedups_tp2.6_2026-10-19.tsv):

| Config | 32 threads (s) | 1 thread (s) |
|---|---|---|
| all four off | 7.97 | 37.43 |
| `FUSE_UT` only | 6.79 | 36.14 |
| `SKIP_H2D` only (`xypug.h2d` 0.43 s to 0.06 s) | 8.10 | 37.00 |
| `LAYOUT` only | 7.96 | 36.82 |
| `CFL_PAR` only | 7.81 | 36.92 |
| all four on | 6.57 | 36.50 |

Only `FUSE_UT` and the sum at 32 threads are outside the spread of the runs. The model's total time
was not compared ⚠.

## How it was regtested

The batched kernel, 2026-10-19 (v): `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix 4
serial -R '^00[12]:'` at ww3-gpu `94b3118`, WW3 `aa382393` (the fork branch rebased onto `fac74069`; first run 2026-10-18 at `84df86e0` gave the same counts): `ww3_tp2.6` `ST4` and `ST4_ASCII`, `test.comp` 0 of 36
and 0 of 38 files different, `nccmp-tol` PASS, 777 600 components on Kokkos, `WW3GPU_STRICT=ON`
([record](../../docs/data/ww3gpu_matrix_xypug_tp2.6_2026-10-19.tsv),
[log](../../docs/log/2026-10-19-xypug-tp2.6-rebased.md), [first run](../../docs/log/2026-10-18-xypug-tp2.6.md)). That test turns off `FLCTH`, `FLCK` and `FLSOU`, so `KTP3` and the
source terms on an unstructured grid were not exercised ⚠. No timing.

The serial host translation, 2026-10-16: not yet, and nothing was run. What was done, 2026-10-16 (v): a scratch translation unit that
calls `w3xypug` compiled with `prop.cpp`'s `serial-debug` flags (`-std=c++20 -Wall -Wextra
-Wpedantic -Wshadow -Wconversion -ffp-contract=off`, ASan and UBSan) with no warning, and its code
was generated; the same unit at `-O3` was read in assembly. `prop.cpp` does not include the
header yet, so no preset builds it; CUDA was not compiled ⚠.
