# SET_IOBDP_PDLIB_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim and the hunks of its
two callers first.

- (v) Fresh review 2026-10-16: rebuilt WW3's CMake Release `ww3_lib` under a copy of `switch_PDLIB`
  in `/tmp/claude-1000/sib-review/` (gfortran 16.2.0, pinned `#ww3` shell) and read the routine's
  disassembly and its `-O3` GIMPLE (the `-fdump-tree-optimized` rebuild disassembles identically):
  `cvtss2sd` of `DMIN`, `addsd` of `9.99999974737875163555145263671875e-6` (`.rodata.cst8`
  `0x3ee4f8b580000000`) once before the loop, `DW` widened, `_20 > _96; ~` stored as
  `integer(kind=1)`, `comisd`/`setbe`, so `IOBDP_LOC = 1` when `DW >= DMIN + DTHR` or `DW` is NaN;
  `iplg` at `offset + JSEA`, `DW` by descriptor at element `IP_glob` with lower bound 0 (kept by the
  pointer association `w3adatmd.F90:2828`). The C++ compiled with zero warnings (`g++ -O3
  -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
  -ffp-contract=off -fopenmp`, kokkos-openmp 5.2.2, `DefaultExecutionSpace` (OpenMP) and `Serial`):
  same literal, `thr > (double) dw; ~; (signed char)`, `vcomisd`/`setbe`, no FMA. CUDA, compile only
  (`#cuda` shell, kokkos-cuda 5.2.2 `nvcc_wrapper`, nvcc 12.9, `-arch=sm_89 --fmad=false -Xcompiler
  -fopenmp -Wall -Wextra -ffp-contract=off`, zero warnings): `thr` is added on the host and passed as
  an `f64` parameter; the device does `cvt.f64.f32` then `setp.leu.f64` (SASS `F2F.F64.F32`,
  `DSETP.GEU`), the same predicate with NaN giving 1. Every line citation re-read at `8e433f99`. No
  defect in the C++; the report's `NSEA = NX` caveat was sharpened with the grid preprocessor's lines.

The wet/dry depth flag of the nodes of one PDLIB rank: for each local node `IP = 1..npa`,
`IOBDP_LOC(IP) = 0` when the depth `DW` of its global node `IPLG(IP)` is below `DMIN + DTHR`,
`DTHR = 10E-6`, and 1 otherwise. The explicit and implicit PDLIB schemes multiply by it, for
example the node CFL and update of `PDLIB_W3XYPFSN2` (`w3profsmd_pdlib.F90:1057`, `:1124`). It is
the PDLIB counterpart of `SETDEPTH` ([`SETDEPTH_KOKKOS.md`](SETDEPTH_KOKKOS.md)). Callers:
`W3INIT` on computing ranks (`w3initmd.F90:1456-1460`) and `W3ULEV` for unstructured grids
(`w3updtmd.F90:2579-2581`), both under `W3_PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `SET_IOBDP_PDLIB`, `model/src/w3profsmd_pdlib.F90:6890-6975` at h0ffmann/WW3 `8e433f99`, text identical at NOAA-EMC/WW3 `03adda91` (`git diff 03adda91 8e433f99 -- model/src/w3profsmd_pdlib.F90` is empty), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/pdlib_set_iobdp_pdlib.hpp`](../src/ww_kokkos/pdlib_set_iobdp_pdlib.hpp), `ww::pdlib::set_iobdp_pdlib<ExecSpace>(config, iplg, dw, iobdp_loc)`: `npa` (yowNodepool), `DMIN` (W3GDATMD), `iplg(1:npa)` 0-based, `DW(0:NSEA)` passed with its 0 element so `dw(k)` is `DW(k)`, and `IOBDP_LOC(1:npa)` (`INTEGER*1`) 0-based |
| C shim | none yet; the routine calls nothing, so it has no hook table and no exit code |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

Ported: every statement `switch_PDLIB` compiles (it has no `S`). Not ported, compiled only under
`W3_S` (tracing, no arithmetic): `USE W3SERVMD, only: STRACE` (`:6935-6937`), `IENT`
(`:6951-6953`) and `CALL STRACE` (`:6960-6962`). `USE W3PARALL, only: INIT_GET_ISEA` (`:6941`)
is never called in the body.

## Arithmetic and structure

The reference is the `-O3` object of `w3profsmd_pdlib.F90` from WW3's CMake Release build
(`-O3 -fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none -DSCOTCH_707`,
gfortran 16.2.0, `switch_PDLIB`, the pinned `picasso/labs/pratico#ww3` shell), rebuilt by the
translator with `-fdump-tree-optimized` to a disassembly identical to the CMake build's (apart
from the file-name line), and read as GIMPLE and disassembly, 2026-10-16.

- (v) `REAL*8, PARAMETER :: DTHR = 10E-6` (`:6959`) has a default-`REAL` literal: the Fortran
  GIMPLE adds `9.99999974737875163555145263671875e-6`, which is `(double)1e-5f`, not `1e-5`. The
  C++ builds it as `static_cast<double>(10E-6f)` with a `static_assert` against `0x1.4f8b58p-17`;
  its GIMPLE adds the same literal, and both objects' `.rodata.cst8` hold `0x3ee4f8b580000000`.
- (v) The threshold `DMIN + DTHR` (`:6966`) is computed once before the loop in both objects:
  `cvtss2sd` of `DMIN`, then `addsd` (`vcvtss2sd`/`vaddsd` in the C++). gfortran hoists the load of
  `DMIN` out of the loop; the C++ reads it from the config on the host. Only when `npa >= 1`, in
  both.
- (v) The test `DW(IP_glob) .LT. DMIN + DTHR` (`:6966`) is in double in both: GIMPLE
  `_20 > (double) DW; ~; (integer(kind=1))` in the Fortran and `thr > (double) dw; ~; (signed
  char)` in the C++, `comisd`/`setbe` into the byte in both objects. A NaN depth gives 1 in both.
- (v) Index maps: the Fortran object reads `iplg` at `offset + JSEA` and `DW` at
  `(offset + IP_glob*stride)*span` with `DW`'s lower bound 0 (`w3adatmd.F90:1004`), so `DW` is
  read at element `IP_glob` counted from 0; the C++ reads `iplg(ip - 1)` and `dw(ip_glob)`, and
  writes `iobdp_loc(ip - 1)`.
- (v) No FMA, no libm call, no reduction in either object; zero warnings with the
  `openmp-release` flags, and none from the header with `-Wconversion -Wsign-conversion` added.
- (v) Structural change, no arithmetic: the `JSEA` loop (`:6963-6971`) is one `parallel_for`.
  Each element of `IOBDP_LOC` is written once from its own `IP`, and `iplg`, `DW` and `DMIN` are
  not written, so the order cannot change a bit. The loop nesting is the Fortran's (one loop).
- ⚠ `DW` is a sea-point array (`DW(0:NSEA)`, `w3adatmd.F90:1004`) and the routine indexes it by
  global node `IPLG(IP)` (`:6965-6966`), which is WW3's own assumption that every node of the mesh
  is a sea point (`NSEA = NX`). (v) For an unstructured grid `ww3_grid` sets every node to sea
  (`TMPSTA = 1`, `w3gridmd.F90:4103-4104`) before `NSEA` is counted (`:4876-4883`); a mask read
  from a file (`:4840-4866`) or excluded points could lower `NSEA`, and whether any PDLIB regtest
  does so was not checked. The C++ reproduces the Fortran indexing; the shim must pass the same `DW`.
- ⚠ The `@pre` (every `IPLG(IP)` inside `DW`, `IOBDP_LOC` at least `npa` long) is not checked by
  the Fortran or the C++. `IOBDP_LOC` is allocated with `NPA` elements (`w3profsmd_pdlib.F90:600`);
  the range of `IPLG` against `NSEA` is the `NSEA = NX` caveat above.
- ⚠ CUDA compiled and its PTX and SASS read (fresh review below), never run: the device does one
  exact widening and one compare, and the add is on the host, so no GPU math-library or
  contraction question arises, but no GPU result was compared.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The PDLIB profile gives the routine 0.00 s of self
time (`docs/data/ww3_pdlib_gprof.tsv:70`, `just profile-case pdlib`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16:

```bash
# in nix develop "path:picasso/labs/pratico#ww3"
cmake -S WW3 -B /tmp/claude-1000/sib-scratch/ww3 -DCMAKE_BUILD_TYPE=Release \
      -DSWITCH=/tmp/claude-1000/sib-scratch/switch      # a copy of ww3_tp2.17/input/switch_PDLIB
cmake --build /tmp/claude-1000/sib-scratch/ww3 --target ww3_lib -j 8
gfortran <flags.make> -fdump-tree-optimized -c WW3/model/src/w3profsmd_pdlib.F90   # objdump identical
g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp \
    -I kokkos/src/ww_kokkos -I <kokkos-openmp-5.2.2>/include -fdump-tree-optimized -c tu.cpp
```

`tu.cpp` includes the header and instantiates `set_iobdp_pdlib` for `Kokkos::OpenMP` and
`Kokkos::Serial`; it was compiled, never linked or run.
