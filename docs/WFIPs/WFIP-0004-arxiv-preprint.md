# WFIP-0004: An arXiv preprint of the bit-for-bit DIA, built and gated from the repository

| | |
|---|---|
| **Status** | Draft |
| **Author** | M. Hoffmann (asked 2026-10-16: "help me build a credible arXiv record with the current work, comparisons to the prior work on arXiv:2606.11356, next steps") |
| **Created** | 2026-10-16 |
| **Deliverable** | D6 |
| **Related** | [`pubs/paper/paper.md`](../../pubs/paper/paper.md); [`docs/log/2026-10-14-snl1-bitwise-af11.md`](../log/2026-10-14-snl1-bitwise-af11.md); [WFIP-0003](WFIP-0003-kokkos-regtest-matrix.md) (the matrix whose records the paper reports); [ADR-0010](../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md), [ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md) |
| **Effort** | M: the text and two scripts exist; v1 waits on one clean re-run of the matrix and two decisions of the author |
| **Gain** | `proposal` (D6: results published so others can repeat them); `science` (the `AF11` finding and the bit-for-bit criterion, citable on their own); `outreach` (an arXiv record next to the Zenodo one) |
| **Effort vs Gain** | do when the clean matrix re-run lands: the text is ready, the evidence under it is a day old and recorded from modified trees |
| **Depends on** | the owner's workstation for the re-run (CPU, about 45 minutes for both selections); an arXiv account with an endorsement for physics.ao-ph; the author's decision on co-authors |
| **Blocked by** | none |
| **Risk** | moderation: arXiv may hold or reclassify a short single-routine result; a physics.ao-ph primary with results, not a position paper, is the mitigation |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | none needed: §7 is the task list |
| **Tests** | `scripts/arxiv_bundle.py --self-test`, `tests/test_arxiv_bundle.py`; the strict reference gate `scripts/refs_check.py --strict pubs/paper/refs.bib pubs/paper/REFERENCES.md` |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

A short research paper, `pubs/paper/paper.md`, reports the one result the repository can defend
today: the Kokkos DIA byte-identical to the Fortran DIA inside WW3, on the CPU, on all 125 serial
and OpenMP `NL1` entries of WW3's regression matrix, and the compiler finding (`AF11`) that made
it so. It positions the result against the FESOM2 port of arXiv:2606.11356. `just arxiv` builds
the PDF, the source tarball arXiv compiles, and the form fields; the gates refuse an unchecked
reference, an uncited one, a placeholder for the author and a bundle pdfLaTeX cannot compile.

## 2. Motivation

The repository is archived on Zenodo with every release (concept DOI 10.5281/zenodo.23221351)
but has no paper a wave modeller or an HPC reader would find on arXiv. Zenodo's description
claimed more than the evidence: "shown bit-identical to the Fortran on the Serial, OpenMP and CUDA
backends by a field-by-field parity harness" (v, `.zenodo.json` at `f83aa0c`), while CUDA has
never run inside WW3 (`kokkos/port_report/W3SNL1_KOKKOS.md`) and the harness was removed by
ADR-0010. A paper forces the claim down to what was measured; this WFIP corrects the description
in the same change.

## 3. What changes for the reader

Before: the result exists as a research-log entry, two TSV records and a row of `ports.tsv`.
After: `build/arxiv/ww3-gpu-dia-bit-for-bit.pdf` (11 pages, draft), with every number in a final
table beside the command that reproduces it, and an arXiv abstract page once submitted.

## 4. Sources and dependencies reviewed

- **arXiv submission and versions** (v, read 2026-10-16): a new version is a replacement, every
  version stays public (`info.arxiv.org/help/replace.html`, `/help/versions.html`); automated
  deposit (SWORD) needs arXiv's permission (`/help/submit_sword.html`). Submission is therefore a
  person's act, not a workflow.
- **arXiv CS review and position papers** (v, `blog.arxiv.org`, 2025-10-31): accepted only after
  peer review. This paper reports a measured result and is submitted to physics.ao-ph, with
  physics.comp-ph and cs.DC as cross-lists; arXiv:2606.11356 is in physics.ao-ph and
  physics.comp-ph (v, arXiv API, 2026-10-16).
- **Paper discipline** taken from h0ffmann/closed-source (the reference log with verdicts, the
  `**[AUTHOR ...]**` marker, the Statements block with the use of AI, the pdfLaTeX bundle gate) and
  marola-dev/marola-research's `ouro-paper` skill (what counts as reading a source, every number
  with its command, the uncited-key gate, a cold reader of the PDF before submission). Ideas only;
  no file was copied, and nothing from marola-research's `marola-bench/`, which is all rights
  reserved.

## 5. Design

- `pubs/paper/paper.md`: one file, pandoc front matter with an `arxiv:` block (primary,
  cross-lists, licence, comments).
- `pubs/paper/refs.bib`, `pubs/paper/REFERENCES.md`: the bibliography and its log, with a *Read*
  column (full text, abstract, record) so a claim leans only on what was read.
- `scripts/arxiv_bundle.py`: pandoc with citeproc to one `.tex` (no `.bib` in the tarball), the git
  revision in the date line, pdflatex twice in an empty directory, the checks of its docstring.
- `just arxiv [--draft|--check]`; CI runs the strict reference gate and the bundle's self-test.
- After announcement: the arXiv DOI goes into `.zenodo.json` (`isVariantFormOf`), `CITATION.cff`
  and both READMEs, read from DataCite that day (`AGENTS.md`, releases).

## 6. Parity and physics impact

None: it does not touch a kernel.

## 7. Verification plan and definition of done

- [x] The paper builds and compiles with pdfLaTeX in a clean directory: `just arxiv --draft`
- [x] Every reference has a verdict and a reading, and every key is cited: `just arxiv --check`, `python3 -m unittest tests.test_arxiv_bundle`
- [ ] Both matrix selections re-run at a clean commit at the current WW3 pin, the records of Table 1 replaced: `just ww3-gpu-matrix 8 serial`, `just ww3-gpu-matrix 4 omp`
- [ ] A search for GPU ports of SWAN and of the DIA, recorded in "Looked for and not cited": `pubs/paper/REFERENCES.md`
- [ ] The `x**11` scratch program committed as a compiler experiment, so the 643 427 is reproduced by a command: `docs/log/2026-10-14-snl1-bitwise-af11.md`
- [x] A cold reader (a fresh agent with only the PDF and ten questions) reported, and its certain findings fixed: the PR description
- [ ] The author's two markers resolved (competing interests; co-authors): `just arxiv` without `--draft`
- [ ] v1 submitted and announced; the arXiv DOI in `.zenodo.json`, `CITATION.cff` and both READMEs: `just codemeta --check`

## 8. Risks, limitations, and honest caveats

- The evidence is one routine, on one CPU, with one compiler; the paper says so in its own
  section, and a reviewer may still find it thin. The ST4 matrix is the obvious v2.
- The three matrix records were made from modified trees; the re-run of §7 is the condition for
  v1, not an option.
- An arXiv version cannot be withdrawn from the record; v1 is submitted only when §7 is ticked
  through the author's markers.

## 9. Alternatives considered

- **Wait for a GPU result.** Better paper, but the GPU run is supervised and may not meet the
  criterion; the CPU result and the `AF11` finding stand on their own.
- **Submit the book.** A book in progress on agentic coding is close to a position paper in cs.SE
  and would meet the 2025 policy; the result is the part that is new.
- **Do nothing.** The result stays in a log entry no search finds.

## 11. Open questions

- Co-authors: the two advisers are contributors on Zenodo (Supervisor, Other); whether either signs
  the paper is the author's decision.
- Whether v1 waits for the ST4 matrix, which would let the paper report 87 % of run time ported
  and regtested rather than 48 %.
