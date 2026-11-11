# GFS-Wave's switch against the batched W3SRCE gate; one ufs case run and profiled on one rank

2026-10-19, Claude (agent), goal be5bdbea-3817, plan
[`2026-10-19-007-gfs-wave-target-plan.md`](../plans/2026-10-19-007-gfs-wave-target-plan.md)
(U1, U2, U3). Regtest pending ⚠ (ADR-0016): the runs below are diagnostics of this machine, not a
validation verdict, and no `regtested` cell of `kokkos/ports.tsv` moved.

## Question

Which words of the GFS-Wave regtests' switch make the batched Kokkos `W3SRCE` refuse, does the
Fortran of the routines the port covers still equal the one the ports were checked against, can a
GFS-Wave case run on this node with and without Kokkos, and where does its time go?

## U1: the switch words (v)

Read with `cat` and `comm` at `WW3@aa382393` (v7.14, `git -C WW3 rev-parse HEAD`).

`regtests/ww3_ufs1.1/input/switch_MPI`:
`NCO NOGRB DIST MPI SCRIP SCRIPNC WRST PR3 UQ FLX0 SEED ST4 STAB0 NL1 BT1 DB1 MLIM FLD2 TR0 BS0 RWND WNX1 WNT1 CRX1 CRT1 O0 O1 O2 O3 O4 O5 O6 O7 O14 O15 IC0 IS0 REF0`.
`ww3_ufs1.1/input/switch_MPI_OMPH`, `ww3_ufs1.2/input/switch_MPI_OMPH` and
`ww3_ufs1.3/input/switch_MPI_OMPH` carry the same words plus `OMPG OMPH` (`diff` of the sorted
word lists: ufs1.2 against ufs1.1's `switch_MPI` adds exactly those two; ufs1.1's `switch_MPI_OMPH`
and ufs1.3's are identical).

| Against | Only in the ufs switch | Only in the other |
|---|---|---|
| `switches/switch_lab_shrd` | `DIST FLD2 MPI NCO O15 SCRIP SCRIPNC SEED WRST` | `F90 LN1 LRB4 NC4 NOPA SHRD XX0` |
| `switches/switch_lab_mpi` | `FLD2 NCO O15 SCRIP SCRIPNC SEED WRST` | `F90 LN1 LRB4 NC4 NOPA XX0` |

The gate for the batched `W3SRCE` is the `#if` chain at `model/src/w3kokkosmd.F90:1840-1846` (the plan quotes 1838-1845, a few lines off); it
sets `IS_LAB_SWITCH` (`:1847`) only when `BT1 DB1 LN1 MLIM` are all defined and none of the words
the chain lists is. Going through every list of the chain against the ufs switch, exactly three
words make it refuse:

1. `LN1` is absent (the chain requires it; the ufs switch has `SEED` in its place).
2. `FLD2` is present (listed at `:1842`).
3. `SEED` is present (listed at `:1845`).

No other word of the ufs switch is in a forbidden list (`DIST`, `MPI`, `NCO`, `SCRIP`, `WRST`,
`O15` are not tested by the chain; `STAB0`, `IC0`, `IS0`, `REF0`, `TR0`, `BS0`, `FLX0` are the
accepted members of their families). `DIST MPI` is not a reason for this gate; it matters for
`XYP3`, `OUTG`, `IOPE` and `UPDV`, which need `SHRD` (ADR-0017). With `WW_KOKKOS_ALL=1` the gate's
refusal is a counted fallback (`:1857-1864`); with `WW_KOKKOS_SRCE=1` it stops the model in
`WW_SRCE_INIT`, which a first run showed (below).

### Rows of `kokkos/ports.tsv` stamped with an old pin

The file's header stamps `ww3_rev 03adda91`; `docs/data/ww3_full_gprof.tsv` is stamped `6d1bcdfe`.
No row cell was edited. For the routines the GFS case calls, the Fortran file at the new pin
against each old one (`git -C WW3 diff --numstat <old> HEAD -- model/src/<file>.F90`):

| File (routines) | against `03adda91` | against `6d1bcdfe` |
|---|---|---|
| `w3srcemd.F90` (`W3SRCE`) | +456, -0 | +431, -4 |
| `w3src4md.F90` (`W3SIN4`, `W3SDS4`, `W3SPR4`), `w3snl1md.F90` (`W3SNL1`), `w3pro3md.F90` (`W3XYP3`, `W3KTP3`), `w3sbt1md.F90` (`W3SBT1`), `w3sdb1md.F90` (`W3SDB1`), `w3sln1md.F90`, `w3uqckmd.F90` | no output (identical) | no output (identical) |

(v) Eight of the nine routines' files are byte-identical at the pin to both old stamps. In
`w3srcemd.F90` against `03adda91` nothing is deleted; the added lines are the fork's `W3_KOKKOS`
hooks. Against `6d1bcdfe` four lines are gone: the `W3_KOKKOS` block that called `WW_SNL1`, which
is elsewhere in the fork now (not traced ⚠). That every added line sits inside a `W3_KOKKOS` guard was checked on
a sample of the added lines that do not name Kokkos (preprocessor `#endif`, `USE` lines and the
`ELSE`/`W3SPR0`/`W3SPR1` branches, all inside the `#ifdef` blocks of the hooks), not line by line
⚠. The stamps are therefore still true for the Fortran statements the ports translate; moving
the stamp to `aa382393` is a decision for `port_score.py --measure` and the owner.

## U2: the case, one rank

The cheapest ufs configuration is `ww3_ufs1.1` with `grdset_c`: the 72 x 35 global grid at 5
degrees (`input/ww3_grid_glo_5deg.nml`), 18 frequencies and 12 directions, the points grid, 24 h,
`ww3_multi`. `matrix.base` lists it as entry 323 (`-s MPI -w work_c -m grdset_c`) with the
regtest's own switch.

**The forcing is on this machine.** `gfs_wind.nc`, `ice.nc` and `current.nc` of the case are in
`data_regtests/ww3_ufs1.1/input/` of WW3's ftp archive, which pratico packages as
`ww3-regtest-data` (`nix build --no-link --print-out-paths picasso/labs/pratico#ww3-regtest-data`
gave `/nix/store/97lk2yrkjdjxaniq48mq2p6ciw9y79js-ww3-regtest-data-7.14.2`; sizes 207 658 056,
8 315 616 and 74 763 220 bytes). It is fetched by WW3's own `model/bin/ww3_from_ftp.sh`
(`https://ftp.emc.ncep.noaa.gov/static_files/public/WW3/ww3_from_ftp.<version>.tar.gz`), pinned by
the SHA-256 in pratico's flake. Nothing was downloaded or committed here.

**`scripts/ww3-gpu.sh` was not changed.** Its three cases (`smoke`, `full`, `coast`) run `ww3_shel`
on one grid with a `switches/switch_lab_*` file; a ufs case needs `ww3_multi` under `mpirun`, two
`ww3_grid` runs, three `ww3_prnc` runs, renamed `mod_def.*` files and the regtest's own switch.
That is what `regtests/` (WFIP-0003) already does for the entry, one Fortran run and one Kokkos run
of the same executable, compared by `test.comp` and `nccmp-tol`, so the matrix driver is the way to
run it and no script gained a case. The pair to pair with `--ww3` (WW3-noaa without the KOKKOS
switch) is not made by the matrix and was not run ⚠.

Commands (machine: Intel Core i9-14900, 32 threads, `OMP_NUM_THREADS=1`, 1 rank; ww3-gpu
`04149a9`, `WW3@aa382393`, gfortran 16.2.0, Open MPI 5.0.11 from the pinned shell):

1. `WW3GPU_KOKKOS=SRCE WW3GPU_MPI_RANKS=1 WW3GPU_OMP_THREADS=1 just ww3-gpu-matrix 1 mpi -R '^323:'`.
   The Fortran run finished (`work_F_323`, `End of program`). The Kokkos run stopped in `W3INIT`:
   `*** WAVEWATCH III ERROR IN W3KOKKOSMD : KOKKOS WW_SRCE_INIT FAILED, CODE 5`. This is the
   named `SRCE` part asked for on a switch the kernel does not translate, as
   the comment at `w3kokkosmd.F90:1855-1856` says; a refusal by design, not a defect.
2. `WW3GPU_KOKKOS=all WW3GPU_MPI_RANKS=1 WW3GPU_OMP_THREADS=1 just ww3-gpu-matrix 1 mpi -R '^323:'`
   (`WW_KOKKOS_ALL=1`, no `WW_KOKKOS_STRICT`). Result row (`summary.tsv`): built yes, ran yes,
   `kokkos_log` yes, `test.comp` 0 of 30 files differ, `nccmp-tol` PASS, judged fields
   `dir,dp,fp,hs`, `ww3_self` none needed. The Kokkos run printed

   `Kokkos parts (K Kokkos, F Fortran): SNL1=K SDS4=K SIN4=K SPR4=K ST4_TABLES=F SRCE=F UWND=K UTRN=K DZXY=K MAP3=K MAPT=K DISTAB=K CPRT=F OUTG=F IOPE=F XYP3=F KTP3=F UPDV=F (10 of 18 on Kokkos)`

   and, at exit, `calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead): GATH=1 SCAT=1 SRCE=1`.

**Fallback count.** `SRCE=1`, `GATH=1`, `SCAT=1`: each is counted once, when the part is set up
(`KOKKOS_FALLBACK`, `w3kokkosmd.F90:1860`, called from the setup, not per `W3SRCE` call). So the
batched `W3SRCE` fell back as a whole, as the gate predicts, but the count says nothing about
how many source-term calls ran in Fortran. The per-routine counters give that: `W3SNL1 spectra
computed: 47725`, `W3SIN4: 128736`, `W3SDS4: 47725`, `W3SPR4: 114297`.

**This corrects the plan's gap section** ("every source-term call of a GFS-Wave run is the
Fortran today"). Inside the Fortran `W3SRCE`, the per-point kernels for `SNL1`, `SIN4`, `SDS4`
and `SPR4` already run on Kokkos under the ufs switch and give the Fortran's bits on this case
(0 of 30 files differ). What is Fortran is the batched `W3SRCE` team body, `W3FLD2`, `W3QCK1`,
`W3KTP3`, `W3XYP3` and the rest of the list below. Whether a batched `W3SRCE` for `SEED`, `FLD2`
and no `LN1` pays beyond the per-point kernels is the question U4 has to answer with a timing.

One run each, 1 rank, 1 thread, `ww3_multi`'s own `Elapsed time`: Fortran 3.31 s, Kokkos on
OpenMP 4.64 s. ⚠ A single run each on a shared machine; not a benchmark (median of three after the
regtest), and a 2520-point grid is a launch-overhead case, not the operational one. The build
directory was `build/ww3-gpu-matrix-mpi-kokkos-all/` (not committed).

## U3: the profile

`just profile-case gfs` (new case of `scripts/profile_case.sh`): builds WW3 as written with
`-pg` and the regtest's `switch_MPI`, no KOKKOS switch, runs `ww3_multi` under `mpirun -np 1`,
`OMP_NUM_THREADS=1`, ten times (one run is 3 s, 330 samples of gprof's 10 ms tick) and sums the
`gmon` files with `gprof -s`. Written to
[`docs/data/ww3_gfs_gprof.tsv`](../data/ww3_gfs_gprof.tsv): 48.8 s of wall time summed over ten
runs, 25.2 s of it in sampled routines (the rest is the C library, MPI and NetCDF, which gprof
does not see). One rank: no halo exchange or any other communication cost. `port_score.py
--write` and `--check` both pass; the new profile adds rows to `kokkos/PORT_TABLE.tsv`.

| Rank | Routine | Self share | State in `ports.tsv` | On Kokkos in the run above |
|---|---|---|---|---|
| 1 | `w3snl1md:w3snl1` | 24.0 % | L2 | yes |
| 2 | `w3uqckmd:w3qck1` | 12.3 % | ported, inside `KTP3` | no, `KTP3=F` (opt-in, not under `ALL`) |
| 3 | `w3fld2md:w3fld2` | 11.8 % | **not ported** | no |
| 4 | `w3pro3md:w3xyp3` | 9.2 % | ported | no, `XYP3` needs `SHRD` (ADR-0017) |
| 5 | `w3src4md:w3sds4` | 8.0 % | ported | yes |
| 6 | `w3src4md:w3sin4` | 6.5 % | ported | yes |
| 7 | `w3pro3md:w3ktp3` | 6.5 % | ported | no, opt-in |
| 8 | `w3uqckmd:w3qck3` | 5.5 % | ported | no (`XYP3`) |
| 9 | `w3srcemd:w3srce` | 2.9 % | ported | no, the gate refuses |
| 10 | `w3src4md:w3spr4` | 2.6 % | ported | yes |

Not ported and above 1 %: `w3fld2md:w3fld2` (11.8 %) and `init` (1.7 %, WW3 start-up). Everything
else in the top ten has a Kokkos translation; the ones that did not run on Kokkos above are held
back by a switch of the run (`SHRD`, an opt-in flag) or by the `SRCE` gate. The cheapest next
steps this profile points to, each a plan unit and not done here: (a) the `SRCE` gate for `SEED`
and `FLD2` without `LN1` (U4, which needs `W3FLD2` translated or called from the team body, since
`FLD2` is in the switch); (b) `W3FLD2` as a port (ADR-0002 ranks it third in this profile, first
among the routines with no Kokkos code); (c) `KTP3` on in the preset for an MPI switch (it covers
`PR3 UQ` under MPI, `docs/log/2026-10-18-resident-mpi-lab-switch.md`).

⚠ The profile is the 5 degree grid with 18 x 12 components. The operational grids have 50 x 36
(GFS-Wave) and far more points; the shares move with the spectrum and grid size, since `W3SNL1`
and `W3FLD2` scale with the spectrum, propagation with the points. A 1 degree profile (`grdset_a`,
`glo_1deg`, 50 x 36) was not made: one rank would take hours (estimate from the point and
spectrum counts, not measured ⚠).

## What stays open

- The `ww3_ufs1.2` and `ww3_ufs1.3` grid sets were not run; their inputs are in the same data
  package.
- The pair to pair with `--ww3` for this case, and the ranks and threads of the operational
  layout (`OMPH`, several ranks), were not run.
- `ports.tsv`'s `ww3_rev` and the profile stamps keep their old values.

## Addendum: ufs1.2 (the GFSv16 three-grid case), measured and stopped

2026-10-19, same goal. The owner asked for `ww3_ufs1.2` (`aoc_9km`, `gnh_10m`, `gsh_15m`, 50 x 36
spectrum, 24 h) as the GFS case in addition to `ww3_ufs1.1` `grdset_c`.

**Forcing.** `~/src/ww3-data` does not exist on this machine. The forcing and grids are in the
pinned regtest data used above, `/nix/store/97lk2yrkjdjxaniq48mq2p6ciw9y79js-ww3-regtest-data-7.14.2`:
`gfs_wind.nc`, `ice.nc` and `current.nc` under `data_regtests/ww3_ufs1.1/input/` (which
`model/bin/ww3_from_ftp.sh` copies into `ww3_ufs1.2/input/`), the `*.bot`, `*.mask`, `*.obs` files and
the two `rmp_src_to_dst_conserv_*.nc` remap files under `data_regtests/ww3_ufs1.2/input/`. Nothing
was downloaded.

**Entry.** `matrix.base:2229-2234` gives the base entry `-s MPI_OMPH -w work_a -m grdset_a`,
entry 338 in the `mpi` selection (339 is the restart pair `grdset_b`).

**Command.** `WW3GPU_KOKKOS=all WW3GPU_MPI_RANKS=16 WW3GPU_OMP_THREADS=2 just ww3-gpu-matrix 1 mpi -R '^338:'`
on the i9-14900 (32 threads), `WW3@aa382393`, ww3-gpu `eafc912`. The build and the setup finished
and the Fortran run reached its first half-hour step; I stopped it by hand before the pair
completed, so there is no `test.comp`, `nccmp-tol` or fallback count for ufs1.2 ⚠.

**Why stopped.** From `ww3_multi.out`: initialisation reached 2021-04-01 00:00 at 19:42:18 and
the 00:30 step at 19:44:06, 108 s for the first half hour (it includes start-up). If the other 47
half hours cost the same, one Fortran run takes about 85 min and the Fortran/Kokkos pair about
3 h ⚠ (a two-point extrapolation, load average 70 while two jobs shared the node, so the
real figure may be lower). Each of the 16 ranks held about 12.4 GB resident (`ps` RSS of three
ranks), roughly 200 GB for 16 ranks against 184 GB of RAM ⚠ (three ranks sampled), so a 16 rank
run is also near the memory limit of this node. A single-rank gprof run would be several times
slower than 16 ranks and needs the same 12 GB per rank, so `docs/data/ww3_gfs12_gprof.tsv` was not
made and `profile_case.sh` has no `gfs12` case.

**Ranking against ufs1.1.** Not measured, so the question is open. What can be said from the
code: `W3SNL1`, `W3SIN4`, `W3SDS4` and `W3SPR4` scale with `NK x NTH` (900 here, 216 in ufs1.1),
`W3FLD2` with the points and the wind step, and propagation with the points and the number
of components, so a 50 x 36 spectrum should raise the source-term share of ufs1.1's table
above ⚠ (a reading of the loops, not a profile).

**To finish.** Fewer ranks and a shorter stop time (`DOMAIN%STOP` of `ww3_multi_grdset_a.inp`) with
the memory per rank checked first; the owner's call on the time budget.
