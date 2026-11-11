# How an agent reads WW3's manual: a survey

2026-10-29, for the owner (goal `ef7f3ca1`). Status: survey; nothing below is built. Since
2026-11-11 a hand-written summary of the manual by topic is in [ww3-manual/](ww3-manual/README.md);
the index of the LaTeX source proposed here (option C) is still not built. The question:
the WW3 manual is about 466 pages; how can an agent look up the physics of a routine it ports
(the equation, its parameters, their defaults, the paper) in one bounded step, cite it, and not
mistake the manual for the code?

Short answer: do not read the PDF. The manual's LaTeX source is in WW3's own tree, pinned with the
code, one file per physics switch. Index that source with the standard library, the way
`scripts/route.py` already indexes this repository, and tie each equation to the code through the
namelist names its tables print. The manual explains the code; it is never cited as the code's
behaviour (AGENTS.md, "WW3 source is read-only": a WW3 fact cites `model/src/<file>.F90:<line>`).

## 1. What the document is

| Fact | Value | How it was read |
|---|---|---|
| The published PDF | version 6.07, March 2019, Tech. Note 333, 466 pages; 6 658 638 bytes at `https://github.com/NOAA-EMC/WW3/wiki/files/manual.pdf` | (v) title page, [pubs/proposal/REFERENCES.md](../pubs/proposal/REFERENCES.md) row `ww3manual`; size `curl -sIL <url>`, 2026-10-29 |
| Its page count as WW3 states it | "326 pp. + Appendices" in the citation text, "465 pp. + Appendices" in the BibTeX `NOTE` of the same file | (v) `manual/README_manual_reference` at NOAA-EMC/WW3 `03adda91` |
| The source | `manual/` in the WW3 tree: 160 `.tex` files, 15 071 lines at NOAA `03adda91`, 14 954 at metocean `92e2cf66` | (v) `find manual -name '*.tex' \| xargs cat \| wc -l` on a sparse checkout of each pin (§6) |
| Its version | `\newcommand{\WWver}{7.14}` at both pins | (v) `manual/defs.tex:1` |
| Chapters (lines of `.tex`) | intro 439, eqs 5 098, num 2 703, run 1 568, impl 1 158, sys 2 435, app 1 248 | (v) `cat <chapter>.tex <chapter>/*.tex \| wc -l` at `03adda91` |
| Physics, one file per switch | 41 files in `manual/eqs/`, e.g. `NL1.tex`, `ST4.tex` (28 281 bytes), `ST6.tex`; 45 distinct `\opthead{<switch>}` headers name the switch and the authors | (v) `ls manual/eqs/*.tex`; `grep -rho '\\opthead{[^}]*}'`; `\opthead` is defined at `manual/defs.tex:232` |
| Equations | 370 `equation`/`eqnarray` environments, 535 `\label{}` | (v) `grep -c` over `manual/**/*.tex` at `03adda91` |

What this means for an agent:

- **The PDF is the wrong version.** It is 6.07 (2019); the code and the source are 7.14. The
  pins also differ from each other: 11 files of `manual/` differ between `03adda91` and
  `92e2cf66` (v, `diff -rq`), among them `eqs/ST4.tex` (the saturation factor
  $M_l(k,\theta)=1+M_\theta\sqrt{\mathrm{mss}}+N_\theta\sqrt{\mathrm{nss}}$ is in MetOcean's copy and
  not in NOAA's at `03adda91`) and `eqs/NL1.tex`. The manual to read is the one at the pin
  (`just src-st`).
- **The manual does not describe local changes.** MetOcean's USTAR floors and `ISNAN` guards
  (`w3src4md.F90:680`, `:780` at `92e2cf66`,
  [log 2026-10-29](log/2026-10-29-metocean-ww3-714-mos.md)) appear in no `.tex` file (v,
  `grep -rn "0.0001\|ISNAN" manual/eqs/ST4.tex` finds none). The code wins.
- **Its pointers into the code are stale.** `sys/files_w3.tex` names `.ftn` files 69 times
  (`w3srcemd.ftn` at `:399`, `w3src4md.ftn` at `:473`) (v); the tree has `.F90` files. Routine
  names are rare in the physics chapter (`w3src3md` 3 times, most others twice or less) (v).
- **The bridge to the code is the parameter name.** The ST4 tables print the namelist name next
  to the symbol. Worked example at `03adda91` (v, each line read):
  `manual/eqs/ST4.tex:125` ($p_\mathrm{in}$, `SINTHP`, namelist `SIN4`, default 2) →
  `model/src/w3gridmd.F90:1014` `NAMELIST /SIN4/ … SINTHP` → `:1722` `SINTHP = 2.` →
  `:1814` `SSINTHP = SINTHP` → `model/src/w3src4md.F90:756` `… COSWIND**SSINTHP`. The name changes
  once (`SINTHP` to `SSINTHP`), so a lookup must follow the assignment, not only the spelling.

## 2. Options

| | Option | For | Against | Verdict |
|---|---|---|---|---|
| A | Read the PDF into context, whole or by page range | nothing to build | 466 pages of the wrong version; equations survive only as images or broken text ⚠ (not measured); every session pays for it again | no |
| B | Convert the PDF to text once (`pdftotext`) and grep it | one command | the wrong version; equations and tables lose their structure ⚠ (not measured); no link to the pin | no |
| C | **Index the LaTeX at the pin** with the standard library: section tree, `\label`, `\opthead` switch, equation blocks, the parameter tables' namelist names, each with `file:line` | deterministic, moves with `WW3/`, no model call, no dependency; equations stay as LaTeX, which an agent reads well; fits `scripts/route.py` (stdlib, from `git ls-files`, output capped at 40 lines) | the index is only as good as the manual's own labels; parameter tables differ per file and need a small parser per table shape ⚠ | **yes** |
| D | Embeddings and a vector store over the manual | finds paraphrases | needs a model provider or a local model and a dependency, not reproducible from a command; ADR-0007 already refused a semantic pass over WW3's manual ("sends the text to a model provider … a second, unreviewed reading", [ADR-0007](ADRs.md#adr-0007)) | no |
| E | graphify over the manual | already used on the code | same refusal (ADR-0007); its edges are inferred, not read | no |
| F | **Hand-written physics cards**, one per ported routine, in its port report: the equation (LaTeX copied from the pinned `.tex` with its `file:line`), each symbol → namelist name → Fortran variable → `file:line`, the paper by DOI | reviewed by a person; carries `(v)`/`⚠` like any page; what a scientist reads | written by hand, so it covers only the routines ported | **yes, as the layer above C** |
| G | Context7 | already enabled | a remote service for library documentation; the WW3 manual is not known to be in it ⚠ (not queried), and an answer is third-party data, never a pin | no |

## 3. Recommendation

C for finding, F for citing.

1. **`route manual <words>`** in `scripts/route.py`: BM25 over the `.tex` files of `WW3/manual/`
   at the pin (the same ranking `route why` uses), returning `manual/<file>.tex:<line>`, the
   section path and the `\opthead` switch. `route manual-eq <label>` prints one equation block
   with its surrounding sentence; `route manual-param <NAME>` prints the table row and the
   namelist, default and assignment lines in `model/src` (the §1 chain). The output cap and the
   "nothing found exits 1" rule stay. `where` already says when `WW3/` is not checked out; the
   manual needs `manual/` in the sparse checkout ⚠ (to check against `just submodule-init`).
2. **A physics card per port report** (`kokkos/port_report/<ROUTINE>_KOKKOS.md`), with the
   equation and the symbol table above; the `ww3-port` skill says to write it, from step 1's
   output, and to mark each row `(v)` only after reading the `.tex` and the `.F90` lines.
3. **A rule for the reader**, in the `routing` skill: the manual answers "what is this term meant
   to compute and from which paper"; `model/src` answers "what does it compute". When they
   disagree, the code is the fact, and the disagreement goes into a `docs/log/` entry.
4. **The pin is part of the citation**: `WW3/manual/eqs/ST4.tex:125` at the commit `just src-st`
   prints. The PDF of 6.07 is cited only as the published reference (`ww3manual` in
   `pubs/proposal/refs.bib`), never for a 7.14 equation.

Not recommended now: building the PDF from the source (needs `latex`, `bibtex`, `dvipdf`, and
`INPDIR` pointing at the `.inp` files, `manual/README` at `03adda91` (v)). A PDF adds pages, not
lookups; it is worth having for people, not for agents.

## 4. What it would take

| Step | Size | Gate |
|---|---|---|
| `route manual`, `manual-eq`, `manual-param` | small: one script, standard library | `python3 -m unittest discover tests` with fixture `.tex` files written for the test (not WW3 text, which is not vendored) |
| `manual/` in the `WW3/` sparse checkout | small | `just route manual "DIA"` returns `manual/eqs/NL1.tex` |
| Physics card for `W3SNL1`, then `W3SDS4`, `W3SIN4` | one page each | the owner's read; `just vale` |
| `routing` and `ww3-port` skill text | small | `python3 .claude/hooks/check_agent_frontmatter.py` |

If the upstream moves to `metocean/WW3`
([plan 2026-10-29-001](plans/2026-10-29-001-metocean-upstream-plan.md)), steps 1 and 2
are unchanged; the cards are re-read at the new pin, since `eqs/ST4.tex` and `eqs/NL1.tex` differ.

## 5. Not covered

- The quality of the manual's physics against its papers: a card cites the paper by DOI and the
  reader checks it.
- Chapters other than `eqs/` and `num/` (running, compiling, the system), which `route why` over
  this repository's lessons already answers for most questions ⚠.

## 6. Reproduce

The facts in §1 come from a sparse, shallow checkout of each pin's `manual/` and two source files,
outside this repository:

```bash
D=$(mktemp -d); f(){ git init -q "$D/$1" && git -C "$D/$1" remote add origin "$2" &&
  git -C "$D/$1" config extensions.partialclone origin &&
  git -C "$D/$1" sparse-checkout set --no-cone /manual/ /model/src/w3src4md.F90 /model/src/w3snl1md.F90 &&
  git -C "$D/$1" fetch -q --depth 1 --filter=blob:none origin "$3" && git -C "$D/$1" checkout -q FETCH_HEAD; }
f noaa https://github.com/NOAA-EMC/WW3 03adda91a1421898ac1ac4433f909e8519e13800
f mos  https://github.com/metocean/WW3 92e2cf66af6e15ae80eb12562f2b05d4a2d75df5
find "$D/noaa/manual" -name '*.tex' | xargs cat | wc -l      # 15071
diff -rq "$D/noaa/manual" "$D/mos/manual" | wc -l             # 11
grep -n SINTHP "$D/noaa/manual/eqs/ST4.tex"                   # :125
```

`w3gridmd.F90` was read with
`gh api "repos/NOAA-EMC/WW3/contents/model/src/w3gridmd.F90?ref=03adda91a1421898ac1ac4433f909e8519e13800" --jq .content | base64 -d`.
