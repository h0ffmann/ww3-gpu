---
name: figure
description: "Add, change or explain a Mermaid diagram or mind map anywhere in this repo's Markdown (course lessons, the proposal's mind maps, docs). Use when asked for a diagram, flowchart, mind map, gantt or 'a picture of' something, to update one after its prose changed, or when scripts/figures.py check fails. Bar charts of a measured table go through `figures.py chart`."
---

**Audience.** The repo's first readers are scientists (PhD and independent researchers; see
`CONTRIBUTING.md`, "Who this repository is for"): a figure's claim and evidence are written for
them. The reading card under each figure is the exception that serves newcomers (students, the
DEL committee, researchers from another field): simplify the words, never the physics, so a
wave modeller reading it finds nothing wrong.

# figure

A diagram is a ```` ```mermaid ```` fence in the page it illustrates; GitHub draws it there.
`scripts/figures.py` is the harness: it requires a header and a reading card on every fence,
renders each one with a pinned mermaid-cli and font for the PDF and Word builds
(`pubs/filters/mermaid.lua` swaps the fence for the render), and keeps the gallery
`pubs/figures/README.md`. CI runs `python3 scripts/figures.py check`.

## Steps

1. **Read the prose the figure sits beside.** Draw only what it says; the text is the source of
   truth and a figure that disagrees with it is a bug in the figure. If a copy of the figure
   exists in the other language (proposal mind maps ↔ lesson 13), check both say the same.
2. **Write the fence.** First two lines, which Mermaid ignores:

   ```
   %% figure: <id>          lowercase words joined by '-', unique in the repo
   %% title: <question>     the question the figure answers; becomes the PDF caption
   ```

   Then the diagram, at most about 15 nodes; split it rather than grow it. A `gantt` also needs
   `todayMarker off`, or the render changes with the day it was made.
3. **Write the reading card** right after the closing fence, in the page's language:

   ```
   <details open>
   <summary>How to read this figure</summary>     (pt-BR: Como ler esta figura)

   **Takeaway.** one plain sentence, what to remember        (Em uma frase.)
   **How to read.** direction, what shapes and arrows mean   (Como ler.)
   **Not shown.** what was left out on purpose               (Fora da figura.)
   **Evidence.** file, paragraph or command, with (v) or ⚠  (Evidência.)

   </details>
   ```

   `open` keeps the explanation visible under the diagram; a reader should never have to find it.
   Each label is its own paragraph (blank lines between). The takeaway has no jargon, not even
   *spectrum* or *kernel*. Evidence names the paragraph the figure summarises; a discrepancy you
   found between two copies or between figure and text is stated there with ⚠, not hidden.
4. **Render**: `just figures` (pinned mermaid-cli, Geist and DejaVu Sans from `flake.lock`'s nixpkgs).
   It re-renders only fences whose source changed and rewrites `index.json` and the gallery.
5. **Look at it**: open `pubs/figures/mermaid/<id>.png`. Fix crowding in the source (shorter
   labels, `<br/>`, `direction`, splitting), never by editing a render.
6. **Check and commit** the page, the renders, `index.json` and the gallery together:
   `just figures-check` and `python3 -m unittest tests/test_figures.py`. A fence under
   `pubs/proposal/` also goes through the `revisor-proposta` subagent.

## Visual grammar, the same in every figure

- Rectangle: a thing or a step. Diamond: a test, written as a question. Every test labels both
  its *yes* and its *no* edge.
- Solid arrow: makes or leads to the next thing. Dashed arrow (`-. label .->`): checks, tests or
  compares against. Label every arrow that is not obvious from its two ends.
- No self-loops: an arrow from a box back to itself says nothing about what it is compared with.
  Draw the check as a dashed arrow to the thing it is checked against.
- Number boxes that happen in order (`1 ·`, `2 ·`) so the sequence reads without the arrows.
- Mind-map leaves say what role they play when a branch mixes kinds (`Risco 1 ·`, `Resposta:`,
  `Causa:`); a reader cannot tell a problem from its fix by position alone.
- One flow direction per figure (`LR` or `TD`); groups (`subgraph`) for stages.
- Colour comes from the house style, not from the fence: leave `classDef`, `style` and
  `%%{init}%%` out. A colour never carries meaning on its own; the label or shape says it.

## House style

`pubs/figures/mermaid-config.json` applies to every render. It follows the editorial skin of
[`diagram-design`](https://github.com/cathrynlavery/diagram-design) (MIT), the diagram rules
`marola-dev/agent-skills` catalogues for the marola docs: a hand-drawn look (`look: handDrawn`,
fixed `handDrawnSeed`, so renders stay byte-identical), Geist, a warm paper fill with ink-coloured
strokes, one accent (`#eb6c36`, atomic tangerine) kept for the centre of a mind map and the second
series of a chart, and muted sage, dusty-blue, mustard, rust and slate tints for mind-map branches.
Changing the file marks every render stale (it is part of each figure's sha256 in `index.json`);
re-render all of them in the same commit. GitHub's own preview of a fence ignores this file and
draws its default theme.

## Not this skill

A Mermaid chart of typed-in numbers is not evidence. A measured table (a profile, a benchmark)
becomes a bar chart with `python3 scripts/figures.py chart TABLE.md --label COL --value COL
[--value COL] --id ID --title QUESTION --y-title 'UNIT'`: the first value column is the bars,
any others are lines, and a `total` row is skipped. It prints the fence and a card stub; the
fence keeps a `%% data:` line, and `check` fails if the chart and its table drift apart. Edit the
table, never the chart, then rerun the command. Anything a bar chart cannot show (error bars,
scaling curves, distributions) comes from the data with a plotting script that is committed and
named in the figure's evidence.
