#!/usr/bin/env python3
"""refs_check: every entry of a refs.bib has one row in its REFERENCES.md, with a verdict.

    python3 scripts/refs_check.py                  # pubs/proposal/refs.bib against pubs/proposal/REFERENCES.md
    python3 scripts/refs_check.py BIB REFERENCES   # another pair
    python3 scripts/refs_check.py --strict         # an `unverified` row also fails
    python3 scripts/refs_check.py --self-test

Fails when a bib key has no row or more than one, a row names no bib key, or a status is not one
of `verified`, `partial`, `unverified`. An `unverified` row, and an entry with neither a `doi` nor
a `url`, are reported but pass by default: the proposal may still cite a source that has not been
checked, or one that has no link yet, and a change to its refs.bib goes through `revisor-proposta`
first (AGENTS.md, "The proposal"). `--strict` makes both failures, the gate for a text that may not. Adapted from
h0ffmann/closed-source's `tools/fabric refs-check` (`tools/src/fabric/refs_check.gleam`, at 5cf9a6a).
"""

import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIB = ROOT / "pubs" / "proposal" / "refs.bib"
REFS = ROOT / "pubs" / "proposal" / "REFERENCES.md"
STATUSES = ("verified", "partial", "unverified")


def bib_entries(text):
    """{key: set of field names} for each @type{key, ...} entry."""
    entries = {}
    starts = [m for m in re.finditer(r"@\w+\s*\{\s*([^,\s]+)\s*,", text)]
    for i, m in enumerate(starts):
        body = text[m.end(): starts[i + 1].start() if i + 1 < len(starts) else len(text)]
        fields = {f.lower() for f in re.findall(r"(?:^|[,\s])(\w+)\s*=", body)}
        entries[m.group(1)] = fields
    return entries


def rows(text):
    """[(key, status)] from the table rows whose first cell is a `key` in backticks."""
    out = []
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 2 and re.fullmatch(r"`[^`]+`", cells[0]):
            out.append((cells[0].strip("`"), cells[1].strip("*").lower()))
    return out


def check(bib_text, refs_text, strict=False):
    """(problems, notes): problems fail the check, notes are printed."""
    entries = bib_entries(bib_text)
    table = rows(refs_text)
    problems, notes = [], []
    seen = {}
    for key, status in table:
        seen[key] = seen.get(key, 0) + 1
        if key not in entries:
            problems.append(f"row `{key}` names no entry of the bib")
        if status not in STATUSES:
            problems.append(f"`{key}`: status {status!r} is not one of {', '.join(STATUSES)}")
        elif status == "unverified":
            (problems if strict else notes).append(f"`{key}` is unverified")
    for key, fields in entries.items():
        n = seen.get(key, 0)
        if n == 0:
            problems.append(f"`{key}` has no row in REFERENCES.md")
        elif n > 1:
            problems.append(f"`{key}` has {n} rows in REFERENCES.md")
        if "doi" not in fields and "url" not in fields:
            (problems if strict else notes).append(f"`{key}` has neither a doi nor a url")
    return problems, notes


def run(bib, refs, strict):
    problems, notes = check(bib.read_text(encoding="utf-8"), refs.read_text(encoding="utf-8"), strict)
    for n in notes:
        print(f"refs_check: note: {n}")
    for p in problems:
        print(f"refs_check: {p}", file=sys.stderr)
    n = len(bib_entries(bib.read_text(encoding="utf-8")))
    print(f"refs_check: {bib.relative_to(ROOT) if bib.is_relative_to(ROOT) else bib}: {n} entries, {len(problems)} problem(s)")
    return 1 if problems else 0


def self_test():
    bib = "@article{a, title={A}, doi={10.1/a}}\n@misc{b, title={B}, url={https://b}}\n"
    good = "| Key | Status | Checked against | Note |\n|---|---|---|---|\n| `a` | verified | x | y |\n| `b` | partial | x | y |\n"
    cases = [
        ("a good pair passes", bib, good, False, 0),
        ("a missing row fails", bib, good.replace("| `b` | partial | x | y |\n", ""), False, 1),
        ("a row with no entry fails", bib, good + "| `c` | verified | x | y |\n", False, 1),
        ("a duplicate row fails", bib, good + "| `a` | verified | x | y |\n", False, 1),
        ("an unknown status fails", bib, good.replace("partial", "ok"), False, 1),
        ("an entry with no doi or url passes by default", bib + "@misc{c, title={C}}\n", good + "| `c` | verified | x | y |\n", False, 0),
        ("an entry with no doi or url fails with --strict", bib + "@misc{c, title={C}}\n", good + "| `c` | verified | x | y |\n", True, 1),
        ("unverified passes by default", bib, good.replace("partial", "unverified"), False, 0),
        ("unverified fails with --strict", bib, good.replace("partial", "unverified"), True, 1),
    ]
    fails = 0
    for label, b, r, strict, want in cases:
        got = len(check(b, r, strict)[0])
        ok = (got > 0) == (want > 0)
        fails += 0 if ok else 1
        print(f"  {'ok  ' if ok else 'FAIL'} {label}")
    print("refs_check self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    strict = "--strict" in argv
    paths = [a for a in argv if not a.startswith("--")]
    bib, refs = (Path(paths[0]), Path(paths[1])) if len(paths) == 2 else (BIB, REFS)
    if len(paths) not in (0, 2):
        print("refs_check: usage: refs_check.py [--strict] [BIB REFERENCES]", file=sys.stderr)
        return 2
    return run(bib.resolve(), refs.resolve(), strict)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
