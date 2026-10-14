# DESIGN.md

The look of the website (`site/`, `just site`), in rules. It follows the manner of the author's
sister sites, h0ffmann/h0ffmann and h0ffmann/closed-source, with its own palette. The record of
the decision is [ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md).

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

Every fact on the page comes from a file of this repository, through `site/content`: the chapters
from `course/`, the decisions from `docs/ADRs/`, the designs from `docs/WFIPs/`, the notes from
`notes/`, the port's benchmarks from `kokkos/port_report/`, the DOI and the people from `CITATION.cff` and `.zenodo.json`, the latest release from
`RELEASES.md`. A fact that no other file holds (a related author, a sister work) is a row of
`site/data/related.tsv`, with its source. Never type a title, a status, a date, a count or a DOI
into `site/src`.

## Logos and pictures

No logo is stored as a file, and none is drawn. The favicon is an original drawing of two waves.

## Motion

None. The page is prerendered and loads no JavaScript.

## Before you change the look

1. Change this file in the same pull request.
2. `just site`, then look at the page at 1280 px and at 390 px.
3. Tab through the page: every link shows its focus ring.
4. Make the pictures in `site/preview/` again.
