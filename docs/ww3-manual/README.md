# The WAVEWATCH III manual, summarised for agents

2026-11-11, for the owner (goal `kuhMqCdF-6680`). A summary of the user manual and system
documentation of WAVEWATCH III® (WW3) version 7.14, split by topic, so that an agent knows how
the model works before it opens `WW3/model/src`. It is a map, not a source: the manual explains
what the code is meant to compute, `model/src` says what it computes, and a WW3 fact in this
repository still cites `model/src/<file>.F90:<line>` at the pin (AGENTS.md, "WW3 source is
read-only").

## Pages

Read the one the task needs. Each is under 14 kB.

| Page | Manual chapter | Read it when |
|---|---|---|
| [equations.md](equations.md) | 2.1, 2.2, 2.3.1, 2.6 | you need the action balance equation as WW3 writes it, the spectrum it carries, the tail, or what an output field means |
| [source-terms.md](source-terms.md) | 2.3 to 2.5 | you port or review a source term: every switch, its routine file, its equations for `NL1` and `ST4`, the namelist names |
| [numerics.md](numerics.md) | 3, appendix A | you touch the time loop: the splitting, the four time steps, the propagation schemes, the source term integration, the limiters |
| [grids.md](grids.md) | 3.4, 3.14, 6.5 | you need the grid types, the sea-point storage `a(ith,ik,isea)`, the status maps, MPI's data layout, nesting |
| [code-structure.md](code-structure.md) | 4, 6 | you need the programs, their files, the call tree of `W3INIT` and `W3WAVE`, which module holds what |
| [switches.md](switches.md) | 5.9, 6.2 | you read a `switch` file or an `#ifdef W3_<name>` |

## Evidence

- `(v)` on these pages means: read in the manual text on 2026-11-11, at the section named in the
  heading or the sentence. Every statement is `(v)` in that sense unless it carries `⚠`.
- Nothing here was checked against `model/src`, with one exception: a sentence that says "at the
  pin" was read at NOAA-EMC/WW3 `03adda91`, and gives its file and line.
- A parameter default, a constant or an equation taken from here into a port report, a commit or
  a table is `⚠` until its line in `model/src` was read. The manual's namelist name and the
  Fortran variable differ by a doubled first letter (`SINTHP`, `SSINTHP`; manual, tables 2.5 to
  2.8), so follow the assignment in `w3gridmd.F90`, not the spelling.
- Equations are written from the manual in LaTeX. The text supplied was a PDF converted to plain
  text, in which equations lose their layout; each equation here was rebuilt from that text and
  is `⚠` in its details (an exponent, a sign) until compared with `manual/eqs/<file>.tex` at the
  pin. The equation numbers are those of the supplied PDF and shift at the pin.

## Source and version

| Fact | Value | How it was read |
|---|---|---|
| Title | "User manual and system documentation of WAVEWATCH III® version 7.14", The WAVEWATCH III® Development Group (WW3DG), NOAA/NWS/NCEP/EMC/MMAB, College Park, MD | (v) title page of the supplied text |
| Status | "(DRAFT) please do not refer to this manuscript"; the title page is dated March 2019 and asks readers to cite the released manual of version 6.07 (Tech. Note 333, 2019, 465 pp. + appendices) | (v) title page |
| What was supplied | `manual_WW3.pdf.txt`, 812 441 bytes, 17 791 lines: the PDF as plain text, given by the owner on 2026-11-11 | (v) `wc -lc` |
| Where the manual lives | `manual/` in the WW3 tree, LaTeX, one file per section (`manual/eqs/ST4.tex`, `manual/num/source.tex`, ...) | (v) `git ls-tree 03adda91 manual/` |
| Which source the supplied PDF was built from | NOAA-EMC/WW3 `develop` between `3f27304` (2022-03-16) and the parent of `8ae6a85` (2022-09-16): it has the output field `TOC` added by the first and still prints "version x.xx" where the second wrote "6.07" and "5.16". No commit between them touches `manual/` | (v) `git log 3f27304..8ae6a85 -- manual/`; `grep -n "TOC\|version x.xx" manual_WW3.pdf.txt`. ⚠ a fork with the same `manual/` would give the same text |
| Published reference | version 6.07 (2019), `ww3manual` in `pubs/proposal/refs.bib`. A 7.14 equation is cited as `manual/<file>.tex:<line>` at the pin, never as this draft | [WW3_MANUAL_DIGEST_SURVEY_202610.md](../WW3_MANUAL_DIGEST_SURVEY_202610.md) |

The date on the title page is not the date of the text: the manual names version 7.14
throughout, and its section 2.3.6 cites a 2021 paper.

## Licence

The owner described the manual as public domain. Its own front matter does not say so ⚠:

- Section 1.3, "Licensing terms", counts "related documentation" as part of the software,
  furnished free of charge, and its text (written for the distribution before GitHub) includes
  "No disclosure of any portion of the software ... may be made to any third party by the
  Licensee" (v).
- Section 1.4: "WAVEWATCH III® © 2009-2016 National Weather Service, National Oceanic and
  Atmospheric Administration. All rights reserved. WAVEWATCH III® is a trademark of the National
  Weather Service" (v).
- The repository the manual's source is in carries `LICENSE.md`, which puts "the WAVEWATCH III
  code" under the GNU Lesser General Public License v3 (v, `LICENSE.md` at `03adda91`). It does
  not name the manual; `manual/intro/license.tex` at the same commit still holds the section 1.3
  text (v).

What this page does about it: no text of the manual is copied here beyond the short quotations
above. The pages are a summary in this repository's words, with the equations and names a
scientist needs to find the original. The PDF and its text are not committed. Whether the manual
may be redistributed is the owner's call, not settled here.

## The pin

"WW3 version 7.14" does not name one commit. NOAA-EMC/WW3 has two tags, `6.07` and `6.07.1`
(v, `git ls-remote --tags https://github.com/NOAA-EMC/WW3.git`, 2026-11-11); 7.14 is the number
`develop` has carried since `cb5d2b4` (2021-11-02).

| Tree | Commit | Version it declares | Read with |
|---|---|---|---|
| `WW3-noaa/` | NOAA-EMC/WW3 `03adda91` (2026-10-05) | 7.14 | (v) `git show 03adda91:VERSION`; `manual/defs.tex:1`, `\newcommand{\WWver}{7.14}` |
| `WW3/` (the fork) | based on `03adda91` ([VERSIONS.md](../../VERSIONS.md)) | 7.14 ⚠ (the fork was not read in this session) | `cat WW3/VERSION` |

So the pins already are version 7.14, and they were not moved. Pinning to the tree the supplied
PDF was built from would mean a 2022 commit: 127 files of `model/src` differ between the parent
of `6b9edfa` (2023-09) and `03adda91`, `w3snl1md.F90`, `w3src4md.F90`, `w3srcemd.F90` and
`w3wavemd.F90` among them (v, `git diff --stat 6b9edfa~1 03adda91 -- model/src`), and the fork's
Kokkos patch and every regtest comparison are made on `03adda91`.

### What the manual at the pin says that the supplied PDF does not

Twelve files of `manual/` changed between the PDF's source and `03adda91` (v,
`git diff --stat 8ae6a85~1 03adda91 -- manual`). For the ports here:

- **`eqs/NL1.tex`** was rewritten: the DIA is presented as two mirror-image quadruplets per
  component, each discrete component exchanging energy with 12 others before the bilinear
  interpolation, and a new subsection describes the Gaussian Quadrature Method (GQM), an exact
  calculation that replaces the DIA inside the `NL1` switch when the namelist parameter `IQTYPE`
  is negative.
- **`eqs/ST4.tex`** lost the long-wave modulation factor $M_l$ (equation 2.126 of the PDF, with
  `mss` and `nss`) from the saturation term, removed in `3952826` (2024-01-16),
  and cites a 2023 adjustment of the `SDSBCHOICE=3` option.
- **`eqs/ST3.tex`** writes the roughness as $z_1=\alpha_0\tau/\sqrt{1-\tau_w/u_\star^2}$.
- **`eqs/ICE4.tex`** adds methods 8 to 10 and the `IC4CN` namelist array; **`eqs/ST6.tex`** and
  **`eqs/output.tex`** (skewness and `QKK` fields) changed too.

The pages mark these places. Read the `.tex` at the pin before quoting an equation of `NL1`,
`ST3`, `ST4`, `ST6` or `IC4`.

## Where the manual is stale against the pin

- It names source files `*.ftn`, a preprocessor `w3adc`, a `bin/switch` file and
  `make_makefile.sh` (chapters 5 and 6). At the pin the files are `model/src/*.F90` with
  `#ifdef W3_<SWITCH>` (v, `model/src/w3srcemd.F90:569`, `#ifdef W3_ST4`), built by CMake
  (`CMakeLists.txt`, `model/CMakeLists.txt`) (v, `git ls-tree 03adda91`). The change is commit
  `cb5d2b4` (2021-11-02).
- Its module list (6.3) has no `w3sic4md`, `w3sic5md`, `w3sis1md`, `w3snl4md`, `w3snl5md`,
  `w3gkemd`, `w3uostmd`, `w3parall`, `w3fld1md`, `w3flx4md` or the PDLIB files, all present at
  the pin (v, `git ls-tree 03adda91 model/src/`).
- Its switch list (5.9) has no `nl5` and no `ic5`, though chapter 2 describes both.
- Chapter 6.6 says the variables are documented in the source and no longer in the manual.

## Reproduce

```bash
D=$(mktemp -d) && git init -q "$D" && git -C "$D" remote add origin https://github.com/NOAA-EMC/WW3.git
git -C "$D" fetch -q --filter=blob:none origin develop      # history, no file contents
git -C "$D" show 03adda91:VERSION                           # 7.14
git -C "$D" log --format='%h %cs' 3f27304..8ae6a85 -- manual/    # 8ae6a85 only
git -C "$D" diff --stat 8ae6a85~1 03adda91 -- manual         # 12 files
git -C "$D" diff --stat 6b9edfa~1 03adda91 -- model/src | tail -1    # 127 files
```
