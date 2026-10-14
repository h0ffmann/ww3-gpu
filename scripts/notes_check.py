#!/usr/bin/env python3
"""notes_check: the slip box keeps the rules of NOTES.md.

    scripts/notes_check.py              # check notes/ (exit 1 on any fault)
    scripts/notes_check.py DIR          # check another notes directory
    scripts/notes_check.py --self-test  # give it bad notes and expect each fault

Checks: the front-matter fields, the id against the file name, unique ids, the known types and
statuses, at least one [[link]] per note, every link resolves, and index.md links every note.
"""
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIELDS = ("id", "title", "type", "status", "source", "tags", "created")
TYPES = {"fleeting", "literature", "permanent"}
STATUSES = {"unverified", "partial", "verified", "open-question", "approved"}
NAME = re.compile(r"^(\d{12})-[a-z0-9]+(?:-[a-z0-9]+)*\.md$")
LINK = re.compile(r"\[\[([^\]|#]+)(?:[|#][^\]]*)?\]\]")


def front_matter(text):
    """The front matter as {field: raw value}, or None when the file has none."""
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---", 4)
    if end < 0:
        return None
    fields = {}
    for line in text[4:end].splitlines():
        m = re.match(r"^([a-z_-]+):\s*(.*)$", line)
        if m:
            fields[m.group(1)] = m.group(2).strip()
    return fields


def check(notes_dir):
    """Every fault in notes_dir, as a list of strings."""
    faults = []
    notes = sorted(p for p in notes_dir.glob("*.md") if p.name != "index.md")
    stems = {p.stem for p in notes}
    ids = {}
    for p in notes:
        where = p.name
        m = NAME.match(p.name)
        if not m:
            faults.append(f"{where}: name is not <YYYYMMDDHHMM>-<slug>.md")
        text = p.read_text(encoding="utf-8")
        fm = front_matter(text)
        if fm is None:
            faults.append(f"{where}: no front matter")
            continue
        for f in FIELDS:
            if not fm.get(f):
                faults.append(f"{where}: missing {f}")
        nid = fm.get("id", "")
        if not re.fullmatch(r'"\d{12}"', nid):
            faults.append(f'{where}: id is not a quoted "YYYYMMDDHHMM": {nid}')
        nid = nid.strip('"')
        if m and nid != m.group(1):
            faults.append(f"{where}: id {nid} does not match the file name")
        if nid in ids:
            faults.append(f"{where}: id {nid} also used by {ids[nid]}")
        ids[nid] = where
        if fm.get("type") and fm["type"] not in TYPES:
            faults.append(f"{where}: unknown type {fm['type']}")
        if fm.get("status") and fm["status"] not in STATUSES:
            faults.append(f"{where}: unknown status {fm['status']}")
        links = [t.strip() for t in LINK.findall(text)]
        if not links:
            faults.append(f"{where}: links to no other note")
        for t in links:
            if t not in stems:
                faults.append(f"{where}: [[{t}]] does not resolve")
    index = notes_dir / "index.md"
    if not index.exists():
        faults.append("index.md: missing")
    else:
        listed = {t.strip() for t in LINK.findall(index.read_text(encoding="utf-8"))}
        for s in sorted(stems - listed):
            faults.append(f"index.md: does not link {s}")
        for s in sorted(listed - stems):
            faults.append(f"index.md: [[{s}]] does not resolve")
    return faults


def note(nid, slug, links, **over):
    fields = {"id": f'"{nid}"', "title": "T", "type": "permanent", "status": "verified",
              "source": "S", "tags": "[x]", "created": "2026-10-14"}
    fields.update(over)
    head = "\n".join(f"{k}: {v}" for k, v in fields.items() if v is not None)
    body = " ".join(f"[[{t}]]" for t in links)
    return f"---\n{head}\n---\n\n# T\n\n{body}\n"


def self_test():
    a, b = "202610141900-a", "202610141901-b"
    cases = {
        "a good box": ({a: note("202610141900", "a", [b]), b: note("202610141901", "b", [a])}, None),
        "a missing field": ({a: note("202610141900", "a", [b], source=None), b: note("202610141901", "b", [a])}, "missing source"),
        "an id that is not the file's": ({a: note("202610141999", "a", [b]), b: note("202610141901", "b", [a])}, "does not match"),
        "an unquoted id": ({a: note("202610141900", "a", [b], id="202610141900"), b: note("202610141901", "b", [a])}, "not a quoted"),
        "an unknown type": ({a: note("202610141900", "a", [b], type="idea"), b: note("202610141901", "b", [a])}, "unknown type"),
        "an unknown status": ({a: note("202610141900", "a", [b], status="done"), b: note("202610141901", "b", [a])}, "unknown status"),
        "a note with no link": ({a: note("202610141900", "a", []), b: note("202610141901", "b", [a])}, "links to no other note"),
        "a link to nothing": ({a: note("202610141900", "a", ["202610141902-c"]), b: note("202610141901", "b", [a])}, "does not resolve"),
        "a note the index misses": ({a: note("202610141900", "a", [b]), b: note("202610141901", "b", [a])}, "does not link"),
    }
    fails = 0
    for label, (files, expect) in cases.items():
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            for stem, text in files.items():
                (d / f"{stem}.md").write_text(text)
            listed = list(files) if expect != "does not link" else [a]
            (d / "index.md").write_text("\n".join(f"- [[{s}]]" for s in listed) + "\n")
            got = check(d)
            ok = not got if expect is None else any(expect in f for f in got)
            print(f"  {'ok  ' if ok else 'FAIL'} {label}" + ("" if ok else f": {got}"))
            fails += 0 if ok else 1
    print("notes_check self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main(argv):
    if argv[:1] == ["--self-test"]:
        return self_test()
    d = Path(argv[0]) if argv else ROOT / "notes"
    faults = check(d)
    for f in faults:
        print(f"notes_check: {f}", file=sys.stderr)
    n = len([p for p in d.glob("*.md") if p.name != "index.md"])
    print(f"notes_check: {n} note(s), {len(faults)} problem(s)")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
