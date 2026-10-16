#!/usr/bin/env python3
"""route — answer a "where is it" question in one bounded call, instead of reading a whole file.

    scripts/route.py term NL1             # the GLOSSARY.md row(s), not the 200 KB file
    scripts/route.py where W3SNL1         # where a routine, function, recipe or type is defined
    scripts/route.py docs snl1_shim.cpp   # which pages name a file or symbol (doc -> code, reversed)
    scripts/route.py why "tolerance"      # the decision, plan or log paragraphs that best match
    scripts/route.py section docs/AGENTS_KOKKOS_202609.md 1.5   # one section of a page
    scripts/route.py ... --budget 80      # output lines (default 40)

The ww3-gpu adaptation of marola's MIP-0084 (`route`) and MIP-0085 (doc<->code, decisions): one
repository, so the indexes are built from `git ls-files` on each call (well under a second) rather
than shipped as a CI bundle. The submodules are skipped, except `WW3/model/src` for `where`, which
says so when it is not checked out. An answer is a locator: a claim still cites the `file:line` it
read. Standard library only.
"""
import argparse
import math
import pathlib
import re
import subprocess
import sys
from collections import Counter

ROOT = pathlib.Path(__file__).resolve().parent.parent
SUBMODULES = ("WW3/", "WW3-noaa/", "WW4/", "picasso/")
CODE = (".cpp", ".hpp", ".h", ".cu", ".py", ".sh", ".F90", ".f90", ".cmake", "CMakeLists.txt",
        "justfile", ".nix")
WHY_DIRS = ("docs/", "notes/", "kokkos/port_report/")
STOP = set("a an and are as at be by for from how in is it of on or that the this to was what "
           "when where which who why with does do".split())


def tracked(root=ROOT):
    try:
        out = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True,
                             check=True).stdout.split("\n")
    except (OSError, subprocess.CalledProcessError):
        out = [str(p.relative_to(root)) for p in root.rglob("*") if p.is_file()]
    return [f for f in out if f and not f.startswith(SUBMODULES) and (root / f).is_file()]


def lines(root, f):
    try:
        return (root / f).read_text(encoding="utf-8", errors="replace").split("\n")
    except OSError:
        return []


def heading_before(text, i):
    for j in range(i, -1, -1):
        if text[j].startswith("#"):
            return text[j].lstrip("#").strip()
    return ""


def term(name, root=ROOT):
    """GLOSSARY.md rows whose first cell names `name`; an exact name first, then a substring."""
    text = lines(root, "docs/GLOSSARY.md")
    want, exact, near = name.lower(), [], []
    for i, ln in enumerate(text):
        if not ln.startswith("| ") or ln.startswith(("| Term", "|---")):
            continue
        cell = ln.split("|")[1].replace("`", "").strip().lower()
        parts = {p.strip() for p in re.split(r"\s*(?:/|,|\(|\))\s*", cell) if p.strip()}
        for p in list(parts):  # a family row, "NL0–NL5", names NL1 too
            r = re.fullmatch(r"(\D+)(\d+)[–-](?:\1)?(\d+)", p)
            if r:
                parts |= {f"{r[1]}{d}" for d in range(int(r[2]), int(r[3]) + 1)}
        if want == cell or want in parts:
            exact.append(i)
        elif want in cell:
            near.append(i)
    sec = lambda i: heading_before(text, i)
    return [f"docs/GLOSSARY.md:{i + 1}  [{sec(i)}]  {text[i]}" for i in exact + near]


def _def_patterns(name):
    n = re.escape(name)
    return [
        (CODE[:4], re.compile(rf"^\s*(?:(?:struct|class|enum|union|namespace|using)\s+{n}\b"
                              rf"|#\s*define\s+{n}\b"
                              rf"|(?!.*\b(?:return|if|while|for)\b)[\w:<>,*&\s]*\b{n}\s*\([^;]*$)")),
        ((".py",), re.compile(rf"^\s*(?:(?:async\s+)?def|class)\s+{n}\b|^{n}\s*=")),
        ((".sh",), re.compile(rf"^\s*(?:function\s+)?{n}\s*\(\)")),
        (("justfile",), re.compile(rf"^@?{n}(?:\s+[^:]*)?:(?!=)")),
        ((".cmake", "CMakeLists.txt"), re.compile(rf"^\s*(?:function|macro)\s*\(\s*{n}\b", re.I)),
        ((".F90", ".f90"), re.compile(rf"^\s*(?:[\w(),*\s]*\s)?(?:subroutine|function|module|"
                                      rf"program|interface|type(?:\s*,[^:]*::|\s*::|\s))\s*{n}\b",
                                      re.I)),
    ]


def where(name, root=ROOT):
    """Definitions of `name` in this repository's code, then in WW3/model/src at the pin."""
    pats = _def_patterns(name)
    files = [f for f in tracked(root) if f.endswith(CODE)]
    src = root / "WW3" / "model" / "src"
    ww3 = sorted(str(p.relative_to(root)) for p in src.glob("*.F90")) if src.is_dir() else []
    out = [f"{f}  (file)" for f in files + ww3 if pathlib.Path(f).stem.lower() == name.lower()]
    for f in files + ww3:
        text = lines(root, f)
        if name.lower() not in "\n".join(text).lower():
            continue
        for exts, rx in pats:
            if f.endswith(exts):
                out += [f"{f}:{i + 1}  {ln.strip()[:120]}" for i, ln in enumerate(text)
                        if rx.search(ln)]
    if not ww3:
        out.append("(WW3/model/src not checked out: `git submodule update --init WW3` to search "
                   "the Fortran; cite it at the pin, `just src-st`)")
    return out


def docs(target, root=ROOT, per_file=3):
    """Markdown lines that name `target` in backticks or in a link, with the section they sit in."""
    base = target.rstrip("/").split("/")[-1].lower()
    tick = re.compile(r"`([^`]+)`|\]\(([^)]+)\)")
    out = []
    for f in tracked(root):
        if not f.endswith(".md"):
            continue
        text, hits = lines(root, f), 0
        for i, ln in enumerate(text):
            for a, b in tick.findall(ln):
                tok = (a or b).lower().split("#")[0].split(":")[0]
                if tok and (tok == target.lower() or tok.rstrip("/").split("/")[-1] == base
                            or re.search(rf"\b{re.escape(target.lower())}\b", tok)):
                    if hits < per_file:
                        sec = heading_before(text, i)[:60]
                        out.append(f"{f}:{i + 1}" + (f"  [{sec}]" if sec else ""))
                    hits += 1
                    break
        if hits > per_file:
            out.append(f"{f}  (+{hits - per_file} more lines)")
    return out


def _words(s):
    return [w for w in re.findall(r"[a-z0-9_]+", s.lower()) if w not in STOP and len(w) > 1]


def why(question, root=ROOT, k=8):
    """BM25 over the paragraphs of docs/ (ADRs, WFIPs, plans, log), notes/ and the port reports."""
    paras = []
    for f in tracked(root):
        if not (f.endswith(".md") and f.startswith(WHY_DIRS)):
            continue
        text, start, buf = lines(root, f), 0, []
        for i, ln in enumerate(text + [""]):
            if ln.strip():
                if not buf:
                    start = i
                buf.append(ln)
            elif buf:
                paras.append((f, start, " ".join(buf)))
                buf = []
    toks = [_words(p[2]) for p in paras]
    if not toks:
        return []
    df = Counter(w for t in toks for w in set(t))
    avg = sum(map(len, toks)) / len(toks)
    q = _words(question)
    scored = []
    for (f, s, p), t in zip(paras, toks):
        tf, score = Counter(t), 0.0
        for w in q:
            if tf[w]:
                idf = math.log(1 + (len(toks) - df[w] + 0.5) / (df[w] + 0.5))
                score += idf * tf[w] * 2.2 / (tf[w] + 1.2 * (0.25 + 0.75 * len(t) / avg))
        if score:
            scored.append((score, f, s, p))
    scored.sort(key=lambda x: -x[0])
    return [f"{f}:{s + 1}  " + re.sub(r"\s+", " ", p)[:180] for _, f, s, p in scored[:k]]


def section(path, which, root=ROOT):
    """One section of a Markdown file: from the heading that matches to the next of its level."""
    text = lines(root, path)
    want = which.lower().lstrip("§")
    for i, ln in enumerate(text):
        m = re.match(r"^(#+)\s+(.*)", ln)
        if m and (m.group(2).lower().startswith(want) or want in m.group(2).lower()):
            lvl, end = len(m.group(1)), len(text)
            for j in range(i + 1, len(text)):
                n = re.match(r"^(#+)\s", text[j])
                if n and len(n.group(1)) <= lvl:
                    end = j
                    break
            return [f"{path}:{i + 1}-{end}"] + text[i:end]
    return []


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["term", "where", "docs", "why", "section"])
    ap.add_argument("args", nargs="+")
    ap.add_argument("--budget", type=int, default=40, help="output lines (default 40)")
    a = ap.parse_args(argv)
    if a.command == "section":
        if len(a.args) != 2:
            ap.error("section takes FILE HEADING")
        out = section(*a.args)
    else:
        out = {"term": term, "where": where, "docs": docs, "why": why}[a.command](" ".join(a.args))
    if not out:
        print(f"nothing for {a.command} {' '.join(a.args)!r}; try `git grep -n` scoped to a "
              "directory, never the submodules")
        return 1
    print("\n".join(out[:a.budget]))
    if len(out) > a.budget:
        print(f"… {len(out) - a.budget} more; --budget to widen, or narrow the question")
    return 0


if __name__ == "__main__":
    sys.exit(main())
