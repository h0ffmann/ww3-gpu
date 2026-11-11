# Preprint: one rule, two languages

**Status: preprint, not peer reviewed.** A note that compares two ports of WAVEWATCH III (WW3)
held to the same rule, the Fortran's bits: this repository's C++/Kokkos port and the Rust port in
[h0ffmann/chaceon](https://github.com/h0ffmann/chaceon). It is the contribution prepared for
aiXiv, 2026-11-11. Authors: Matheus Hoffmann, Pedro Veras Guimarães (LabECO, UFSC) and Pedro Cruz
(GTA, DEL/Poli/UFRJ); the ORCIDs are in `metadata.md` and match the public ORCID records (v,
`pub.orcid.org`, read 2026-11-11).

| File | What it is |
|---|---|
| [`metadata.md`](metadata.md), [`prose/`](prose/) | The source, Pandoc Markdown, in ASD-STE100 style |
| [`refs.bib`](refs.bib) | The references |
| [`format/`](format/) | The LaTeX template and pandoc-crossref settings of h0ffmann/closed-source (MIT) at `6b3aae5` |
| [`two-ports.tex`](two-ports.tex) | The LaTeX that pandoc writes, the source sent to aiXiv |
| [`two-ports.pdf`](two-ports.pdf) | The PDF, XeLaTeX run twice |

`just preprint` builds both, with pandoc, pandoc-crossref and XeLaTeX from h0ffmann/picasso's
`labs/paper` shell; `just preprint --self-test` checks that the inputs exist.

Every number in it was read from a file at ww3-gpu `58167d7` (`kokkos/PORT_STATUS.md`) or
chaceon `0c098fe` (`README.md`, `docs/WW3_GPU_PARITY_202611.md`) (v, read 2026-11-11). Section 0 sets it beside Koldunov et al. (2026), the FESOM2 port by a
language model (arXiv:2606.11356), as summarised in
[`notes/202610170112`](../../notes/202610170112-fesom2-went-through-c-ww3-goes-straight-to-kokkos.md). It cites
the NOAA progress record and ADR-0001 at `58167d7`,
[h0ffmann/ww3-dia-gpu-v2](https://github.com/h0ffmann/ww3-dia-gpu-v2) at `32aa2ef`, and the Lean 4
`ocean-waves` package in [h0ffmann/proof](https://github.com/h0ffmann/proof/tree/b863c15/ocean-waves)
at `b863c15`, whose theorems are quoted from its README (v, read 2026-11-11). Nothing was run for
it, and it claims no speed-up and no real-GPU run. The full paper on the discrete interaction
approximation (DIA) port is a separate document, under review on its own branch.
