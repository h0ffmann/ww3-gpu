# ADR-0015: Triton kernels are allowed as an exception to "no Python in lab code", under three conditions

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-17 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | Whether a GPU kernel written in Triton may enter this repository's lab code, which AGENTS.md otherwise limits to C++, Fortran and shell. Not the Kokkos port, not repo tooling, not the GPU parity rule (ADR-0014) |
| **Related** | issue #171 (Triton trial ports), issue #45 (`W3SDS4` in Triton); [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [ADR-0014](ADR-0014-gpu-runs-judged-by-tolerance.md); [ADR-0002](ADR-0002-port-order-wall-time.md) |

## Context

AGENTS.md: "Lab code is C++, Fortran and shell. Python appears only in the publishing and repo
tooling (`scripts/*.py`, `tests/`), never in lab code or kernels." Triton is "a language and
compiler for parallel programming" with "a Python-based programming environment" for writing
kernels (v, https://triton-lang.org/main/index.html, read 2026-10-17), so a Triton kernel is
Python. Issue #171 asked the owner how to treat that; the owner answered on 2026-10-17: allow
Triton kernels as an exception, if better fitted for the problem, if they can be integrated with
the Fortran/C++ code, and if they can be regtested.

## Decision

A Triton kernel may enter lab code when all three hold, each shown in its pull request:

1. **Better fitted.** For the routine it ports, it beats the Kokkos kernel of the same routine on
   the same GPU, kernel-only and end to end, median of three runs after parity (`bench/README.md`),
   or it does something the Kokkos port cannot (#45's GEMM form of the cumulative term is the
   example on record). Otherwise the routine stays in Kokkos.
2. **Integrated.** WW3 calls it through the same kind of path as a Kokkos part: a `bind(C)` shim
   behind a `WW_KOKKOS_*`-style run-time flag that defaults to the Fortran, the Fortran routine
   kept selectable at run time, the memory contract written down. Whether that needs a Python
   interpreter at run time, or a kernel compiled ahead of time and launched from C, is shown in the
   pull request ⚠ (not checked on 2026-10-17).
3. **Regtested.** It is validated as the Kokkos port is (ADR-0010): by WW3's regtests, the Fortran
   against the Triton routine in one executable, judged on the GPU by `nccmp-tol`'s tolerances
   (ADR-0014). No test of its own replaces that.

Everything else in AGENTS.md applies unchanged: translate, don't improve; reductions keep the
Fortran's order unless the owner decides otherwise in its own pull request; GPU runs only under
the owner's supervision.

## Alternatives considered

- **Python only at build time.** Keeps the rule intact, and depends on an ahead-of-time route whose
  existence is not checked ⚠; condition 2 still prefers it where it exists.
- **Triton as a scratch study outside the lab code.** Safe, and leaves any good result unusable by
  WW3.

## Consequences

- AGENTS.md's language rule names this exception.
- A Triton kernel that fails any condition stays in a study branch or a `docs/log/` entry, never in
  lab code.

## Revisit when

- the first Triton kernel meets, or fails, the three conditions (#171, step 3);
- Triton's ahead-of-time route is checked.
