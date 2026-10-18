# W3CPRT_KOKKOS

**Status:** ported, phase 2 (on the resident spectrum), wired on the fork branch
`ww3-gpu/u9-cprt` (PATCH.md section 27, 2026-10-18): `W3KOKKOS_CPRT` replaces `W3WAVE`'s host read
of `VA` and `CALL W3CPRT ( IMOD )` under `WW_KOKKOS_CPRT=1` (opt-in, not under `WW_KOKKOS_ALL`).
Not at the `WW3/` pin until the integrator merges the branch. Run byte-identical on the CPU
(below); a GPU never ran it ⚠.

`W3CPRT` partitions the spectrum of every local sea point at an output time with partitioning on
(`FLPART`: gridded partition fields or the partition file, `w3wavemd.F90:2680-2682` at `80644ba9`).
Per point it turns the action spectrum `N(k,theta)` of `VA` into the energy spectrum
`E(f,theta) = N * 2 pi sigma / c_g` (`w3iosfmd.F90:262-267`), calls the watershed partitioning
`W3PART` (`w3partmd.F90:138-497`) and stores the `NP+1` columns `XP(1:15,0:NP)` it returns, the
whole spectrum first and then each partition, in `DTPRT`, one point after the other, with
`ICPRT(JSEA,1) = NP+1` and `ICPRT(JSEA,2)` the point's first column (`w3iosfmd.F90:284-326`).
`W3OUTG` section 6 and `W3IOSF` read only that table. On the Fortran path the whole `VA` came back
from the device at every such output time for it; here only the table does.

## What was ported

| | |
|---|---|
| WW3 routine | `W3CPRT`, `model/src/w3iosfmd.F90:109-344` at h0ffmann/WW3 `80644ba9` (the `WW3/` pin), identical at NOAA-EMC/WW3 `03adda91` (v, `git log 03adda91..80644ba9 -- model/src/w3iosfmd.F90 model/src/w3partmd.F90 model/src/w3dispmd.F90` empty, 2026-10-18) |
| Kokkos code | [`../src/ww_kokkos/iosf_w3cprt.hpp`](../src/ww_kokkos/iosf_w3cprt.hpp): `w3cprt_point` (sections 2-4, `:245-279`) and `w3cprt_store` (section 5, `:284-303`); the launches are in the shim |
| Callees | `W3PART` and what it calls, translated before and called unchanged: [`part_w3part.hpp`](../src/ww_kokkos/part_w3part.hpp) ([`W3PART_KOKKOS.md`](W3PART_KOKKOS.md)), `ptsort`, `ptnghb` (the `NEIGH` table, built on the host once per `NK`, `NTH` and copied in), `pt_fld`, `ptmean`, `wavnu1` |
| Not ported, by switch | `W3_T` (test output, `:313-317`), `W3_S` (`STRACE`). `W3_MPI`: `INIT_GET_ISEA` is taken as `ISEA = JSEA` (`W3_SHRD`, `w3parall.F90:1236`); `W3KOKKOS_CPRT_SETUP` refuses the flag under `MPI` and `T` |
| `PTMETH` | 1 to 5, all of `W3PART`'s branches; `DIMXP` is 2 for 4 and 5 and `((NK+1)/2)*((NTH-1)/2)` otherwise (`:213-219`) |
| Left to the Fortran | A sea point with a NaN depth (`:270-274`: two lines and `STOP`) and a spectrum `PTMEAN` cannot take (`IFPMAX = 0`, where the Fortran reads out of bounds, [`PTMEAN_KOKKOS.md`](PTMEAN_KOKKOS.md)): `W3KOKKOS_CPRT` then reads `VA` to the host and calls the Fortran `W3CPRT` whole |
| Structural changes | Points run in parallel, each one the Fortran's serial code for that point. The Fortran's `TMP` array grows by `TSFAC*NSEAL` columns and is copied into `DTPRT(:,1:FINSIZ)` (`:288-326`); every column `1..FINSIZ` is written once by `:303`, so `DTPRT` is the concatenation of the points' `XP(:,0:NP)` in `JSEA` order, which an integer prefix sum over `NP+1` places directly. `PTMEAN`'s format 1000 (`XP ARRAY TOO SMALL`) is counted per call on the device and written by `W3KOKKOS_CPRT` the same number of times, with the same text and value `DIMXP+1` |
| C shim | [`../src/fortran_iface/cprt_shim.cpp`](../src/fortran_iface/cprt_shim.cpp), `ww_cprt_va` and `ww_cprt_fetch` of [`cprt_c.hpp`](../src/fortran_iface/cprt_c.hpp) (ABI `ww_kokkos` 0.10); the Fortran side is `W3KOKKOS_CPRT` of `model/src/w3kokkos_cprtmd.F90` on `ww3-gpu/u9-cprt` |
| Partition fields | `ww_outg_part` of [`outg_c.hpp`](../src/fortran_iface/outg_c.hpp) runs `W3OUTG` section 6 (`w3iogomd.F90:2242-2340`, `ww::iogo::w3outg_part`, the code that was inside `w3outg`, moved unchanged) after `ww_outg_va`, so a gridded output with partition fields no longer makes `W3KOKKOS_OUTG` fall back to the Fortran `W3OUTG` and its host read of `VA` |

## Arithmetic

The reference is the `-O3` GIMPLE (`-fdump-tree-optimized-lineno`) of `w3iosfmd.F90` compiled with
the flags and defines of WW3's CMake Release build under `switch_lab_shrd` (gfortran 16.2.0, the
`pratico#ww3` shell), read beside the GIMPLE of `cprt_shim.cpp` with the `openmp-release` flags
(`-O3 -march=x86-64-v3 -std=c++20 -ffp-contract=off -fopenmp`, g++ 16.2.0), 2026-10-18.

- (v) `UABS = U10*ASF` is one product in both (`_198 = _187 * _197`, `uabs_99 = _95 * _98`).
- (v) `UDIR = U10D*RADE`: both multiply by the same folded constant, `5.72957763671875e+1`.
- (v) `FACT = TPI*SIG(IK)/CG(IK,ISEA)`: both compute `SIG*6.283185482025146484375e+0` and divide by
  `CG` (`fact_569 = _232 / _245`, `fact_125 = _115 / _124`); the product is commuted, which changes
  no bit.
- (v) `E2(IK,ITH) = VA(ITH+(IK-1)*NTH,JSEA)*FACT`, one product per bin, commuted in the C++.
- (v) No FMA: contraction is off in the C++ (`-ffp-contract=off`), and WW3's Fortran flags have
  no `-march` (`-O3 -g -fno-second-underscore ...`, `flags.make` of the build), so the x86-64
  baseline has no FMA instruction to contract into; the products and the quotient above stay
  separate statements in both GIMPLE dumps.
- (v) The libm functions the shim's object imports are `cosf`, `logf`, `atan2f`, `sqrtf`, `fmodf`
  and `roundf` (`nm -u`), those of `W3PART`'s translation; their parity with `w3partmd.o`
  (`cosf`, `logf`, `atan2f`, `lroundf`, inline `sqrtss` and `fprem`) is argued in
  [`W3PART_KOKKOS.md`](W3PART_KOKKOS.md) and [`PTMEAN_KOKKOS.md`](PTMEAN_KOKKOS.md).
- (v) `ICPRT` and the `DTPRT` columns are integers and copies: no arithmetic.
- ⚠ On a device `cosf`, `logf`, `atan2f`, `fmodf` and `roundf` are CUDA's, not glibc's (as for
  `W3PART`). The CUDA build of the shim was not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The wall times the runs below print are one run
each on a machine shared with other jobs ⚠ and are not quoted.

## How it was regtested

`VA` traffic, the checks and the commands are in
[`PATCH.md` section 27](../src/fortran_iface/PATCH.md) and
[`docs/log/2026-10-18-cprt-resident.md`](../../docs/log/2026-10-18-cprt-resident.md). In short, on
the CPU (OpenMP backend, gfortran/g++ 16.2.0, i9-14900 shared, never `--gpu`):

- `scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos SRCE,XYP3,KTP3,OUTG,IOPE,UPDV,CPRT` on two
  throwaway variants of `bench_small` with partitioning on, `WW_KOKKOS_VA_POISON=1`: every model
  file and `partition.ww3` byte-identical to the Fortran of the same executable and to `--ww3`
  (v); see the log for the five runs.
- `VA` device-to-host copies in those 12-hour runs (13 output times): 13 before, 1 after, the read
  at `W3WAVE`'s return (v).
- WW3's regtest `ww3_tpt1.1`, the five partitioning methods `PTM = 1..5`, through the matrix
  driver's `regtests/cmake/run_entry.cmake` with `WW3GPU_KOKKOS=CPRT`: `test.comp` found 0 of 22
  files different and `nccmp-tol` passed on each; 2 Kokkos `W3CPRT` calls on 18 sea points per
  run, no fallback (v). The staged switch had `PR3 UQ` for the test's `PR0`, since the fork's
  `KOKKOS` build needs `PR3` ⚠; `ww3_tpt1.1` is not in `matrix.base`, and the one serial
  `matrix.base` entry with partition fields, `ww3_tnc1`, is `PR0` and does not build with
  `KOKKOS`, so the matrix proper has not run this port ⚠.
