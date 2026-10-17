# ADR-0014: A GPU run is judged by nccmp-tol's tolerances; the CPU backend stays bit for bit

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-17 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The verdict of a `--ww3-gpu --gpu` run (and of `just ww3-gpu-matrix-cuda`) against the Fortran of the same executable and against `--ww3`, and so whether its timing may enter a table. Not the CPU backend, whose rule is unchanged; not the tolerances themselves; not the GPU supervision rule |
| **Related** | AGENTS.md, "Bit for bit with the Fortran" (its GPU paragraph, which this ADR answers); [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [ADR-0013](ADR-0013-operational-parts-default-to-kokkos.md) (its condition 3); [`kokkos/tools/nccmp-tol/tolerances.txt`](../../kokkos/tools/nccmp-tol/tolerances.txt); [`bench/README.md`](../../bench/README.md) |

## Context

AGENTS.md says a GPU port that cannot be bit for bit with the Fortran stops and asks the owner.
It cannot be, at known sites:

- ST4 reproduces glibc's vector math at five loops of `w3src4md.F90` (`:316`, `:317`, `:2315`,
  `:2500` with `powf`, `:2509` with `expf`); "on a device there is no libmvec ... such a build
  cannot be bit for bit at these sites" (v, `kokkos/src/ww_kokkos/st4_libmvec.hpp:10-13`).
- On CUDA, `logf`, `tanhf`, `sincosf`, `expf` and `powf` come from NVIDIA's libdevice, whose bits
  against glibc are not known (v, `kokkos/src/fortran_iface/PATCH.md`, the parity column of the
  run-time flags table).

The owner was asked on 2026-10-17 how GPU runs are judged, and chose the tolerances of
`nccmp-tol`, with bit for bit kept for the CPU backend.

## Decision

1. **CPU backend (Serial, OpenMP): unchanged.** Bit for bit with the Fortran of the same
   executable; `test.comp` finding a file different fails, as AGENTS.md says.
2. **GPU backend (CUDA): the verdict is `nccmp-tol` with `tolerances.txt`**, as the L2 replay
   already runs it, against the Fortran of the same executable and against `--ww3`. A field the
   file lists passes when every value meets its `abs` or `rel` bound; a run passes when every
   listed field does. The run also prints `max|d|`, the rms and the largest relative difference
   of every field, listed or not, and the record keeps them.
3. **Never widened.** A tolerance is not changed to make a GPU run pass. A change to
   `tolerances.txt` is its own pull request, reviewed by a person (AGENTS.md, "Never cheat a
   regtest tool"), with the evidence for the new bound.
4. **A GPU timing enters a table** only after such a pass on the same commit, and its record says
   "GPU: judged by tolerance, not bits" in `regtest`.

## Alternatives considered

- **Bits only, GPU diagnostic.** Keeps one rule for every backend, and stops all GPU performance
  work until a device math library with glibc's bits exists, which no plan foresees.
- **Decide after the first supervised run.** Sees the differences before ruling, and leaves
  every GPU number unquotable meanwhile; this ADR keeps that option as its first review.

## Consequences

- The ST4 libmvec sites and the libdevice calls are expected to differ by a few ULP per step; how
  much that grows over a run is unknown until the first supervised run ⚠.
- `tolerances.txt` judges five fields (`hs`, `fp`, `dir`, `dp`, `t0m1`). The smoke writes 26
  variables and judges one of them, `hs` ("1 judged, 25 unlisted"); the full case judges `hs` of
  five (v, the runs of 2026-10-16 and 2026-10-17). An unlisted field is reported, not judged ⚠:
  judging more fields is a change to `tolerances.txt` under point 3.
- `scripts/results.py record` writes the wording of point 4 in the `regtest` of a CUDA record.
- AGENTS.md, "Bit for bit with the Fortran", states this rule in place of "stops and asks the owner".

## Revisit when

- the first supervised `just perf-ww3-gpu full 3` has run: read its `nccmp-tol` table, every field;
- a device math library with glibc's bits, or a reimplementation of the five libmvec loops and
  the libdevice calls with glibc's algorithms, exists;
- a field outside `tolerances.txt` shows a difference that matters to a reader of the output.
