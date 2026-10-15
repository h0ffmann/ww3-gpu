---
id: "202610141900"
title: "Bit-for-bit is the gate"
type: permanent
status: verified
source: "kokkos/PORT_STATUS.md, docs/ADRs/ADR-0001-proof-language.md and docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md, 2026-10-14"
tags: [porting, evidence]
created: 2026-10-14
---

# Bit-for-bit is the gate

A ported kernel is done only when WW3, run with it, gives the Fortran's answer. The aim is bit for
bit; the gate is WW3's own regtests, run with the Fortran routine and with the Kokkos routine on
the CPU and compared within `kokkos/tools/nccmp-tol/tolerances.txt` (ADR-0010: "A ported routine
is validated only by WW3's regression tests") `(v)`. The port has no tests of its own.

`W3SNL1` was "bit-identical on all three presets" (Serial, OpenMP, CUDA) in the unit tests that
ADR-0010 removed, a historical result in `kokkos/PORT_STATUS.md` `(v)`. Inside WW3 it was not
byte-identical until 2026-10-15, when `AF11` took gfortran's `-O3` chain for `x**11`; since then
WW3's `test.comp` finds 0 differing files on all 100 entries of `just hoff-matrix` on the CPU
(`kokkos/port_report/W3SNL1_KOKKOS.md`) `(v)`. The same holds on its 25 shared-memory OpenMP entries, with
`OMP_MAX_ACTIVE_LEVELS=2` (`docs/data/hoff_matrix_omp_2026-10-15.tsv`) `(v)`. `just hoff-matrix` is
a CTest driver in `regtests/`, and CTest there only runs WW3's regtests: it does not make them a
test of the port's own. A bit-for-bit claim is settled by mechanised checks,
not by a proof language: ADR-0001 decides "No proof assistant or dependently typed language is
adopted" `(v)`.

A speed figure comes after this gate, never before: [[202610141903-no-timing-without-parity]].
The physics it reproduces is WW3's, untouched: [[202610141904-ww3-is-read-only]].
