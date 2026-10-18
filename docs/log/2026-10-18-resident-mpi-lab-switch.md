# The resident preset under MPI: refused by design; SRCE and KTP3 alone run, one entry differs

2026-10-18, an agent for the owner. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
plan [`2026-10-18-001-cpu-resident-validation-plan.md`](../compound/plans/2026-10-18-001-cpu-resident-validation-plan.md)
(U4a), [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md). The same day the owner put MPI
out of scope for the Kokkos port (single node: `SHRD` with OpenMP and one GPU; an MPI build keeps
the Fortran path; an ADR will record it). This entry is the evidence behind that scope.

## Question

The resident preset (`--kokkos resident`: `SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV`) is
byte-identical on the CPU in smoke and full, but every run so far used a `SHRD` switch. NOAA's
operational GFS-Wave switches (`WW3/model/bin/switch_NCEP_st4`, `switch_NCEP_st4sbs`) are
`DIST MPI`. What does the preset do under MPI, with the lab switch's source terms
(`WW3GPU_LAB_SWITCH=ON`)?

## What was done

All on the owner's i9-14900 (32 hardware threads), ww3-gpu `db2c27c`, `WW3/` at `80644ba9`, in
the pinned `ww3` shell, at most 2 matrix jobs (another matrix shared the machine). The two
summaries, each with its header, are in
[`ww3gpu_matrix_mpi_resident_lab_2026-10-18.tsv`](../data/ww3gpu_matrix_mpi_resident_lab_2026-10-18.tsv).

1. `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix-list mpi`: 43 entries.
2. A sample of four, chosen to cover different tests, not exhaustive:
   `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON WW3GPU_OMP_THREADS=8 just ww3-gpu-matrix 2 mpi -R '^(001|007|008|011):'`
   (`ww3_tp2.15` MPI, `ww3_ts3` `ST4_PR3_UQ_MPI`, `mww3_test_05` `ST4_PR3_UQ_MPI`, `ww3_ts3`
   `ST4_PR3_UQ_MPI_OMPH`).
3. The parts the code allows under MPI (read below), on entries without the moving-grid words
   `MGW MGP MGG`:
   `WW3GPU_KOKKOS=SRCE,KTP3 WW3GPU_LAB_SWITCH=ON WW3GPU_SWITCH_WORDS='ST4 NL1 PR3 UQ !PDLIB' WW3GPU_OMP_THREADS=8 just ww3-gpu-matrix 2 mpi -R '^(001|003|004|023):'`
   (`ww3_tp2.15` `PR3_UQ_MPI` and `PR3_UQ_RHO_MPI`, `mww3_test_08` `ST4_PR3_UQ_MPI`, `ww3_ufs1.1`
   MPI). A first try without `WW3GPU_SWITCH_WORDS` selected other entries (`ww3_tp2.2`,
   `ww3_tp2.3`, PR1, built "no") because a part list selects "any physics and MPI"; its summary
   was overwritten by this run and is not recorded.
4. `switch_NCEP_st4` against `switches/switch_lab_shrd`, word by word, and a CMake build of the
   fork with `switch_NCEP_st4` plus `KOKKOS` (`cmake -S WW3 -DSWITCH=<switch> -DWW_KOKKOS=ON
   -DCMAKE_PREFIX_PATH=kokkos/build/openmp-release/install`, `cmake --build -j 2`, in
   `build/ncep_st4_kokkos/`).

## Result

**Which resident parts refuse MPI, by design** (v), read at `WW3/` `80644ba9`; with
`WW_KOKKOS_<PART>=1` on a build a part does not cover, `KOKKOS_PART` stops the model
(`model/src/w3kokkosmd.F90:1188-1203`):

| Part | Covers MPI | Where |
|---|---|---|
| `SRCE` | yes, on the lab source-term set | `model/src/w3kokkosmd.F90:1770-1777` (no `MPI` or `DIST` in the forbidden list; `SEED`, `FLD1`, `FLD2` are in it, lines 1772 and 1775) |
| `KTP3` | yes, `PR3` `UQ`, rectilinear or curvilinear, no `PDLIB`, `MGP`, `MGG`, `REFRX` | `model/src/w3kokkos_prop3md.F90:122-133` |
| `XYP3` | no: needs `SHRD` | `model/src/w3kokkos_prop3md.F90:127-129` |
| `OUTG` | no: needs `SHRD`, refuses `MPI` | `model/src/w3kokkos_outgmd.F90:61-66` |
| `IOPE` | no: needs `SHRD`, refuses `MPI` | `model/src/w3kokkos_pntmd.F90:49-54` |
| `UPDV` | no: needs `SHRD`, refuses `MPI` and `PDLIB` | `model/src/w3kokkos_updvmd.F90:57-62` |

**The resident preset on four MPI entries** (v, summary rows 1, 7, 8, 11): every entry built;
every Kokkos run stopped in `W3INIT` with `WW_KOKKOS_OUTG=1, BUT ITS KOKKOS TRANSLATION DOES NOT
COVER THIS SWITCH FILE` (read in each `work_K_<n>/*.out`); `ran` is fortran-only, `test.comp`
n/a. A refusal by design, not a port defect: `OUTG` is the first preset part whose setup runs.

**`SRCE,KTP3` on four MPI entries** (v, second summary; the Kokkos runs printed `SRCE=K` and
`KTP3=K` in their parts line):

| Entry | test.comp (Fortran/Kokkos) | ww3_self | nccmp-tol | What differs |
|---|---|---|---|---|
| 1 `ww3_tp2.15` `PR3_UQ_MPI` | 1/24 | 0/24 | PASS | only `fort.20`, the run's log: its first line is the Kokkos parts line itself (`diff`, 5 lines); every other file identical |
| 3 `ww3_tp2.15` `PR3_UQ_RHO_MPI` | 1/26 | 0/26 | PASS | the same: `fort.20` only |
| 4 `mww3_test_08` `ST4_PR3_UQ_MPI` (`ww3_multi`) | 6/24 | 0/24 | FAIL | `fort.50-52`, `out_grd.inner`, `out_pnt.inner`, `ww3_ounf_inner/ww3.201510.nc`: `hs` max \|d\| 1.069e-02 m, `dir` up to 360° (a wrap), on the inner grid only |
| 23 `ww3_ufs1.1` MPI | n/a | n/a | n/a | built "no": see below |

- ⚠ Entries 1 and 3: the model output is identical; the strict verdict fails only on the log
  line that names the Kokkos parts. Whether `test.comp` should read `fort.20` is the driver's
  question for the owner, not changed here.
- ⚠ Entry 4 is a real difference of the port under `ww3_multi` with MPI: two Fortran runs give
  the same bits, Fortran and Kokkos do not. Which of `SRCE` or `KTP3` causes it was not
  isolated (U4a step 2), because the owner stopped MPI runs the same day. `ww3_multi`'s one
  context per grid is a branch the plan lists as never run (U4a 4b).
- (v) Entry 23: the switch `WW3GPU_LAB_SWITCH` writes for `ww3_ufs1.1` is invalid. The entry's
  switch has `SEED` as its linear-input word; the rewrite keeps it and adds the lab's `LN1`,
  and WW3's CMake needs exactly one word of category `s_ln`, whose options are `LN0`, `SEED`,
  `LN1` (`WW3/model/src/cmake/switches.json:210-222`, the check at
  `WW3/model/src/cmake/check_switches.cmake:93-95`). The rewrite's pattern
  (`regtests/cmake/ww3gpu_common.cmake:193`) does not list `SEED`, nor `FLD`. Every
  `ww3_ufs1.x` entry has `SEED` and `FLD2`, so even with the pattern fixed, `FLD2` would make
  `SRCE` refuse (`model/src/w3kokkosmd.F90:1772`). Not fixed here.

**`switch_NCEP_st4` against `switch_lab_shrd`** (v, `cat` of both):

- Only in `switch_NCEP_st4`: `NCO DIST MPI` (and `SBS` in `switch_NCEP_st4sbs`).
- Only in `switch_lab_shrd`: `F90 NOPA LRB4 NC4 SHRD XX0 RWND O3 O14`.
- The source-term words are the same set (`FLX0 LN1 ST4 STAB0 NL1 BT1 DB1 MLIM TR0 BS0 IC0 IS0
  REF0`), so the `SRCE` gate accepts it (v, by reading `model/src/w3kokkosmd.F90:1770-1777`).
- Words that change the Kokkos path: `DIST MPI` without `SHRD` turn off `XYP3`, `OUTG`, `IOPE`,
  `UPDV` (table above) and the `W3GATH`/`W3SCAT` translation (`W3KOKKOS_HAS_GATH` only under
  `W3_SHRD`, `model/src/w3kokkosmd.F90:37-39`). `RWND` absent sets `W3KOKKOS_UWND`'s
  relative-wind factor and current flag to 0 (`model/src/w3kokkosmd.F90:3101-3127`), a branch
  the lab runs have not taken. `NCO` changes file handling, not a Kokkos call that was found
  ⚠. `O3`, `O14` absent remove outputs the lab switch writes ⚠ (not traced). `SBS` only loops
  the driver over input times ⚠.
- (v) WW3 at `80644ba9` builds and installs with `switch_NCEP_st4` plus `KOKKOS` (exit 0; without
  `KOKKOS` in the switch, `-DWW_KOKKOS=ON` stops at `model/src/CMakeLists.txt:27`, as designed).
- ⚠ The parts line of that build was not obtained: `ww3_grid` on `ww3_ts1`'s inputs exited 2
  after "Preprocessing namelists ..." with no message (`build/ncep_st4_kokkos/work_srce_ktp3/`),
  and the cause was not found. From the gates above, a `switch_NCEP_st4` run with the
  resident preset would stop on `OUTG`, and with `SRCE,KTP3` would show at most `SRCE=K KTP3=K`.

No wall time is quoted: the machine was shared, and no run here passed.

## What it changes

- MPI is out of the port's scope (the owner, 2026-10-18): the resident preset is a `SHRD`
  preset, and an operational `DIST MPI` switch such as `switch_NCEP_st4` keeps the Fortran for
  four of its six parts by construction.
- Open for whoever revisits MPI: entry 4's difference (`mww3_test_08`, `SRCE,KTP3`); the
  `WW3GPU_LAB_SWITCH` pattern, which writes an invalid switch for a `SEED` entry; whether
  `test.comp`'s comparison of `fort.20` should count a log line.
