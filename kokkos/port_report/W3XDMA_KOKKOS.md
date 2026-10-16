# W3XDMA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3INIT`/`DO_OUTPUT_EXCHANGES` wiring first.

- (v) Fresh review 2026-10-16: read the routine (`:1522-2409`) against both headers, all 93
  arrays: condition, shape, second lower bound per branch, `STAT=` or not, allocation and fill
  order; rebuilt `ww3_lib` (CMake Release, `switch_lab_shrd`, pinned `ww3` shell) in
  `/tmp/claude-1000/xdma-review/` and recompiled `w3adatmd.F90` with `-fdump-tree-optimized-lineno`
  (disassembly identical to the CMake object, 0 differing lines): 186 `malloc`, 167 `EXTCDE`
  (3 checks, 164 `CHECK_ALLOC_STATUS`), 23 `_gfortran_os_error_at` at `:1680-1713`, `:1983`,
  `:2276-2302`, no `free`; `:1984` tests the `ISTAT` of `:1981`; `XQP(1)` one store; the
  `outflags` layout is `OutputFlags::all` of `pdlib_do_output_exchanges.hpp`. Two defects found
  and fixed: `AINIT2` had a second home (`WadatX::ainit2`; now `scalars[imod-1].ainit2`, the
  `WadatScalars` that `W3NAUX` clears and `W3XETA` reads), and `NSEALM` was read through
  `Wadats::iadata`, which `W3XETA` moves, instead of the `W3SETA` alias target
  (`W3setaTargets::base`). One comment corrected (the `XQKK` fill reuses the `UNDEF` load of
  `:2311`). The changed header compiles with no warning: g++ 16.2 `-O3 -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -ffp-contract=off -fopenmp` and `-O0 -Wsign-conversion
  -fsanitize=address,undefined`, explicit instantiations on `Kokkos::DefaultExecutionSpace`
  (OpenMP) and `Serial`, 0 FMA and 0 scalar float add, sub, mul or div in either; `nvcc_wrapper`
  (CUDA 12.9, Kokkos 5.2.2, `-arch=sm_89 --fmad=false -Xcompiler -fopenmp`, `#cuda` shell) on
  `Cuda` and `Serial`, compile only. Nothing was linked or run.

Allocates the `X*` output arrays of model `IMOD` in `WADATS(IMOD)`, the full-grid copies
(`NXXX = NSEALM*NAPROC` points) that the process writing output gathers, each at full size when
its output flag is set and at one point otherwise; fills them with `UNDEF`; sets `AINIT2`. In
WW3's words, "Version of W3DIMX for extended ouput arrays only" (`w3adatmd.F90:1537`). It does
no floating-point arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XDMA`, `model/src/w3adatmd.F90:1522-2409` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it. The lab switch compiles the routine (no `#ifdef` around it); its body depends on no switch but `W3_S` and `W3_T`, so `switch_PDLIB` compiles the same statements |
| Kokkos code | [`../src/ww_kokkos/adat_w3xdma.hpp`](../src/ww_kokkos/adat_w3xdma.hpp), a host-side function template over the execution space; the `X*` components of `TYPE WADAT` (`w3adatmd.F90:414-497`) in [`../src/ww_kokkos/adat_w3xdma_wadat.hpp`](../src/ww_kokkos/adat_w3xdma_wadat.hpp), beside `Wadat` (`adat_wadat.hpp`, unchanged); `AINIT2` (`:568`) is `WadatScalars::ainit2` (`adat_w3naux.hpp`), its one home, which `W3NAUX` clears (`:806`) and `W3XETA` reads |
| Memory | every array a `Kokkos::View<..., Kokkos::LayoutLeft, ExecSpace::memory_space>`, created `WithoutInitializing` outside any kernel; the second lower bound of a 2-D array is stored per array (`*_lb`), because the one-point shape `(1,1)` has lower bound 1 where the full shape has `0`, `E3DF(2,k)` or `P2MSF(2)` |
| Errors | the `EXTCDE` exit codes returned as `W3xdmaStatus` (1, 2, 3, 99); the gfortran run-time error of an `ALLOCATE` without `STAT=` as `alloc_runtime_error`, process status 1 (`w3xdma_exit_status`) |
| Callers at the pin | `W3INIT` (`w3initmd.F90:3440`, `:3442`, `:4875`, under `W3_MPI`); `DO_OUTPUT_EXCHANGES` (`pdlib_field_vec.F90:1324`), whose hook `w3xdma` in [`pdlib_do_output_exchanges.hpp`](../src/ww_kokkos/pdlib_do_output_exchanges.hpp) passes `FLGRDALL` in the layout of the `outflags` argument here |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Order of operations

As the Fortran, `w3adatmd.F90`:

1. `memunit = 30000+IAPROC` (`:1570`), read before any check; it feeds only `print_memcheck`
   (`:2387`), which writes nothing without `W3_MEMCHECK` (`w3servmd.F90:2502`). Not ported.
2. `NGRIDS == -1` exits 1 (`:1571`); `IMOD < 1` or `IMOD > NADATA` exits 2 (`:1576`);
   `WADATS(IMOD)%AINIT2` (`scalars[imod-1].ainit2`, not the alias) exits 3 (`:1581`).
3. `W3SETG(IMOD)` when `IGRID /= IMOD` (`:1590-1591`): the caller passes grid `IMOD`'s `NK`,
   `E3DF`, `P2MSF` in `W3xdmaGrid`.
4. `NXXX = NSEALM*NAPROC` (`:1596`), `NSEALM` through the unconditional alias of the last
   `W3SETA` (`:2754`), `W3setaTargets::base` in the port, which `W3XETA` does not move.
5. Groups 2 to 10, each `IF (OUTFLAGS(j,k))` block one `ALLOCATE` statement per array, then the
   group's fills, in the order of the table below.
6. `WADATS(IMOD)%AINIT2 = .TRUE.` (`:2375`, `scalars[imod-1].ainit2`); `W3SETG(JGRID)`
   (`:2385`) is the caller's.

## Arrays

"full" is the shape when the condition holds, "else" when it does not. `NXXX` as above. Every
array is filled with `UNDEF` (`-999.9`, bits `0xc479f99a`) except `XQP`, where only `XQP(1)` is.

| Arrays | Condition `OUTFLAGS(j,k)` | full | else | `ALLOCATE` has `STAT=` | `w3adatmd.F90` |
|---|---|---|---|---|---|
| `XHS`, `XWLM`, `XT02`, `XT0M1`, `XT01` | `(2,1)`..`(2,5)` | `(NXXX)` | `(1)` | yes | `:1598-1636`, fill `:1732-1736` |
| `XFP0` | `(2,6)` or `(2,18)` | `(NXXX)` | `(1)` | yes | `:1638-1645`, `:1737` |
| `XTHM`, `XTHS`, `XTHP0`, `XHSIG` | `(2,7)`..`(2,10)` | `(NXXX)` | `(1)` | yes | `:1647-1677`, `:1738-1741` |
| `XSTMAXE`, `XSTMAXD`, `XHMAXE`, `XHCMAXE`, `XHMAXD`, `XHCMAXD` | `(2,11)`..`(2,16)` | `(NXXX)` | `(1)` | no | `:1679-1714`, `:1742-1747` |
| `XWBT`, `XWNMEAN` | `(2,17)`, `(2,19)` | `(NXXX)` | `(1)` | yes | `:1716-1730`, `:1748-1749` |
| `XEF`, `XTH1M`, `XSTH1M`, `XTH2M`, `XSTH2M` (k = 1..5) | `(3,k)` | `(NXXX, E3DF(2,k):E3DF(3,k))` | `(1,1)` | yes | `:1751-1789`, `:1791-1795` |
| `XPHS`, `XPTP`, `XPLP`, `XPDIR`, `XPSI`, `XPWS`, `XPTHP0`, `XPQP`, `XPPE`, `XPGW`, `XPSW`, `XPTM1`, `XPT1`, `XPT2`, `XPEP` | `(4,1)`..`(4,15)` | `(NXXX, 0:NOSWLL)` | `(1,1)` | yes | `:1797-1915`, `:1933-1949` |
| `XPWST`, `XPNR` | `(4,16)`, `(4,17)` | `(NXXX)` | `(1)` | yes | `:1917-1931`, `:1939-1940` |
| `XCHARN`, `XCGE`, `XPHIAW` | `(5,2)`..`(5,4)` | `(NXXX)` | `(1)` | yes | `:1951-1973`, `:2016-2019` |
| `XTAUWIX`, `XTAUWIY` | `(5,5)` | `(NXXX)` | `(1)` | yes, except `XTAUWIY(1)` (`:1983`) | `:1975-1985`, `:2020-2021` |
| `XTAUWNX`, `XTAUWNY` | `(5,6)` | `(NXXX)` | `(1)` | yes | `:1987-1997`, `:2022-2023` |
| `XWHITECAP` | `(5,7)` or `(5,8)` or `(5,9)` or `(5,10)` | `(NXXX, 4)` | `(1, 4)` | yes | `:1999-2006`, `:2024` |
| `XTWS` | `(5,11)` | `(NXXX)` | `(1)` | yes | `:2008-2014`, `:2017` |
| `XSXX`, `XSYY`, `XSXY` | `(6,1)` | `(NXXX)` | `(1)` | yes | `:2026-2040`, `:2158-2160` |
| `XTAUOX`, `XTAUOY`; `XBHD`; `XPHIOC`; `XTUSX`, `XTUSY`; `XUSSX`, `XUSSY`; `XPRMS`, `XTPMS` | `(6,2)`; `(6,3)`; `(6,4)`; `(6,5)`; `(6,6)`; `(6,7)` | `(NXXX)` | `(1)` | yes | `:2042-2104`, `:2161-2170` |
| `XUS3D` | `(6,8)` | `(NXXX, 2*NK)` | `(1,1)` | yes | `:2106-2112`, `:2171` |
| `XP2SMS` | `(6,9)` | `(NXXX, P2MSF(2):P2MSF(3))` | `(1,1)` | yes | `:2114-2120`, `:2172` |
| `XTAUICE` | `(6,10)` | `(NXXX, 2)` | `(1, 2)` | yes | `:2122-2128`, `:2174` |
| `XPHICE` | `(6,11)` | `(NXXX)` | `(1)` | yes | `:2130-2136`, `:2173` |
| `XUSSP` | `(6,12)` | `(NXXX, 2*NK)` | `(1,1)` | yes | `:2138-2144`, `:2175` |
| `XTAUOCX`, `XTAUOCY` | `(6,13)` | `(NXXX)` | `(1)` | yes | `:2146-2156`, `:2176-2177` |
| `XABA`, `XABD`; `XUBA`, `XUBD`; `XPHIBBL` | `(7,1)`; `(7,2)`; `(7,4)` | `(NXXX)` | `(1)` | yes | `:2179-2217`, `:2227-2232` |
| `XBEDFORMS` | `(7,3)` | `(NXXX, 3)` | `(1, 3)` | yes | `:2203-2209`, `:2231` |
| `XTAUBBL` | `(7,5)` | `(NXXX, 2)` | `(1, 2)` | yes | `:2219-2225`, `:2233` |
| `XMSSX`, `XMSSY`; `XMSCX`, `XMSCY`; `XMSSD`; `XMSCD` | `(8,1)`; `(8,2)`; `(8,3)`; `(8,4)` | `(NXXX)` | `(1)` | yes | `:2235-2273`, `:2305-2310` |
| `XQP` | `(8,5)` | `(NXXX)` | `(1)` | no | `:2275-2279`; only `XQP(1)` at `:2311` |
| `XQKK`, `XSKEW`, `XEMBIA1`, `XEMBIA2` | `(8,6)`..`(8,9)` | `(NXXX)` | `(1)` | no | `:2281-2303`, `:2312-2315` |
| `XDTDYN`, `XFCUT`, `XCFLXYMAX`, `XCFLTHMAX`, `XCFLKMAX` | `(9,1)`..`(9,5)` | `(NXXX)` | `(1)` | yes | `:2317-2355`, `:2357-2361` |
| `XUSERO` | `(10,2)` alone decides the final shape | `(NXXX, 2)` | `(1, 2)` | yes | `:2363-2371`, `:2373` |

A bound pair with `UB < LB` gives a zero-size dimension, as `ALLOCATE` does (`NOSWLL = -1`
gives `0:-1`); `NXXX <= 0` gives a zero-size first dimension. `OUTFLAGS(2,20)`, `(5,1)` and
`(6,14)` onwards are not read.

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, built 2026-10-16 in
`/tmp/claude-1000/xdma-scratch/` by WW3's own CMake Release build of `ww3_lib` under
`switch_lab_shrd` (gfortran 16.2, the pinned `ww3` shell), then recompiled by hand with the flags
and defines of `ww3_lib.dir/flags.make` plus `-fdump-tree-optimized-lineno`; read as GIMPLE and
assembly.

- (v) The object is WW3's: the whole disassembly of the recompile is identical to the CMake
  object's (`objdump -d --no-show-raw-insn`, 0 differing lines; `__w3adatmd_MOD_w3xdma` is 9272
  lines in both).
- (v) The source and the C++ agree on the order of the 93 arrays, the output flag of each block
  (`OUTFLAGS(2,6) .OR. (2,18)`, the four flags of `XWHITECAP`), which `ALLOCATE` has `STAT=`
  in each branch, and the order of the 92 fills plus `XQP(1)`; checked by a script that parses
  both files, after reading them.
- (v) The GIMPLE has 186 `malloc` calls (93 arrays, two branches each), 167 `EXTCDE` calls
  (the three checks and 164 `CHECK_ALLOC_STATUS`), and 23 `_gfortran_os_error_at` for the
  `ALLOCATE`s without `STAT=` (11 full, 12 one-point). 163 of the `CHECK_ALLOC_STATUS` follow a
  `STAT=` `malloc` and reach `EXTCDE(99)` on `NULL`; the 164th, `:1984`, follows the `STAT`-less
  `XTAUWIY(1)`, reads the `ISTAT = 0` of `:1981` and cannot fire. `_gfortran_os_error_at` ends
  in `exit_error(1)` (disassembly of `libgfortran.so.5.0.0` of the shell: `mov $0x1,%edi`
  before the call).
- (v) `NXXX = NSEALM*NAPROC` (`:1596`) is an `integer(kind=4)` product of the targets of the
  module pointers `NSEALM` (`W3SETA`, `:2754`) and `NAPROC` (`W3SETO`, `w3odatmd.F90:1660`),
  loaded after `W3SETG(IMOD)`; a full array is `malloc(NXXX*4)`, or `malloc(1)` when
  `NXXX <= 0`. The port reads `NSEALM` of model `W3setaTargets::base` (the target of the
  alias, which `W3XETA` does not move, so not `Wadats::iadata`) and multiplies in `int`.
- (v) `XUSERO` (`:2363-2371`, a `POINTER`, `:497`): the loop runs twice (`NOEXTR = 2`,
  `w3odatmd.F90:335`) with no `free` between the two `malloc`s, then one fill of the second
  array at `:2373`. This is a defect of WW3 at the pin, kept as written: `OUTFLAGS(10,1)`
  sizes an array that is then dropped, and with `(10,1)` set and `(10,2)` unset
  `DO_OUTPUT_EXCHANGES` writes `USERO(1:NSEA,1)` (`pdlib_field_vec.F90:1723-1728`, `USERO`
  aliased to `XUSERO` by `W3XETA`, `w3adatmd.F90:3280`) past a `(1,2)` array. The same defect
  is recorded from the caller's side in
  [`DO_OUTPUT_EXCHANGES_KOKKOS.md`](DO_OUTPUT_EXCHANGES_KOKKOS.md).
- (v) `XQP(1) = UNDEF` (`:2311`) is one 4-byte store to the first element; the rest of a full
  `XQP` stays undefined. The port stores into `subview(xqp, 0)` only.
- (v) Every other fill is a scalar loop of 4-byte stores of `UNDEF`, loaded from
  `__constants_MOD_undef` before the loop, no arithmetic; the `XQKK` fill (`:2312`) reuses the
  load of `:2311` (92 loads for 93 statements).
- (v) The C++ compiles to no floating-point arithmetic and no FMA instruction (`objdump` of the
  throwaway unit: 0 `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub`, 0 scalar add, sub, mul or div in
  `w3xdma`).
- ⚠ The fills go through `Kokkos::deep_copy(exec, view, value)`. That they store the same bits is
  argued from what a fill is, not read in the C++ GIMPLE.
- ⚠ The first `XUSERO` View is released when the second is assigned; the Fortran leaks it. Only
  memory differs; no value is read from it.
- ⚠ An `ALLOCATE` failure is `malloc` returning `NULL` in the Fortran. In the port it is the
  exception Kokkos throws when a View allocation fails, caught per statement. That Kokkos throws,
  not aborts, was not checked on the CUDA backend.
- ⚠ Three codes are not WW3's, each in place of undefined behaviour the Fortran would execute:
  `nsealm_unassociated` (-1, `W3setaTargets::base` 0: no `W3SETA` yet, so `:1596` reads
  through an unassociated pointer), `x_not_sized` (-2, `WadatsX::models` or the `WadatScalars`
  vector shorter than `IMOD`, impossible in the Fortran) and `xqp_empty` (-3, `OUTFLAGS(8,5)` set
  and `NXXX <= 0`, so `:2311` stores 4 bytes into a 1-byte `malloc`; whether the Fortran then
  runs on depends on the allocator's minimum chunk, not checked). No caller at the pin was shown
  to reach them. `w3xdma_exit_status` returns them unchanged: they have no Fortran exit status.
- ⚠ `NSEALM*NAPROC` overflows `int` above 2^31 points in both languages (undefined in both); not
  guarded, as in the Fortran.
- ⚠ `IAPROC` is dereferenced at `:1570` before the checks; an unassociated `IAPROC` crashes the
  Fortran there. The port does not read it.
- ⚠ `NAPROC` and `NOSWLL` are whatever the `W3ODATMD` pointers point at when `W3XDMA` is called
  (the last `W3SETO`), and `NSEALM` that of model `W3setaTargets::base` (the last `W3SETA`);
  the shim has to pass those, not `OUTPTS(IMOD)`'s.
- ⚠ A zero-extent Fortran array is `ASSOCIATED`, a zero-extent View carries no such flag;
  `AINIT2` is the record.

## Not ported

- `W3_S` STRACE and the `W3_T` prints: not compiled under the lab switch.
- The `WRITE(NDSE)` messages and `EXTCDE`: returned as the code it would exit with.
- `W3SETG(IMOD)` (`:1591`) and `W3SETG(JGRID)` (`:2385`): the caller passes grid `IMOD`'s state.
- `memunit` and `print_memcheck` (`:1570`, `:2387`): no effect without `W3_MEMCHECK`.

## Benchmarks

None. No timing without parity (`AGENTS.md`); the routine runs once per model at set-up, and
`docs/data/ww3_pdlib_gprof.tsv` gives it 0.00 s.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v), all in the pinned `ww3` shell
(`nix develop "path:picasso/labs/pratico#ww3"`):

- `cmake -S WW3 -B /tmp/claude-1000/xdma-scratch/ww3 -DCMAKE_BUILD_TYPE=Release
  -DSWITCH=<switch_lab_shrd>` and `cmake --build ... --target ww3_lib`, then `gfortran` on
  `w3adatmd.F90` with the flags of `flags.make` and `-fdump-tree-optimized-lineno`, and
  `objdump -d --no-show-raw-insn` of both objects (`/tmp/claude-1000/xdma-scratch/build.sh`).
- `objdump` of `libgfortran.so.5.0.0` for `_gfortran_os_error_at`.
- A Python script, run once, that compared the array order, flags, `STAT=` presence and fill
  order of the source with the header's.
- A throwaway translation unit with an explicit instantiation of
  `ww::adat::w3xdma<Kokkos::DefaultExecutionSpace>` (OpenMP), compiled with g++ 16.2, no
  warning (`/tmp/claude-1000/xdma-scratch/cxx.sh`):

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -ffp-contract=off -fopenmp -c tu_w3xdma.cpp
```

The same unit also compiled at `-O0` with `-Wsign-conversion` and
`-fsanitize=address,undefined`, again with no warning. It was never linked or run.
