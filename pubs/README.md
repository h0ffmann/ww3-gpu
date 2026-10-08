# Publications: markdown to PDF and Word

Two documents are built from the markdown in this repository: the book, *Without Changing the
Answer* (working title), whose chapters are the lessons in `course/`, and the UFRJ/DEL project
proposal, in English and Portuguese. CI builds them on every merge to `main` and commits the
results to [`pdf/`](../pdf/). The directory keeps that name for the sake of existing links, though
it also holds `.docx` files.

The book is the repository's public form ([ADR-0003](../docs/ADRs/ADR-0003-repository-is-a-book.md)):
a book in progress on agentic coding and agentic research, written around moving WW3's kernels to
GPUs without changing the answer. [WFIP-0002](../docs/WFIPs/WFIP-0002-lab-as-a-book.md) is the plan
that turns the lessons into parts, adds the chapters on how this lab is run with agents, checks
every quoted listing against the code on each build, and attaches the PDF to every release. Until
its tasks land, `pdf/ww3-lab-course.pdf` is the sixteen lessons under the new title, one chapter
each.

The toolchain (pandoc, TeX Live, Python) comes from
[`nix-config/labs/publisher`](https://github.com/h0ffmann/nix-config/tree/main/labs/publisher). The
root `flake.nix` uses its `mkPdf` and `mkDocx` helpers, so `nix build .` produces the three PDFs and
the Word files in a sandbox.

```bash
just book                 # course/*.md -> build/ww3-lab-course.pdf (one chapter per lesson)
just book-docx            # the same course as build/ww3-lab-course.docx (Word, no TeX in the path)
just proposal-docx pt     # the proposal for review in Word -> build/proposal_pt.docx (pt|en)
just proposal en          # pubs/proposal/en/*.md -> build/proposal_en.pdf (DEL proposal layout)
just proposal pt ieee     # Portuguese copy; second arg picks the citation style: abnt (default) | ieee
just translate            # pubs/proposal/en -> pt via any OpenAI-compatible endpoint (changed files only)
just proposal-review      # parecer do revisor-proposta sobre pubs/proposal (ABNT, DEL, registro científico)
just pubs                 # all three PDFs
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

## Explaining the vocabulary

`/eli5 <topic>` explains any of this (the action balance equation, a switch file, a Kokkos backend,
bit-for-bit parity) to someone who has never seen it, grounded in `docs/GLOSSARY.md` and the
lessons, in the language you ask in ([`.claude/skills/eli5`](../.claude/skills/eli5/SKILL.md);
adapted from the community skill by Thariq Shihipar, MIT).
