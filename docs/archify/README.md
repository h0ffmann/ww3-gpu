# Archify pages

Interactive HTML twins of Mermaid figures, drawn with the `archify` skill (tt-a1i/archify v3.0.1,
MIT). The skill is the submodule `vendor/archify`, pinned and sparse, with
`.claude/skills/archify` a symlink into it; nothing of it is copied here. Each page is one
standalone file with inline SVG, light and dark themes, and image export.

A page is never a figure of the book: the figure is the Mermaid fence with its *How to read this figure* card, which carries the evidence and the `⚠` marks; every node and label traces to a file read, a WW3 fact cites `model/src/<file>.F90:<line>` at the pin, and a speed figure says "regtest pending" until WW3's regtests have run on its commit.

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
source badges hidden and the viewBox cut to the height `dark.tsv` gives.

```bash
just archify png           # each drawing of dark.tsv -> <slug>.png (committed), then png.lock
just archify site <dir>    # each drawing of dark.tsv -> <dir>/<slug>/index.html, and <dir>/index.html
```

`png` takes the picture with headless Chrome over its DevTools pipe
([`scripts/archify_shot.py`](../../scripts/archify_shot.py)), at twice the drawing's size; the
README shows it. `site` is what `pages.yml` runs to publish the pages at
<https://h0ffmann.github.io/ww3-gpu/figures/>, so no HTML is committed. `check` and `refresh`
cover the PNGs too: [`png.lock`](png.lock) records the sha256 of each drawing's JSON, the look,
`dark.tsv` and the dresser, and the Archify commit.

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
| [ww3-gpu-four-runtimes.json](ww3-gpu-four-runtimes.json) | [PDF](ww3-gpu-four-runtimes.pdf), [PNG](ww3-gpu-four-runtimes.png), [page](https://h0ffmann.github.io/ww3-gpu/figures/ww3-gpu-four-runtimes/) | architecture | Which four runtimes run WAVEWATCH here, and what must each give the same bits as? | [README](../../README.md), the preprint's Figure 1 ([`pubs/preprint/`](../../pubs/preprint/README.md)) |
