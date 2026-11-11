# One run command on MetOcean's WW3, with chaceon, fresh wind and two grid types

2026-11-11, agent session for the owner; no issue. Goal branch
`goal/Z8BeBtzW-4817-single-runtime-command`. The page that describes the result is
[`docs/RUN_COMMAND_202611.md`](../RUN_COMMAND_202611.md); the earlier entry on MetOcean's WW3 is
[2026-10-29](2026-10-29-metocean-ww3-714-mos.md), and its plan is
[2026-10-29-001](../plans/2026-10-29-001-metocean-upstream-plan.md).

## Question

Can one command run a case in six runtimes (`--ww3` on MetOcean's WW3, `--ww3-gpu` on the CPU and
on the GPU, `--chaceon` on the CPU and on the GPU, `--ww4`), take its wind from `just ifs-get`
and its temperature from Copernicus, and do so on a regular grid and on a triangular one? And
which MetOcean WW3 is "7.13"?

## What was done

Machine: a cloud container, 4 cores, x86_64, Ubuntu 24.04, gfortran 13.3.0, CMake 3.28.3,
netCDF-Fortran, Open MPI, NCO, CDO and ecCodes from apt, cargo 1.97.0. `WW3GPU_IN_SHELL=1`, so
the script did not enter `nix develop`: the pinned shell could not be built there, because the
container's network policy refused one source archive of its closure (ParMETIS). No GPU.

- Read the refs of metocean/WW3 and NOAA-EMC/WW3 with `git ls-remote`.
- Added the submodules `WW3-metocean/` (metocean/WW3 `92e2cf66`) and `chaceon/`
  (h0ffmann/chaceon `89a11e4`).
- Applied the fork's diff to MetOcean's tree in a scratch clone:
  `git -C WW3 diff 03adda91 96892c95 > patch.diff`, then `git apply --3way patch.diff` in a clone
  of metocean/WW3 at `92e2cf66`.
- Built MetOcean's WW3 with `switches/switch_lab_shrd`, with `switches/switch_lab_ompg`, and with
  `chaceon/switches/switch_chaceon_shrd` plus `-DCMAKE_Fortran_FLAGS=-ffp-contract=off`.
- Ran, through `scripts/ww3-gpu.sh`: `--ww3 smoke`, `--ww3 full`, `--ww3 coast`,
  `--chaceon smoke`, `--ww3-noaa smoke`, and `--case <dir> --wind ifs` on a triangular grid
  (`ww3_tp2.6` of the MetOcean tree, LIMON) and on a regular one (example 02 with a flat test
  bathymetry), in `--ww3` and, for the regular one, in `--chaceon`.
- Asked the Copernicus Marine Toolbox 2.5.1 to describe the temperature dataset
  (`copernicusmarine describe`, no account needed).

## Result

- metocean/WW3 has no 7.13: tags `7.14`, `7.14.mos-92e2cf6`, `7.14.mos-cfcd089`; branches
  `develop` and `mos-custom` `(v)`. NOAA-EMC/WW3's tags are `6.07` and `6.07.1` `(v)`. The
  submodule is pinned to `7.14.mos-92e2cf6`, the owner's choice of 2026-10-29; "7.13" is asked ⚠.
- The fork does not rebase mechanically: 23 conflict hunks in four files, `w3srcemd.F90` 8,
  `w3wavemd.F90` 13, `w3initmd.F90` 1, `w3iotrmd.F90` 1 `(v)` `grep -c '^<<<<<<<'` after the
  three-way apply. So plan unit U2 was done as an added submodule, not as a replacement of
  `WW3-noaa/`, and the fork's pair is the new mode `--ww3-noaa`.
- MetOcean's WW3 at `92e2cf66` `(v)`:
  - builds with `switch_lab_shrd`; `smoke` and `full` reach "End of program" (`full`: 4 min 27 s
    of `ww3_shel`, one run, serial);
  - does not build with `OMPG`: `model/src/w3iogomd.F90:2380`, "Unexpected !$OMP PARALLEL DO
    statement", the directive of `:2374` opened again;
  - its `ww3_bounc` stops with a segmentation fault at `model/src/ww3_bounc.F90:556` on the
    `coast` fixture's boundary spectra; cause not found ⚠;
  - silently ignores the output fields `SKW`, `EMB`, `EMC` that the fork's 2026 `ww3_ts1`
    namelist asks for.
- chaceon on `smoke`, against MetOcean's WW3 built with chaceon's switch and contraction off, on
  x86_64 `(v)`: `mod_def.ww3` (28 497 841 bytes) and `restart.ww3` (3 456 bytes) byte-identical;
  `out_grd.ww3` different in 156 of 47 762 bytes; `out_pnt.ww3` in 1 134 608 of 7 652 218. With
  the lab switch and default flags, `mod_def.ww3` differed in size, which is why the reference is
  built chaceon's way. On the regular grid with IFS wind, 3 h: `mod_def.ww3` (31 031 761 bytes)
  byte-identical, `out_grd.ww3` different in 12 917 of 2 805 674 bytes, `out_pnt.ww3` in 2 314 of
  93 913. chaceon claims bit for bit on linux/arm64, so this is counted, not judged.
- `ww3_prnc` read the file `just ifs-get` writes `(v)`, IFS 2026-11-10 00z:
  - triangular grid (1 778 nodes): 8 × 8 input grid after the box cut, accepted with the
    latitudes descending; `ww3_shel` ran 1 h in 321 s (`DTMAX` of the regtest is 1 s);
  - regular grid: refused, "LATITUDE VALUES MUST BE REVERSED" (`model/src/ww3_prnc.F90:942`).
    `scripts/ifs_get.sh` now writes the latitudes south to north (`ncpdq -a -latitude`). With
    that, `ww3_prnc` read a 41 × 41 input grid at two times, `ww3_shel` ran 3 h in 180 s, the
    model's `uwnd` at 00z has the minimum of the file's `u10` (-9.2255 m s⁻¹), and `hs` grows
    from rest to a maximum of 0.80 m on the flat test bathymetry.
- Examples 01 and 02 carried `FIELD%TIMESTOP` and `POINT%TIMESTOP`, keys neither WW3 pin has
  (`model/src/w3nmlounfmd.F90:27`); `ww3_ounf` stops on them. They now use `TIMECOUNT`.
- Copernicus `(v)`: product `GLOBAL_ANALYSISFORECAST_PHY_001_024`, dataset
  `cmems_mod_glo_phy-thetao_anfc_0.083deg_PT6H-i`, variable `thetao`
  (`sea_water_potential_temperature`, °C), 6-hourly, 1/12°, times to 2026-11-21. The download
  itself was not run ⚠ (no account in the container).
- `--ww3-gpu` was not built ⚠: the fork needs Kokkos from the pinned toolchain. Its arguments are
  covered by `bash scripts/ww3-gpu.sh --self-test`; CI's h1 runs it.

## What it changes

- `--ww3` is MetOcean's WW3 from now on, and what earlier entries call `--ww3` is `--ww3-noaa`.
- The open question 1 of the plan (the name of the reference submodule) has a provisional answer,
  `WW3-metocean/`, which the owner can change; units U1 and U3 to U5 are open, and the 23 hunks
  are their first measured size.
- Next: the fork on `92e2cf66` (U1), then `--ww3` as the pair of `--ww3-gpu`; MetOcean's WW3 in
  the Docker images; the temperature as forcing if the owner wants `STAB3`; a run of
  `just temp` with an account; the cause of the `ww3_bounc` fault.
