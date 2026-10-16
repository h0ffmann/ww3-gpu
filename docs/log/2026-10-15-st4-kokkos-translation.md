# ST4 translated to Kokkos, eight routines in parallel, not regtested

2026-10-15, the owner with an agent. Issue #45, [`ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md),
branch `claude/45-port-st4`; fork branch `ww3-gpu/kokkos-st4` (`3f07a0f2`).

## Question

Can all of ST4 (`w3src4md.F90` at `WW3@bb08623a`) be translated to Kokkos in one session, one
subagent per routine, without changing a bit of what WW3's `-O3` build computes?

## What was done

- The call graph inside ST4 has two edges, `W3SPR4 → CALC_USTAR` (`:361`) and
  `INSIN4 → TABU_*` (`:1122-1127`). A common layer fixed every signature first
  (`st4_params.hpp`, `st4_arrays.hpp`, stubs, the shim, commit `d839878`), so the eight routines
  were translated at once, each by its own subagent in its own git worktree, and cherry-picked in
  rank order. The fork wiring ran in parallel against the same C ABI.
- One read-only subagent built WW3's `w3src4md.F90` with the CMake Release flags and
  `-fdump-tree-optimized` and wrote the inventory of what `-O3` computes; four fresh reviewers
  then compared the C++ with it (plan Task 6), three of them by a GIMPLE comparison.
- Gates, on the final commit: `just kokkos-test serial-debug` and `openmp-release` (10/10 each,
  no warning), `cuda-release` compiled (never run), the fork built and linked with `KOKKOS`
  (never run).

## Result

- (v) gfortran `-O3` computes five ST4 loops with glibc's libmvec (`_ZGVbN4vv_powf` at `:316`,
  `:317`, `:2315`, `:2500`; `_ZGVbN4v_expf` at `:2509`), in groups of four from the first
  element, then scalar. libmvec differs from scalar libm on 5.3 % of `powf` and 51 % of `expf`
  inputs, up to 3 ULP (a test program on random inputs). The owner chose to call the same
  functions with the same grouping on the host (`st4_libmvec.hpp`). ⚠ A device has no libmvec,
  so a GPU build cannot be bit for bit at those sites.
- (v) For `IKD = 1` of `INSIN4` gfortran folds `TANH(1.01**-999)` at compile time to a value
  6e-6 away from the runtime `__powisf2`; the port hard-codes the dump's constants. ⚠ They are
  gfortran 16.2's and move with the toolchain.
- (v) The reviewers found no difference on finite valid inputs. They found, and the branch fixed,
  `INT` of a `REAL` (a C++ cast is undefined where `cvttss2si` gives INT_MIN; `fint`), the dump's
  form of `FLOOR`, a `MAX` that `-O3` drops, and a table rebuild that wiped an earlier grid's
  module tables. NaN and signed-zero `MAX`/`MIN` orders, the `W3_STAB3` path (no dump: the lab
  compiles STAB0) and an out-of-bounds `COEF4(0)` that the Fortran shares stay ⚠ in the port
  reports.
- Nothing ran inside WW3. No routine is L2; every row of `kokkos/ports.tsv` says
  `no: waiting for the checkpoint of ST4 (ADR-0011)`.

## What it changes

The next step is the ST4 checkpoint of the plan's §12: the `regtests/` changes of §10.3, the
`WW3/` pin bump to `ww3-gpu/kokkos-st4` with `just smoke`, then `just smoke-ww3-gpu`,
`just full-ww3-gpu` and `just ww3-gpu-matrix` with `WW3GPU_PHYSICS=ST4`, one flag at a time.
The libmvec choice ties the Fortran reference and the port to the same glibc and CPU dispatch.
