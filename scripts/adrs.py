#!/usr/bin/env python3
"""adrs — one line per Architecture Decision Record in docs/ADRs.md: number, status, date, decision.

    scripts/adrs.py              # all ADRs
    scripts/adrs.py --status accepted
    scripts/adrs.py --site TEMPLATE OUT   # the site's /adrs/ page: TEMPLATE with the list filled in

Every ADR is a section of docs/ADRs.md (ADR-0022) whose heading, `## ADR-NNNN: …`, states its
choice. Status and Date come from the section's header table. Standard library only.
"""
import argparse
import json
import pathlib
import re

ADRS = pathlib.Path(__file__).resolve().parent.parent / "docs" / "ADRs.md"
TITLE_RE = re.compile(r"^##\s*ADR-(\d{4}):\s*(.+?)\s*$", re.M)
ROW_RE = re.compile(r"^\|\s*\*\*(Status|Date)\*\*\s*\|\s*(.*?)\s*\|\s*$", re.M)


def load(path=ADRS):
    """(number, status, date, title) per section, in the document's order."""
    text = path.read_text(encoding="utf-8")
    heads = list(TITLE_RE.finditer(text))
    out = []
    for m, nxt in zip(heads, heads[1:] + [None]):
        rows = dict(ROW_RE.findall(text[m.end():nxt.start() if nxt else len(text)]))
        out.append((f"ADR-{m.group(1)}", rows.get("Status", "?"), rows.get("Date", "?"), m.group(2)))
    return out


def site(template, out, path=ADRS):
    """Write TEMPLATE to OUT with `const ADRS = [];` replaced by the records, newest number first."""
    rows = [dict(zip(("n", "status", "date", "title"), r), file=f"{path.name}#{r[0].lower()}")
            for r in load(path)][::-1]
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
