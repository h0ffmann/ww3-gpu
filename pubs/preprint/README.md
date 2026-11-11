# Preprint: one rule, two languages

**Status: preprint, not peer reviewed.** A two-page note that compares two ports of WAVEWATCH III
(WW3) held to the same rule, the Fortran's bits: this repository's C++/Kokkos port and the Rust
port in [h0ffmann/chaceon](https://github.com/h0ffmann/chaceon). It is the contribution prepared
for aiXiv, 2026-11-11.

| File | What it is |
|---|---|
| [`two-ports.html`](two-ports.html) | The source |
| [`two-ports.pdf`](two-ports.pdf) | The PDF, printed from the source with Chromium (Playwright `page.pdf`, A4) |

Every number in it was read from a file at ww3-gpu `58167d7` (`kokkos/PORT_STATUS.md`) or
chaceon `0c098fe` (`README.md`, `docs/WW3_GPU_PARITY_202611.md`) (v, read 2026-11-11). Nothing was
run for it, and it claims no speed-up and no real-GPU run. The full paper on the discrete
interaction approximation (DIA) port is a separate document, under review on its own branch.
