# Publications: markdown to PDF and Word

The texts of this repository have Markdown sources here
([ADR-0023](../docs/ADRs.md#adr-0023)): the UFRJ/DEL project proposal, in
English and Portuguese, which CI builds on every merge to `main` and commits to [`pdf/`](../pdf/);
the preprint in [`preprint/`](preprint/README.md); and, when it starts, the thesis. The directory
`pdf/` keeps that name for the sake of existing links, though it also holds `.docx` files. The
course and the book built from it were removed on 2026-11-11.

A short document is a report in easy Brazilian Portuguese for Prof. Pedro Cruz, D.Sc.
(UFRJ), in a full version, [`relatorio.easy.pt-BR.md`](relatorio-gta/relatorio.easy.pt-BR.md), and a
short one of about 15 minutes' reading, [`relatorio.curto.pt-BR.md`](relatorio-gta/relatorio.curto.pt-BR.md).
`scripts/build_relatorio.sh` turns them into the pages `/relatorio-gta/` and `/relatorio-gta/curto.html`,
and with `--pdf` prints `pdf/relatorio-gta-pedro-cruz.pdf` and `pdf/relatorio-gta-pedro-cruz-curto.pdf`
with Chromium, since it needs no TeX.

The toolchain (pandoc, TeX Live, Python) comes from
[`picasso/labs/publisher`](https://github.com/h0ffmann/picasso/tree/main/labs/publisher). The
root `flake.nix` uses its `mkPdf` and `mkDocx` helpers, so `nix build .` produces the two proposal PDFs and
their Word files in a sandbox.

```bash
just proposal-docx pt     # the proposal for review in Word -> build/proposal_pt.docx (pt|en)
just proposal en          # pubs/proposal/en/*.md -> build/proposal_en.pdf (DEL proposal layout)
just proposal pt ieee     # Portuguese copy; second arg picks the citation style: abnt (default) | ieee
just translate            # pubs/proposal/en -> pt via any OpenAI-compatible endpoint (changed files only)
just proposal-review      # parecer do revisor-proposta sobre pubs/proposal (ABNT, DEL, registro científico)
just pubs                 # both proposal PDFs
```

## Word files

pandoc writes the `.docx` directly, so the LaTeX templates do not apply and Word's defaults do the
styling. The proposal's Word file has no DEL cover page or signature block: it is the text the
advisors comment on, and the signed document is the PDF. Citations go through citeproc with the
same CSL as the PDF, so author-date calls and the reference list match the paper version. The build
is byte-reproducible (pandoc dates every zip entry 1980-01-01), so a rebuild that changes nothing
commits nothing.

## The proposal

The proposal is written in English under `proposal/en/`. `proposal/pt/` started as a machine
translation and was then revised by hand (2026-09-15 and 2026-09-16), so it is the reference
Portuguese text. `just translate` rewrites a `pt/` file only when its English source changes, or with
`--force`, and either would discard that revision. After editing the English, port the change to the
Portuguese by hand.

The schedule (`08-schedule.md`) stays a two-column Activity | Deadline table in the Markdown.
In the PDF and the Word file, `filters/cronograma.lua` draws it as the month grid a cronograma is
read as: one row per activity, one column per month, a bar in the activity's month and a diamond for
the defence. Change the dates in the table only; the grid and the Gantt figure in
`proposal/mapas-mentais.pt.md` follow it, and `scripts/figures.py check` fails if that figure's
months drift from the table.

The DEL section names are a fixed glossary in `scripts/translate_md.py`. Header fields (student,
advisors, date) live in `proposal/meta.{pt,en}.yaml`. The LaTeX layout is the department's own
proposal template (`proposal/template.tex`, styles under `proposal/shared/`).

Every change to `proposal/` is reviewed by the `revisor-proposta` subagent before the pull request.
It checks Escola Politécnica's Resolução 05 de 28/11/2012 and the DEL section structure, ABNT
citation practice, impersonal scientific register in pt-BR, and the wave-modelling and HPC
vocabulary. It reports and does not rewrite. `just proposal-review` runs it, and
`.claude/hooks/proposal-review.sh` reminds any agent that edits a file there to run it before
finishing. See [`CONTRIBUTING.md`](../CONTRIBUTING.md).

The translation backend reads `TRANSLATE_BASE_URL`, `TRANSLATE_API_KEY` and `TRANSLATE_MODEL`.
Locally, `http://127.0.0.1:11434/v1`, `ollama` and an Ollama model work. In CI the same three names
are repository secrets; without them the step is skipped and the committed `pt/` is used.

## The preprint

[`preprint/`](preprint/README.md) holds a short preprint, not peer reviewed, that compares this
repository's C++/Kokkos port with the Rust port in
[h0ffmann/chaceon](https://github.com/h0ffmann/chaceon) under the same bit-for-bit rule. Its
source is Pandoc Markdown, built to LaTeX and PDF by `just preprint` with h0ffmann/closed-source's
template.

## Explaining the vocabulary

`/eli5 <topic>` explains any of this (the action balance equation, a switch file, a Kokkos backend,
bit-for-bit parity) to someone who has never seen it, grounded in `docs/GLOSSARY.md`, in the language you ask in ([`.claude/skills/eli5`](../.claude/skills/eli5/SKILL.md);
adapted from the community skill by Thariq Shihipar, MIT).
