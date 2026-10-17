# References of the arXiv paper: how each was checked

One row for each entry of [`refs.bib`](refs.bib). The statuses are those of the proposal's log
([`../proposal/REFERENCES.md`](../proposal/REFERENCES.md)): `verified`, `partial`, `unverified`.
The paper is held to the strict gate, `python3 scripts/refs_check.py --strict pubs/paper/refs.bib
pubs/paper/REFERENCES.md`: an `unverified` row, or an entry with neither a `doi` nor a `url`,
fails. *Read* says how much of the source was read, since the paper may only lean on what was:
`full text`, `abstract`, or `record` (the bibliographic record alone, enough to cite the work for
its existence and nothing more). A search result's snippet, a summary, memory and another
paper's reference list are not a reading. The discipline follows marola-dev/marola-research's
`ouro-paper` skill and h0ffmann/closed-source's reference log.

| Key | Status | Read | Checked against, date | What the paper uses it for |
|---|---|---|---|---|
| `ww3manual` | verified | record | the manual's PDF, `https://github.com/NOAA-EMC/WW3/wiki/files/manual.pdf`, title page read 2026-10-14 (proposal log); URL answered 200 on 2026-10-16 | that WW3 integrates the action balance equation with the listed source terms |
| `tolman1991` | verified | record | Crossref, `10.1175/1520-0485(1991)021<0782:ATGMFW>2.0.CO;2`, 2026-10-14 (proposal log) | the model WW3 descends from; the governing equation |
| `hasselmann1985` | verified | record | Crossref, `10.1175/1520-0485(1985)015<1378:CAPOTN>2.0.CO;2`, 2026-10-16: title, four authors in order, JPO 15(11), 1378–1391, 1985 (Crossref spells "Specturm"; the entry corrects it) | the origin of the DIA. Its description in the paper (one quadruplet configuration and its mirror) is checked against WW3's `w3snl1md.F90` at `03adda91`, not against the paper's text |
| `ikuyajolu2023` | verified | abstract; full text of §2.4 and §3.3 | Crossref, `10.5194/gmd-16-1445-2023`, abstract read 2026-10-16; the PDF `gmd.copernicus.org/articles/16/1445/2023/gmd-16-1445-2023.pdf`, read 2026-10-16 | abstract: `W3SRCEMD` the most expensive module, GPU acceleration for E3SM, 2 and 4 times average speed-ups on P100 and V100, transfer-limited. §3.3 (p. 1453) and Fig. 7: a 5 h run, "We verify the GPU model for correctness using significant wave heights (SWHs) from the CPU-only simulation", averaged over the last 2 h; "the SWH is nearly identical, and the error is negligible and acceptable. It is possible that the error stems from the difference in mathematical precision between the GPU and CPU" |
| `yuan2024` | verified | abstract | Crossref, `10.5194/gmd-17-6123-2024`, abstract read 2026-10-16 | WAM6 on GPUs; up to 37 times on eight A100 over a dual-socket CPU node. Both in the abstract |
| `trott2022` | verified | record | Crossref, `10.1109/TPDS.2021.3097283`, 2026-10-14 (proposal log) | Kokkos as the programming model |
| `koldunov2026` | verified | full text | arXiv API (`export.arxiv.org/api/query?id_list=2606.11356`) and the PDF `arxiv.org/pdf/2606.11356` v1, 2026-10-16; the PDF was read by a helper agent and the quoted passages were re-read by the writing agent in the extracted text | abstract: three practices, "was not permitted to 'improve' the source", 1.6–3.7 times per A100 node; §5: "cannot be bit-for-bit: a change of language, compiler and arithmetic ordering makes byte-level equality neither attainable nor meaningful"; Table 2 and §4.1: about 74 000 lines of core Fortran, two stages; §5.2: GPU not bit-identical ("a combined multiply-and-add, and the built-in exponential and logarithm", and summation order); Appendix B: host FMA kept off; double precision; primary physics.ao-ph, cross-list physics.comp-ph (arXiv API) |
| `ranasinghe2025` | verified | record | Crossref, `10.18653/v1/2025.aisd-main.6`, 2026-10-16: title, eight authors in order, pages 58–69, 2025 | that LLMs are used to translate Fortran to C++ (existence only) |
| `chen2024fortran2cpp` | verified | record | arXiv API, `2412.19770`, 2026-10-16: title, seven authors in order, submitted 2024-12-27, primary cs.LG | the same (existence only) |
| `monniaux2008` | verified | record | Crossref, `10.1145/1353445.1353446`, 2026-10-16: title, Monniaux, TOPLAS 30(3), 1–41, 2008 | that floating-point results depend on the compiler's choices (cited for the general point, which the paper's own measurement carries) |

## Looked for and not cited

- A GPU port of SWAN, and a GPU port of WW3's DIA in a peer-reviewed venue: not searched for
  systematically before this version. ⚠ The related-work section is therefore limited to the
  works the repository already cited; a search is a task before v1 (WFIP-0004 §7).
- A DIA GPU kernel published as code (`consigcody94/ww3-dia-gpu@38919d2`), compared with this
  port in `docs/W3SNL1_GPU_COMPARISON_202610.md`: not cited, since it is code with no paper and
  the comparison is about synthetic tables.
