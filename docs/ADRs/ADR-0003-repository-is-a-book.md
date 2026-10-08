# ADR-0003: The repository is a book in progress on agentic coding and agentic research

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-08 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | What the repository is to a reader: its public form, its first page, its citation metadata and the way its material is organised. Not the lab's rules (`AGENTS.md`), which this decision keeps |
| **Related** | [WFIP-0002](../WFIPs/WFIP-0002-lab-as-a-book.md) (the plan that builds the book); marola's [MIP-0014](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md) (the design it is translated from); [`CONTRIBUTING.md`, "Who this repository is for"](../../CONTRIBUTING.md#who-this-repository-is-for); the `release` skill; [`pubs/README.md`](../../pubs/README.md) |

## Context

The repository started as a lab: a course on running WW3, a pinned toolchain, a Kokkos port of one
kernel, benchmarks, plans and a project proposal `(v)` [`README.md`](../../README.md) at
`main@5a4f3bf`. Along the way it acquired a method that is now its most distinctive content: coding
agents port Fortran under a bit-for-bit gate and a validation ladder (`AGENTS_KOKKOS`, lesson 13);
every claim carries `(v)` or `⚠` and every number its command; a change is designed as a WFIP with a
definition of done before it is built; a decision is an ADR; what was tried goes in a dated log;
releases carry the WFIP status into Zenodo `(v)` [`AGENTS.md`](../../AGENTS.md), read 2026-10-08.

That method is spread over `AGENTS.md`, two plans, five skills, the WFIP template and sixteen
lessons. A reader who wants it has to assemble it. The lessons already compile into a PDF on every
merge (`just book`, `pdf/ww3-lab-course.pdf`) `(v)` [`pubs/README.md`](../../pubs/README.md), so the
form exists and only the framing is missing. marola's MIP-0014 found the same gap in its own
repository and proposed a self-published technical book built from the repository, in the tradition
of Sandy Maguire's and Gabriel Volpe's books `(v)` MIP-0014 §1–§4, read 2026-10-08 (the books
themselves were fetched by that MIP on 2026-09-05, not here ⚠).

## Decision

**The repository is a book in progress, compiled from the repository, on agentic coding and agentic
research applied to one problem: moving WW3's kernels to GPUs without changing the answer.**

1. The lessons in `course/` are the book's chapters, grouped in parts; the rules the lab runs by
   become chapters of their own (WFIP-0002 §5.2). Code, plans, measurements and the proposal stay
   where they are and keep their rules; they are the book's evidence, versioned with the text.
2. The README leads with the book. The repository keeps its name, `ww3-gpu`, and its title, WW3 GPU
   Lab; the book has a title of its own (WFIP-0002 §5.1, a person's choice).
3. Each tagged release is an edition: the PDF is attached to the release and archived with the
   source on Zenodo; the citation metadata (title, abstract, keywords) describes the book and its
   two audiences.
4. The shift is incremental. One PR per task of `WFIP-0002.tasks.md`; after each, the repository
   is consistent and every gate is green.
5. Nothing in `AGENTS.md` changes: WW3 read-only, translate don't improve, no timing without parity,
   the proposal through its reviewer, `Tested:`/`Cost:` on every commit. A chapter that contradicts
   `PORT_STATUS.md` is a bug in the chapter.

## Alternatives considered

| Option | Why not |
|---|---|
| Stay a lab, keep the course as a by-product | The method stays implicit and the second audience (people running agents on scientific code) never finds it; the PDF keeps a title the repository no longer has. |
| A separate book repository, as MIP-0014 §5.4 picked for marola | Right where the TeX closure would land on every contributor; here the publisher toolchain is already in the root flake and costs a contributor nothing, and the code the book cites is versioned next to the text. |
| Rename the repository to the book's title | Breaks every inbound link and the Zenodo and Software Heritage chain for a name; the book's title can live on the title page. |
| A book on wave modelling alone | The lessons already are one; what is new and scarce is the agentic method with its evidence, which is why it leads. |

## Consequences

- WFIP-0002 carries the work: parts and front matter, a listings gate, the PDF on each release,
  three new chapters, the metadata.
- The README, `DESCRIPTION.md`, `CITATION.cff`, `.zenodo.json` and `codemeta.json` describe a book
  from the next release on; the concept DOI does not change.
- The audience rule of `CONTRIBUTING.md` gains one sentence for the second reader and loses
  nothing: a chapter is wrong if a wave modeller reading over the shoulder finds a mistake.
- Every document in `docs/` is a chapter candidate, which raises the bar on its prose: `just vale`
  and `humanizer` apply to anything that may be compiled.
- A chapter that quotes a file is checked against that file in CI (WFIP-0002 §5.3), so a stale
  `file:line` fails the build instead of reaching a reader.

## Revisit when

- The first edition is cut (every box of WFIP-0002 §7 ticked) and a reader outside wave modelling
  has read it: does the framing hold?
- The proposal is defended (05/2027 in `08-schedule.md`): the book's Part V may become the report.
- The book outgrows the repository (a second volume, a publisher, a translation team) and the
  separate-repository option of MIP-0014 becomes the cheaper one.
