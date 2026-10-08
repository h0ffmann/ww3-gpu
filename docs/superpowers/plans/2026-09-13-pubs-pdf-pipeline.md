# Publications pipeline (course book + UFRJ/DEL proposal PT/EN) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build two PDF products from markdown, reproducibly: the 12-lesson course book, and a ~10-page "Proposta de Projeto de Graduação" (EN source, PT generated) about reducing WW3 forecast time for ReNOMO at LabECO/UFSC.

**Architecture:** One shell script (`scripts/build_pdf.sh`) runs pandoc → xelatex for both products; a small Python pre-step prepares the book chapters (ids + cross-links); a Python translator turns `pubs/proposal/en` into `pubs/proposal/pt` with protected placeholders and a fixed heading glossary. A new root `flake.nix` supplies the toolchain (devShell) and sandboxed `nix build` packages; `just` recipes wrap the devShell; CI runs `nix flake check`, uploads PDFs, and commits them on `main`.

**Tech Stack:** Nix flakes (nixpkgs `eaad089433ca2bb662274377d33df3d0e51ef28b`), pandoc 3.7, TeX Live 2025 via `texlive.combine` (xelatex, babel, DejaVu fonts by filename), Python 3 (`openai` client), just, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-13-pubs-pdf-pipeline-design.md`

## Global Constraints

- Root `flake.nix` is new; `nix-config/labs/pratico/flake.nix` is not modified.
- nixpkgs pinned to `eaad089433ca2bb662274377d33df3d0e51ef28b` (same as pratico).
- One TeX engine everywhere: xelatex from `texlive.combine`. No Tectonic.
- Every pandoc run uses `--fail-if-warnings`.
- English is the proposal's source; `pubs/proposal/pt/*.md` are generated and overwritten; both `meta.*.yaml` are hand-written. Generated PT headings must be exactly: TÍTULO, ÊNFASE, TEMA, DELIMITAÇÃO, JUSTIFICATIVA, OBJETIVO, METODOLOGIA, CRONOGRAMA, Referências Bibliográficas.
- Citation style option: `abnt` (default) or `ieee`; both CSL files vendored under `pubs/csl/`.
- PDFs: `build/` locally (gitignored); `pdf/` committed by CI on `main` only.
- `course/` content is read in place and never edited for the PDF.
- Proposal target length: 8–12 pages at 12 pt A4 (aim 10).
- Commit trailers (`Tested:`) go in the final block of the message; PRs opened with `just pr`.

---

## File structure

```
flake.nix                                  toolchain + packages (Task 1, 5)
.gitignore                                 + build/ (Task 1)
scripts/build_pdf.sh                       pandoc+xelatex driver (Task 1)
scripts/book_prep.py                       chapter ids + cross-link rewriting (Task 2)
scripts/translate_md.py                    PT→EN with placeholders and cache (Task 4)
tests/test_book_prep.py, tests/test_translate_md.py   unittest (Tasks 2, 4)
pubs/book/{defaults.yaml,template.tex}     (Task 2)
pubs/proposal/{template.tex,meta.pt.yaml,meta.en.yaml,refs.bib}   (Task 3)
pubs/proposal/en/0{1..8}-*.md              (Task 3)
pubs/proposal/pt/                          generated (Task 4)
pubs/csl/{abnt.csl,ieee.csl}               (Task 3)
justfile                                   + pubs recipes (Task 1, 2, 3, 4)
.github/workflows/pubs.yml                 (Task 6)
README.md                                  + Publications section (Task 6)
```

---

### Task 1: Toolchain flake and the build driver

**Files:**
- Create: `flake.nix`, `scripts/build_pdf.sh`
- Modify: `.gitignore` (append `build/`), `justfile` (new section)

**Interfaces:**
- Produces: `scripts/build_pdf.sh <book|proposal> [pt|en] [abnt|ieee]` → writes `build/<name>.pdf`, exit non-zero on any pandoc/xelatex failure. Env `OUT_DIR` overrides `build/`. `nix develop .` shell with `pandoc`, `xelatex`, `python3` (+openai), `just`, `pdftotext`, `pdfinfo`.

- [x] **Step 1: Write `flake.nix` (devShell only; packages come in Task 5)**

```nix
{
  description = "ww3-lab publications: markdown -> LaTeX -> PDF (course book, UFRJ/DEL proposal)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/eaad089433ca2bb662274377d33df3d0e51ef28b"; # same pin as nix-config/labs/pratico
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        # Discovered with \listfiles on both documents; keep sorted.
        tex = pkgs.texlive.combine {
          inherit (pkgs.texlive) scheme-medium
            babel-portuges hyphen-portuguese
            dejavu fontspec unicode-math xetex
            booktabs longtable multirow caption float
            titlesec enumitem microtype xcolor
            fvextra upquote csquotes lineno;
        };
        py = pkgs.python3.withPackages (ps: [ ps.openai ]);
        pubsTools = [ pkgs.pandoc tex py pkgs.just pkgs.poppler_utils ];
      in {
        devShells.default = pkgs.mkShell {
          name = "ww3-lab-pubs";
          packages = pubsTools;
          shellHook = ''
            echo "pubs: pandoc $(pandoc --version | head -1 | cut -d' ' -f2) | $(xelatex --version | head -1)"
          '';
        };
      });
}
```

- [x] **Step 2: Write `scripts/build_pdf.sh`**

```bash
#!/usr/bin/env bash
# build_pdf — the one pandoc + xelatex invocation for every PDF in this repo.
#   scripts/build_pdf.sh book [abnt|ieee]              -> build/ww3-lab-course.pdf
#   scripts/build_pdf.sh proposal [pt|en] [abnt|ieee]  -> build/proposal_<lang>.pdf
# OUT_DIR overrides build/. Runs inside `nix develop .` (pandoc, xelatex on PATH).
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
usage() { sed -n '2,5p' "$0" >&2; exit 2; }

target="${1:-}"; shift || true
case "$target" in
  book) lang=en; style="${1:-abnt}" ;;
  proposal) lang="${1:-pt}"; style="${2:-abnt}" ;;
  *) usage ;;
esac
case "$lang" in pt|en) ;; *) usage ;; esac
case "$style" in abnt|ieee) ;; *) usage ;; esac

out_dir="${OUT_DIR:-$root/build}"
mkdir -p "$out_dir"
csl="$root/pubs/csl/$style.csl"

if [ "$target" = book ]; then
  prep="$out_dir/book"
  python3 "$root/scripts/book_prep.py" "$root/course" "$prep"
  mapfile -t inputs < <(ls "$prep"/[0-9][0-9]-*.md | sort)
  pandoc "${inputs[@]}" \
    --defaults "$root/pubs/book/defaults.yaml" \
    --template "$root/pubs/book/template.tex" \
    --resource-path "$root/course" \
    --csl "$csl" \
    --fail-if-warnings \
    -o "$out_dir/ww3-lab-course.pdf"
  echo "build_pdf: $out_dir/ww3-lab-course.pdf"
else
  src="$root/pubs/proposal/$lang"
  mapfile -t inputs < <(ls "$src"/[0-9][0-9]-*.md | sort)
  [ "${#inputs[@]}" -gt 0 ] || { echo "build_pdf: no $src/NN-*.md" >&2; exit 1; }
  case "$lang" in pt) plang=pt-BR ;; en) plang=en-US ;; esac
  pandoc "${inputs[@]}" \
    --template "$root/pubs/proposal/template.tex" \
    --metadata-file "$root/pubs/proposal/meta.$lang.yaml" \
    --metadata lang="$plang" \
    --top-level-division=section --number-sections \
    --citeproc --bibliography "$root/pubs/proposal/refs.bib" --csl "$csl" \
    --pdf-engine=xelatex \
    --fail-if-warnings \
    -o "$out_dir/proposal_$lang.pdf"
  echo "build_pdf: $out_dir/proposal_$lang.pdf"
fi
```

- [x] **Step 3: Append `build/` to `.gitignore`; add just recipes**

```make
# ---------------------------------------------------------------------
# Publications: markdown -> PDF (flake.nix at the repo root; see pubs/)
# ---------------------------------------------------------------------

# Enter the publications shell (pandoc, xelatex, python+openai).
pubs-shell:
    nix develop "{{justfile_directory()}}"

# Course book PDF from course/*.md -> build/ww3-lab-course.pdf.
book style="abnt":
    nix develop "{{justfile_directory()}}" --command scripts/build_pdf.sh book {{style}}

# Proposal PDF -> build/proposal_<lang>.pdf; style abnt (default) or ieee.
proposal lang="pt" style="abnt":
    nix develop "{{justfile_directory()}}" --command scripts/build_pdf.sh proposal {{lang}} {{style}}

# Translate pubs/proposal/en -> pt (changed files only; --force, --dry-run).
translate *args:
    nix develop "{{justfile_directory()}}" --command python3 scripts/translate_md.py "$@"

# Everything: book + proposal pt + proposal en (same as `nix build`).
pubs: book (proposal "pt") (proposal "en")
```

- [x] **Step 4: Verify the shell**

Run: `nix develop . --command bash -c 'pandoc --version | head -1; xelatex --version | head -1; python3 -c "import openai; print(openai.__version__)"; kpsewhich DejaVuSerif.ttf'`
Expected: pandoc 3.7.0.2, XeTeX 3.14…, an openai version, and a path ending in `dejavu/DejaVuSerif.ttf`.

- [x] **Step 5: Verify `build_pdf.sh` argument checking**

Run: `scripts/build_pdf.sh nope; echo rc=$?` → usage text, rc=2. `scripts/build_pdf.sh proposal fr; echo rc=$?` → rc=2.

- [x] **Step 6: Commit**

```bash
git add flake.nix flake.lock scripts/build_pdf.sh .gitignore justfile
git commit -m "feat(pubs): toolchain flake and build_pdf.sh driver

Tested: nix develop . shows pandoc 3.7.0.2 and xelatex; kpsewhich finds DejaVuSerif.ttf; usage errors exit 2"
```

---

### Task 2: Course book

**Files:**
- Create: `scripts/book_prep.py`, `tests/test_book_prep.py`, `pubs/book/defaults.yaml`, `pubs/book/template.tex`

**Interfaces:**
- Consumes: `scripts/build_pdf.sh book` (Task 1) calls `python3 scripts/book_prep.py <course_dir> <out_dir>`.
- Produces: `book_prep.py` writes `<out_dir>/NN-slug.md` for every `NN-*.md` in `<course_dir>` (README.md skipped): first `# ` heading gets `{#ch-<slug>}`; links `](NN-slug.md)` → `](#ch-slug)`, `](NN-slug.md#a)` → `](#a)`. Exit 1 if a link targets a file that does not exist in `<course_dir>`.

- [x] **Step 1: Write the failing test**

```python
# tests/test_book_prep.py
import pathlib, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "book_prep.py"


class BookPrep(unittest.TestCase):
    def run_prep(self, files):
        src = pathlib.Path(tempfile.mkdtemp()); out = pathlib.Path(tempfile.mkdtemp())
        for name, text in files.items():
            (src / name).write_text(text)
        p = subprocess.run([sys.executable, SCRIPT, src, out], capture_output=True, text=True)
        return p, out

    def test_ids_and_links(self):
        p, out = self.run_prep({
            "00-orientation.md": "# Orientation\n\nSee [grids](03-grids.md) and [dx](03-grids.md#resolution).\n",
            "03-grids.md": "# Grids\n\n## Resolution\n",
            "README.md": "# not a chapter\n",
        })
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(sorted(x.name for x in out.iterdir()), ["00-orientation.md", "03-grids.md"])
        text = (out / "00-orientation.md").read_text()
        self.assertIn("# Orientation {#ch-orientation}", text)
        self.assertIn("[grids](#ch-grids)", text)
        self.assertIn("[dx](#resolution)", text)

    def test_dangling_link_fails(self):
        p, _ = self.run_prep({"00-a.md": "# A\n\n[x](09-missing.md)\n"})
        self.assertEqual(p.returncode, 1)
        self.assertIn("09-missing.md", p.stderr)


if __name__ == "__main__":
    unittest.main()
```

- [x] **Step 2: Run it, expect failure**

Run: `python3 -m unittest tests/test_book_prep.py -v`
Expected: FAIL / error (script missing).

- [x] **Step 3: Write `scripts/book_prep.py`**

```python
#!/usr/bin/env python3
"""book_prep — copy course/NN-*.md into a build dir with stable chapter ids and in-book links.

    python3 scripts/book_prep.py course build/book

- the first `# ` heading of NN-slug.md becomes `# Title {#ch-slug}`
- `](NN-slug.md)`   -> `](#ch-slug)`      (link to the chapter)
- `](NN-slug.md#a)` -> `](#a)`            (pandoc's auto id of that heading; duplicated
                                          section titles across lessons get -1/-2 suffixes,
                                          so such a link may land on the first twin)
Exit 1 on a link to a lesson file that does not exist.
"""
import pathlib
import re
import sys

LINK = re.compile(r"\]\((?:\./)?(\d{2}-[a-z0-9-]+)\.md(#[A-Za-z0-9_-]+)?\)")


def slug(stem: str) -> str:
    return stem[3:]  # drop "NN-"


def prep(course: pathlib.Path, out: pathlib.Path) -> int:
    chapters = sorted(p for p in course.glob("[0-9][0-9]-*.md"))
    known = {p.stem for p in chapters}
    out.mkdir(parents=True, exist_ok=True)
    bad = []
    for path in chapters:
        text = path.read_text(encoding="utf-8")

        def fix(m: re.Match) -> str:
            target, anchor = m.group(1), m.group(2)
            if target not in known:
                bad.append(f"{path.name}: link to {target}.md, which is not a lesson")
                return m.group(0)
            return f"](#{anchor[1:]})" if anchor else f"](#ch-{slug(target)})"

        text = LINK.sub(fix, text)
        text = re.sub(r"^# (.+?)\s*$", rf"# \1 {{#ch-{slug(path.stem)}}}", text, count=1, flags=re.M)
        (out / path.name).write_text(text, encoding="utf-8")
    for b in bad:
        print(f"book_prep: {b}", file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    sys.exit(prep(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])))
```

- [x] **Step 4: Run the test, expect pass**

Run: `python3 -m unittest tests/test_book_prep.py -v` → 2 tests OK.

- [x] **Step 5: Write `pubs/book/defaults.yaml` and `pubs/book/template.tex`**

```yaml
# pubs/book/defaults.yaml — pandoc defaults for the course book
from: gfm+tex_math_dollars+footnotes+definition_lists
to: pdf
pdf-engine: xelatex
top-level-division: chapter
number-sections: true
toc: true
toc-depth: 2
highlight-style: tango
metadata:
  title: "wavewatch lab — III & IV"
  subtitle: "A self-paced course on WAVEWATCH III, from the wave spectrum to GPUs, WW4 and SWAN"
  author: "Matheus Hoffmann"
  date: "2026-09"
  lang: en-US
  documentclass: book
  classoption: [a4paper, 11pt, openany]
  geometry: margin=2.5cm
  linkcolor: NavyBlue
  urlcolor: NavyBlue
  colorlinks: true
```

```latex
% pubs/book/template.tex — minimal book template. Fonts loaded by filename from TeX Live's
% dejavu package (no fontconfig needed inside the Nix sandbox).
\documentclass[$for(classoption)$$classoption$$sep$,$endfor$]{$documentclass$}
\usepackage[$geometry$]{geometry}
\usepackage{fontspec}
\setmainfont{DejaVuSerif}[Extension=.ttf, UprightFont=*, BoldFont=*-Bold, ItalicFont=*-Italic, BoldItalicFont=*-BoldItalic]
\setsansfont{DejaVuSans}[Extension=.ttf, UprightFont=*, BoldFont=*-Bold, ItalicFont=*-Oblique]
\setmonofont{DejaVuSansMono}[Extension=.ttf, UprightFont=*, BoldFont=*-Bold, Scale=0.88]
\usepackage{amsmath,amssymb}
\usepackage[$babel-lang$]{babel}
\usepackage{microtype}
\usepackage{booktabs,longtable,array,multirow}
\usepackage{graphicx}
\usepackage{xcolor}
\usepackage{fvextra}
\DefineVerbatimEnvironment{Highlighting}{Verbatim}{commandchars=\\\{\},breaklines,fontsize=\small}
$if(highlighting-macros)$$highlighting-macros$$endif$
\usepackage{hyperref}
\hypersetup{colorlinks=$colorlinks$,linkcolor=$linkcolor$,urlcolor=$urlcolor$,pdftitle={$title$},pdfauthor={$author$}}
\providecommand{\tightlist}{\setlength{\itemsep}{0pt}\setlength{\parskip}{0pt}}
\setlength{\parskip}{6pt plus 2pt}
\setlength{\parindent}{0pt}
\title{$title$$if(subtitle)$\\[0.5em]{\large $subtitle$}$endif$}
\author{$author$}
\date{$date$}
\begin{document}
\frontmatter
\maketitle
$if(toc)$\tableofcontents$endif$
\mainmatter
$body$
\end{document}
```

- [x] **Step 6: Build and smoke-test**

Run: `just book && pdfinfo build/ww3-lab-course.pdf | grep Pages && pdftotext build/ww3-lab-course.pdf - | grep -cE '⚠|\(v\)' && pdftotext build/ww3-lab-course.pdf - | grep -m1 -E 'partial N|∂'`
Expected: a page count > 40, marker count > 0, and the action-balance equation text present. If pandoc reports an unsupported glyph or a warning, fix the template (add the glyph's font via `\newfontfamily`) or the defaults; never edit `course/`.

- [x] **Step 7: Commit**

```bash
git add scripts/book_prep.py tests/test_book_prep.py pubs/book
git commit -m "feat(pubs): course book PDF from course/*.md

Tested: unittest tests/test_book_prep.py (2 pass); just book renders N pages, ⚠/(v) markers and lesson-00 equations present"
```

---

### Task 3: Proposal: template, metadata, bibliography, styles, English text

**Files:**
- Create: `pubs/proposal/template.tex`, `pubs/proposal/meta.pt.yaml`, `pubs/proposal/meta.en.yaml`, `pubs/proposal/refs.bib`, `pubs/csl/abnt.csl`, `pubs/csl/ieee.csl`, `pubs/proposal/en/01-title.md … 08-schedule.md`

**Interfaces:**
- Consumes: `scripts/build_pdf.sh proposal en [style]` (Task 1).
- Produces: metadata keys read by the template: `university`, `school`, `department`, `doc_kind`, `student`, `email`, `advisor`, `coadvisor`, `coadvisor_affiliation`, `city`, `date`, `label_student`, `label_advisor`, `label_coadvisor`, `refs_title`. The last EN file ends with `# References {-}` and an empty `::: {#refs}\n:::` div so citeproc places the bibliography there.

- [x] **Step 1: Fetch the two CSL files**

Run:
```bash
mkdir -p pubs/csl
curl -sSfL https://raw.githubusercontent.com/citation-style-language/styles/master/associacao-brasileira-de-normas-tecnicas.csl -o pubs/csl/abnt.csl
curl -sSfL https://raw.githubusercontent.com/citation-style-language/styles/master/ieee.csl -o pubs/csl/ieee.csl
head -3 pubs/csl/abnt.csl
```
Expected: both files start with `<?xml`. (CC BY-SA 3.0; note the origin in the commit message.)

- [x] **Step 2: Write `pubs/proposal/template.tex`**

```latex
% pubs/proposal/template.tex — "Proposta de Projeto de Graduação" (UFRJ / Poli / DEL) layout,
% reproduced from the owner's 2020 proposal. Body comes from markdown via pandoc.
\documentclass[a4paper,12pt]{article}
\usepackage[margin=2.5cm]{geometry}
\usepackage{fontspec}
\setmainfont{DejaVuSerif}[Extension=.ttf, UprightFont=*, BoldFont=*-Bold, ItalicFont=*-Italic, BoldItalicFont=*-BoldItalic]
\setmonofont{DejaVuSansMono}[Extension=.ttf, UprightFont=*, BoldFont=*-Bold, Scale=0.85]
\usepackage{amsmath,amssymb}
\usepackage[$babel-lang$]{babel}
\usepackage{microtype}
\usepackage{booktabs,longtable,array}
\usepackage{caption}
\usepackage{graphicx}
\usepackage{xcolor}
\usepackage{fvextra}
\DefineVerbatimEnvironment{Highlighting}{Verbatim}{commandchars=\\\{\},breaklines,fontsize=\small}
$if(highlighting-macros)$$highlighting-macros$$endif$
\usepackage[hidelinks]{hyperref}
\providecommand{\tightlist}{\setlength{\itemsep}{0pt}\setlength{\parskip}{0pt}}
\setlength{\parskip}{6pt plus 2pt}
\setlength{\parindent}{1.25cm}
\renewcommand{\thesection}{\arabic{section}.}
\renewcommand{\thetable}{\arabic{table}}
\usepackage{titlesec}
\titleformat{\section}{\normalfont\bfseries}{\thesection}{0.5em}{\MakeUppercase}
\titlespacing*{\section}{0pt}{18pt}{6pt}
\pagestyle{plain}
\begin{document}
\begin{center}
{\bfseries $university$\\ $school$\\ $department$}\\[1.5em]
{\bfseries $doc_kind$}
\end{center}
\vspace{1em}
\noindent $label_student$: $student$ \hfill \texttt{$email$}\\
\noindent $label_advisor$: $advisor$\\
$if(coadvisor)$\noindent $label_coadvisor$: $coadvisor$$if(coadvisor_affiliation)$ ($coadvisor_affiliation$)$endif$\\$endif$
\vspace{1em}

$body$

\vspace{2em}
\noindent $city$, $date$.
\vspace{3em}

\noindent\begin{tabular}{@{}p{0.6\textwidth}@{}}
\rule{0.6\textwidth}{0.4pt}\\
$student$ -- $label_student$
\end{tabular}
\vspace{2.5em}

\noindent\begin{tabular}{@{}p{0.6\textwidth}@{}}
\rule{0.6\textwidth}{0.4pt}\\
$advisor$ -- $label_advisor$
\end{tabular}
$if(coadvisor)$
\vspace{2.5em}

\noindent\begin{tabular}{@{}p{0.6\textwidth}@{}}
\rule{0.6\textwidth}{0.4pt}\\
$coadvisor$ -- $label_coadvisor$
\end{tabular}
$endif$
\end{document}
```

- [x] **Step 3: Write `meta.pt.yaml` and `meta.en.yaml`**

```yaml
# pubs/proposal/meta.pt.yaml
university: UNIVERSIDADE FEDERAL DO RIO DE JANEIRO
school: ESCOLA POLITÉCNICA
department: DEPARTAMENTO DE ENGENHARIA ELETRÔNICA E DE COMPUTAÇÃO
doc_kind: PROPOSTA DE PROJETO DE GRADUAÇÃO
student: Matheus Hoffmann Fernandes Santos
email: hoffmann@poli.ufrj.br
advisor: "A definir (DEL/UFRJ)"          # owner fills in
coadvisor: "Pedro Veras Guimarães, Dr."
coadvisor_affiliation: "LabECO, Departamento de Engenharia Mecânica, UFSC"
city: Rio de Janeiro
date: 13 de setembro de 2026
label_student: Aluno
label_advisor: Orientador
label_coadvisor: Coorientador
link-citations: true
```

```yaml
# pubs/proposal/meta.en.yaml
university: FEDERAL UNIVERSITY OF RIO DE JANEIRO
school: POLYTECHNIC SCHOOL
department: DEPARTMENT OF ELECTRONIC AND COMPUTER ENGINEERING
doc_kind: UNDERGRADUATE PROJECT PROPOSAL
student: Matheus Hoffmann Fernandes Santos
email: hoffmann@poli.ufrj.br
advisor: "To be defined (DEL/UFRJ)"
coadvisor: "Pedro Veras Guimarães, Dr."
coadvisor_affiliation: "LabECO, Department of Mechanical Engineering, UFSC"
city: Rio de Janeiro
date: September 13, 2026
label_student: Student
label_advisor: Advisor
label_coadvisor: Co-advisor
link-citations: true
```

- [x] **Step 4: Write `pubs/proposal/refs.bib`** (keys used by the text: `ww3manual`, `ww3repo`, `ww4repo`, `on525`, `ikuyajolu2023`, `yuan2024`, `trott2022`, `renomo`, `labeco`, `wwlab`)

```bibtex
@techreport{ww3manual,
  author = {{The WAVEWATCH III Development Group}},
  title = {User manual and system documentation of {WAVEWATCH III} version 6.07},
  institution = {NOAA/NWS/NCEP/MMAB}, type = {Tech. Note}, number = {333}, year = {2019},
  address = {College Park, MD, USA}
}
@misc{ww3repo, author = {{NOAA-EMC}}, title = {{WAVEWATCH III} source code repository}, year = {2026},
  howpublished = {\url{https://github.com/NOAA-EMC/WW3}}, note = {develop branch, v7.14} }
@misc{ww4repo, author = {{NOAA-EMC}}, title = {{WAVEWATCH IV} ({WW4}) repository}, year = {2026},
  howpublished = {\url{https://github.com/NOAA-EMC/WW4}}, note = {pre-alpha} }
@techreport{on525, author = {{NOAA/NWS/NCEP}}, title = {{WAVEWATCH IV}: plan for the next-generation wave model},
  institution = {NCEP}, type = {Office Note}, number = {525}, year = {2025}, doi = {10.25923/h7j3-1h25} }
@article{ikuyajolu2023, author = {Ikuyajolu, Olawale James and Van Roekel, Luke and Brus, Steven R. and Thomas, Erin E. and Deng, Yi and Sreepathi, Sarat},
  title = {Porting the {WAVEWATCH III} (v6.07) wave action source terms to {GPU}},
  journal = {Geoscientific Model Development}, volume = {16}, pages = {1445--1458}, year = {2023}, doi = {10.5194/gmd-16-1445-2023} }
@article{yuan2024, author = {Yuan, Ye and others}, title = {{WAM6-GPU} v1.0: {GPU}-accelerated third-generation wave model},
  journal = {Geoscientific Model Development}, volume = {17}, pages = {6123--6136}, year = {2024}, doi = {10.5194/gmd-17-6123-2024} }
@article{trott2022, author = {Trott, Christian R. and Lebrun-Grandi{\'e}, Damien and Arndt, Daniel and Ciesko, Jan and Dang, Vinh and Ellingwood, Nathan and Gayatri, Rahulkumar and Harvey, Evan and Hollman, Daisy S. and Ibanez, Dan and others},
  title = {Kokkos 3: Programming model extensions for the exascale era},
  journal = {IEEE Transactions on Parallel and Distributed Systems}, volume = {33}, number = {4}, pages = {805--817}, year = {2022}, doi = {10.1109/TPDS.2021.3097283} }
@misc{renomo, author = {{LabECO/UFSC}}, title = {{ReNOMO} -- Rede Nacional de Observação e Monitoramento Oceânico}, year = {2026},
  howpublished = {\url{https://labeco.ufsc.br/renomo/}}, note = {Chamada CNPq/MCTI/Finep 062/2022} }
@misc{labeco, author = {{UFSC}}, title = {Laboratório de Engenharia e Ciências Oceânicas ({LabECO})}, year = {2026},
  howpublished = {\url{https://emc.ufsc.br/portal/laboratorios-2/labeco/}} }
@misc{wwlab, author = {Hoffmann, Matheus}, title = {ww-lab: a bootstrap repository for {WAVEWATCH III}}, year = {2026},
  howpublished = {\url{https://github.com/h0ffmann/ww-lab}} }
```

- [x] **Step 5: Write the eight EN section files** (the translator maps the headings to the DEL Portuguese names) (~3 800 words total; the full text is authored during execution, following this outline and the spec's argument; each file starts with its `# ` heading exactly as below so pandoc numbers them 1–8)

| File | Heading | Content (target words) |
|---|---|---|
| `01-title.md` | `# TITLE` | one sentence: *Reducing simulation and forecast time of the WAVEWATCH III model in ReNOMO operations at LabECO/UFSC: compile options, modern Fortran and GPU feasibility* (40) |
| `02-emphasis.md` | `# EMPHASIS` | Computing (5) |
| `03-theme.md` | `# THEME` | WW3 as the operational spectral wave model; what a forecast cycle costs; why wall-clock bounds runs, members and resolution; the lab and the network; cites `ww3manual`, `renomo`, `labeco` (450) |
| `04-scope.md` | `# SCOPE` | the four-rung ladder (switches/flags; run config; targeted modern-Fortran refactors of measured hotspots; H100 feasibility as a study); what is out: `ww3_multi`, coupling, PDLIB, WW4 contributions, physics changes (600) |
| `05-justification.md` | `# JUSTIFICATION` | the case: operational value of faster cycles; upstream WW3 has no GPU path, the only published port got ~1.3× on Summit and was transfer-bound (`ikuyajolu2023`); WAM6-GPU shows what full residency gives (`yuan2024`); WW4 is pre-alpha, C++, first release hoped 2027 (`on525`, `ww4repo`) so the lab's WW3 stays operational for years; Kokkos as the portability layer if rung 4 goes ahead (`trott2022`); explicit non-overlap paragraph (1 100) |
| `06-objective.md` | `# OBJECTIVE` | general objective + 5 specific, measurable objectives (reproducible benchmark; profile table; rung-by-rung speed-up with parity evidence; H100 feasibility memo; written recommendation to LabECO) (350) |
| `07-methodology.md` | `# METHODOLOGY` | profiling-first method; parity gates (bit-for-bit for build-flag rungs, tolerance for refactors); regtests + the lab's operational case; tooling (`wwlab`: Nix toolchain, just recipes, submodules, CI); how results are recorded; risks (600) |
| `08-schedule.md` | `# SCHEDULE` | intro sentence; markdown table (Stage, Deadline) with 8 rows Oct 2026 → Apr 2027, caption `Table: Undergraduate project schedule.`; then `# References {-}` + `::: {#refs}\n:::` (150 + table) |

- [x] **Step 6: Build EN with both styles and check length and layout**

Run:
```bash
just proposal en && pdfinfo build/proposal_en.pdf | grep Pages
pdftotext -layout build/proposal_en.pdf - | grep -nE 'UNDERGRADUATE PROJECT PROPOSAL|^ *1\. TITLE|References|Rio de Janeiro, September' 
just proposal en ieee && pdftotext build/proposal_en.pdf - | grep -m1 '\[1\]'
```
Expected: Pages between 8 and 12; the four layout markers found; IEEE build shows numeric `[1]` citations. If pages < 8, extend Justificativa/Metodologia; if > 12, trim.

- [x] **Step 7: Commit**

```bash
git add pubs/proposal pubs/csl
git commit -m "feat(pubs): UFRJ/DEL proposal (English source) — WW3 forecast-time reduction for ReNOMO/LabECO

CSL files from citation-style-language/styles (CC BY-SA 3.0).

Tested: just proposal en renders N pages with header, numbered sections, references and signature block; ieee style builds"
```

---

### Task 4: Translation EN → PT-BR

**Files:**
- Create: `scripts/translate_md.py`, `tests/test_translate_md.py`, `pubs/proposal/pt/*.md` (generated), `pubs/proposal/.translation-cache.json`

**Interfaces:**
- Produces: `python3 scripts/translate_md.py [--force] [--dry-run]`; env `GITHUB_TOKEN` (default backend `https://models.github.ai/inference`, model `openai/gpt-4o-mini`), overrides `TRANSLATE_BASE_URL`, `TRANSLATE_MODEL`, `TRANSLATE_API_KEY`. Module functions `protect(text) -> (masked, table)` and `restore(masked, table) -> text` (used by the test), placeholder format `⟦N⟧`.

- [x] **Step 1: Write the failing test**

```python
# tests/test_translate_md.py
import importlib.util, pathlib, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("translate_md", ROOT / "scripts" / "translate_md.py")
tm = importlib.util.module_from_spec(spec); spec.loader.exec_module(tm)

SAMPLE = """# THEME

Text with `code`, formula $E = m c^2$ and a block:

```bash
just rt ww3_tp1.1
```

See [the repository](https://github.com/h0ffmann/ww-lab) and the citation [@ikuyajolu2023, p. 3].

$$\\frac{\\partial N}{\\partial t} = S$$

<!-- comment -->
"""


class Protect(unittest.TestCase):
    def test_round_trip_is_identity(self):
        masked, table = tm.protect(SAMPLE)
        self.assertEqual(tm.restore(masked, table), SAMPLE)

    def test_protected_content_is_hidden(self):
        masked, _ = tm.protect(SAMPLE)
        for s in ("`code`", "$E = m c^2$", "just rt", "https://github.com", "@ikuyajolu2023", "\\frac", "comment", "# THEME"):
            self.assertNotIn(s, masked)
        self.assertIn("Text with", masked)

    def test_headings_localized(self):
        self.assertEqual(tm.localize_headings(["# THEME", "# References {-}"]), ["# TEMA", "# Referências Bibliográficas {-}"])
        with self.assertRaises(tm.PlaceholderError):
            tm.localize_headings(["# SOMETHING ELSE"])

    def test_missing_placeholder_is_detected(self):
        masked, table = tm.protect(SAMPLE)
        broken = masked.replace("⟦0⟧", "")
        with self.assertRaises(tm.PlaceholderError):
            tm.restore(broken, table)


if __name__ == "__main__":
    unittest.main()
```

- [x] **Step 2: Run, expect failure**: `python3 -m unittest tests/test_translate_md.py -v` → error (module missing).

- [x] **Step 3: Write `scripts/translate_md.py`**

```python
#!/usr/bin/env python3
"""translate_md — EN-US -> PT-BR for pubs/proposal, markdown-aware, hash-cached.

    python3 scripts/translate_md.py            # translate files whose EN hash changed
    python3 scripts/translate_md.py --force    # everything
    python3 scripts/translate_md.py --dry-run  # list what would run, no API calls

Backend: any OpenAI-compatible endpoint. Default is GitHub Models with GITHUB_TOKEN;
TRANSLATE_BASE_URL / TRANSLATE_MODEL / TRANSLATE_API_KEY override (e.g. a local Ollama:
TRANSLATE_BASE_URL=http://127.0.0.1:11434/v1 TRANSLATE_MODEL=llama3.2 TRANSLATE_API_KEY=ollama).
Protected before the call and restored after: fenced code, inline code, math, link/image
targets, citation keys, HTML comments. Adapted from forecast-energy-demand/scripts/translate_latex.py.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "pubs" / "proposal" / "en"
OUT_DIR = ROOT / "pubs" / "proposal" / "pt"
CACHE = ROOT / "pubs" / "proposal" / ".translation-cache.json"
HEADER = "<!-- generated by scripts/translate_md.py from ../en/{name} — edit the English, not this file -->\n"
HEADINGS = {  # fixed glossary: the DEL section names
    "TITLE": "TÍTULO", "EMPHASIS": "ÊNFASE", "THEME": "TEMA", "SCOPE": "DELIMITAÇÃO",
    "JUSTIFICATION": "JUSTIFICATIVA", "OBJECTIVE": "OBJETIVO", "METHODOLOGY": "METODOLOGIA",
    "SCHEDULE": "CRONOGRAMA", "References": "Referências Bibliográficas",
}

PATTERNS = [
    re.compile(r"```.*?```", re.S),            # fenced code
    re.compile(r"<!--.*?-->", re.S),           # html comments
    re.compile(r"\$\$.*?\$\$", re.S),          # display math
    re.compile(r"(?<!\$)\$[^$\n]+?\$(?!\$)"),  # inline math
    re.compile(r"`[^`\n]+`"),                  # inline code
    re.compile(r"\]\([^)]*\)"),                # link / image targets
    re.compile(r"\[-?@[^\]]+\]|(?<![\w@])@[A-Za-z0-9_:-]+"),  # citations
    re.compile(r"^# .+$", re.M),               # section headings: mapped by the glossary, not the model
    re.compile(r"^Table:", re.M),              # pandoc caption keyword
]
PH = "⟦{}⟧"
PH_RE = re.compile(r"⟦(\d+)⟧")

SYSTEM = """You are an academic translator (engineering / computer science). Translate the markdown
from American English to Brazilian Portuguese. Rules: translate prose, headings, table cells
and list items only; keep every ⟦N⟧ placeholder exactly as is and in place; keep markdown
structure (headings, lists, tables, emphasis) unchanged; do not add or remove paragraphs;
output only the translated markdown, no preamble."""


class PlaceholderError(RuntimeError):
    pass


def protect(text: str) -> tuple[str, list[str]]:
    table: list[str] = []

    def stash(m: re.Match) -> str:
        table.append(m.group(0))
        return PH.format(len(table) - 1)

    for pat in PATTERNS:
        text = pat.sub(stash, text)
    return text, table


def localize_headings(table: list[str]) -> list[str]:
    out = []
    for item in table:
        m = re.match(r"^# (.+?)( \{[^}]*\})?$", item)
        if m and m.group(1) in HEADINGS:
            item = f"# {HEADINGS[m.group(1)]}{m.group(2) or ''}"
        elif m:
            raise PlaceholderError(f"heading not in the DEL glossary: {item}")
        out.append(item)
    return out


def restore(masked: str, table: list[str]) -> str:
    found = {int(n) for n in PH_RE.findall(masked)}
    missing = sorted(set(range(len(table))) - found)
    if missing:
        raise PlaceholderError(f"placeholders lost by the model: {missing}")
    # Placeholders can nest (a link target inside a stashed paragraph is not possible, but a
    # citation inside a link text is), so restore until none remain.
    prev = None
    while prev != masked:
        prev = masked
        masked = PH_RE.sub(lambda m: table[int(m.group(1))], masked)
    return masked


def sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def client():
    from openai import OpenAI
    base = os.environ.get("TRANSLATE_BASE_URL", "https://models.github.ai/inference")
    key = os.environ.get("TRANSLATE_API_KEY") or os.environ.get("GITHUB_TOKEN")
    if not key:
        sys.exit("translate_md: set GITHUB_TOKEN (GitHub Models) or TRANSLATE_API_KEY/TRANSLATE_BASE_URL")
    return OpenAI(base_url=base, api_key=key), os.environ.get("TRANSLATE_MODEL", "openai/gpt-4o-mini")


def translate_text(cl, model: str, masked: str) -> str:
    r = cl.chat.completions.create(
        model=model, temperature=0.2,
        messages=[{"role": "system", "content": SYSTEM}, {"role": "user", "content": masked}],
    )
    return r.choices[0].message.content.strip() + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    cache = json.loads(CACHE.read_text()) if CACHE.exists() else {}
    todo = []
    for pt in sorted(SRC_DIR.glob("[0-9][0-9]-*.md")):
        h = sha(pt.read_text(encoding="utf-8"))
        en = OUT_DIR / pt.name
        if a.force or cache.get(pt.name) != h or not en.exists():
            todo.append((pt, h))
    if not todo:
        print("translate_md: everything up to date")
        return 0
    for pt, _ in todo:
        print(f"translate_md: {'would translate' if a.dry_run else 'translating'} {pt.name}")
    if a.dry_run:
        return 0
    cl, model = client()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    rc = 0
    for pt, h in todo:
        src = pt.read_text(encoding="utf-8")
        masked, table = protect(src)
        try:
            out = restore(translate_text(cl, model, masked), localize_headings(table))
        except PlaceholderError as e:
            print(f"translate_md: {pt.name}: {e} — PT copy left untouched", file=sys.stderr)
            rc = 1
            continue
        (OUT_DIR / pt.name).write_text(HEADER.format(name=pt.name) + out, encoding="utf-8")
        cache[pt.name] = h
        CACHE.write_text(json.dumps(cache, indent=2, sort_keys=True) + "\n")
    return rc


if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 4: Run the tests, expect pass**: `python3 -m unittest tests/test_translate_md.py -v` → 4 OK.

- [x] **Step 5: Generate PT and build it**

Run: `just translate --dry-run` (lists 8 files), then `GITHUB_TOKEN="$(gh auth token)" just translate` — if GitHub Models rejects the token, fall back to `TRANSLATE_BASE_URL=http://127.0.0.1:11434/v1 TRANSLATE_MODEL=dolphin-mixtral:8x7b TRANSLATE_API_KEY=ollama just translate`. Then `just proposal pt && pdfinfo build/proposal_pt.pdf | grep Pages`.
Expected: 8 PT files with the generated header and the DEL headings; PT PDF within 8–12 pages; `just translate` again prints "everything up to date".

- [x] **Step 6: Commit**

```bash
git add scripts/translate_md.py tests/test_translate_md.py pubs/proposal/pt pubs/proposal/.translation-cache.json
git commit -m "feat(pubs): EN->PT markdown translator; generated Portuguese proposal

Tested: unittest tests/test_translate_md.py (3 pass); just translate produced 8 files via <backend>; just proposal pt renders N pages; second run is a no-op"
```

---

### Task 5: Sandboxed `nix build` packages and flake check

**Files:**
- Modify: `flake.nix`

**Interfaces:**
- Produces: `nix build .#book`, `.#proposal-pt`, `.#proposal-en`, `.#default` (all three, `result/*.pdf`); `nix flake check` builds them.

- [x] **Step 1: Add packages to `flake.nix` (inside the `let … in` from Task 1, replace the attrset)**

```nix
        src = pkgs.lib.cleanSourceWith {
          src = ./.;
          filter = path: _type:
            let p = toString path; r = toString ./.;
            in pkgs.lib.any (d: pkgs.lib.hasPrefix "${r}/${d}" p || p == "${r}/${d}") [ "course" "pubs" "scripts" ];
        };
        mkPdf = name: args: pkgs.stdenv.mkDerivation {
          inherit name src;
          nativeBuildInputs = pubsTools;
          buildPhase = ''
            export HOME=$TMPDIR TEXMFVAR=$TMPDIR/texmf-var
            OUT_DIR=$TMPDIR/out bash scripts/build_pdf.sh ${args}
          '';
          installPhase = "mkdir -p $out; cp $TMPDIR/out/*.pdf $out/";
        };
        book = mkPdf "ww3-lab-course" "book";
        proposalPt = mkPdf "proposal-pt" "proposal pt";
        proposalEn = mkPdf "proposal-en" "proposal en";
        all = pkgs.symlinkJoin { name = "ww3-lab-pubs"; paths = [ book proposalPt proposalEn ]; };
      in {
        devShells.default = pkgs.mkShell { name = "ww3-lab-pubs"; packages = pubsTools; };
        packages = { inherit book all; proposal-pt = proposalPt; proposal-en = proposalEn; default = all; };
        checks.pubs = all;
      });
```

- [x] **Step 2: Build and check**

Run: `nix build . && ls -l result/ && nix flake check`
Expected: three PDFs under `result/`, `nix flake check` exits 0. If xelatex fails on a missing package in the sandbox, add it to `texlive.combine` and note it in the flake comment.

- [x] **Step 3: Commit**

```bash
git add flake.nix flake.lock
git commit -m "feat(pubs): nix build packages for the three PDFs; flake check

Tested: nix build . yields result/{ww3-lab-course,proposal_pt,proposal_en}.pdf; nix flake check passes"
```

---

### Task 6: CI, README, pull request

**Files:**
- Create: `.github/workflows/pubs.yml`
- Modify: `README.md` (new "Publications" section after the Nix toolchain section; `pubs/` row in the table)

- [x] **Step 1: Write the workflow**

```yaml
# pubs — build every PDF with nix flake check; upload as artifacts; on main, also translate the
# proposal and commit pdf/ + en/ back. Mirrors forecast-energy-demand's thesis-pdf.yml.
name: Publications

on:
  pull_request:
    paths: ['course/**', 'pubs/**', 'scripts/build_pdf.sh', 'scripts/book_prep.py', 'scripts/translate_md.py', 'flake.nix', 'flake.lock', '.github/workflows/pubs.yml']
  push:
    branches: [main]
    paths: ['course/**', 'pubs/**', 'scripts/build_pdf.sh', 'scripts/book_prep.py', 'scripts/translate_md.py', 'flake.nix', 'flake.lock', '.github/workflows/pubs.yml']
  workflow_dispatch:

permissions:
  contents: write
  models: read

jobs:
  build:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - uses: actions/checkout@v4
        with: { fetch-depth: 0 }
      - uses: DeterminateSystems/nix-installer-action@v16
      - uses: DeterminateSystems/magic-nix-cache-action@v9
      - name: unit tests
        run: nix develop . --command python3 -m unittest discover -s tests -v
      - name: translate EN -> PT (main only)
        if: github.ref == 'refs/heads/main' && github.event_name == 'push'
        env: { GITHUB_TOKEN: '${{ secrets.GITHUB_TOKEN }}' }
        run: nix develop . --command python3 scripts/translate_md.py
      - name: build all PDFs
        run: nix build . --print-build-logs && mkdir -p pdf && cp -L result/*.pdf pdf/
      - uses: actions/upload-artifact@v4
        with: { name: 'pubs-pdfs-${{ github.sha }}', path: 'pdf/*.pdf', retention-days: 30 }
      - name: commit pdf/ and en/ (main only)
        if: github.ref == 'refs/heads/main' && github.event_name == 'push'
        run: |
          git config user.name "github-actions[bot]"
          git config user.email "github-actions[bot]@users.noreply.github.com"
          git add -f pdf/*.pdf pubs/proposal/pt pubs/proposal/.translation-cache.json
          git diff --staged --quiet || git commit -m "ci: rebuild PDFs [skip ci]"
          git push
```

- [x] **Step 2: README**: add `| \`pubs/\` | Course book and UFRJ/DEL proposal sources; \`just book\`, \`just proposal pt|en [abnt|ieee]\`, \`just translate\`; PDFs in \`pdf/\` |` to the table and a short "Publications" section listing the four recipes, the flake (`nix build .`), the EN-is-source rule, and the style option.

- [x] **Step 3: Open the PR**

Run: `just pr` (commit message of the last commit becomes the PR summary; `Tested:` trailer present). Then check the Publications workflow run on the PR uploads three PDFs.

---

## Self-review

- Spec coverage: layout ✓ (T1–T3), toolchain flake ✓ (T1, T5), build step ✓ (T1), just recipes ✓ (T1), translation ✓ (T4), CI ✓ (T6), error handling: `--fail-if-warnings` ✓, dangling-link failure ✓ (T2), usage errors ✓ (T1), placeholder loss ✓ (T4), missing token ✓ (T4). Acceptance items 1–5 map to T2 step 6, T3 step 6, T3 step 6 (ieee), T4 step 4, T5 step 2 / T6 step 3.
- Placeholders: the PT prose is the one deliberately deferred artefact (a writing task; outline and word budget given). No "TBD" elsewhere.
- Names: `build_pdf.sh` args `<book|proposal> [pt|en] [abnt|ieee]` used identically in T1, T5 (`mkPdf` args) and the just recipes; `book_prep.py <course> <out>` identical in T1 and T2; `protect`/`restore`/`PlaceholderError` identical in T4 test and script.
