# `W3XYP3` under `MPI` with the aggregated exchange, first CPU runs

2026-11-12, an agent for the owner; plan unit M2 of
[`2026-11-01-002-mpi-kokkos-plan.md`](../plans/2026-11-01-002-mpi-kokkos-plan.md), ADR-0020 (Proposed),
[`PATCH.md` section 33](../../kokkos/src/fortran_iface/PATCH.md#33-w3xyp3-under-mpi-the-aggregated-exchange-on-the-resident-va-ww3-gpumpi-xyp3-dsvr2gew).
Goal `dSvr2GEW-5307`.

## Question

Does the aggregated exchange (one message per pair of ranks and direction, packed and unpacked on
the resident `VA`) give the bits of the Fortran's `IRQSG1`/`IRQSG2` exchange, at even and uneven
splits of the sea points, and does it add whole-spectrum copies?

## What was done

- Fork commit `9601dc1` (`ww3-gpu/mpi-xyp3-dSvr2GEW`, on `96892c95`) and the shim of `ww_kokkos`
  `0.13`, built in a cloud container (4 cores) with gfortran 16.2.0, g++ 16.2.0, OpenMPI 5.0.11 and
  Kokkos 5.2.2 (OpenMP backend) from `picasso/labs/pratico`'s pinned nixpkgs. The lab's `#ww3` shell
  did not build there (ParMETIS's source download got HTTP 403), so the same packages, without
  METIS and ParMETIS, came from a `nix-shell` on the lab's `nixpkgs` input, and `scripts/ww3-gpu.sh`
  ran inside it with `WW3GPU_IN_SHELL=1`. CPU only; never `--gpu`.
- `OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1 PRTE_MCA_rmaps_default_mapping_policy=:oversubscribe`
  (the container runs as root and has 4 cores), then, for each row of the table below:

  ```bash
  WW3GPU_SWITCH=switch_lab_mpi WW3GPU_MPI_RANKS=<n> WW_KOKKOS_VA_COUNT=1 WW_KOKKOS_PROP3_COUNT=1 \
    bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos <parts>
  ```

  `full` is `bench_small`, `NSEA = 2301` (the 57 525 `W3SRCE` points of 25 calls at 4 ranks), so 2
  and 5 ranks leave short ranks with `NSEAL = NSEALM - 1` and 3 ranks does not.

## Result

| Ranks | `--kokkos` | Against the Fortran, same executable | Against `--ww3-noaa` | `W3XYP3` spectra per rank | Exchange per rank (filled = read) | `VA` H2D / D2H per rank |
|---|---|---|---|---|---|---|
| 1 | `XYP3` | every model file byte-identical (`cmp`) | all but the NetCDF byte-identical, `nccmp-tol` PASS | 13 824 | 0 | 24 / 24 |
| 2 | `XYP3` | byte-identical | the same | 6 912 | 48 buffers, 254.47 MB | 24 / 24 |
| 3 | `XYP3` | byte-identical | the same | 4 608 | 48, 226.20 MB | 24 / 24 |
| 4 | `XYP3` | byte-identical | the same | 3 456 | 48, 190.85 MB | 24 / 24 |
| 5 | `XYP3` | byte-identical | the same | 2 760 (one rank 2 784) | 48, 162.77 MB (one 163.30) | 24 / 24 |
| 4 | `SRCE,KTP3,XYP3` | byte-identical | the same | 3 456 | 48, 190.85 MB | 1 / 13 |

(v) for every cell, read from the runs' logs and `build/runs/ww3-gpu-full-openmp/kokkos/ww3_shel.out`.

- The Kokkos path ran: `XYP3=K` in the parts line, and the spectra per rank are `576 x 24 / NAPROC`
  (576 components, 24 propagation steps), the uneven 5-rank split of `IAPPRO` included.
- The exchange moves `(NSPEC - NSPLOC) x NSEALM` REALs out and back per step, which matches the
  MB above (4 ranks: `2 x 432 x 2301/4 x 4 B` is about 7.95 MB a step, 190.85 MB over 24) (v,
  arithmetic on the counts).
- No whole-spectrum copy is added: with `XYP3` alone the Fortran `W3SRCE` and `W3KTP3` write `VA` on
  the host every step, hence 24 / 24; with `SRCE,KTP3,XYP3` the counts are 1 / 13, those of the
  `SHRD` resident replay of 2026-10-17 (`kokkos/ports.tsv`, `w3pro3md:w3xyp3`).
- The batched `W3SRCE` ran under `MPI` for the first time (plan item 2, unit M1), byte-identical.
- The 4-rank `XYP3` run took 61.69 s against 60.53 s for the Fortran, one run each, 4 ranks
  oversubscribing nothing but sharing 4 cores. Not a timing ⚠: one run, a shared cloud machine, and
  no regtest pass on this commit (AGENTS.md, "Speed is the goal").

These are diagnostic runs of one case, not WW3's regtest matrix: `regtested` stays "no" for the MPI
arm, and the matrix's 43 MPI entries (plan item 3 and 4) are still to run ⚠.

## What it changes

M2's static argument holds on `bench_small` at 1 to 5 ranks. Left for the workstation: the MPI
matrix with `XYP3` (plan item 4), `mww3_test_08` alone, and the timing with `--repeat 3` (item 5),
which decides whether M3 onward goes ahead (ADR-0020, "Revisit when"). `bench_small` has no
boundary input, no currents and no global closure, so those branches of the MPI arm never ran ⚠.
