#!/usr/bin/env python3
"""adrs — one line per Architecture Decision Record in docs/ADRs/: number, status, date, decision.

    scripts/adrs.py              # all ADRs
    scripts/adrs.py --status accepted

The decision is the ADR's title (the `# ADR-NNNN: …` line); every ADR so far titles itself with its
choice. Status and Date come from the header table. Standard library only.
"""
import argparse
import pathlib
import re

ADRS = pathlib.Path(__file__).resolve().parent.parent / "docs" / "ADRs"
TITLE_RE = re.compile(r"^#\s*ADR-(\d{4}):\s*(.+?)\s*$", re.M)
ROW_RE = re.compile(r"^\|\s*\*\*(Status|Date)\*\*\s*\|\s*(.*?)\s*\|\s*$", re.M)


def load(folder=ADRS):
    out = []
    for p in sorted(folder.glob("ADR-[0-9][0-9][0-9][0-9]-*.md")):
        text = p.read_text(encoding="utf-8")
        m = TITLE_RE.search(text)
        rows = dict(ROW_RE.findall(text))
        out.append((f"ADR-{m.group(1)}" if m else p.stem[:8], rows.get("Status", "?"),
                    rows.get("Date", "?"), m.group(2) if m else p.stem))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--status", help="only ADRs whose status starts with this (case-insensitive)")
    args = ap.parse_args(argv)
    rows = [r for r in load() if not args.status or r[1].lower().startswith(args.status.lower())]
    for num, status, date, title in rows:
        print(f"{num}  {status:<10}  {date}  {title}")


if __name__ == "__main__":
    main()
