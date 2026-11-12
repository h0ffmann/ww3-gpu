# Batch 4, units U1 and U2: six measured routines and five callees translated to Kokkos

2026-11-12, the owner with agents. Goal `UrLTXtPC-4817`, plan
[`docs/plans/2026-11-01-001-port-batch-4-plan.md`](../plans/2026-11-01-001-port-batch-4-plan.md),
issue #42 (the port planner).

## Question

Every routine the committed profiles time in compute is translated (99.3 % of `full`,
`kokkos/PORT_STATUS.md`). The first real routines left in `kokkos/PORT_TABLE.tsv` are `W3CSPC`
(0.20 % of `gfs`), `W3GFCL_R8` (0.16 %), `W3CGDM_R4` (0.08 %) and `W3UCUR` (0.08 %), units U1 and
U2 of the parked batch 4 plan, with `W3UBPT` and `W3UICE` beside `W3UCUR`. None is MPI, so
ADR-0017 holds and the MPI plan's M2 gate is not touched. Can they be translated to the bit for
bit standard, with one translator and one fresh reviewer per unit, and nothing run?

## What was done

- One WW3 CMake Release build of the fork at `WW3@96892c95`, target `ww3_lib`, switch
  `switches/switch_lab_shrd`, with gfortran 16.2.0 from the nixpkgs revision that
  `picasso/labs/pratico/flake.lock` pins (the same nix wrapper as the `ww3` shell). The full
  `ww3` shell could not be entered in the cloud session: ParMETIS's source download returned
  HTTP 403, so the build used `nix shell --inputs-from picasso/labs/pratico` with gfortran,
  NetCDF, CMake and a Kokkos 5.2.2 Serial+OpenMP built from the flake's `kokkosHost` recipe ⚠.
  `w3cspcmd.F90`, `w3gsrumd.F90` and `w3updtmd.F90` are byte-identical to NOAA `03adda91` (v,
  `git -C WW3 diff 03adda91 HEAD -- <file>` is empty). Every translator and reviewer recompiled
  its module from that build's `flags.make` and checked the object identical to the CMake one (v).
- U1 (host-only C++): `W3CSPC` (`cspc_w3cspc.hpp`), `W3GFCL_R8` with `W3CKCL_R8`, `W3DIST_R8`,
  `W3NNSC` (`gsru_w3gfcl_r8.hpp`), `W3CGDM_R4` with `GET_FDW3`, `W3FDWT_R8`
  (`gsru_w3cgdm_r4.hpp`; `DXYDP`/`DXYDQ` are hooks, not translated). They reuse the `GsuClass`
  of `gsru_gsu_create.hpp`.
- U2 (Kokkos device functions and launchers): `W3UCUR` (`updt_ucur.hpp`, the `CRT1` branch),
  `W3UBPT` (`updt_ubpt.hpp`, the non-`PDLIB` branch), `W3UICE` (`updt_uice.hpp`, the `IC0` and
  `ST4` branches).
- 11 rows in `kokkos/ports.tsv`, a port report each, glossary entries, the headers included and
  instantiated from `wave_support.cpp`.
- Not run: no ctest of a ported routine (ADR-0010), no regtest (ADR-0016), no GPU, no wiring.

## Result

- (v) U2, fresh review: no arithmetic, branch, indexing or constant defect. `MAX(1.E-7,DT0N)` is
  `maxss` with `DT0N` first in both objects; `W3UBPT` keeps the Fortran's parentheses
  (`PAREN_EXPR`), nine scalar operations per point in both; `W3UICE` has no floating-point
  arithmetic beyond ordered compares. Five citations and notes were corrected.
- ⚠ U2: in the Kokkos kernels g++ puts the memory operand of a commutative product second, so
  `RD*CAI` (`W3UCUR`, `:436`), `CABS*COS` (`:452-453`) and `RDBPI*ABPI` (`W3UBPT`) have the other
  operand order. Only a product of two NaNs can differ, in its payload. `RD` is finite (DSEC21 of
  integers); whether `RDBPI` can be NaN was not checked.
- (v) U1, fresh review: two defects of the translation fixed. `W3CSPC` stopped while building its
  table on an index the Fortran only stores (`:407`); the check moved to the read (`:532`).
  `W3CGDM_R4` did not reproduce the `ALLOCATE(STAT=)` codes 5014 and 5020 (`:4172`). `W3GFCL_R8`
  had none.
- ⚠ U1: a throwaway differential program (not kept, ADR-0010) linked the Fortran objects and
  compared them with the C++: 128 606 comparisons, 0 failures for `W3CSPC` and `W3GFCL_R8`; for
  `W3CGDM_R4`, 43 of 1 056 510 outputs at `-O3` (234 at `-O0`) are NaN in both with the other sign,
  from the operand order GCC chooses (the Fortran's own GIMPLE has two copies of `:4312-4333`
  ordered differently). Not fixable from C++ source. No command in the repository reproduces
  these counts.
- (v) WW3 defects found, reproduced or read and marked ⚠ in the reports, not fixed: `W3CSPC`'s
  `NRMAX` loop does not end for a NaN or infinite `XF` (`w3cspcmd.F90:440-445`); `W3GFCL_R8`
  assigns `GSU%PTR%NNB` through an `INTENT(IN)` argument and leaks on every `FNCL` call
  (`w3gsrumd.F90:1563`, LeakSanitizer); `W3CGDM_R4`'s `SINAL` and `SINTP` (`:4328`, `:4330`) look
  like slips (⚠, not reproduced numerically); `GET_FDW3` leaves part of `K`/`C` undefined.

## What it changes

The translated level (`python3 scripts/port_score.py --write`): 327 of 1 023 routines, 71 303 of
263 375 lines (27.07 %), from 316 and 69 024. The run-time shares do not move: none of the eleven
is wired, and the `full` profile does not time them. Wiring, the checkpoint (ADR-0011) and the
regtests are the next steps, and the owner's.
