---
id: "202610150226"
title: "A partial distillation of WW3, on my own"
type: permanent
status: partial
source: "Matheus Hoffmann, message in the WFIP-0003 session, 2026-10-14: his own words; kokkos/PORT_STATUS.md and README.md at a59625a"
tags: [porting, ww3, licence]
created: 2026-10-15
---
<!-- cite: main="Matheus Hoffmann"; last="Matheus Hoffmann" -->

# A partial distillation of WW3, on my own

My position: this repository is a partial distillation of WAVEWATCH III, made on my own. A
routine is drawn out of WW3's Fortran into C++ and Kokkos, and it counts as distilled only when
WW3's own regtests give the same answer with the Kokkos routine as with the Fortran one
(ADR-0010).

**Partial.** What has been distilled so far is the DIA, `W3SNL1` and its tables `INSNL1`: 2 of
1023 routines, 663 of 263 375 lines of `model/src`, and 48.4 % of the profiled self time `(v)`
(`kokkos/PORT_STATUS.md`, written by `python3 scripts/port_score.py --write`). The rest of the
model is WW3 as NOAA wrote it, and it stays read-only: [[202610141904-ww3-is-read-only]].

**On my own.** The work is mine, with agents, and not NOAA's: the repository "is not affiliated
with, sponsored by or endorsed by NOAA" `(v)` (`README.md`, "trademarks"). The distillation is
checked against what NOAA itself runs, the regtest matrix, not against a test of my own:
[[202610141900-bit-for-bit-is-the-gate]].

**Status: partial.** The numbers and the non-affiliation are read from the repository; that
"distillation" is the right word is my position, not a fact with a source. It is not model
distillation in the machine-learning sense, where a small model learns from a large one's outputs:
here the code is translated and the outputs are compared, case by case. By that rule the DIA is
distilled: WW3's `test.comp` finds 0 differing files on all 100 serial `NL1` entries of its
regtest matrix `(v)` (`docs/data/hoff_matrix_2026-10-15.tsv`).

What it is not: an improvement of WW3. A distilled routine reproduces the Fortran's arithmetic,
and the order in which routines are drawn out follows wall time:
[[202610141901-port-order-is-measured-wall-time]].
