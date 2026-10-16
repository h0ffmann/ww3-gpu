# W3WAVE's per-step support routines on Kokkos (phase 1, translation only)

Status: **every routine translated and reviewed, with the PR3 maps added**, 2026-10-16, under [ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md).
Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42), the port planner.

The owner, 2026-10-16: "Start porting routines with discipline to Kokkos. Without any regtests.
Focus on writing correct bit to bit translation on Kokkos (CUDA OR OPENMP backend)", with Opus
subagents at low to medium effort depending on the routine. Consequences:

- **Order.** What is left above gprof's 10 ms line in [`kokkos/PORT_RANKING.md`](../kokkos/PORT_RANKING.md)
  is the propagation package (`PR3`, `UQ`), which another branch translates
  ([`claude/42-port-propagation`](https://github.com/h0ffmann/ww3-gpu/tree/claude/42-port-propagation)).
  This unit takes the next rows that branch leaves out (`W3GATH`, `W3SCAT`, `W3UWND`) and the
  routines below the 10 ms line that `W3WAVE` calls around the propagation (`W3DZXY`, `W3UTRN`)
  or at set-up (`DISTAB` with `WAVNU2`), easiest first.
- **No test runs.** No ctest, no regtest. The C++ is compiled to show it builds; bit parity is
  argued statically from the `-O3` GIMPLE and assembly of both compilers (ADR-0011) until the
  unit's checkpoint. The port has no tests of its own ([ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)).
- **Backends.** Each routine is a header-only `KOKKOS_INLINE_FUNCTION` that compiles for OpenMP
  and CUDA. A libm call (`atan2f`, `tanhf`, ...) takes glibc's bits on the host only; on CUDA the
  math library's algorithms differ ⚠ (`AGENTS.md`, "Bit for bit").

## Scope

Lines at NOAA-EMC/WW3 `03adda91` (`WW3-noaa/`), the fork's base (v, read 2026-10-16). Shares
from the `bench_small` profile (`just profile-case full`, `WW3@6d1bcdfe`) (v).

| Rank | Routine | Lines | Self time | Called from |
|---|---|---|---|---|
| 6 | `W3SCAT` | `w3wavemd.F90:3345-3644` | 0.21 s | `W3WAVE`, `w3wavemd.F90:2003` |
| 7 | `W3GATH` | `w3wavemd.F90:3024-3329` | 0.15 s | `W3WAVE`, `w3wavemd.F90:1954` |
| 9 | `W3UWND` | `w3updtmd.F90:488-822` | 0.01 s | `W3WAVE`, `w3wavemd.F90:1233` |
| 72 | `W3DZXY` | `w3updtmd.F90:3211-3529` | < 0.01 s | `W3WAVE`, `w3wavemd.F90:1213-1214,1491,1507` |
| 73 | `W3UTRN` | `w3updtmd.F90:2809-3194` | < 0.01 s | `W3WAVE`, `w3wavemd.F90:1465` |
| 41 | `DISTAB` | `w3dispmd.F90:552-683` | < 0.01 s | `W3IOGR`, `w3iogrmd.F90:2019` |
| 17 | `WAVNU2` | `w3dispmd.F90:204-346` | < 0.01 s | `DISTAB` |
| 57 | `W3MAP3` | `w3pro3md.F90:139-518` | < 0.01 s | `W3WAVE`, `w3wavemd.F90:1463` |
| 58 | `W3MAPT` | `w3pro3md.F90:527-625` | < 0.01 s | `W3WAVE`, `w3wavemd.F90:1467` |

Under `switches/switch_lab_shrd` (`SHRD`), `W3GATH` and `W3SCAT` are the shared-memory copy between
`VA(ISPEC,ISEA)` and the propagation's `FIELD`; their MPI branches are not compiled and not ported.
`WAVNU2` is already translated inside `st4_insin4.cpp` for `INSIN4`; that copy is left alone
(the ST4 checkpoint is running on it), and `DISTAB` gets its own.

Out of scope: wiring into `W3WAVE` (the rows stay `wired` no), `W3UCUR`, `W3ULEV`, `W3UINI`
(not called by the lab case), and anything the propagation branch owns.

## Method

The ST4, W3SRCE and propagation method:

1. The `-O3` objects of `w3wavemd`, `w3updtmd` and `w3dispmd`, built by gfortran 16.2 with the
   flags of WW3's CMake Release build under the lab switch, are byte-identical to the CMake
   objects (v, 2026-10-16). They are only identical inside the pinned `ww3` shell: the same
   gfortran wrapper outside it gave another `w3dispmd.o`.
2. One Opus agent per routine reads the preprocessed Fortran and its GIMPLE and assembly, writes
   the spec (arguments, module state, every floating-point statement in emitted order, folded
   constants, libm calls, `MIN`/`MAX` operand order from the assembly) and translates it into a
   header in `kokkos/src/ww_kokkos/`, then compiles a throwaway unit with the `openmp-release`
   flags and compares.
3. A fresh agent per routine reviews the C++ object against the Fortran object.
4. Ledger rows `ported`, `wired` no, `regtested` no; port reports; glossary; log.

## State, 2026-10-16

| Routine | Header | Translator's static comparison | Fresh review |
|---|---|---|---|
| `W3GATH`, `W3SCAT` | `wave_gath_scat.hpp` | same moves as the Fortran object (v) | no defect (v) |
| `W3DZXY` | `updt_dzxy.hpp` | same operations and constants, no FMA (v) | no bit change for finite inputs (v); three products commuted in the assembly, which C++ source cannot reorder: a NaN payload with a NaN grid metric only ⚠; author and the strided caller at `w3wavemd.F90:1507` fixed |
| `W3UWND` | `updt_uwnd.hpp` | same operations, `atan2f`, `sincosf`, inline x87 `fprem` for `MOD` in both, MAX operand order (v) | rerun 2026-10-16: no arithmetic defect (v); one comment cited the wrong allocation line, fixed |
| `DISTAB`, `WAVNU2` | `disp_distab.hpp` | same operations and folded constants (v) | rerun 2026-10-16: no defect (v); the 87 FFMA of the CUDA SASS are CUDA's correctly rounded division and libdevice `tanhf`/`coshf`/`sinhf`, none from the header (v), whose bits against glibc are unknown ⚠ |
| `W3UTRN` | `updt_utrn.hpp` | translated in the stopped session, header recovered from its worktree (v) | no defect (v); the `TRNY(0)` read and the `NX` or `NY` = 1 edge reads depend on the shim ⚠ |
| `W3MAP3` | `prop_map3.hpp` | integer only, no floating-point operation in either object (v) | no defect, line by line against the preprocessed Fortran (v) |
| `W3MAPT` | `prop_mapt.hpp` | same `minss` operand order, `0.95` bits, compare sense (v) | no defect (v) |

What is not done: the shims and the `W3WAVE` and `W3IOGR` hunks; no header is wired into WW3.
Nothing was run: no ctest of the port, no regtest. Since 2026-10-16 `wave_support.cpp`
instantiates every launcher on the default execution space, so `serial-debug`,
`openmp-release` and `cuda-release` build them (the CUDA build never run). `W3MAP3` and
`W3MAPT` were added to this unit on 2026-10-16: `W3WAVE` calls them around `W3UTRN`
(`w3wavemd.F90:1463-1467`), and the propagation plan had left them out as set-up.