#!/usr/bin/env python3
"""notebooklm_bundle — the repository's prose as a handful of NotebookLM sources.

    scripts/notebooklm_bundle.py            # -> build/notebooklm/*.md + sources.txt
    scripts/notebooklm_bundle.py --out DIR

NotebookLM takes Markdown, PDF and URLs as sources, with a cap on sources per notebook (50 on the
free plan) and on words per source (500 000). The repo has ~60 Markdown files, so this joins them
into one file per subject, writes the proposal's bibliography next to the text that cites it, and
lists the cited papers' DOI and URL links in sources.txt for adding as web sources. Upload by hand
(see docs/LLM_TOOLING_202610.md). Standard library only.
"""
import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
WORD_CAP = 500_000

BUNDLES = {
    "01-proposta-pt.md": ["pubs/proposal/pt/[0-9][0-9]-*.md"],
    "02-proposal-en.md": ["pubs/proposal/en/[0-9][0-9]-*.md"],
    "03-course.md": ["course/[0-9][0-9]-*.md"],
    "04-glossary.md": ["docs/GLOSSARY.md"],
    "05-plans-and-notes.md": ["docs/*_2026??.md", "README.md", "CONTRIBUTING.md"],
}


def bib_entries(path: pathlib.Path) -> dict:
    """{key: {field: value}} from a BibTeX file, enough for refs.bib (one level of nested braces)."""
    out = {}
    for m in re.finditer(r"@\w+\{([^,\s]+),(.*?)\n?\}\s*(?=@|\Z)", path.read_text(encoding="utf-8"), re.S):
        fields = re.findall(r"(\w+)\s*=\s*\{((?:[^{}]|\{[^{}]*\})*)\}", m.group(2))
        out[m.group(1)] = {k.lower(): re.sub(r"[{}]|\\['`^~\"]", "", v) for k, v in fields}
    return out


def reference_list(bib: dict) -> str:
    lines = ["\n# Referências / References\n"]
    for key, f in sorted(bib.items()):
        link = f"https://doi.org/{f['doi']}" if "doi" in f else f.get("url", "")
        lines.append(f"- [@{key}] {f.get('author', '').rstrip('.')}. {f.get('title', '')}. {f.get('year', '')}. {link}".rstrip())
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "build" / "notebooklm")
    a = ap.parse_args(argv)
    a.out.mkdir(parents=True, exist_ok=True)
    bib = bib_entries(ROOT / "pubs" / "proposal" / "refs.bib")
    for name, patterns in BUNDLES.items():
        files = [f for p in patterns for f in sorted(ROOT.glob(p))]
        parts = [f"<!-- {f.relative_to(ROOT)} -->\n" + re.sub(r"<!--.*?-->\n?", "", f.read_text(encoding="utf-8"), flags=re.S)
                 for f in files]
        text = "\n\n".join(parts) + (reference_list(bib) if name.startswith(("01", "02")) else "")
        (a.out / name).write_text(text, encoding="utf-8")
        words = len(text.split())
        flag = "  over NotebookLM's per-source cap, split it" if words > WORD_CAP else ""
        print(f"{name}: {len(files)} files, {words} words{flag}")
    links = sorted({f"https://doi.org/{f['doi']}" if "doi" in f else f["url"] for f in bib.values() if "doi" in f or "url" in f})
    (a.out / "sources.txt").write_text("\n".join(links) + "\n", encoding="utf-8")
    print(f"sources.txt: {len(links)} links; {len(BUNDLES)} files + {len(links)} links = "
          f"{len(BUNDLES) + len(links)} sources -> {a.out.relative_to(ROOT) if a.out.is_relative_to(ROOT) else a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
