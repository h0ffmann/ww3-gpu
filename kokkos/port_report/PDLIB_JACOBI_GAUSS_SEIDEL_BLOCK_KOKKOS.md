# PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that implements the
MPI hooks, the U_JAC contract below and the `W3WAVE` hunk.

The implicit solver of WW3's unstructured grids: per time step, it assembles the sparse system
for all spectral components, then solves it by block Jacobi or Gauss-Seidel sweeps over the
nodes until a convergence test passes, with an optional limiter. It is 37.5 % of the attributed
time of the PDLIB profile (`docs/data/ww3_pdlib_gprof.tsv`, `ww3_tp2.17` grid c), the most
expensive routine there.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK`, `model/src/w3profsmd_pdlib.F90:5463-6406` at NOAA-EMC/WW3 `03adda91` |
| Kokkos code | [`../src/ww_kokkos/pdlib_jgs_block.hpp`](../src/ww_kokkos/pdlib_jgs_block.hpp), driver `pdlib_jacobi_gauss_seidel_block<kRef1, kTrnk, kSt4>`; `<false, false, true>` is `ww3_tp2.17/input/switch_PDLIB` |
| Calls | [`calcarray_jacobi_vec<kRef1>`](CALCARRAY_JACOBI_VEC_KOKKOS.md), [`calcarray_jacobi_spectral_1`](CALCARRAY_JACOBI_SPECTRAL_1_KOKKOS.md), the boundary condition, and `st4::w3spr4_point` in the limiter, all through host hooks or functors |
| C shim | none yet. Every MPI call (`PDLIB_exchange*`, the integer and norm `MPI_ALLREDUCE`) and the iteration report are host hooks, called in the Fortran's order |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## What WW3 does here, at the pin

Read by the translator and confirmed by a fresh reviewer, 2026-10-16 (v):

- The routine calls no source term but `W3SPR4`. `W3PARALL` fixes `LSLOC = .TRUE.` and `IMEM = 1`
  as parameters (`w3parall.F90:89-90`), so `CALCARRAY_JACOBI_SOURCE_1` (`:5756`, behind
  `.NOT. LSLOC`) is dead. The implicit source terms come from `W3SRCE`, which `W3WAVE` calls first
  (`w3wavemd.F90:1617`, under `LPDLIB .AND. FLSOU .AND. FSSOURCE`) and which adds into `B_JAC` and the
  diagonal of `ASPAR_JAC` (`w3srcemd.F90:1627-1628`).
- The convergence norm's `MPI_ALLREDUCE` (`:6190`) sends the `REAL(8)` `Sum_L2` with
  `rtype = MPI_REAL4` (`PDLIB/yowdatapool.F90:77`) into `Sum_L2_GL`, a non-`SAVE` local (`:5593`) that
  nothing else writes. Its high four bytes are never written, so its value is undefined and no port
  can reproduce it. It is used only with `JGS_TERMINATE_NORM = T`, which no regtest sets.
- The limiter's `W3SPR4` call reads `TAUWX`, `TAUWY` and `LLWS`, which the routine never assigns, on
  every point; `USTAR` and `USTDIR` are unset on the first. With `FSFREQSHIFT` and `.NOT. LSIG` the
  norm reads a `DMM` this call never wrote. All undefined in the Fortran; `JGS_LIMITER = F` in every
  regtest.

## Arithmetic

Reference: the `-O3` objects of `w3profsmd_pdlib.F90` (WW3's CMake Release flags, gfortran 16.2)
for `switch_PDLIB`, with `REF1` and with `TRNK`, read by the translator and a fresh reviewer.

- (v) The Gauss-Seidel sweep (`:5808-6074`) and the norm (`:6131-6185`) each run on one thread,
  `IP = 1..np`, reading just-updated values in the Fortran's order; the Fortran object has no
  reassociated sum (`addpd`, `haddpd`), and `Sum_L2` starts from 0 in both. The other passes are
  parallel only where each index writes its own column; the limiter is serial (its locals carry
  from node to node).
- (v) Hooks: the Fortran object makes three zero exchanges, one 2-D exchange, each `CALCARRAY_*` and
  boundary call once, two `MPI_ALLREDUCE` and one write; the C++ calls the same hooks in the same
  order with the same exit tests (`MAXITER`, difference, norm).
- (v) `REF1`, `TRNK` and `ST4` change arithmetic and are template flags; of the 44 switch files with
  `PDLIB`, 16 have `ST4`, 16 `ST4` and `REF1`, 2 `TRNK`, 8 `ST0`, 2 `ST1`. No switch or CMake file
  defines `FLX5`, `NOCGTABLE` or `WEIGHTS`, and `FLX5` cannot compile here (`DAIR` is not in the
  routine's `USE CONSTANTS`).
- (v) Every `MIN`/`MAX` operand order from the object, including the `CP_SIG`/`CM_SIG` vector body
  against the odd-element and `NSPEC = 1` tails; constants `0x3a54562f`, `0x37248473`,
  `0x3e22f983`; `TINY` added in double; float against double per variable as declared
  (`:5586-5613`); no FMA in the C++ object.
- ⚠ The U_JAC contract: on the Jacobi path (`ww3_tp2.17` grids d and e,
  `JGS_BLOCK_GAUSS_SEIDEL = F`) the sweep never writes `U_JAC` at dry nodes, so `:6089` copies the
  contents left from allocation into `VA`, and wet rows read them through their off-diagonals. The
  shim must alias the Fortran's `U_JAC` or copy it in and out every call, never use a fresh zeroed
  View.
- ⚠ `W3WAVE` zeroes `B_JAC` and `ASPAR_JAC` at `w3wavemd.F90:1543-1546` when neither `FLSOU` nor
  `FSSOURCE` is set and at `:1563-1565`, before `W3SRCE`, when both are; with exactly one set nothing
  zeroes them (the translator's correction of the review's note).
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`; no `fma.rn` in the solver kernels, those in `TRNK` and
  the limiter are libdevice's), never run; PTX `min`/`max` on −0 not checked.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
