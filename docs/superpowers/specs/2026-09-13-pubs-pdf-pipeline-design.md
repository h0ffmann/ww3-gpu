# Publications pipeline: markdown → LaTeX → PDF (course book + UFRJ proposal PT/EN)

**Date:** 2026-09-13 · **Status:** approved design, awaiting spec review · **Owner:** hoffmann

## Goal

Two PDF products built from markdown in this repo, reproducibly, locally and in CI:

1. **The course book**: one PDF compiled from `course/00…11-*.md` in lesson order, with title
   page, table of contents and one chapter per lesson.
2. **A "Proposta de Projeto de Graduação"** for UFRJ's Escola Politécnica / DEL, in English (source of truth) and Portuguese (generated), in the department's proposal format: header block,
   student and advisor lines, eight numbered sections (Título, Ênfase, Tema, Delimitação,
   Justificativa, Objetivo, Metodologia, Cronograma), references, place/date and signature lines.
   The reference layout is the owner's 2020 proposal (MQTT/Kafka platform, advisor Miguel Elias
   Mitre Campista, D.Sc.), reproduced from the text pasted on 2026-09-13. This is a short
   `article`-class document, **not** the book-class thesis template of forecast-energy-demand;
   only the translation tooling is borrowed from that repo.

### The proposal's subject

*Redução do tempo de simulação e previsão do WAVEWATCH III na operação da ReNOMO (LabECO/UFSC)*,
working title. Co-advised by **Pedro Veras Guimarães, Dr.** (doctorate in fluid mechanics, École
Centrale de Nantes, 2018; "Dr." is the correct rendering, not D.Sc.), head of the Laboratório de
Engenharia e Ciências Oceânicas (LabECO), Departamento de Engenharia Mecânica, UFSC, within the
Rede Nacional de Observação e Monitoramento Oceânico (ReNOMO, CNPq/MCTI/Finep 062/2022,
coordinated from IO-FURG). The UFRJ/DEL advisor line is left as a metadata field to fill.

The argument, in the order the sections will make it:

1. **Tema/Justificativa:** operational wave forecasting at LabECO runs WW3; wall-clock time
   bounds how many runs, members and resolutions a day are possible.
2. **Delimitação:** a ladder of increasingly invasive optimisations, each measured on the lab's
   own configuration before the next is attempted: (a) compile-time switches and build flags
   (`switch` file, `-O3 -march=native`, OpenMP/MPI hybrid layout, NetCDF-4 I/O); (b) run
   configuration (domain decomposition, time steps, output frequency); (c) targeted modern-Fortran
   refactors of the measured hotspots (`W3SRCE`, propagation); (d) GPU feasibility on an H100,
   framed honestly as a study, since upstream WW3 has no GPU path and the only published port got
   ~1.3× (docs/KOKKOS_H100_PLAN_202609.md, docs/AGENTS_KOKKOS_202609.md).
3. **Objetivo:** measurable: X× reduction of the operational cycle's wall-clock at equal output,
   with a reproducible benchmark and a written recommendation for the lab.
4. **Metodologia:** profiling first (regtest closest to the operational grid, `perf`/`gprof`,
   the `ww3-lab` just recipes and Nix toolchain), then one rung at a time, each gated by
   bit-for-bit or tolerance parity against the current run; results published in this repo.
5. **Non-overlap with WW4:** the proposal states explicitly that it does not contribute to
   NOAA-EMC/WW4 (pre-alpha, C++ rewrite, first release hoped for 2027) and does not fork WW3
   physics; it optimises the operation of the WW3 the lab runs today, and any GPU kernel work is
   documented so it can inform, not compete with, WW4.
6. **Cronograma:** a markdown table of stages and dates, rendered as the numbered table the
   format expects.

Decisions already taken with the owner:

- Markdown source for the proposal, rendered through a LaTeX template that reproduces the DEL
  proposal layout (not raw LaTeX, not a plain pandoc look).
- English is the source language (owner's request, 2026-09-13); Portuguese is machine-translated,
  committed, and overwritten on each run (fixes go into the EN source or the prompt, never into
  `pt/`). The DEL section headings are a fixed glossary in the translator (TÍTULO, ÊNFASE, TEMA,
  DELIMITAÇÃO, JUSTIFICATIVA, OBJETIVO, METODOLOGIA, CRONOGRAMA, Referências Bibliográficas) and
  a heading outside that list fails the translation.
- PDFs are committed back to `main` by CI (parity with forecast-energy-demand), plus uploaded as
  workflow artifacts on every PR.
- Citation style is a recipe option: `abnt` (default, UFRJ) or `ieee`.
- A **new, standalone `flake.nix` at the repo root** provides the toolchain and the builds. The
  WW3 toolchain flake in `nix-config/labs/pratico` is not touched.

## Non-goals

- HTML or EPUB output. Per-file PDFs of `docs/*.md` (can be a later recipe on the same pipeline).
- Editing `course/` content to suit the PDF; the Lua filter adapts, the prose does not.
- A LaTeX-fluent authoring path: authors write markdown only. Raw LaTeX is allowed inline where
  pandoc passes it through, but nothing in `proposal/pt` should require it.

## Layout

```
flake.nix                      NEW: devShell (pandoc, texlive, python+openai, just) and packages
pubs/
  book/
    defaults.yaml              pandoc defaults for the book (metadata, toc, input-files in order)
    template.tex               short xelatex book template (fonts covering ⚠ ✓ →, code style)
    filters/course-links.lua   [x](03-grids.md#anchor) → internal cross-reference
  proposal/
    en/                        SOURCE: 01-title.md … 08-schedule.md, one numbered section per file
    pt/                        GENERATED by scripts/translate_md.py; committed (same file names)
    meta.pt.yaml               header fields: university/school/department lines, doc kind, student,
                               email, advisor (+title), coadvisor (+title, institution), city, date
    meta.en.yaml               same keys in English (both metadata files are hand-written)
    refs.bib                   this proposal's bibliography (WW3, WW4, GPU ports, ReNOMO, LabECO)
    template.tex               article-class pandoc template reproducing the DEL proposal layout:
                               centred header block, "Aluno:"/"Orientador:" lines, numbered
                               sections from $body$, "Referências Bibliográficas", closing
                               "<city>, <date>." and signature lines (Aluno / Orientador / Coorientador)
  csl/abnt.csl, csl/ieee.csl   citation styles (citation-style-language/styles, CC BY-SA 3.0)
pdf/
  ww3-lab-course.pdf           committed by CI on main
  proposal_pt.pdf, proposal_en.pdf
scripts/translate_md.py        PT → EN for markdown, hash-cached, GitHub Models or local llm/Ollama
scripts/build_pdf.sh           the one pandoc+xelatex invocation both just and the flake call
.github/workflows/pubs.yml     build on PR (artifacts); on main push: build, translate, commit pdf/
```

`course/` stays where it is and is read in place; the book's chapter order is the explicit
`input-files` list in `defaults.yaml`, and the build fails if a `course/NN-*.md` exists that is
not in the list.

## Toolchain (`flake.nix`)

- Inputs: `nixpkgs` pinned to the same revision as pratico
  (`eaad089433ca2bb662274377d33df3d0e51ef28b`, pandoc 3.7.0.2, TeX Live 2025) so both flakes
  agree; `flake-utils` for `forAllSystems`.
- `packages.tex`: `texlive.combine { inherit (texlive) scheme-medium babel-portuges
  fontspec unicode-math xetex …; }`. The set is whatever `\listfiles` on the two documents
  needs, recorded in a comment. No Tectonic: sandboxed `nix build` has no network, and one TeX
  engine everywhere is simpler.
- `devShells.default`: pandoc, that TeX Live, `python3.withPackages (openai)`, `just`, `fontconfig`
  with DejaVu and Noto (glyph coverage for ⚠ ✓ ≥ →), `ghostscript` (EPS logo conversion).
- `packages.book`, `packages.proposal-pt`, `packages.proposal-en`: `stdenv.mkDerivation` calling
  `scripts/build_pdf.sh` with `src = ./.` filtered to `course/`, `pubs/`, `scripts/`. Output
  `$out/*.pdf`. `packages.default` = all three via `symlinkJoin`.
- `checks.pubs` = `packages.default`, so `nix flake check` builds every PDF.
- No Poli logo or EPS conversion is needed for the proposal format; `ghostscript` is dropped
  unless the book needs it.

## Build step (`scripts/build_pdf.sh <book|proposal> [pt|en] [abnt|ieee]`)

One script, no logic in the justfile or the flake:

```
pandoc <inputs> \
  --defaults pubs/<target>/defaults.yaml \
  --template pubs/<target>/template.tex \
  --top-level-division=chapter --number-sections --toc \
  --pdf-engine=xelatex --citeproc --bibliography pubs/proposal/shared/refs.bib \
  --csl pubs/csl/<style>.csl --metadata-file pubs/proposal/meta.<lang>.yaml \
  --lua-filter pubs/book/filters/course-links.lua \      # book only
  --fail-if-warnings -o build/<name>.pdf
```

- Book: inputs are the `input-files` list; `--resource-path=course`; `lang: en`.
- Proposal: inputs are `pubs/proposal/<lang>/*.md` sorted; `lang: pt-BR` or `en-US` sets babel;
  `--top-level-division=section` (the format numbers sections 1–8, no chapters); the template's
  header and signature blocks read the `meta.<lang>.yaml` fields; the Cronograma markdown table
  gets a caption via a pandoc table caption line so it renders as "Tabela 1".
- Style option: `abnt` default for both languages; `ieee` selectable. Both CSL files are vendored.
- Output lands in `build/` (gitignored) locally, `$out` under nix.

## just recipes

```
just book [style]              build/ww3-lab-course.pdf
just proposal [pt|en] [style]  build/proposal_<lang>.pdf
just translate [--force]       pubs/proposal/en → pt (changed files only)
just pubs                      book + proposal pt + proposal en (== nix build)
```

Recipes run inside `nix develop` of the new flake (`nix develop . --command …`), the same way the
existing WW3 recipes wrap the pratico shell.

## Translation (`scripts/translate_md.py`)

Ported from forecast-energy-demand's `translate_latex.py`, adapted to markdown and reversed
(EN → PT-BR):

- Same cache file shape (`pubs/proposal/.translation-cache.json`, sha256 of the EN source file) and CLI (`--force`, `--dry-run`).
- Protects, by replacing with numbered placeholders before the API call and restoring after:
  fenced code blocks, inline code, `$…$`/`$$…$$` math, link targets and image paths, citation
  keys (`@key`, `[@key]`), YAML front matter keys, HTML comments.
- Prompt: translate prose only, keep markdown structure, keep placeholders verbatim, American English → Brazilian Portuguese, academic register, with the fixed heading glossary
  above and pandoc's `Table:` caption keyword kept verbatim.
- Backend: OpenAI-compatible client. Default base URL GitHub Models
  (`https://models.github.ai/inference`, `GITHUB_TOKEN`); `TRANSLATE_BASE_URL`/`TRANSLATE_MODEL`
  override to point at local Ollama through its OpenAI endpoint for offline drafts.
- Neither metadata file is generated; both are hand-written (names and institutional lines must
  be exact).

## CI (`.github/workflows/pubs.yml`)

- Triggers: PR touching `course/**`, `pubs/**`, `scripts/build_pdf.sh`, `scripts/translate_md.py`,
  `flake.nix`, the workflow itself; push to `main` on the same paths; `workflow_dispatch`.
- Steps: install Nix (`DeterminateSystems/nix-installer-action`) + magic Nix cache;
  `nix flake check`; upload `result/*.pdf` as artifacts (30 days).
- On `main` push only: `python scripts/translate_md.py` (GitHub Models, `models: read`),
  commit `pubs/proposal/pt/**`, the cache file and `pdf/*.pdf` with `[skip ci]`, push. Same
  bot identity and guard as forecast-energy-demand.
- The existing `ci.yml` lychee job keeps checking links; `pdf/` is binary and ignored by it.

## Error handling

- `--fail-if-warnings` turns undefined references, missing citations and bad cross-links into
  build failures.
- The Lua filter fails the build on a link to a `course/*.md` that is not in the chapter list.
- `build_pdf.sh` refuses unknown target/lang/style arguments with a usage line.
- `translate_md.py` exits non-zero if a placeholder is missing from the model's output for a
  file, leaves that file's EN copy untouched, and continues with the others.
- A missing `GITHUB_TOKEN` (or override URL) is reported once, before any file is processed.

## Testing and acceptance

1. `nix build .#book` produces a PDF where lesson 00's two display equations, the `⚠` and `(v)`
   markers, a bash and a python code block, and at least one cross-lesson link (as a page
   reference) render correctly. Checked by eye once; then `pdfinfo` page count and `pdftotext |
   grep` for those markers in CI as a smoke test.
2. `nix build .#proposal-pt` (generated PT) and `.#proposal-en` produce PDFs; the PT one's first page
   matches the 2020 proposal's layout (header block, Aluno/Orientador lines, "1. TÍTULO" …) and
   whose last page carries the references, the "Rio de Janeiro, <date>." line and the three
   signature lines.
3. `just proposal en ieee` builds with IEEE numbering; `just proposal pt` with ABNT author-date.
4. `scripts/translate_md.py --dry-run` lists exactly the files whose hash changed; a round trip on a
   fixture with code, math, links and citations restores every placeholder byte-for-byte.
5. `nix flake check` passes locally and in CI; the workflow uploads three PDFs on a PR.

## Open items for the implementation plan

- The exact `texlive.combine` package list is discovered during the first build (`\listfiles`).
- The EN text itself is a writing task on top of this pipeline: first draft from
  docs/KOKKOS_H100_PLAN_202609.md, docs/AGENTS_KOKKOS_202609.md and course lessons 09–10, then the
  owner and the co-advisor revise. The UFRJ/DEL advisor name and the cronograma dates are inputs
  from the owner.
- Confirm with the co-advisor how LabECO's operational WW3 is configured today (grid, switch
  file, MPI ranks, forecast horizon and cadence, hardware); the ReNOMO and LabECO web pages do
  not say, and the Delimitação depends on it.
- Generated `pt/` files carry a one-line header comment marking them generated.
