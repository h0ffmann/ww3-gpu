---
id: "202610141900"
title: "Bit-for-bit is the gate"
type: permanent
status: verified
source: "kokkos/PORT_STATUS.md and docs/ADRs/ADR-0001-proof-language.md at da226b2"
tags: [porting, evidence]
created: 2026-10-14
---

# Bit-for-bit is the gate

A ported kernel is done only when it gives the Fortran's answer bit for bit. `W3SNL1` is
"bit-identical on all three presets" (Serial, OpenMP, CUDA) in `kokkos/PORT_STATUS.md` `(v)`.
The claim is settled by mechanised checks, not by a proof language: ADR-0001 decides "No proof
assistant or dependently typed language is adopted" `(v)`.

A speed figure comes after this gate, never before: [[202610141903-no-timing-without-parity]].
The physics it reproduces is WW3's, untouched: [[202610141904-ww3-is-read-only]].
