# DESIGN.md

The look of this repository's pictures, in rules: the palette, the type and the dark manner of every
drawing. The record of the decision is [ADR-0009](docs/ADRs.md#adr-0009). The website's front page
and its lineage moved to piabrota/nix-os on 11 November 2026 (`ww3-site/`, served at
wave.piabrota.art, [ADR-0024](docs/ADRs.md#adr-0024)); the rules of that page's layout, words and
two languages are kept there now, with its code.

## One sentence

A sea at night, and three lights on it.

## The three lights

Each light means one thing, everywhere it is used. A thing gets one light. Words always back up
the colour: a reader who does not see colour loses nothing.

| Light | Hex | It means |
|---|---|---|
| `--wave` | `#36B7FF` | the sea: the physics, the model, WW3; the links and the title |
| `--code` | `#FFB547` | the machine: the GPU, the port, the code; a record not yet decided (proposed, draft) |
| `--proof` | `#3DFF96` | evidence: `(v)`, an accepted decision, a verified note |

`--warn` (`#FF4D62`) is not a light. It is the `⚠` of an unchecked claim, and it is rare.

No fourth light. A new colour is a new meaning, and it starts here.

## The ground

Dark only, with no light theme: `--night #05090E` behind everything, `--deep #0A121B`,
`--surface #0E1924`, `--rule #1C2B38` for the lines between sections, `--ink #E7F2EF` for the text
and `--muted #8FA3AD` for what is secondary. One gradient: a faint `--wave` glow at the top right of
the page.

## Type

- Bricolage Grotesque (800 and 600) for the title and the headings.
- Newsreader for the prose.
- JetBrains Mono for data: ids, dates, DOIs, statuses, the navigation.

The fonts load from Google Fonts, as on the sister sites. No font file is stored here.

## Logos

No logo is stored as a file, and none is drawn. The website's favicon, an original drawing of two
waves, lives with the page's code in piabrota/nix-os.

## Pictures

All pictures made here are dark: the ground above (`--night`, `--surface`, `--rule`,
`--ink`, `--muted`) and the three lights, with the same meanings. A picture is never drawn on a
white or light grey ground, and no picture has a light theme to switch to. The manner is that of
piabrota/nix-os's figures (its `docs/archify/piabrota.css` and `just figure`): only the drawing,
no toolbar and no grid, on the night ground with the faint `--wave` glow.

- **Every diagram is an Archify drawing**, never Mermaid (the owner, 11 November 2026). A
  diagram that is not yet redrawn stays a Mermaid fence only until it is ⚠.
- **An Archify drawing** (`docs/archify/<slug>.json`) is dressed by
  [`docs/archify/ww3-gpu.css`](docs/archify/ww3-gpu.css): a box for a source the repository reads
  is `--wave`, a box for what it builds or runs is `--code`, a gate or a check is `--proof`, and
  every other kind of box is neutral. A "same bits as" (dashed) arrow is `--proof`, an emphasised
  arrow `--code`, every other arrow `--muted`. Labels are Bricolage Grotesque, the small text JetBrains Mono.
  Archify's legend and its source badges are hidden: the reading card under the picture carries
  that, in words.
- **Where it lives.** A drawing listed in `docs/archify/dark.tsv` is a PNG for the page that shows it
  (`just archify png`, committed with `png.lock`) and an HTML page on the website, at
  `/figures/<slug>/`, built by `pages.yml` with `just archify site`. The picture links to
  the page. No HTML is committed.
- **Every picture names its WAVEWATCH III.** A strip under the drawing prints the `ww3` column of
  `docs/archify/figures.tsv` in `--muted` JetBrains Mono, 1/69 of the frame's width and never under
  9 viewBox units (7.1 pt on the paper row), so the PNG, the PDF and the page all say which WW3 the
  figure is drawn from. The website's figure index groups the drawings by `docs/archify/groups.tsv`,
  each group opened by a paragraph that ties it to the next.
- The PDFs of `docs/archify/` are printed in the same look, and a paper prints those PDFs:
  `scripts/preprint.sh` copies the preprint's Figure 1 from `docs/archify/ww3-gpu-runtimes.pdf`
  at every build.

### Pictures in a paper

A figure attached to a paper is drawn for the page that prints it and for the eye that reads it
(the owner, 11 November 2026). A drawing made for a wide screen and scaled down to a column is a
defect, however correct its content: the preprint's first Figure 1 was 1 510 units wide on a
174 mm column, so the text of its boxes printed at about 3 pt (v, `scripts/archify_fit.py` on
`docs/archify/ww3-gpu-four-runtimes.json` at `56c5a32`, 2026-11-11).

- **Proportion.** At the width it is printed, the frame is between 5:4 and 2:1, width to height.
  A wider frame is a strip whose type shrinks with it; a taller one takes the page from the text.
- **Type the eye can read.** At the printed width the text of a box is at least 7 pt and no text
  is under 6 pt. Elsevier's artwork rules ask for the same: "a finished, printed size of 7 pt for
  normal text" and "no smaller than 6 pt for subscript and superscript characters" (v, Elsevier,
  "Artwork sizing", read 2026-11-11). Archify draws a box's label at 11 units, its second line at
  9 and an arrow's label at 8 (v, the SVG of `ww3-gpu-runtimes` at `vendor/archify` `2ab3cae`),
  so a drawing printed 174 mm wide is at most 634 units wide.
- **Balance.** One reading direction. The thing every other box is compared with sits in the
  centre or at the start, not at an edge. The frame has the same margin on every side
  (`dark.tsv` gives the frame as height, x, y, width), and no half of it is empty. Say once what
  a repeated arrow means, in its label or in the caption.
- **What checks it.** [`docs/archify/paper.tsv`](docs/archify/paper.tsv) lists each drawing a
  paper prints, with the printed width in mm. `just archify png` refuses to draw one whose
  proportion or type does not fit, and `just archify fit` measures every row
  ([`scripts/archify_fit.py`](scripts/archify_fit.py)). Balance is a person's judgement: build
  the paper and look at the page at 100 %.
- A paper in Portuguese prints the drawing in Portuguese (`<slug>-pt.json`), not the English one
  with a note.

The report for the GTA (`pubs/relatorio-gta/`) prints `ww3-gpu-at-a-glance-pt` and
`kokkos-interop-one-binary`, drawn for a screen. At 174 mm their box text would print at 3.9 pt
and 3.8 pt (v, `scripts/archify_fit.py`, 2026-11-11); they are not in `paper.tsv` and are not
redrawn yet ⚠.
