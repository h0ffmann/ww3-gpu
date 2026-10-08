# WFIP-0002: The lab as a book: compiled from the repository, built in CI, released with each version

| | |
|---|---|
| **Status** | Draft |
| **Author** | M. Hoffmann (asked 2026-10-08 in the project thread: shift the repository, incrementally, into a compiled book on agentic coding and agentic research, in the tradition of Volpe's and Maguire's self-published books) |
| **Created** | 2026-10-08 |
| **Deliverable** | D6 |
| **Related** | [ADR-0003](../ADRs/ADR-0003-repository-is-a-book.md) (the decision this WFIP builds); [ADR-0004](../ADRs/ADR-0004-book-channels-and-first-slice.md) (the channels, with the results of task 1 and 4b); marola's [MIP-0014](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md), the design this one is translated from; [`pubs/README.md`](../../pubs/README.md) and [`pubs/book/`](../../pubs/book/) (the pipeline that already exists); [`course/README.md`](../../course/README.md) (the chapters); the `release` skill |
| **Effort** | L: no new toolchain (the pandoc and TeX pipeline is in the root flake); the work is front matter and parts, three new chapters, a listings gate, a PDF on each release, and the metadata |
| **Gain** | `outreach` (a citable, dated book that a researcher can read start to finish, where today there are sixteen lessons and a dozen plans); `proposal` (D6 asks for tools, results and recommendations published so the lab can repeat them: the book is that publication); `lab/dev-loop` (the rules agents follow here become chapters, so a new agent or person reads one text, not twelve) |
| **Effort vs Gain** | do next, in slices: each slice is one PR that leaves the repository consistent, and nothing here competes with a measurement for the same machine |
| **Depends on** | the publisher toolchain in `nix-config/labs/publisher` (exists `(v)` [`flake.nix`](../../flake.nix)); a person's decision on the title, the prose licence and the distribution (§11) |
| **Blocked by** | none |
| **Risk** | the chapters drift from the code they describe: a `file:line` or a pasted listing that no longer exists; MIP-0014 §5.3 names this as the central risk of a book about a moving repository, and this repository already quotes `WW3/model/src/<file>.F90:<line>` by hand in dozens of places |
| **Cost so far** | — |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | [`WFIP-0002.tasks.md`](WFIP-0002.tasks.md) |
| **Tests** | `tests/test_check_listings.py` for the gate of §5.3; none for the prose |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

Turn the repository into a book that compiles from the repository: the sixteen lessons become its
chapters, grouped in parts, with three new chapters on how this lab is run with coding agents and
how it does research with them; every listing and `file:line` in the text is checked against the
code on each build; the PDF is attached to every tagged release and archived with the source on
Zenodo. The code, plans and proposal stay where they are and keep their rules: the book is the
form the repository is read in, not a second repository. The shift is incremental, one PR per
task, and the repository stays consistent after each.

## 2. Motivation

The repository has more written-down method than narrative. `course/` has sixteen lessons `(v)`
[`course/README.md`](../../course/README.md); `docs/` has two plans, an evaluation, a proof note
and a glossary of some 400 terms; `AGENTS.md`, `AGENTS_KOKKOS` and the `wfip` and `release`
skills say how agents port a kernel, write a plan and cut a release `(v)` read 2026-10-08. All of it
is reference: it answers "what did we build" one file at a time, never "how do you run a
scientific port with coding agents without losing the evidence, read start to finish". That is
the same gap MIP-0014 §2 named for marola `(v)` [MIP-0014](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md), read 2026-10-08.

The repository's own working method is what a reader from outside wave modelling asks about: a
Fortran routine ported by an agent and shown bit-identical on three backends `(v)`
[`kokkos/PORT_STATUS.md`](../../kokkos/PORT_STATUS.md); plans whose every claim carries `(v)` or
`⚠`; a proposal whose deliverables are tracked by design docs with a definition of done. None of
that is a lesson yet. The course's own README says lessons 09 to 13 are "the proposal's ladder"
`(v)` [`course/README.md`](../../course/README.md): the ladder is explained, the agents that climb
it are not.

The book pipeline exists and runs on every merge: `just book` builds `pdf/ww3-lab-course.pdf`
from `course/*.md` with pandoc and xelatex through `nix-config/labs/publisher` `(v)`
[`pubs/README.md`](../../pubs/README.md), [`scripts/build_pdf.sh`](../../scripts/build_pdf.sh).
Its title page still says "WW Lab" `(v)` [`pubs/book/defaults.yaml`](../../pubs/book/defaults.yaml),
the name the repository had before it was renamed.

## 3. What changes for the reader

Before: the README opens with "An open lab for running WW3"; `pdf/ww3-lab-course.pdf` is titled
"WW Lab", has sixteen numbered chapters and no parts; a release's assets are the source archives
GitHub generates.

After, once every box of §7 is ticked:

```
$ just book
build_pdf: build/ww3-lab-course.pdf
$ pdfinfo build/ww3-lab-course.pdf | grep -E 'Title|Pages'
Title:   Without Changing the Answer
Pages:   ...
$ pdftotext -f 3 -l 3 build/ww3-lab-course.pdf - | head
Part I   Running the model                   (chapters 0 to 8)
Part II  Porting without changing the answer (9 to 12)
Part III Agentic coding                      (13, 16, 17)
Part IV  Agentic research                    (18)
Part V   What comes next                     (14, 15)
$ python3 scripts/check_listings.py
check_listings: 212 references, 0 missing
$ gh release view v0.3.0 --json assets --jq '.assets[].name'
ww3-lab-course-v0.3.0.pdf
```

The counts above are the shape of the output, not measurements ⚠.

## 4. Sources and dependencies reviewed

### marola MIP-0014, the design this one translates

Read 2026-10-08 at `marola-dev/marola@main`. It proposes a LaTeX book about marola, versioned in
its own repository, built in CI, in the tradition of self-published FP books. Its §4 reviews the
prior art, fetched by that MIP on 2026-09-05 and not re-fetched here ⚠ (this session's GitHub
access is scoped to this repository): Sandy Maguire's *Thinking with Types* is LaTeX in a public
repository with tested, generated listings, sold on Leanpub; his *Algebra-Driven Design* moved to
Markdown and a Pandoc filter because LaTeX "became a liability" for multi-format output; Gabriel
Volpe's *Practical FP in Scala* keeps the manuscript closed and ships the companion code as a
CI-green public repository; Milewski's *Category Theory for Programmers* has a community LaTeX
port with a Nix flake and a release workflow that attaches a PDF per tag. Its conclusions this
WFIP keeps: listings are extracted and checked, never pasted (§5.3); a PDF per tag (§5.5); the
licence of the prose and the distribution are a person's decision (§5.6, §5.7). Its conclusions
this WFIP drops: LaTeX as the source (§4.5 there), and a separate repository (§5.4 there).

### This repository's pipeline

`pubs/book/defaults.yaml`, `template.tex`, `scripts/build_pdf.sh`, `scripts/book_prep.py` and
`.github/workflows/pubs.yml` `(v)` read 2026-10-08. Markdown (GitHub-flavoured, with math and
footnotes) through pandoc to xelatex, one chapter per lesson, Mermaid fences swapped for their
renders by `pubs/filters/mermaid.lua`, the Word edition by `build_docx.sh`. CI builds and commits
`pdf/` on every merge to `main`. The toolchain comes from `nix-config/labs/publisher`, consumed by
the root flake, so a contributor who never builds the book pays nothing for it: the argument
MIP-0014 §5.4 made for a separate repository does not hold here.

### The translation pipeline

`scripts/translate_md.py` translates the proposal's `en/` into `pt/` through any OpenAI-compatible
endpoint with a per-file hash cache `(v)` [`pubs/README.md`](../../pubs/README.md). It is file-level,
the granularity MIP-0014 §4.6.1 showed re-sends a whole chapter for a one-word change. A pt-BR
edition of the book is possible with it today and expensive to keep current; §11 parks it.

### The release chain

`just release` tags; `release.yml` and `weekly-release.yml` create the GitHub release with notes
that open with `wfip.py status`; Zenodo archives the tagged source and mints a version DOI;
Software Heritage archives the history `(v)` [`.claude/skills/release/SKILL.md`](../../.claude/skills/release/SKILL.md).
Zenodo archives what GitHub's release carries, so a PDF attached to the release travels into the
record ⚠ (Zenodo's GitHub integration archives the release's source archive; whether it also
copies release assets was not checked).

**Pick.** Keep Markdown and the existing pipeline; keep the book inside this repository; add
parts, front matter, a listings gate and a release asset. Nothing is rewritten in LaTeX.

## 5. Design

### 5.1 Title, audience and voice

Working title **Without Changing the Answer**, subtitle *Agentic coding and agentic research on a
GPU port of WAVEWATCH III*. The title is the repository's thesis: a kernel moves to the GPU only if
its output is the Fortran's, bit for bit, and an agent's work counts only when the evidence is
attached. Alternatives for the person who decides (§11): *The Fortran Is the Spec*; *Bit for Bit*.

Audience, unchanged from `CONTRIBUTING.md`: researchers in wave modelling, numerical methods and
HPC first. The book adds the reader the repositioning is for: a researcher or engineer in another
field who wants to run coding agents on scientific code without losing the answer. Every chapter
keeps the `(v)`/`⚠` marks and the commands; `humanizer` is the filter before a chapter is called
done.

### 5.2 Parts and chapters, mapped to files

| Part | Chapters | Grounded in |
|---|---|---|
| I. Running the model | 00–08, as they are | `course/00`–`08`, `examples/`, `exercises/` |
| II. Porting without changing the answer | 09–12 | `course/09`–`12`, `kokkos/`, `proof/`, `docs/BITWISE_PROOF_202610.md`, ADR-0001, ADR-0002 |
| III. Agentic coding | 13; new 16 *How this lab is run*; new 17 *Evidence as a contract* | `course/13`, `AGENTS.md`, `docs/AGENTS_KOKKOS_202609.md`, `.claude/skills/`, `docs/WFIPs/`, `docs/log/`, `bench/results/`, `scripts/results.py` |
| IV. Agentic research | new 18 *Plans an agent can be held to* | `docs/W3SDS4_TRITON_PLANO_202610.pt.md`, `docs/W3SDS4_TRITON_202610.md`, WFIP-0001, the `ww4-status` skill, PR #25 and #41 |
| V. What comes next | 14, 15, the H100 plan as an appendix | `course/14`, `15`, `docs/KOKKOS_H100_PLAN_202609.md` |

Parts are pandoc `\part` divisions: `book_prep.py` inserts a part heading file before the first
chapter of each part, from a `parts.yaml` beside `defaults.yaml`. Chapter numbers stay the lesson
numbers so existing links hold. Each new chapter ends with a boxed *Verify this yourself* naming
the `just` recipe or the gate, the device MIP-0014 §5.2 proposes.

### 5.3 Listings true: `scripts/check_listings.py`

Every reference of the form `` `path`:line `` or `` `path:line` `` in `course/*.md` and
`docs/*.md` resolves: the path exists in this repository or in the `WW3/` submodule at its pinned
commit, and the line is within the file. A fenced block that quotes a file carries
`<!-- listing: path[:from-to] -->` above it, and the script diffs the block against the file.
Python, in `scripts/`, with `tests/test_check_listings.py`; `ci.yml`'s lint job runs it; `just
listings` wraps it. A failing check is the signal a chapter needs a rewrite, not a reason to
delete the reference.

### 5.4 Build

`just book` as today, with the parts, the title page and a copyright page naming the commit the
build is from and the WW3 pin (`git rev-parse --short HEAD`, `just src-st`). `just book-docx`
follows. No new `just` recipe other than `listings`.

### 5.5 Release

`release.yml` and `weekly-release.yml` upload `pdf/ww3-lab-course.pdf` to the release as
`ww3-lab-course-<tag>.pdf`, read from the committed `pdf/` so the release job needs no Nix. The
release notes keep opening with `wfip.py status`. The repository's tags are the book's editions;
the book has no version number of its own.

### 5.6 Metadata and discovery

`CITATION.cff` and `.zenodo.json` carry the title with its new subtitle, an abstract that names
the book, and keywords for the second audience (agentic coding, AI coding agents, agentic
research, code translation, bit-for-bit reproducibility, scientific software engineering, open
textbook), `codemeta.json` regenerated from them. GitHub topics are set by the owner in the
repository settings (a person's act): `wavewatch-iii`, `ocean-waves`, `wave-modeling`, `gpu`,
`kokkos`, `cuda`, `fortran`, `cpp`, `hpc`, `agentic-coding`, `ai-coding-agents`,
`agentic-research`, `code-translation`, `reproducibility`, `research-software`,
`open-textbook`, `nix`.

### 5.7 Licence and distribution

Today everything is MIT with the LGPL kernels `(v)` [`LICENSE`](../../LICENSE). The default is to
leave the prose under MIT and distribute the PDF free from the release and Zenodo, and a free
Leanpub edition fed by CI: `.github/workflows/leanpub.yml` exports the lessons as a Markua
manuscript (`scripts/leanpub_manuscript.py`, `pubs/book/parts.json`) and pushes it to the
`leanpub` branch the Leanpub book reads; [`LEANPUB_202610.md`](../LEANPUB_202610.md) has the
author's steps and what Leanpub charges `(v)` leanpub.com/pricing, 2026-10-08: a one-time fee per
new book, a reader price that may be zero, and a Pro plan for the API. A move of the prose to
CC BY-SA 4.0, or a paid edition, is a person's decision (§11); either is a one-file change plus a
note in the README.

## 6. Parity and physics impact

None: it does not touch a kernel, a namelist or a measurement.

## 7. Verification plan and definition of done

- [ ] ADR-0003 is Accepted and the README, `AGENTS.md`, `CONTRIBUTING.md` and `DESCRIPTION.md`
      say the repository is a book in progress: `grep -l "ADR-0003" README.md README.pt-BR.md docs/ADRs/README.md`
- [ ] The next release's Zenodo record carries the new title and keywords:
      `curl -s 'https://api.datacite.org/dois?query=ww3-gpu' | python3 -c 'import json,sys; print(json.load(sys.stdin)["data"][0]["attributes"]["titles"])'`
- [ ] The book builds with its title page, parts and copyright page: `just book && pdfinfo build/ww3-lab-course.pdf`
- [ ] The listings gate exists, is tested and runs in CI: `python3 scripts/check_listings.py && python3 -m unittest tests/test_check_listings.py`
- [ ] Chapters 16, 17 and 18 exist, pass `just vale`, and each ends with *Verify this yourself*: `ls course/1[678]-*.md && just vale course/16-*.md course/17-*.md course/18-*.md`
- [ ] A tagged release carries the PDF: `gh release view <tag> --json assets --jq '.assets[].name' | grep ww3-lab-course`
- [ ] The Leanpub edition is live and fed by CI: the `leanpub` branch holds `manuscript/Book.txt` from the latest `main` (`git ls-tree origin/leanpub manuscript/`) and `https://leanpub.com/<slug>` serves the book at minimum price 0

## 8. Risks, limitations, and honest caveats

- **Drift is the risk, and the gate is the mitigation only if it runs.** An unenforced
  `check_listings.py` is worse than none (MIP-0014 §8 says the same); it goes into `ci.yml` in the
  same PR that adds it.
- **A book about an unfinished port can overclaim.** The `⚠`/`(v)` rule and the `results.py` gate
  on timings exist for this; a chapter sounding more certain than `PORT_STATUS.md` is a bug.
- **Author hours, not agent hours.** MIP-0014 §7 estimates 15 to 30 hours per chapter for its
  book ⚠ (its estimate, not a measurement); here most chapters exist, and the three new ones are
  the cost. They compete with the proposal's schedule (`08-schedule.md`), so they are slices, not
  a sprint.
- **The Portuguese reader waits.** The README has a pt-BR mirror; the book does not. §11 parks the
  pt-BR edition until the English parts are stable, the order MIP-0014 §5.8 also chose.
- **A title change moves the citation.** The Zenodo title changes at the next release; the
  concept DOI stays `10.5281/zenodo.23221351`, and the README's citation examples cite that DOI.

## 9. Alternatives considered

- **Do nothing**: the lessons and plans already serve the reader who knows where to look; the
  method stays implicit, and the second audience never finds the repository. Lost.
- **A separate book repository** (MIP-0014 §5.4's pick): right for marola, where the TeX closure
  would land on every contributor; wrong here, where the toolchain is already in the root flake
  and the code is the book's evidence, versioned with it. Lost.
- **Rewrite in LaTeX** (MIP-0014 §4.5): finer control of listings and cross-references, at the
  cost of rewriting sixteen chapters and losing GitHub's in-place rendering of the Mermaid
  figures. Maguire's own second book went the other way. Lost.
- **A blog series**: no PDF, no release, no gate on listings. Lost.
- **Rename the repository to the book's title**: breaks every link and the Zenodo chain for a
  name. The repository stays `ww3-gpu` and the README stays "WW3 GPU Lab"; the book has the title.

## 11. Open questions

- Which title? **Default:** *Without Changing the Answer*, until Hoffmann picks one of the three in
  §5.1 or another.
- Which licence for the prose? **Default:** MIT as today; Hoffmann decides before the first
  release that carries the PDF.
- Free PDF only, or also Leanpub? **Default:** both free: the PDF from the release and Zenodo, and
  a free Leanpub edition kept current by `leanpub.yml` once Hoffmann creates the book
  (`LEANPUB_202610.md`); a paid edition is a later decision and changes no file in this WFIP.
- When does the pt-BR edition start? **Default:** after the English parts are stable (§7 all
  ticked), as its own WFIP, with segment-level translation memory if `translate_md.py`'s per-file
  cache proves too expensive.
- Do the three new chapters go through the proposal's reviewer? **Default:** no; `revisor-proposta`
  is for `pubs/proposal/`. The chapters pass `just vale` and `humanizer`, and a person reads them.

## Appendix

### Checked live

- `marola-dev/marola@main`, `docs/MIPs/MIP-0014-marola-book.md`, read in full 2026-10-08 (clone).
- `marola-dev/marola-devkit@ae64f4f`, `plugins/marola-devkit/skills/sharingan/`, MIT, 2026-10-08.
- Leanpub, 2026-10-08: pricing, the API help page, the Git and GitHub writing mode and webhooks
  articles, the LFM manual on `Book.txt` and images, the Markua manual on headings and resources
  (cited in `LEANPUB_202610.md`).
- This repository at `main@5a4f3bf`: `pubs/README.md`, `pubs/book/defaults.yaml`,
  `scripts/build_pdf.sh`, `scripts/book_prep.py`, `.github/workflows/pubs.yml`, `release.yml`,
  `weekly-release.yml`, `.claude/skills/release/SKILL.md`, `course/README.md`, `LICENSE`.

### Not checked

The four prior-art repositories (Maguire, Volpe, Milewski, HoTT): relayed from MIP-0014 §4, which
fetched them on 2026-09-05. Whether Zenodo's GitHub integration copies release assets. Leanpub's
current terms. The page count and reference count in §3.
