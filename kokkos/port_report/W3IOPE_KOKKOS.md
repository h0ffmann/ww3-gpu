# W3IOPE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim that
hands it the module arrays and `OUTPTS(IMOD)%OUT2`, and the `WW_W3WAVE_W3IOPE_IOPO` hook of the
Kokkos `W3WAVE` (`wave_w3wave_hooks.hpp`) or a `W3WAVE` hunk calling it.

- (v) Fresh review 2026-10-16: `w3iopomd.F90` of `WW3-noaa/` recompiled in the pinned `ww3` shell
  with the defines, includes and flags of WW3's CMake `flags.make` under `switch_lab_shrd`
  (`.text`, `.rodata`, `.rodata.cst4` identical to the CMake object), and the header recompiled
  with the `openmp-release` flags (`-O3 -DNDEBUG -march=x86-64-v3 -fopenmp -ffp-contract=off`);
  the GIMPLE of both read statement by statement and the call targets read from the relocations
  (Fortran: four `sincosf`, two `atan2f`; C++: one `sincosf` site in a four-pass loop, two
  `atan2f`; two `sqrtss` each, no FMA in either). Checked: every sum left to right with `RD·X`
  first; the `0.0 + x` of `RDS`, `RDI` and the non-PDLIB `ICEFO` in both, none in the PDLIB
  branch; `RD·UA` formed before the `cexpif`/`sincosf` factor; `x·x + y·y`, `sqrt`, `atan2f` only
  above `1.E-7`; the second `FACRD` dividing the first test's sum; `TPI·SIG` folded to
  `6.283185482025146484375` once per `IK` and divided by `CG(IK,IS(J))` lane by lane (Fortran by
  4 and 2, C++ by 8 and 4); `SPCO = ((RD·SP)·FAC)` summed in order, scalar in the Fortran and 8/4
  lanes in the C++, the same tree per lane; the `CG` stride `NK+2`, `SIG` from index 0, the
  `SPCO` stride `NSPEC` of `W3DMO2` (`w3odatmd.F90:1108`, `NSPEC` from `W3GDATMD`, `:1035`); the
  bounds pre-pass accepting `MAPFS` 0. The commuted operands (`X·RD`, `rdi + rds`) can differ
  only in which NaN payload propagates when both operands are NaN, as SSE returns the first
  source's NaN. One defect found and fixed: with `NOPTS` 0 the pre-pass was skipped and a null
  `CX` or `CY` was written through; the null test of `CX`, `CY` now runs first, since the Fortran
  writes `CX(0)`, `CY(0)` whatever `NOPTS` is. One citation fixed (the `USE` block starts at
  `:956`). Recompiled after the fix: same floating-point statements in the GIMPLE, no warning on
  OpenMP, ASan/UBSan, the `wave_support.cpp` copy at both flag sets, and CUDA sm_89
  `--fmad=false`; nothing of the port was run.

`W3IOPE` extracts the point output at an output time. For each point `W3IOPP` accepted it reads
the four corners `IPTINT(:,J,I)` of the enclosing cell and their bilinear weights `PTIFAC(J,I)`,
counts the land (`MAPSTA = 0`), sea (`MAPSTA > 0`) and ice (`MAPSTA < 0`) corners into `IL`, `IW`,
`II`, zeroes the weight of a land corner and normalises the rest to sum one. With these weights it
interpolates ice concentration, thickness and floe size, depth, the wind and current vectors
(stored as speed and direction) and the air-sea temperature difference, ignoring ice. It then
renormalises the weights over the sea corners only and interpolates the spectrum,

  `SPCO(ITH+(IK-1)*NTH, I) = Σ_J RD(J) · A(ITH,IK,IS(J)) · 2π σ_IK / c_g(IK,IS(J))`,

which turns the action density `A` on the storage grid into the variance density F(f,θ)
(`w3iopomd.F90:903-904`, `:1230-1243`). The weights are recomputed at every output time because
the ice edge moves (`:930-931`). `W3WAVE` calls it on the point-output process
(`w3wavemd.F90:2690`); `ww3_multi`'s `WMWAVE` does too (`wmwavemd.F90:1367`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOPE`, `model/src/w3iopomd.F90:873-1293` at NOAA-EMC/WW3 `03adda91`; identical in the `h0ffmann/WW3` fork at the `WW3/` pin (the `W3IOPP` port checked `git -C WW3 diff 03adda91 HEAD -- model/src/w3iopomd.F90` is empty, v 2026-10-16), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iopo_w3iope.hpp`](../src/ww_kokkos/iopo_w3iope.hpp): `ww::iopo::w3iope` (host driver, no kernel), `IopeFields`, `kIopeExitShape`, `kIopeExitBounds` |
| Shared state | [`../src/ww_kokkos/iopo_state.hpp`](../src/ww_kokkos/iopo_state.hpp) of the `W3IOPP` port, read-only here: `IopoState` (`OUTPTS(IMOD)%OUT2`, `w3odatmd.F90:364-388`), `FLogical`. Nothing was added to it |
| Calls not ported here | none: `W3IOPE` calls only intrinsics on the compiled path (`COS`, `SIN`, `SQRT`, `ATAN2`). The `W3_MPI` receives are not compiled |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

What `switch_lab_shrd` compiles in `W3IOPE` (v, `gfortran -E` with the defines of WW3's
`flags.make`, 2026-10-16): the unpacking and land/sea/ice correction (`:1051-1093`), both branches
of the `LPDLIB` test for `ICEFO` (`:1101-1116`), `ICEO`, `ICEHO`, `DPO`, wind, `ASO`, current
(`:1118-1194`), the sea-only renormalisation (`:1198-1201`), the `W3_SHRD` copy of the four
spectra into `SP => SPPNT` (`:1211-1217`) and the spectral interpolation (`:1230-1248`). Not
compiled and not ported: `W3_RTD` (`:1151`, `:1171`, `:1190`, `:1255-1260`), `W3_SETUP` (`DPO`
from `ZETA_SETUP`, `:1127-1130`), `W3_FLX5` (`DAIRO`, `TAUAO`, `TAUDO`, `:1134-1175`), `W3_MPI`
(`:1223-1225`), `W3_DIST`, `W3_T` and `W3_S`. `LPDLIB` is a run-time `LOGICAL` of `CONSTANTS`
(`constants.F90:102`); `W3INIT` sets it `.FALSE.` without `W3_PDLIB` (`w3initmd.F90:642`), so
the lab case takes the non-PDLIB branch, and the port takes `lpdlib` from the caller.

## Arithmetic and control flow

The reference is the `-O3` object of `w3iopomd.F90` compiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (gfortran 16.2.0, pinned `ww3` shell), whose `.text`, `.rodata`,
`.rodata.cst4`, `.rodata.str1.1` and `.rodata.str1.8` are byte-identical to the CMake object
(v, `objcopy` and `cmp`, 2026-10-16, done for the `W3IOPP` port on the same file), read as GIMPLE
(`-fdump-tree-optimized`) and assembly of `__w3iopomd_MOD_w3iope`. The C++ was compiled to GIMPLE
and an object with the `openmp-release` flags (`g++ 16.2.0 -O3 -march=x86-64-v3
-ffp-contract=off`) and compared statement by statement.

- (v) **Weighted sums.** Each sum of `:1114-1125` and `:1178-1184` is `((RD(1)·X1 + RD(2)·X2) +
  RD(3)·X3) + RD(4)·X4` in the object; the C++ emits the same tree. Some products are commuted in
  one object or the other (`X·RD` against `RD·X`), which gives the same bits for every input but
  can pick a different NaN payload when both factors are NaN ⚠.
- (v) **Leading zeros.** `RDS`, `RDI` and the non-PDLIB `ICEFO` start from `+0.0` and the object
  keeps the additions `0.0 + RD(1)` and `0.0 + RD(1)·ICEF(...)` (`rds_1195 = _1375 + 0.0`,
  `_1000 = _999 + 0.0`): a `-0.0` term gives `+0.0`. The C++ keeps the same three additions
  (`rds_1129 = _1271 + 0.0`, `_1025 = _1024 + 0.0`). The PDLIB `ICEFO` (`:1114-1115`) has no
  leading zero, and the C++ branch neither.
- (v) **Normalisations.** `FACRD = 1./(RDS+RDI)` is a `divss` and then four `mulss` (`:1090-1093`).
  The second normalisation (`:1198-1201`) divides the same `RDI+RDS` sum the first test computed;
  the C++ recomputes `rds + rdi`, the same value (the object's operand order is `rdi + rds`, so
  only a NaN payload with both NaN could differ ⚠). The tests are `> 1.E-7` (`comiss`, false on
  NaN) on `RDS+RDI`, `RDS`, `WAO(I)` and `CAO(I)`, with the literal `1.0000000116860974e-7` in
  both GIMPLEs.
- (v) **Wind.** `COS(UD(IS(J)))` and `SIN(UD(IS(J)))` are one `__builtin_cexpif` per corner,
  four `call sincosf@PLT` in the assembly; the C++ calls `::sincosf` four times (one call site in a
  four-iteration loop). `WNDX` is `((RD(1)·UA(IS(1)))·COS(..) + ...)`, the product `RD·UA` formed
  first and shared by `WNDX` and `WNDY`, in both. `WNDX**2 + WNDY**2` is `x·x + y·y`, then
  `sqrtss`; the C++ object has `vsqrtss` and no `sqrtf` call. `ATAN2` is `call atan2f@PLT`, only
  when `WAO(I) > 1.E-7`, in both. The same holds for `CURX`, `CURY`, `CAO`, `CDO`.
- (v) **Spectral factors.** `TPI·SIG(IK)` is folded to the literal `6.283185482025146484375`
  times `SIG(IK)` (`TPI = 2.0·PI` in `REAL`, `constants.F90:72-73`; `kIopeTpi` and its
  `static_assert` in the header), computed once per `IK` and divided by `CG(IK,IS(J))` for the four
  corners. The object vectorises this loop by four (`mulps`, four `divps`) and by two, with a
  scalar tail; the C++ object by eight and four (`vmulps`, `vdivps`) with a scalar tail. Each lane
  is the scalar `(TPI·SIG)/CG`, so the vector width changes no bit.
- (v) **Spectrum.** `SPCO(ISP,I) = ((RD(1)·SP1)·FAC1 + (RD(2)·SP2)·FAC2) + (RD(3)·SP3)·FAC3 +
  (RD(4)·SP4)·FAC4`, with `SP` read back from `SPPNT` after the copy, in both GIMPLEs. The object
  runs this loop scalar; the C++ vectorises it over `ITH` by eight and four, lane by lane the same
  expression tree (`vect_v_399 = vect__199 + vect__191`, then `+ RD(3)` and `+ RD(4)` terms).
- (v) **No FMA.** No `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub` in the C++ object; the Fortran object
  is built without `-march` and has none either.
- (v) **Side effects kept.** `CX(0) = CY(0) = 0.` (`:1038-1039`) are written before the loop, so a
  land corner (`MAPFS = 0`) reads a zero current; `SPPNT` holds the last point's four corner
  spectra on return, as the Fortran leaves it; `IL`, `IW`, `II` are counted in memory in the same
  order.

### What the C++ refuses where the Fortran is undefined

- ⚠ A corner index `IPTINT(:,J,I)` outside `MAPSTA(NY,NX)`, a sea index `MAPFS(IY,IX)` outside
  `0:NSEA` or `0:NSEAL`, `NOPTS > NPT`, or an `OUT2` array shorter than `W3DMO2` allocates makes the
  Fortran read or write out of bounds (no bounds checks in the Release flags). The C++ checks every
  point first and returns `kIopeExitBounds` (-120) or `kIopeExitShape` (-102) before any write,
  including `CX(0)` and `CY(0)`; a null `CX` or `CY` is refused even when `NOPTS` is 0. The extents of `SIG`, `CG`, `A` and the `(0:NSEA)` arrays are
  the caller's to give: they are pointers, not checked.
- ⚠ A land corner reads `CG(IK,0)`, `A(:,:,0)`, `DW(0)`, ... with weight 0. When `CG(IK,0)` is 0,
  `FAC` is infinite and `0·SP·∞` is NaN in both the Fortran and the C++; the port does not guard it,
  and whether WW3 ever leaves `CG(:,0)` at 0 was not checked.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest, and the port was never run. Compiled, 2026-10-16 (v):

- a throwaway translation unit with an out-of-line instance of `w3iope`, with `g++ 16.2.0
  -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic -ffp-contract=off` against the
  pinned `kokkos-openmp-5.2.2` in the `ww3` shell: no warning, none from the header with
  `-Wconversion -Wshadow`, no FMA in the object;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- the same unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89 --fmad=false
  --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against
  `kokkos-cuda-5.2.2` (CUDA 12.9): no warning, compile only (the routine is host code);
- a copy of `kokkos/src/ww_kokkos/wave_support.cpp` with the includes of `iopo_w3iope.hpp` and
  `iopo_w3iopp.hpp` and `(void)&ww::iopo::w3iope;` added, at the `openmp-release` and
  `serial-debug` flags: no warning.

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
