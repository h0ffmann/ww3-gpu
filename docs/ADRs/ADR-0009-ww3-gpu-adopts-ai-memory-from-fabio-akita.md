# ADR-0009: ww3-gpu adopts ai-memory from Fabio Akita, and with it the citation discipline, the second brain and the Gleam website of its sister repositories

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-14 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | How this repository remembers, cites, keeps notes, shows itself on the web and on Zenodo, and what a commit records. Not the port, not the physics, not the proposal's text |
| **Related** | [ADR-0003](ADR-0003-repository-is-a-book.md) (the repository is a book; this record supersedes its `Cost:` clause); [ADR-0004](ADR-0004-book-channels-and-first-slice.md) (the book's channels; the website changes shape here); [ADR-0005](ADR-0005-claude-project-settings-in-repo.md) (the Claude project's settings; the long-term memory is now outside them); PR [#107](https://github.com/h0ffmann/ww3-gpu/pull/107) |

## Context

The owner asked, on 2026-10-14, in this order (verbatim, typos included):

1. "create a Pr in h0ffmann/ww3-gpu adding Pedro Veras Guimaraes officially to the Zenodo record
   with ORCID https://orcid.org/0000-0002-1686-4722", then "add zenodo record as Book not only
   software" and "adjust to Guimarães P.V., as named in his own ORCID record".
2. "in the same PR bring all the citation and paper discipline of closed-source, so ww3-gpu will be
   a 2nd-brain-book-with-gleam-website in the same way h0ffmann/h0ffmann, picasso/h0ffmann,
   h0ffmann/closed-source, h0ffmann/nashkell. Bring the best of each one, and redesign ww3-gpu.
   new Readme.md styling included, in picasso way. Register all this work as ADR name ww3-gpu
   adopts ai-memory from Fabio Akita, with citation discipline. Related author sections on README
   pointing to Guimaraes P.V, his ORDID link and akitaonrails github tab, named as legendary br dev"
3. "Dont add cost to any PR. Any Cost information should EXPLICITLY removed in this same PR."

The same day, h0ffmann/closed-source chose ai-memory as its long-term memory, in a change proposal
and a readiness list ([h0ffmann/closed-source#55](https://github.com/h0ffmann/closed-source/pull/55)).
This record makes the same choice here, and records everything the request brings with it.

## Decision

1. **ai-memory is this repository's long-term memory.** ai-memory is "Long-term memory for AI
   coding agents" by Fabio Akita (github.com/akitaonrails), MIT, "Copyright (c) 2026 Fabio Akita"
   `(v)` its `README.md` and `LICENSE` at
   [`705aba7`](https://github.com/akitaonrails/ai-memory/tree/705aba70bc1a3ce6349194135206fb622ca55749),
   read 2026-10-14. A server on the owner's machine captures each session through lifecycle hooks
   and keeps a git-backed wiki of Markdown pages; the default makes no model call and sends
   nothing off the machine `(v)` its `DATA_HANDLING.md`, same commit. The Claude Code plugin is
   `ai-memory@ai-memory` from the marketplace `akitaonrails/ai-memory`, pinned to the tag `v3.0.0`.
   The files of this repository stay the record. The wiki is on one machine; the pages its owner
   tags `publish` are copied into [`memory/`](../../memory/README.md) by `just memory export`,
   where every reader sees them and `just memory import` loads them into the reader's own
   ai-memory (amended 2026-10-14, while Proposed; `scripts/memory.py check` keeps session pages,
   logs, e-mail addresses and home paths out). [`MEMORY.md`](../../MEMORY.md) says where each kind
   of memory is.
2. **Citation discipline, from h0ffmann/closed-source** `(v)` its `AGENTS.md`, `REFERENCES.md`,
   `RELEASES.md` and `RELATED_WORK.md` at
   [`5cf9a6a`](https://github.com/h0ffmann/closed-source/tree/5cf9a6a):
   - every entry of [`pubs/proposal/refs.bib`](../../pubs/proposal/refs.bib) has a row in
     [`pubs/proposal/REFERENCES.md`](../../pubs/proposal/REFERENCES.md) with a verdict
     (`verified`, `partial`, `unverified`) and what it was checked against; `just refs`
     (`scripts/refs_check.py`) is the gate in CI;
   - [`RELEASES.md`](../../RELEASES.md): each release and its permanent DOI;
   - [`RELATED_WORK.md`](../../RELATED_WORK.md): each tie of the Zenodo record, with a date of
     record, and what points back at it.
3. **A second brain, from h0ffmann/h0ffmann and h0ffmann/closed-source** `(v)` h0ffmann's `NOTES.md`
   at [`9bfe949`](https://github.com/h0ffmann/h0ffmann/tree/9bfe949) and closed-source's
   `SMART_NOTE_POLICY.md` at `5cf9a6a`: [`notes/`](../../notes/index.md), one idea in one note, under
   [`NOTES.md`](../../NOTES.md); `just notes` (`scripts/notes_check.py`) is the gate in CI. The
   journal stays [`docs/log/`](../log/README.md), append-only.
4. **A Gleam website, from h0ffmann/h0ffmann and h0ffmann/nashkell** `(v)` their `site/` at
   `9bfe949` and [`2afe237`](https://github.com/h0ffmann/nashkell/tree/2afe237): a prerendered
   Gleam + Lustre front page at `/`, whose every fact comes from the repository's files through
   `site/content`; the book's chapters move to `/book/`; the PDF, `/andamento/` and `/adrs/` keep
   their URLs. The toolchain is picasso's `labs/gleam-website` at
   [`52c5772`](https://github.com/h0ffmann/picasso/tree/52c5772), through `flake.nix`. The look is
   [`DESIGN.md`](../../DESIGN.md).
5. **A README in picasso's manner** `(v)` picasso's `README.md` at `52c5772`: a centred header with a
   drawing, lower case, flat badges, and the study areas and recipes as before. It names the related
   authors: Guimarães P.V. ([ORCID 0000-0002-1686-4722](https://orcid.org/0000-0002-1686-4722)),
   the co-advisor, and Fabio Akita ([akitaonrails](https://github.com/akitaonrails)), "legendary br
   dev" in the owner's words of 2026-10-14. `README.pt-BR.md` is its mirror.
6. **The Zenodo record is a book.** `.zenodo.json` says `upload_type: publication`,
   `publication_type: book`, as closed-source's record does. A record has one type, so the code is
   linked as software through the repository's tie. Guimarães P.V. is a contributor (Supervisor)
   with his ORCID; a supervisor is a contributor, never a creator.
7. **No cost record.** No commit carries a `Cost:` trailer, no PR has a Cost row, and no WFIP has a
   "Cost so far". This supersedes the clause "`Tested:`/`Cost:` on every commit" of ADR-0003
   (Decision, item 5), which is not edited because it is accepted. A commit carries `Tested:`.

## Alternatives considered

| Alternative | Why not |
|---|---|
| Keep the memory in the repository only (ADR-0005's snapshot, the log) | The files keep what was decided, not what a session tried or where it stopped; the next agent learns it again |
| One ADR for each of the seven parts | The owner asked for one record of all this work, under this name |
| Copy closed-source's Gleam tooling (`tools/fabric`) | It is built for its papers; a small `site/content` generator and two Python checks fit this repository, whose lab code has no Gleam and whose tooling is Python |
| A cite line on every Markdown file (closed-source's `PERSONAS.md` rule) | It would claim an author for every one of this repository's existing files without knowing who wrote each; not adopted |
| Keep the record typed as software and add a second, book record | Zenodo gives one concept DOI to a repository; a second record needs its own deposit, and the owner asked for this record |

## Consequences

- The proposal's references are logged and checked. Four are `partial` and need an edit to
  `refs.bib`, which goes through `revisor-proposta`: the list is in `pubs/proposal/REFERENCES.md`.
- The book's chapter URLs move to `/book/`. Old deep links to a chapter break; the front page links
  every chapter.
- The record becomes a book at the next release; v0.1.0 to v0.1.3 stay software on DataCite.
  marola's and closed-source's metadata name this record with `resource_type: software` until they
  change it.
- The long-term memory works only on the owner's machine, after the owner installs it. Until then
  `MEMORY.md` says it is chosen, not in use. What the owner publishes reaches every reader through
  `memory/`; the rest of the wiki does not.
- A commit message is shorter by one trailer, and `just uprd` writes no Cost row.

## Revisit when

- ai-memory changes its licence, its default of sending nothing, or its plugin's hooks.
- A second person works here every week: the memory then needs a shared server, which this record
  does not cover.
- closed-source or h0ffmann change a rule this repository copied; the commit pins above say which.
