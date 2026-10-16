# SET_IOBPA_PDLIB_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim behind
`PDLIB_IOBP_INIT`'s `set_iobpa_pdlib` hook and the W3INIT hunk first.

- (v) Fresh review 2026-10-16: a reviewer who did not write the translation rebuilt
  `w3profsmd_pdlib.F90` under `switch_PDLIB` with WW3's CMake Release build (gfortran 16.2.0, the
  pinned `#ww3` shell) and again with `-fdump-tree-optimized` (disassembly of
  `__pdlib_w3profsmd_MOD_set_iobpa_pdlib` identical), and checked against its GIMPLE and the
  source: the `== 2` test and the 0/1 byte store, the `JSEA = 1..NSEAL` bound with the early
  return, the `MAPSTA(1,IP_glob)` and `iplg` index maps, no `DTHR` or `INIT_GET_ISEA` in the
  object, one write per element, and every line citation of the header and this report. It
  recompiled the header with zero warnings for `DefaultExecutionSpace` (OpenMP) and `Serial`, and
  for CUDA (`nvcc_wrapper`, `sm_89`, `--fmad=false`, compile only). No defect.

The active-boundary flag of the nodes of one PDLIB rank: for each local sea point
`JSEA = 1..NSEAL`, `IOBPA_LOC(JSEA) = 1` when the status `MAPSTA(1,IPLG(JSEA))` of its global
node is 2, and 0 otherwise. Callers, both under `W3_PDLIB`: the tail of `PDLIB_IOBP_INIT`
(`w3profsmd_pdlib.F90:620`, [`PDLIB_IOBP_INIT_KOKKOS.md`](PDLIB_IOBP_INIT_KOKKOS.md)) and
`W3INIT` right after it on computing ranks of an unstructured grid (`w3initmd.F90:921-924`), so
it runs twice at initialisation with the same inputs.

## What was ported

| | |
|---|---|
| WW3 routine | `SET_IOBPA_PDLIB`, `model/src/w3profsmd_pdlib.F90:6978-7061` at h0ffmann/WW3 `8e433f99`, text identical at NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 8e433f99 -- model/src/w3profsmd_pdlib.F90` is empty), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/pdlib_set_iobpa_pdlib.hpp`](../src/ww_kokkos/pdlib_set_iobpa_pdlib.hpp), `ww::pdlib::set_iobpa_pdlib<ExecSpace>(config, iplg, mapsta, iobpa_loc)`: `NSEAL` (W3GDATMD), `iplg` (yowNodepool) 0-based, `MAPSTA(MY,MX)` (default `INTEGER`) as a 2-D `LayoutLeft` View with `mapsta(0, k - 1)` = `MAPSTA(1,k)`, and `IOBPA_LOC` (`INTEGER*1`) 0-based |
| C shim | none yet; the routine calls nothing, so it has no hook table and no exit code. `PDLIB_IOBP_INIT`'s hook `set_iobpa_pdlib(void* ctx)` takes no argument, as the Fortran; the shim behind it calls this function with the four module values |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

The lab switch does not compile PDLIB, so the translation is against `switch_PDLIB`, the switch
of the PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`). Ported: every statement it compiles (it
has no `S`). Not ported, compiled only under `W3_S` (tracing, no arithmetic): `USE W3SERVMD,
only: STRACE` (`:7023-7025`), `IENT` (`:7038-7040`) and `CALL STRACE` (`:7047-7049`, which traces
under the name `'SETDEPTH_PDLIB'`). `USE W3PARALL, only: INIT_GET_ISEA` (`:7028`) and `REAL*8,
PARAMETER :: DTHR` (`:7046`) are declared and never used; neither appears in the object.

## Arithmetic and structure

The reference is the `-O3` object of `w3profsmd_pdlib.F90` from WW3's CMake Release build
(`-O3 -fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none -DSCOTCH_707`,
gfortran 16.2.0, `switch_PDLIB`, the pinned `picasso/labs/pratico#ww3` shell), rebuilt by the
translator with `-fdump-tree-optimized` to a disassembly identical to the CMake build's (apart
from the file-name line), and read as GIMPLE and disassembly, 2026-10-16.

- (v) No floating point in either object: the routine is an integer load, a compare with 2 and a
  byte store. Fortran GIMPLE `_43 = _27 == 2; _84 = (integer(kind=1)) _43;` stored to
  `IOBPA_LOC`, `cmpl $0x2` then `sete` into the byte; C++ GIMPLE `_114 = _99 == 2; _29 = (signed
  char) _114;`, `cmpl $0x2` then `sete` in both the OpenMP and the Serial instantiation. The
  `IF/ELSE` (`:7052-7056`) is the same 0/1 store in both.
- (v) Loop bounds: the Fortran object reads `NSEAL` through its module pointer once, returns when
  it is `<= 0` and runs `JSEA = 1..NSEAL` (`:7050`); the C++ returns when `nseal <= 0` and runs
  `RangePolicy(1, nseal + 1)`.
- (v) Index maps: the Fortran reads `iplg` at `offset + JSEA`, `MAPSTA` at
  `(offset + dim[0].stride + IP_glob*dim[1].stride)*span`, which is `MAPSTA(1,IP_glob)` with
  lower bounds 1 (`w3gdatmd.F90:1733`), and writes `IOBPA_LOC` at `offset + JSEA*stride`; the C++
  reads `iplg(jsea - 1)` and `mapsta(0, ip_glob - 1)` and writes `iobpa_loc(jsea - 1)`.
- (v) No FMA, no libm call, no reduction in either object; zero warnings with `-O3
  -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp`, and none from
  the header with `-Wconversion -Wsign-conversion` added.
- (v) Structural change, no arithmetic: the `JSEA` loop (`:7050-7057`) is one `parallel_for`.
  Each element of `IOBPA_LOC` is written once from its own `JSEA`, and `iplg`, `MAPSTA` and
  `NSEAL` are not written, so the order cannot change a bit. The loop nesting is the Fortran's
  (one loop).
- ⚠ The loop runs to `NSEAL` while `IOBPA_LOC` is allocated `NPA` (`:602`): correct only when
  `NSEAL <= NPA` on every rank, which neither the Fortran nor the C++ checks and which was not
  checked for the PDLIB regtests. The C++ reproduces the Fortran's bound.
- ⚠ The `@pre` (`iplg` and `iobpa_loc` at least `NSEAL` long, every `IPLG(JSEA)` inside
  `MAPSTA`'s second extent) is not checked by the Fortran or the C++.
- ⚠ The shim must pass `MAPSTA` with its first index 0 meaning Fortran row 1 (for an
  unstructured grid `NY = 1`); a View of the whole `MAPSTA(MY,MX)` does that. Not built yet.
- (v) Elements `NSEAL+1..NPA` of `IOBPA_LOC` are not written by either: the Fortran `ALLOCATE`
  (`:602`) leaves them undefined and `PDLIB_IOBP_INIT`'s C++ allocates them `WithoutInitializing`
  (`pdlib_pdlib_iobp_init.hpp:117`), and neither routine touches them afterwards.
- (v) CUDA compiles: `nvcc_wrapper` (CUDA 12.9, Kokkos 5.2.2 CUDA) `-O3 -std=c++20 -arch=sm_89
  --fmad=false -fopenmp -Wall -Wextra -Wpedantic -ffp-contract=off`, compile only, zero warnings;
  the SASS is `ISETP.NE ... 0x2`, `SEL RZ, 0x1` and `STG.E.U8`, no `FFMA`. Not run on a GPU.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The PDLIB profile gives the routine 0.00 s of self
time (`docs/data/ww3_pdlib_gprof.tsv:87`, `just profile-case pdlib`).

## How it was regtested

Not yet, and nothing was run: no regtest, and the port's code was compiled, never linked or
executed. What was done, 2026-10-16:

```bash
# in nix develop "path:picasso/labs/pratico#ww3"
cmake -S WW3 -B /tmp/claude-1000/sia-scratch/ww3 -DCMAKE_BUILD_TYPE=Release \
      -DSWITCH=/tmp/claude-1000/sia-scratch/switch      # a copy of ww3_tp2.17/input/switch_PDLIB
cmake --build /tmp/claude-1000/sia-scratch/ww3 --target ww3_lib -j 8
gfortran <flags.make> -fdump-tree-optimized -c WW3/model/src/w3profsmd_pdlib.F90   # objdump identical
g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp \
    -I kokkos/src/ww_kokkos -I <kokkos-openmp-5.2.2>/include -fdump-tree-optimized -c tu.cpp
objdump -dr --no-show-raw-insn tu.o    # FMA count 0
```

`tu.cpp` includes the header and instantiates `set_iobpa_pdlib` for `Kokkos::OpenMP` and
`Kokkos::Serial`; it was compiled, never linked or run. The only programs run were the compilers,
CMake (which builds WW3's library, not an executable that was run) and `objdump`.
