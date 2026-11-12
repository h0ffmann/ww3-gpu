# Archify pages

Every diagram of the repository, drawn with the `archify` skill (tt-a1i/archify v3.0.1, MIT); no
diagram is Mermaid (AGENTS.md, "Figures"). The skill is the submodule `vendor/archify`, pinned and sparse, with
`.claude/skills/archify` a symlink into it; nothing of it is copied here. Each page is one
standalone file with inline SVG, light and dark themes, and image export.

A drawing is a figure only with its *How to read this figure* card under its picture, which carries the evidence and the `⚠` marks; every node and label traces to a file read, a WW3 fact cites `model/src/<file>.F90:<line>` at the pin, and a speed figure says "regtest pending" until WW3's regtests have run on its commit.

Only the typed source (`<slug>.json`) is committed. The pages are build output, about 830 kB each
because every page inlines Archify's template, and git ignores them:

```bash
just archify init     # once per clone: the pinned commit, shallow, only archify/ on disk
just archify render   # every docs/archify/<slug>.json -> <slug>.html, validated and browser-checked
just archify pdf      # every rendered <slug>.html -> <slug>.pdf, printed by headless Chrome
just archify check    # the drawings whose JSON or Archify pin moved since their PDF was printed
just archify refresh  # render and print only those, then rewrite pdf.lock
```

The PDFs are committed, so a reader without a browser or a clone can see the diagrams. A drawing
of [`dark.tsv`](dark.tsv) (every drawing today) is printed in the dark look, the drawing alone on
a page its own size; any other would be the page printed with the template's print stylesheet
(light palette, the summary cards after the drawing). Chrome stamps a creation date, so a re-run
gives the same drawing, not the same bytes; that is why [`pdf.lock`](pdf.lock) records the sha256
of each source (with the look, for a dark drawing) and the Archify commit rather than the PDF's
hash. Nothing redraws on a merge: run `/drawings-refresh` (Claude Code) or
`just archify refresh` after changing a source, and commit the PDFs with the lock.

## The dark look

Every picture here is dark ([`DESIGN.md`](../../DESIGN.md), "Pictures"). A drawing listed in
[`dark.tsv`](dark.tsv) is dressed in [`ww3-gpu.css`](ww3-gpu.css), the site's ground and lights,
in the manner of piabrota/nix-os's figures: only the drawing stays, with Archify's legend and
source badges hidden and the viewBox cut to the height `dark.tsv` gives. A row of `dark.tsv` with
five fields (slug, height, x, y, width) frames the drawing on every side, so its margins are equal.

```bash
just archify png           # each drawing of dark.tsv -> <slug>.png (committed), then png.lock
just archify site <dir>    # each drawing of dark.tsv -> <dir>/<slug>/index.html, and <dir>/index.html
just archify fit           # each drawing of paper.tsv against the width its paper prints it at
```

`png` takes the picture with headless Chrome over its DevTools pipe
([`scripts/archify_shot.py`](../../scripts/archify_shot.py)), at twice the drawing's size; the
README shows it. `site` is what `pages.yml` runs to publish the pages at
<https://h0ffmann.github.io/ww3-gpu/figures/>, so no HTML is committed. `check` and `refresh`
cover the PNGs too: [`png.lock`](png.lock) records the sha256 of each drawing's JSON, the look,
`dark.tsv` and the dresser, and the Archify commit.

## The figure index and the WW3 under each picture

[`figures.tsv`](figures.tsv) has one row per drawing of `dark.tsv`: its group, its twin in the
other language, the page it illustrates (with its anchor), the one question it answers, and the
WAVEWATCH III it is drawn from. [`groups.tsv`](groups.tsv) orders the groups and gives each the
paragraph that opens it and leads to the next. Two scripts read them:

- [`archify_dress.py`](../../scripts/archify_dress.py) prints the row's `ww3` text in a strip
  under the drawing, so the PNG, the PDF and the page all name the WW3 they are drawn from: the
  reference `--ww3` (MetOcean's WW3, tag `7.14.mos-92e2cf6`) and the fork the port lives on
  (`WW3/` at `96892c95`, on NOAA-EMC/WW3 `03adda91`), or the commit whose Fortran lines a drawing
  cites when it is another one. A changed stamp makes the drawing stale in `png.lock`.
- [`archify_index.py`](../../scripts/archify_index.py) writes the site's `/figures/` index,
  grouped, with each drawing's question, page, twin and stamp; each figure page's bar links the
  page it illustrates.

`python3 scripts/archify_index.py --check` (and `tests/test_archify_index.py`, which CI runs) fails
when a drawing has no row, a row has no stamp or question, a page or its anchor does not exist, a
group is not defined, or two twins do not name each other. The front page of the website
(piabrota/nix-os, ADR-0024) may read the same two files.

## A drawing a paper prints

A figure attached to a paper is drawn for its page and for the eye ([`DESIGN.md`](../../DESIGN.md),
"Pictures in a paper"). [`paper.tsv`](paper.tsv) lists those drawings with the width they are
printed at:

```bash
just archify fit      # each drawing of paper.tsv: its printed size, proportion and type size
```

`fit` and `png` run [`scripts/archify_fit.py`](../../scripts/archify_fit.py) on the dressed page.
It fails when the frame is outside 5:4 to 2:1, when the text of a box prints under 7 pt, or when
any text prints under 6 pt, and `png` then draws nothing. On 2026-11-11 it prints, for both
rows, `174 x 99 mm, 1.76:1, box text 7.1 pt, smallest text 6.3 pt` (v). `scripts/preprint.sh`
copies the two PDFs into `pubs/preprint/figures/` at every build.

`render` runs Archify's `finalize --quality showcase`, which validates the source, renders the
page and checks it in a real browser. At `2ab3cae` it reproduces, byte for byte, the five pages
first drawn for [`WW3_GPU_VS_NOAA_202610.md`](../WW3_GPU_VS_NOAA_202610.md) (v, `cmp`, 2026-10-17).

Every drawing below also has its dark `<slug>.png` here and its page at
`https://h0ffmann.github.io/ww3-gpu/figures/<slug>/`.

| Source | PDF | Type | Question | Figure |
|---|---|---|---|---|
| [ww3-gpu-vs-noaa-architecture.json](ww3-gpu-vs-noaa-architecture.json) | [PDF](ww3-gpu-vs-noaa-architecture.pdf) | architecture | What is the same and what differs between NOAA's WW3 and ww3-gpu? | [the two builds](../WW3_GPU_VS_NOAA_202610.md#the-two-builds) |
| [ww3-gpu-parity-workflow.json](ww3-gpu-parity-workflow.json) | [PDF](ww3-gpu-parity-workflow.pdf) | workflow | How is a ww3-gpu result judged against NOAA's WW3? | [the verdict](../WW3_GPU_VS_NOAA_202610.md#the-verdict) |
| [ww3-gpu-step-sequence.json](ww3-gpu-step-sequence.json) | [PDF](ww3-gpu-step-sequence.pdf) | sequence | Where does a time step leave the Fortran in ww3-gpu, and what does NOAA's WW3 do at the same place? | [one time step](../WW3_GPU_VS_NOAA_202610.md#one-time-step) |
| [ww3-gpu-port-lifecycle.json](ww3-gpu-port-lifecycle.json) | [PDF](ww3-gpu-port-lifecycle.pdf) | lifecycle | Which states does a WW3 routine pass through on its way to Kokkos? | [a routine's states](../WW3_GPU_VS_NOAA_202610.md#a-routines-states) |
| [ww3-gpu-device-math-dataflow.json](ww3-gpu-device-math-dataflow.json) | [PDF](ww3-gpu-device-math-dataflow.pdf) | dataflow | Why can a GPU result carry the bits of NOAA's CPU run? | [device math](../WW3_GPU_VS_NOAA_202610.md#device-math) |
| [ww3-gpu-at-a-glance.json](ww3-gpu-at-a-glance.json) | [PDF](ww3-gpu-at-a-glance.pdf), [PNG](ww3-gpu-at-a-glance.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/ww3-gpu-at-a-glance/) | architecture | What does ww3-gpu build, from which sources, and on which runtime does each run mode run? | [README](../../README.md); pt-BR: [ww3-gpu-at-a-glance-pt.json](ww3-gpu-at-a-glance-pt.json), [README.pt-BR](../../README.pt-BR.md) |
| [ww3-gpu-runtimes.json](ww3-gpu-runtimes.json) | [PDF](ww3-gpu-runtimes.pdf), [PNG](ww3-gpu-runtimes.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/ww3-gpu-runtimes/) | architecture | Which six runtimes does one ww3-gpu command run, and whose bits must each give? | [README](../../README.md), the preprint's Figure 1 ([`pubs/preprint/`](../../pubs/preprint/README.md)); pt-BR: [ww3-gpu-runtimes-pt.json](ww3-gpu-runtimes-pt.json), [README.pt-BR](../../README.pt-BR.md), the pt-BR preprint |
| [ww3-gpu-at-a-glance-pt.json](ww3-gpu-at-a-glance-pt.json) | [PDF](ww3-gpu-at-a-glance-pt.pdf), [PNG](ww3-gpu-at-a-glance-pt.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/ww3-gpu-at-a-glance-pt/) | architecture | O que o ww3-gpu compila, de quais fontes, e em qual ambiente roda cada modo? | [README.pt-BR](../../README.pt-BR.md); en: [ww3-gpu-at-a-glance.json](ww3-gpu-at-a-glance.json) |
| [ww3-gpu-runtimes-pt.json](ww3-gpu-runtimes-pt.json) | [PDF](ww3-gpu-runtimes-pt.pdf), [PNG](ww3-gpu-runtimes-pt.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/ww3-gpu-runtimes-pt/) | architecture | Quais seis runtimes um comando do ww3-gpu roda, e de quem cada um precisa dar os bits? | [README.pt-BR](../../README.pt-BR.md), the pt-BR preprint; en: [ww3-gpu-runtimes.json](ww3-gpu-runtimes.json) |
| [sds4-sdscum-branches.json](sds4-sdscum-branches.json) | [PDF](sds4-sdscum-branches.pdf), [PNG](sds4-sdscum-branches.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/sds4-sdscum-branches/) | architecture | Which branch of the cumulative-breaking term does W3SDS4 run, and what does each cost? | [SDSCUM](../W3SDS4_TRITON_202610.md#why-sdscum-decides-the-cost) |
| [triton-port-plan.json](triton-port-plan.json) | [PDF](triton-port-plan.pdf), [PNG](triton-port-plan.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/triton-port-plan/) | architecture | In what order is W3SDS4 ported to Triton, and what gate does each step pass? | [W3SDS4_TRITON_202610.md](../W3SDS4_TRITON_202610.md) |
| [triton-benchmark-arms.json](triton-benchmark-arms.json) | [PDF](triton-benchmark-arms.pdf), [PNG](triton-benchmark-arms.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/triton-benchmark-arms/) | architecture | Which implementations of W3SDS4 are timed, and when does a timing count? | [W3SDS4_TRITON_202610.md](../W3SDS4_TRITON_202610.md) |
| [weathernext-wind-chain.json](weathernext-wind-chain.json) | [PDF](weathernext-wind-chain.pdf), [PNG](weathernext-wind-chain.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/weathernext-wind-chain/) | architecture | How does WeatherNext 3 enter the benchmark, and what are the waves compared with? | [W3SDS4_TRITON_202610.md](../W3SDS4_TRITON_202610.md) |
| [openrig-port-feed.json](openrig-port-feed.json) | [PDF](openrig-port-feed.pdf), [PNG](openrig-port-feed.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/openrig-port-feed/) | architecture | How does work enter the ww3-gpu-port rig, and where does it leave? | [WFIP-0007](../WFIPs/WFIP-0007-openrig-port-factory.md) |
| [triton-g-doze-semanas.json](triton-g-doze-semanas.json) | [PDF](triton-g-doze-semanas.pdf), [PNG](triton-g-doze-semanas.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/triton-g-doze-semanas/) | architecture | Em que ordem o braço Triton G acontece em três meses, e onde ficam os portões? | [W3SDS4_TRITON_PLANO_202610.pt.md](../W3SDS4_TRITON_PLANO_202610.pt.md) |
| [kokkos-one-source-backends.json](kokkos-one-source-backends.json) | [PDF](kokkos-one-source-backends.pdf), [PNG](kokkos-one-source-backends.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/kokkos-one-source-backends/) | architecture | How does one Kokkos source reach the CPU and the GPU of the lab's workstation? | [kokkos/README.md](../../kokkos/README.md) |
| [kokkos-interop-one-binary.json](kokkos-interop-one-binary.json) | [PDF](kokkos-interop-one-binary.pdf), [PNG](kokkos-interop-one-binary.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/kokkos-interop-one-binary/) | architecture | How do the original Fortran routine and its Kokkos kernel coexist in one WW3 binary? | [kokkos/README.md](../../kokkos/README.md#the-bindc-boundary) |
