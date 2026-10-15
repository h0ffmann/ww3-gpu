---
id: "202610141901"
title: "Port order is measured wall time"
type: permanent
status: verified
source: "docs/ADRs/ADR-0002-port-order-wall-time.md at da226b2"
tags: [porting, method]
created: 2026-10-14
---

# Port order is measured wall time

The next routine to port is the one that takes the most wall-clock time in the operational case.
ADR-0002: "Nothing else sets the order" `(v)`. Effort, GPU fit and ease of validation do not move
it.

The measurement that sets the order is a timing too, so it obeys
[[202610141903-no-timing-without-parity]]. Each routine in that order then passes
[[202610141900-bit-for-bit-is-the-gate]].
