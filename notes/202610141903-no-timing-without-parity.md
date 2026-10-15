---
id: "202610141903"
title: "No timing without parity"
type: permanent
status: verified
source: "AGENTS.md (Porting, hard rule) and docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md, 2026-10-14"
tags: [evidence, benchmarks]
created: 2026-10-14
---

# No timing without parity

A number of seconds says nothing about a kernel that gives a different answer. So "nothing timed
enters a table without a passed WW3 regtest replay on the same commit" (`AGENTS.md`) `(v)`, and
the timing names the machine, the command and the median of three runs. Before 2026-10-14 the
gate was the port's L1 unit test on the same build; ADR-0010 removed those tests `(v)`.

The regtest replay is the gate of [[202610141900-bit-for-bit-is-the-gate]]. A timing of a kernel
that still copies per call is not the model's speed: [[202610141905-phase-one-copies-per-call]].
