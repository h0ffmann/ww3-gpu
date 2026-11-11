---
description: "Redraw the stale drawings from their current sources: the Mermaid figures (pubs/figures/mermaid/) and the Archify PDFs (docs/archify/). On demand only; nothing runs this on a merge."
argument-hint: "[mermaid|archify]  (default: both)"
---

Bring the committed drawings back in line with their sources, redrawing only what is stale. Run it
when a source changed: a Mermaid fence, a `docs/archify/<slug>.json`, the `vendor/archify` pin.
Nothing in CI or on a merge redraws them, by the owner's choice (2026-10-17).

A drawing is evidence the reader trusts, so the audience rule of `AGENTS.md` holds: a label you
change traces to a file you read, a WW3 fact cites `model/src/<file>.F90:<line>` at the pin, and
the figure's *How to read this figure* card carries the `(v)`/`⚠` marks.

**1. Mermaid** (skip if `$ARGUMENTS` is `archify`). Run `python3 scripts/figures.py check`. If it
names stale figures, run `just figures`, which re-renders only the fences whose hash changed, and
`python3 scripts/figures.py check` again.

**2. Archify** (skip if `$ARGUMENTS` is `mermaid`). Run `just archify check`. If it names stale
drawings: `just archify init` when `vendor/archify/archify/` is missing, then `just archify
refresh`, which renders and prints only those and rewrites `docs/archify/pdf.lock`, and redraws the stale dark PNGs of `docs/archify/dark.tsv` with `png.lock`. A failed
`finalize` gate stops it with the PDF and the lock untouched: read the diagnostics, fix the JSON,
run it again. Then `just archify check` must say every drawing is current.

**3. The two drawings of one figure.** Mermaid and Archify draw the same figure from two sources,
and no tool keeps them in agreement. For every Mermaid fence that changed on this branch
(`git diff origin/main -- <page>`) and has a twin in the table of `docs/archify/README.md`, tell
the owner the twin's JSON may now say something else, and name the node or label. Edit the JSON
only when asked, then repeat step 2.

**4. Look before committing.** For each PDF that changed, rasterise page 1
(`pdftoppm -r 60 -png -f 1 -l 1 <pdf> <out>`) and read it: the whole diagram on the page, no
clipped label. A drawing nobody looked at is `⚠` in the commit.

**5. Commit** the PDFs, PNGs, `pubs/figures/mermaid/index.json`, `pubs/figures/README.md` and
`docs/archify/pdf.lock` that changed, with a `Tested:` trailer naming the two `check` commands and
their output. Do not wire either step into a workflow or a hook.
