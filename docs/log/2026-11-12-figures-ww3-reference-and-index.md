# Every figure names its WAVEWATCH III, and the figure index is grouped

2026-11-12, the owner with an agent. Goal `UrLTXtPC-5203`, from the owner's request in the Claude
project thread: read MetOcean's WW3 7.14, put that reference on the drawings that lack it, and
index the figures with text that connects them.

## Question

Of the 17 Archify drawings, 2 named the WW3 they are drawn from in the picture (the two
`ww3-gpu-runtimes` drawings) and 4 more named a commit somewhere in their source (`grep` of
`7.14`, `92e2cf66`, `03adda91`, `769c44f0` in `docs/archify/*.json`, v). The site's `/figures/`
index was a flat list of titles. What does MetOcean's 7.14 change for the pages that describe WW3,
and can every picture carry its reference?

## What was done

- MetOcean's tree read at `92e2cf66` (`WW3-metocean-7.14/`, tag `7.14.mos-92e2cf6`) beside the
  fork at `96892c95`: `VERSION` and `WWVER` are 7.14 in both; the manual differs in 11 files; the
  four `model/src` lines `docs/ww3-manual/` cites were read again. GitHub's web pages returned
  HTTP 403 from the session's proxy, so nothing was read from github.com in a browser ⚠.
- `docs/archify/figures.tsv` and `groups.tsv`: a row per drawing (group, twin, page, question,
  WW3 stamp) and five groups with the paragraph that opens each.
- `scripts/archify_dress.py` prints the stamp in a strip under every drawing; `png.lock` hashes it.
  `scripts/archify_index.py` writes the grouped index; each figure page's bar links the page it
  illustrates. `tests/test_archify_index.py` gates both tables.
- All 17 PNGs and PDFs redrawn (`just archify refresh`, Chromium 1243 headless).

## Result

- (v) Every drawing now prints its WW3: 15 the default (`--ww3` MetOcean `7.14.mos-92e2cf6`,
  the port on `h0ffmann/WW3 96892c95`, NOAA `03adda91`), 3 of them in Portuguese; 2 the fork
  commit `769c44f0` whose lines they cite.
- (v) The paper rows still fit: `ww3-gpu-runtimes` and `-pt` print at 174 x 104 mm, 1.67:1, box
  text 7.1 pt, smallest text 6.3 pt (`just archify fit`), from 174 x 99 mm, 1.76:1. The bottom
  margin of these two is now wider than the others by the strip ⚠ (balance is a person's call).
- (v) At MetOcean's 7.14, `NL1` has no Gaussian Quadrature Method and the `ST4` manual adds a
  long-wave modulation factor of the saturation; `docs/ww3-manual/README.md`, "At MetOcean's 7.14,
  the reference", lists what differs and where.
- ⚠ The front page of wave.piabrota.art (piabrota/nix-os) was not changed: this session has no
  access to that repository. It can read `docs/archify/figures.tsv` for the same grouping.
