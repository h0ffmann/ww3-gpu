#!/usr/bin/env python3
"""adrs — one line per Architecture Decision Record in docs/ADRs/: number, status, date, decision.

    scripts/adrs.py              # all ADRs
    scripts/adrs.py --status accepted
    scripts/adrs.py --site TEMPLATE OUT   # the site's /adrs/ page: TEMPLATE with the list filled in

The decision is the ADR's title (the `# ADR-NNNN: …` line); every ADR so far titles itself with its
choice. Status and Date come from the header table. Standard library only.
"""
import argparse
import json
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


def site(template, out, folder=ADRS):
    """Write TEMPLATE to OUT with `const ADRS = [];` replaced by the records, newest number first."""
    files = sorted(folder.glob("ADR-[0-9][0-9][0-9][0-9]-*.md"))  # the order load() reads them in
    rows = [dict(zip(("n", "status", "date", "title"), r), file=f.name)
            for r, f in zip(load(folder), files)][::-1]
    text = pathlib.Path(template).read_text(encoding="utf-8")
    if "const ADRS = [];" not in text:
        raise SystemExit(f"{template}: no `const ADRS = [];` placeholder")
    data = json.dumps(rows, ensure_ascii=False).replace("</", "<\\/")
    pathlib.Path(out).write_text(text.replace("const ADRS = [];", f"const ADRS = {data};"), encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--status", help="only ADRs whose status starts with this (case-insensitive)")
    ap.add_argument("--site", nargs=2, metavar=("TEMPLATE", "OUT"), help="fill the site page")
    args = ap.parse_args(argv)
    if args.site:
        return site(*args.site)
    rows = [r for r in load() if not args.status or r[1].lower().startswith(args.status.lower())]
    for num, status, date, title in rows:
        print(f"{num}  {status:<10}  {date}  {title}")


if __name__ == "__main__":
    main()
