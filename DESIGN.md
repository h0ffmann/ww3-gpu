# DESIGN.md

The look of the website (`site/`, `just site`), in rules. It follows the manner of the author's
sister sites, h0ffmann/h0ffmann and h0ffmann/closed-source, with its own palette. The record of
the decision is [ADR-0009](docs/ADRs.md#adr-0009).

## One sentence

A sea at night, and three lights on it.

## The three lights

Each light means one thing, everywhere on the page. A thing gets one light. Words always back up
the colour: a reader who does not see colour loses nothing.

| Light | Hex | It means |
|---|---|---|
| `--wave` | `#36B7FF` | the sea: the physics, the model, WW3; the links and the title |
| `--code` | `#FFB547` | the machine: the GPU, the port, the code; a record not yet decided (proposed, draft) |
| `--proof` | `#3DFF96` | evidence: `(v)`, an accepted decision, a verified note |

`--warn` (`#FF4D62`) is not a light. It is the `⚠` of an unchecked claim, and it is rare.

No fourth light. A new colour is a new meaning, and it starts here.

## The port, measured

The one place a light has shades. Each bench row of `kokkos/port_report/*_KOKKOS.md` is a step,
drawn as a bar as long as the log of its speed-up over its own baseline. The bar is `--proof`,
because a measured number is evidence, at 30 %, 55 %, 80 % and 100 % for a speed-up under 2x,
under 10x, under 100x and from 100x. A step slower than its baseline is `--muted`, never green,
and the words say "slower". The speed-up is computed from the report's numbers, never typed.
Above the steps, the port's global numbers (`site/data/port_score.tsv`, written by
`scripts/port_score.py`): for each level, translated, ported (called by WW3 at the pin) and
regtested, the share of the run time and of the lines of code, and the size of the Kokkos code,
each value in the `--code` light. Under each
report's name, its record from `kokkos/ports.tsv`: regtested, a `--proof` chip for "yes" and a
`--warn` chip for "no" (the googletest column went with the port's own tests, ADR-0010).

## The lineage

A second page, `/lineage/` (and `/pt-BR/lineage/`), linked from the front page's navigation: the
works this one descends from or runs beside, on one vertical line of time, oldest first. Each event
is a row of `site/data/lineage.tsv`: its date, only as precise as its source (`2025`, `2025-11` or
`2025-11-17`), done or planned, a kind, its family, a link, one line of what it is, and where the
date was read. A dot in the `--wave` light is a model (wave, ocean or weather), because a model is
the sea; one in the `--code` light is a port, a model moved to other hardware or another language,
because a port is the machine. A planned event has a hollow dot and a `--code` chip that says
"planned", since it is not yet decided. No new light: a kind other than model or port starts here.
`site/content` stops the build on a row with a bad date, status or kind. To track a new work, add a
row.

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

## Shape and words

- **Lower case** for what the page says in its own voice: headings, buttons, the story. A fact read
  from a file keeps its own case (a chapter's title, an ADR's decision), and so does an acronym.
- **Round.** Buttons and status chips are pills. A section is set apart by a line, not a box.
- **Dashed** means "not current": a rejected or superseded record.
- Glow is in the light's own colour, and only on the title and the main button. Never on prose.

## Facts

Every fact on the page comes from a file of this repository, through `site/content`: the
decisions from `docs/ADRs.md`, the designs from `docs/WFIPs/`, the notes from
`notes/`, the port's benchmarks from `kokkos/port_report/`, the DOI and the people from `CITATION.cff` and `.zenodo.json`, the latest release from
`RELEASES.md`. A fact that no other file holds (a related author, a sister work) is a row of
`site/data/related.tsv`, with its source, and a dated event of the lineage is a row of
`site/data/lineage.tsv`. The sister port in Rust, chaceon, is a `port` row there,
and each fact the page states about it is a `chaceon` row linked to the file of chaceon that says
it, at a named commit; its section and its button in the cover are `--wave` and quiet, like the
repository's. Never type a title, a status, a date, a count or a DOI
into `site/src`.

## Two languages

The page is written once, in English, and built twice: at `/` in English and at `/pt-BR/` in
Brazilian Portuguese, as h0ffmann/h0ffmann does since its PR #86. Each sentence of `site/src` goes
through `t(l, "…")`, and its Portuguese is a row of `site/data/translations/pt-BR.tsv`, keyed by
the English sentence itself: source, translation, status (`draft` until a native reader reads it,
then `reviewed`; an agent never writes `reviewed`), and a note when the translation is not the
literal one. `site/content` stops the build when a sentence has no row, or a row no sentence, so a
translation never stays beside a sentence it no longer translates. The words come from
`README.pt-BR.md` first, and `humanizar` is the filter. A fact read from a file (a chapter's title,
an ADR's decision, a status) keeps its own language. Each page links to the other, named in that
other language: "português" and "english". The language is the address, so the page still loads
no JavaScript.

## Logos and pictures

No logo is stored as a file, and none is drawn. The favicon is an original drawing of two waves.

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

## Motion

None. The page is prerendered and loads no JavaScript.

## Before you change the look

1. Change this file in the same pull request.
2. `just site`, then look at the page at 1280 px and at 390 px.
3. Tab through the page: every link shows its focus ring.
4. Make the pictures in `site/preview/` again.
