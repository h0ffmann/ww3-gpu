# Preprint: wave models on GPUs

**Status: preprint, not peer reviewed.** A note that compares two ports of WAVEWATCH III (WW3)
held to the same rule, the Fortran's bits: this repository's C++/Kokkos port and the Rust port in
[h0ffmann/chaceon](https://github.com/h0ffmann/chaceon). It is the contribution prepared for
aiXiv, 2026-11-11. Authors: Matheus Hoffmann, Pedro Veras Guimarães, PhD (UFSC) and Pedro
Cruz, D.Sc. (UFRJ); the ORCIDs are in `metadata.md` and match the public ORCID records (v,
`pub.orcid.org`, read 2026-11-11).

| File | What it is |
|---|---|
| [`metadata.md`](metadata.md), [`prose/`](prose/) | The source, Pandoc Markdown, in ASD-STE100 style |
| [`refs.bib`](refs.bib) | The references |
| [`figures/`](figures/) | Figure 1 of each edition, the dark Archify drawing `docs/archify/ww3-gpu-runtimes{,-pt}.pdf`, drawn for the 174 mm column ([`DESIGN.md`](../../DESIGN.md), "Pictures in a paper"); `scripts/preprint.sh` copies it at every build |
| [`format/`](format/) | The LaTeX template and pandoc-crossref settings of h0ffmann/closed-source (MIT) at `6b3aae5` |
| [`wave-models-on-gpus.tex`](wave-models-on-gpus.tex) | The LaTeX that pandoc writes, the source sent to aiXiv |
| [`wave-models-on-gpus.pdf`](wave-models-on-gpus.pdf) | The PDF, XeLaTeX run twice |
| [`prose-pt-BR/`](prose-pt-BR/), [`wave-models-on-gpus.pt-BR.pdf`](wave-models-on-gpus.pt-BR.pdf) | The Brazilian Portuguese edition, translated from the controlled English and written in [Português Técnico Controlado](https://github.com/kayquer/portugues-tecnico-controlado) (Kayque Rotondo, MIT, `8938f58`), level `descritivo`; `prose-pt-BR/00-metadata.md` overrides the title, abstract, language and labels |

`just preprint` builds both editions, with pandoc, pandoc-crossref and XeLaTeX from h0ffmann/picasso's
`labs/paper` shell; `just preprint --self-test` checks that the inputs exist.

## arXiv

`just preprint-arxiv` writes `build/arxiv/wave-models-on-gpus-arxiv.tar.gz`, the English
edition's source for arXiv: the committed `.tex` with the Git revision written in, `figures/runtimes.pdf`,
`DejaVuSans.ttf` (the font of the ⚠ mark, which `metadata.md` loads by file name) and a
`00README.json` that asks for `xelatex` on TeX Live 2023. It then unpacks the tarball into an
empty directory, runs XeLaTeX twice there, and fails on an error, a missing glyph or an
undefined reference (`scripts/preprint_arxiv.sh`). CI does the same on every change to
`pubs/preprint/` ([`arxiv.yml`](../../.github/workflows/arxiv.yml)), inside the `texlive/texlive:TL2023-historic`
image pinned by digest, and each `v*` release gets
the tarball as an asset ([`release.yml`](../../.github/workflows/release.yml)). Built here, the
bundle's PDF has the same 7 pages and the same text as `wave-models-on-gpus.pdf` except the
revision stamp (v, `pdftotext` of both, in that image and in Ubuntu 24.04's TeX Live 2023, 2026-11-11).

The upload stays manual. arXiv's deposit API is for proxy submitters it has approved, and asks
them to write to its administrators first; authors submit through the web form (v,
[submit_sword](https://info.arxiv.org/help/submit_sword.html),
[third_party_submission](https://info.arxiv.org/help/third_party_submission.html), read
2026-11-11). The author uploads the release's tarball at <https://arxiv.org/submit>, with the
title, authors and abstract of `metadata.md`, primary category math-ph and cross-list math.NA as
on aiXiv. arXiv may ask a first-time submitter in a category for an endorsement ⚠ (not checked
for this account). A submission is public and cannot be removed, so no tool here makes one. A
new version is a replacement of the same arXiv identifier, uploaded the same way.

That `00README.json` accepts `xelatex` is from arXiv's [00README](https://info.arxiv.org/help/00README.html)
page (v, read 2026-11-11); its TeX page names only pdfLaTeX among the engines it detects by
itself, so the bundle states the engine. arXiv's own compile of this bundle has not been run ⚠.

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
