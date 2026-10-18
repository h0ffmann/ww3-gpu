# Five bit-neutral speed-ups of the resident preset: three built, two checked, one Fortran change tested

2026-10-18, an agent for the owner. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md); the study it feeds is
[`NOAA_PROGRESS_RECORD_202610.md`](../NOAA_PROGRESS_RECORD_202610.md).

## Question

The profile of the same day ([log](2026-10-18-resident-profile.md)) ranks the DIA (`W3SNL1`)
first and `W3SDS4` second inside the batched `W3SRCE` kernel. Which changes make these kernels
faster without a change of one output bit, and does the `W3SDS4` change also hold in the Fortran?

## What was done

On the owner's i9-14900 (32 hardware threads), ww3-gpu from `5b48e08`, `WW3/` at `80644ba9`,
`WW3-noaa/` at `03adda91`, the pinned `ww3` shell (gfortran and g++ 16.2.0, Kokkos 5.2.2 OpenMP).
**The machine was shared** with other sessions for the whole day; the 1-minute load average went
from 5 to 57.

1. **`-fno-math-errno`** on `ww_kokkos` (host compilers only), gfortran's default.
2. **`W3SDS4` saturation integral** (`st4_sds4.hpp`, from `w3src4md.F90:2281-2285`): `J` outer,
   `ITH` inner, each `BTH(IS)` still summed from 0 in the order `J = 1..NSAT`; transposed copies
   `satweights_t`, `satindices_t` (direction index fastest) built in `st4_shim.cpp`'s `upload()`.
3. **DIA halo layout** (`snl1_team.hpp`, `snl1_tables.{hpp,cpp}`): `make_tables` writes each
   index table as a frequency-row map and a direction shift (`w3snl1md.F90:667-727`) and checks
   every entry against them; the host kernel copies `UE`, `SA*` and `DA*` into rows padded with
   the wrapped neighbour directions, so sections 3 and 4 read contiguous memory. The statements
   are those of the gather form. CUDA builds keep the gathers.
4. **Thread placement** and **`W3XYP3` load balance**: read, not changed (below).
5. **The Fortran `W3SDS4` swap**: `git archive` of `WW3-noaa` at `03adda91` into a scratch
   directory, the loop swap and one `INTEGER :: JSAT`, built with `build/ww3/switch` and the same
   CMake flags as `--ww3`. Nothing was written to `WW3/` or `WW3-noaa/`.

## Result

**Bits.** (v) Every Kokkos run of the day, about 40 on `full` at 1 and 32 threads, wrote an
`out_grd.ww3` that `cmp` finds identical to the Fortran's. With the final code:

- `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident`: every model file
  byte-identical to the Fortran of the same executable; `nccmp-tol` PASS against `--ww3`,
  `max|d| = 0` on `hs`; every file but the NetCDF byte-identical to `--ww3`'s.
- The same on `smoke`: `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` and the NetCDF byte-identical
  to the Fortran; `nccmp-tol` PASS against `--ww3`.
- `just kokkos-test serial-debug` and `just kokkos-test openmp-release`: 11 of 11 tests each.
- g++ `-fopt-info-vec` on `srce_shim.cpp`: the halo loops of sections 3 and 4 and the new
  `W3SDS4` loop are vectorised with 32-byte vectors.

**The DIA kernel alone**, `kokkos/tests/bench_snl1.cpp` (1 000 points, `NK=25 NTH=24`, 20 calls),
`main` against this branch, alternated, seven runs each, load 8.4 to 8.9:

| Threads | `main` median (ms) | this branch median (ms) | Ratio |
|---|---|---|---|
| 1 | 12.31 | 6.39 | 1.93x |
| 32 | 2.50 | 2.11 | 1.19x |

`main` was built from `git archive origin/main kokkos` with `-DWW_ENABLE_FORTRAN=OFF`; command:
`OMP_PROC_BIND=false OMP_NUM_THREADS=<t> kokkos/build/openmp-release/tests/ww_bench_snl1`.

**The `W3SDS4` loop alone** (a scratch program, not kept: `NK=24 NTH=24`, 23 weights per
direction, best of 7 x 20 000 calls, same bits checked with `memcmp`): 5.46 to 5.72 µs per point
for the original order, 5.13 µs with `ITH` inner on the original tables (stride 23), 3.03 to
3.32 µs with the transposed tables. The strided form alone gains 7 %; the tables are the gain.

**Inside the model.** One executable with a temporary switch that disabled the halo (removed
before the commit), 32 threads, three alternated pairs, load 31 to 57: the `W3SRCE` kernel took
19.17 / 20.68 s, 18.85 / 20.22 s and 18.06 / 21.37 s with / without the halo ⚠. Whole `ww3_shel`,
`main` against this branch, three alternated runs, load 8 to 38: medians 17.67 s and 17.09 s,
the Fortran of the same executable 29.17 s ⚠. The difference between `main` and this branch is
inside the spread of one configuration (14.4 to 18.8 s for `main`), so the end-to-end gain is
not measured.

**The Fortran swap.** (v) gfortran's `-O3` optimised tree of both versions (`-fdump-tree-optimized`,
the build's flags): the original starts `val = 0.0`, adds `weight*A` in index order and returns
`facsat*val`; the swapped loop starts `BTH = 0.0`, adds the same products in the same order and
multiplies by `facsat`. (v) `full`, 32 threads and twice at 1 thread: `out_grd.ww3` identical to
the unmodified build's. (v) A positive control (`SRHS = SRHS * 1.0001` before the
whitecap-output branch of `W3SDS4`) changes `out_grd.ww3` at byte 267 482, so the comparison sees
`W3SDS4`. The first control, placed before the routine's last `RETURN`, did not: the common path
returns earlier, at the whitecap test. Timing ⚠: CPU time at 1 thread was 296.4 and 280.8 s
unmodified, 295.8 and 196.2 s swapped; the load explains more than the change.

**Not changed.**

- Thread placement: `scripts/ww3-gpu.sh` already passes `OMP_PROC_BIND` and `OMP_PLACES` from
  the environment to both runs (`:587`, `:713`). No measurement on a shared machine can rank
  `spread` against `false`.
- `W3XYP3`: the component list is in ascending `ISPEC`, so the lowest frequencies, with the most
  CFL sub-steps, start first. Kokkos' automatic chunk for 576 items on 32 threads is 1 ⚠ (read
  from Kokkos' rule, not printed). There was nothing bit-neutral left to reorder.

## What it changes

- The DIA halo is the largest gain found: about half the cost of the DIA at one thread.
- The `W3SDS4` gain needs the transposed tables. In C++ the swap on the original tables gained
  7 %; whether gfortran's scalar code of the swapped loop gains more, by breaking the add chain,
  was not measured ⚠. A Fortran proposal should carry the transposed tables too.
- An end-to-end median of three on an idle machine is still needed before a number enters
  `bench/results/`. Command: `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident
  --repeat 3`, on `main` and on this branch.
