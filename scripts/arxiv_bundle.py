#!/usr/bin/env python3
"""arxiv_bundle: the arXiv paper in pubs/paper/ as a PDF, a source tarball and the form fields.

    python3 scripts/arxiv_bundle.py            # -> build/arxiv/<slug>.pdf, .tar.gz, .txt
    python3 scripts/arxiv_bundle.py --draft    # the same, with **[AUTHOR ...]** markers allowed
    python3 scripts/arxiv_bundle.py --check    # the text checks only, no TeX
    python3 scripts/arxiv_bundle.py --self-test

pandoc renders paper.md with citeproc into one standalone .tex, so the tarball needs no .bib or
.bbl; that .tex is compiled twice with pdflatex in an empty directory, as arXiv's compiler would,
and the PDF in build/arxiv/ is the one arXiv will make. It fails when:

- a `**[AUTHOR ...]**` marker is left (an author's decision the text is waiting for), unless --draft;
- a key of refs.bib is not cited, or a citation names no key (pandoc's --fail-if-warnings);
- the abstract is longer than arXiv's 1920 characters, or the title is empty;
- a category id is malformed, or the licence is missing;
- pdflatex fails, or its log reports a character the font lacks.

Submission is a person's act on arxiv.org: endorsement, the licence and the final preview are the
submitter's (pubs/README.md, "The arXiv paper"). The idea and the checks follow
h0ffmann/closed-source's `tools/src/fabric/arxiv.gleam` at 5cf9a6a; this is a rewrite, not a copy.
"""

import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAPER = ROOT / "pubs" / "paper"
OUT = ROOT / "build" / "arxiv"
SLUG = "ww3-gpu-dia-bit-for-bit"
ABSTRACT_MAX = 1920
CATEGORY = re.compile(r"^[a-z-]+(\.[A-Za-z-]+)?$")
MARKER = re.compile(r"\*\*\[AUTHOR\b")
CITE = re.compile(r"@([A-Za-z0-9_:-]+)")


def meta_of(md_path):
    """The front matter as plain strings, through pandoc so YAML needs no Python package."""
    with tempfile.NamedTemporaryFile("w", suffix=".tpl", delete=False) as tpl:
        tpl.write("$meta-json$\n")
    try:
        out = subprocess.run(["pandoc", str(md_path), "--to", "plain", "--template", tpl.name],
                             check=True, capture_output=True, text=True).stdout
    finally:
        Path(tpl.name).unlink()
    return json.loads(out)


def bib_keys(text):
    return set(re.findall(r"@\w+\s*\{\s*([^,\s]+)\s*,", text))


def text_problems(md, meta, bib, draft=False):
    """Problems a reader of paper.md alone can find; each one fails the bundle."""
    problems = []
    if not draft and MARKER.search(md):
        problems.append("an **[AUTHOR ...]** marker is left (pass --draft to build anyway)")
    body = md.split("\n---", 1)[1] if md.startswith("---") else md
    cited = set(CITE.findall(body))
    for key in sorted(bib_keys(bib) - cited):
        problems.append(f"refs.bib key `{key}` is not cited")
    if not str(meta.get("title", "")).strip():
        problems.append("the title is empty")
    abstract = " ".join(str(meta.get("abstract", "")).split())
    if not abstract:
        problems.append("the abstract is empty")
    elif len(abstract) > ABSTRACT_MAX:
        problems.append(f"the abstract has {len(abstract)} characters, arXiv takes {ABSTRACT_MAX}")
    arxiv = meta.get("arxiv") or {}
    cats = [arxiv.get("primary", "")] + list(arxiv.get("cross-list") or [])
    for c in cats:
        if not CATEGORY.match(str(c)):
            problems.append(f"category {c!r} is not an arXiv category id")
    if not arxiv.get("license"):
        problems.append("arxiv.license is missing")
    return problems


def revision():
    git = ["git", "-C", str(ROOT)]
    sha = subprocess.run(git + ["rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(git + ["status", "--porcelain", "--", "pubs/paper"], capture_output=True, text=True).stdout.strip()
    return f"{sha}{'+' if dirty else ''}" if sha else "unknown"


def form_fields(meta, rev):
    a = meta.get("arxiv") or {}
    authors = meta.get("author") or []
    authors = authors if isinstance(authors, list) else [authors]
    return "\n".join([
        f"Title: {' '.join(str(meta['title']).split())}",
        f"Authors: {', '.join(str(x) for x in authors)}",
        f"Primary category: {a.get('primary', '')}",
        f"Cross-lists: {' '.join(a.get('cross-list') or [])}",
        f"License: {a.get('license', '')}",
        f"Comments: {a.get('comments', '')}",
        f"Revision: {rev}",
        "",
        "Abstract:",
        " ".join(str(meta["abstract"]).split()),
        "",
    ])


def build(draft):
    md_path = PAPER / "paper.md"
    md = md_path.read_text(encoding="utf-8")
    meta = meta_of(md_path)
    problems = text_problems(md, meta, (PAPER / "refs.bib").read_text(encoding="utf-8"), draft)
    for p in problems:
        print(f"arxiv_bundle: {p}", file=sys.stderr)
    if problems:
        return 1
    rev = revision()
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tex = Path(tmp) / f"{SLUG}.tex"
        subprocess.run(["pandoc", str(md_path), "--standalone", "--citeproc", "--fail-if-warnings",
                        "--resource-path", str(PAPER), "--pdf-engine", "pdflatex",
                        "--metadata", f"date={meta.get('date', '')}, revision {rev}",
                        "--output", str(tex)], check=True, cwd=PAPER)
        for _ in range(2):
            r = subprocess.run(["pdflatex", "-interaction=nonstopmode", "-halt-on-error", tex.name],
                               cwd=tmp, capture_output=True, text=True)
            if r.returncode:
                print(r.stdout[-3000:], file=sys.stderr)
                print("arxiv_bundle: pdflatex failed", file=sys.stderr)
                return 1
        log = (Path(tmp) / f"{SLUG}.log").read_text(encoding="utf-8", errors="replace")
        missing = re.findall(r"Missing character: [^\n]*", log)
        if missing:
            print("\n".join(sorted(set(missing))), file=sys.stderr)
            print("arxiv_bundle: the font lacks a character the text uses", file=sys.stderr)
            return 1
        shutil.copy(Path(tmp) / f"{SLUG}.pdf", OUT / f"{SLUG}.pdf")
        with tarfile.open(OUT / f"{SLUG}.tar.gz", "w:gz") as tar:
            tar.add(tex, arcname=tex.name)
    (OUT / f"{SLUG}.txt").write_text(form_fields(meta, rev), encoding="utf-8")
    for ext in ("pdf", "tar.gz", "txt"):
        print(f"arxiv_bundle: {(OUT / f'{SLUG}.{ext}').relative_to(ROOT)}")
    return 0


def check_only(draft):
    md_path = PAPER / "paper.md"
    problems = text_problems(md_path.read_text(encoding="utf-8"), meta_of(md_path),
                             (PAPER / "refs.bib").read_text(encoding="utf-8"), draft)
    for p in problems:
        print(f"arxiv_bundle: {p}", file=sys.stderr)
    print(f"arxiv_bundle: {len(problems)} problem(s)")
    return 1 if problems else 0


def self_test():
    meta = {"title": "T", "abstract": "A short abstract.",
            "arxiv": {"primary": "physics.ao-ph", "cross-list": ["cs.DC"], "license": "http://x"}}
    bib = "@article{a, title={A}}\n"
    md = "---\ntitle: T\n---\nAs @a shows.\n"
    cases = [
        ("a good paper passes", md, meta, bib, False, 0),
        ("an author marker fails", md + "**[AUTHOR: x]**\n", meta, bib, False, 1),
        ("an author marker passes with --draft", md + "**[AUTHOR: x]**\n", meta, bib, True, 0),
        ("an uncited key fails", md, meta, bib + "@misc{b, title={B}}\n", False, 1),
        ("a long abstract fails", md, {**meta, "abstract": "x " * 1000}, bib, False, 1),
        ("an empty title fails", md, {**meta, "title": " "}, bib, False, 1),
        ("a bad category fails", md, {**meta, "arxiv": {**meta["arxiv"], "primary": "Ocean Physics"}}, bib, False, 1),
        ("a missing licence fails", md, {**meta, "arxiv": {"primary": "physics.ao-ph"}}, bib, False, 1),
    ]
    fails = 0
    for label, m, me, b, draft, want in cases:
        got = len(text_problems(m, me, b, draft))
        ok = (got > 0) == (want > 0)
        fails += 0 if ok else 1
        print(f"  {'ok  ' if ok else 'FAIL'} {label}")
    print("arxiv_bundle self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    draft = "--draft" in argv
    return check_only(draft) if "--check" in argv else build(draft)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
