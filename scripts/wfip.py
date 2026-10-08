#!/usr/bin/env python3
"""wfip — the Wave Forecaster Improvement Proposals' tooling (docs/WFIPs/).

    scripts/wfip.py new <slug> --title "<title>" --deliverable D5   # next number, from TEMPLATE.md
    scripts/wfip.py index              # regenerate the tables in docs/WFIPs/README.md
    scripts/wfip.py check              # exit 1 on a stale index, unknown deliverable, missing section
    scripts/wfip.py status [--since <ref>]   # markdown for the release notes: what changed
    scripts/wfip.py --self-test

Every table in README.md between `<!-- wfip-*:start -->` and `:end` markers is generated from the
WFIP files; the deliverables table above them is the only hand-written one and is the list of
valid **Deliverable** ids. Only two metadata rows are parsed as data: **Deliverable** and
**Blocked by**; **Depends on** stays prose (marola's mip_graph.py learned that a prose field
carries several relations a regex cannot tell apart). The DoD is the `- [ ]`/`- [x]` boxes of §7.
Standard library only.
"""
import argparse
import datetime as dt
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WFIPS = ROOT / "docs" / "WFIPs"

TITLE_RE = re.compile(r"^#\s*WFIP-(\d{4}):\s*(.+?)\s*$")
ROW_RE = re.compile(r"^\|\s*\*\*(?P<key>[^*]+)\*\*\s*\|\s*(?P<val>.*?)\s*\|\s*$")
NUM_RE = re.compile(r"\b(\d{4})\b")
DELIV_RE = re.compile(r"\bD\d+\b")
BOX_RE = re.compile(r"^\s*- \[( |x|X)\]")
SECTIONS = ["## 1. Summary", "## 2. Motivation", "## 3. What changes for the reader",
            "## 4. Sources and dependencies reviewed", "## 5. Design",
            "## 6. Parity and physics impact", "## 7. Verification plan and definition of done",
            "## 8. Risks, limitations, and honest caveats", "## 9. Alternatives considered",
            "## 11. Open questions", "### Readiness"]
STATUS_CLASS = ["implemented", "accepted", "rejected", "superseded", "draft"]


class Wfip:
    def __init__(self, path, text):
        self.path = path
        lines = text.splitlines()
        self.num, self.title = None, path.stem
        for line in lines[:3]:
            m = TITLE_RE.match(line)
            if m:
                self.num, self.title = int(m.group(1)), m.group(2)
                break
        self.rows = {}
        for line in lines:
            m = ROW_RE.match(line)
            if m and m.group("key").strip().lower() not in self.rows:
                self.rows[m.group("key").strip().lower()] = m.group("val").strip()
        self.status = self.rows.get("status", "Draft")
        bb = self.rows.get("blocked by", "none")
        self.blocked_by = [] if bb.lower().startswith("none") else [int(n) for n in NUM_RE.findall(bb)]
        d = self.rows.get("deliverable", "")
        self.deliverables = [] if d.lower().startswith("none") else DELIV_RE.findall(d)
        self.missing = [s for s in SECTIONS if not any(l.startswith(s) for l in lines)]
        sec7 = text.split("## 7.", 1)[1].split("\n## ", 1)[0] if "## 7." in text else ""
        boxes = [BOX_RE.match(l).group(1) for l in sec7.splitlines() if BOX_RE.match(l)]
        self.dod = (sum(1 for b in boxes if b != " "), len(boxes))

    def status_class(self):
        s = self.status.lower()
        return next((c for c in STATUS_CLASS if s.startswith(c)), "draft")

    def link(self):
        return f"[WFIP-{self.num:04d}]({self.path.name})"


def load(wfips_dir=WFIPS):
    out = {}
    for path in sorted(wfips_dir.glob("WFIP-*.md")):
        if path.name.endswith(".tasks.md"):
            continue
        w = Wfip(path, path.read_text(encoding="utf-8"))
        if w.num is not None:
            out[w.num] = w
    return out


def deliverable_ids(readme_text):
    """The ids of the hand-written deliverables table (rows whose first cell is D<n>), read
    only above the first generated block so the coverage table never feeds itself."""
    hand = readme_text.split("<!-- wfip-index:start -->", 1)[0]
    return [m.group(1) for m in re.finditer(r"^\|\s*(D\d+)\s*\|", hand, re.M)]


def render_index(wfips):
    head = ["| WFIP | Title | Status | Created | Deliverable | Effort | Verdict | DoD | Cost so far |",
            "|---|---|---|---|---|---|---|---|---|"]
    if not wfips:
        return "\n".join(head + ["| — | no WFIP yet | | | | | | | |"])
    rows = []
    for n in sorted(wfips):
        w = wfips[n]
        r = w.rows
        effort = re.split(r":\s+|\s+—\s+|\s+-\s+", r.get("effort", ""), maxsplit=1)[0]
        verdict = re.split(r":\s+|\s+—\s+|\s+-\s+", r.get("effort vs gain", ""), maxsplit=1)[0]
        deliv = ", ".join(w.deliverables) or "none"
        rows.append(f"| {w.link()} | {w.title} | {w.status} | {r.get('created', '')} | {deliv} | "
                    f"{effort} | {verdict} | {w.dod[0]}/{w.dod[1]} | {r.get('cost so far', '—')} |")
    return "\n".join(head + rows)


def render_coverage(wfips, ids):
    lines = ["| Deliverable | WFIPs | Implemented |", "|---|---|---|"]
    for d in ids:
        hits = [w for w in wfips.values() if d in w.deliverables]
        done = [w for w in hits if w.status_class() == "implemented"]
        names = ", ".join(w.link() for w in sorted(hits, key=lambda w: w.num)) or "none yet"
        lines.append(f"| {d} | {names} | {len(done)}/{len(hits)} |")
    return "\n".join(lines)


def render_graph(wfips):
    edges = sorted({(b, w.num) for w in wfips.values() for b in w.blocked_by if b in wfips})
    if not edges:
        return "_No WFIP declares a **Blocked by** relationship yet._"
    nodes = {a for a, _ in edges} | {b for _, b in edges}
    out = ["```mermaid", "flowchart TD"]
    for n in sorted(nodes):
        out.append(f'  W{n:04d}["WFIP-{n:04d}"]:::{wfips[n].status_class()}')
    out += [f"  W{a:04d} --> W{b:04d}" for a, b in edges]
    out += ["  classDef draft stroke-dasharray:3 3;", "  classDef implemented stroke-width:3px;",
            "  classDef accepted stroke-width:2px;", "  classDef rejected color:#999;",
            "  classDef superseded color:#999;", "```"]
    return "\n".join(out)


def splice(text, name, body):
    start, end = f"<!-- wfip-{name}:start -->", f"<!-- wfip-{name}:end -->"
    if start not in text or end not in text:
        raise SystemExit(f"wfip: README.md lacks the {start} … {end} markers")
    head, rest = text.split(start, 1)
    _, tail = rest.split(end, 1)
    return f"{head}{start}\n{body}\n{end}{tail}"


def regenerate(readme_text, wfips):
    ids = deliverable_ids(readme_text)
    text = splice(readme_text, "index", render_index(wfips))
    text = splice(text, "coverage", render_coverage(wfips, ids))
    return splice(text, "graph", render_graph(wfips))


def problems(wfips, readme_text):
    ids = set(deliverable_ids(readme_text))
    errs = []
    for w in wfips.values():
        rel = w.path.relative_to(ROOT) if w.path.is_relative_to(ROOT) else w.path.name
        for d in w.deliverables:
            if d not in ids:
                errs.append(f"{rel}: deliverable {d} is not in README.md's table ({', '.join(sorted(ids))})")
        if not w.deliverables and not w.rows.get("deliverable", "").lower().startswith("none"):
            errs.append(f"{rel}: the **Deliverable** row is missing (ids, or `none: <why>`)")
        for s in w.missing:
            errs.append(f"{rel}: missing section `{s}`")
        for b in w.blocked_by:
            if b not in wfips:
                errs.append(f"{rel}: blocked by WFIP-{b:04d}, which does not exist")
        if w.dod[1] == 0:
            errs.append(f"{rel}: §7 has no `- [ ]` box; the definition of done is empty")
        if w.status_class() == "implemented" and w.dod[0] != w.dod[1]:
            errs.append(f"{rel}: Implemented with DoD {w.dod[0]}/{w.dod[1]}; tick the boxes or change the status")
    if regenerate(readme_text, wfips) != readme_text:
        errs.append("docs/WFIPs/README.md: generated tables are stale; run `just wfip index`")
    return errs


def cmd_new(slug, title, deliverable, wfips_dir=WFIPS, today=None):
    wfips = load(wfips_dir)
    num = max(wfips, default=0) + 1
    template = (wfips_dir / "TEMPLATE.md").read_text(encoding="utf-8")
    block = template.split("```markdown\n", 1)[1].split("\n```", 1)[0]
    body = block.replace("WFIP-NNNN: <Title>", f"WFIP-{num:04d}: {title}")
    body = body.replace("| **Created** | YYYY-MM-DD |", f"| **Created** | {today or dt.date.today().isoformat()} |")
    body = re.sub(r"\| \*\*Deliverable\*\* \|.*\|", f"| **Deliverable** | {deliverable} |", body, count=1)
    body = body.replace("WFIP-NNNN.tasks.md", f"WFIP-{num:04d}.tasks.md")
    path = wfips_dir / f"WFIP-{num:04d}-{slug}.md"
    if path.exists():
        raise SystemExit(f"wfip: {path} exists")
    path.write_text(body + "\n", encoding="utf-8")
    return path


def changed_since(ref, wfips_dir=WFIPS):
    """{'A': [paths], 'M': [paths]} of WFIP files touched since ref, from git; {} without git."""
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "log", "--name-status", "--format=", f"{ref}..HEAD",
                              "--", str(wfips_dir.relative_to(ROOT) / "WFIP-*.md")],
                             capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError, ValueError):
        return {}
    seen, changes = set(), {"A": [], "M": []}
    for line in out.splitlines():
        parts = line.split("\t")
        if len(parts) < 2 or parts[-1].endswith(".tasks.md"):
            continue
        name = pathlib.Path(parts[-1]).name
        if name in seen:
            continue
        seen.add(name)
        changes["A" if parts[0].startswith("A") else "M"].append(name)
    return changes


def render_status(wfips, ids, since=None, changes=None):
    lines = [f"## WFIP status{f' since {since}' if since else ''}", ""]
    if since and changes is not None:
        byname = {w.path.name: w for w in wfips.values()}
        for key, label in (("A", "Created"), ("M", "Revised")):
            names = [n for n in changes.get(key, []) if n in byname]
            if names:
                lines.append(f"**{label}:** " + "; ".join(
                    f"WFIP-{byname[n].num:04d} {byname[n].title} ({byname[n].status}, DoD {byname[n].dod[0]}/{byname[n].dod[1]})"
                    for n in names))
        if not any(changes.get(k) for k in ("A", "M")):
            lines.append("No WFIP created or revised.")
        lines.append("")
    lines += [render_index(wfips), "", "**Deliverables covered**", "", render_coverage(wfips, ids)]
    # the notes are read on GitHub and Zenodo, away from docs/WFIPs/, so links are absolute
    return "\n".join(lines).replace("](WFIP-", "](https://github.com/h0ffmann/ww3-gpu/blob/main/docs/WFIPs/WFIP-")


def self_test():
    import tempfile
    tmp = pathlib.Path(tempfile.mkdtemp())
    (tmp / "TEMPLATE.md").write_text((WFIPS / "TEMPLATE.md").read_text(encoding="utf-8"), encoding="utf-8")
    readme = (WFIPS / "README.md").read_text(encoding="utf-8")
    p = cmd_new("x", "X", "D1", tmp, today="2026-01-01")
    w = load(tmp)[1]
    assert w.title == "X" and w.deliverables == ["D1"] and w.dod == (0, 2) and not w.missing, (w.title, w.deliverables, w.dod, w.missing)
    assert render_index({1: w}).count("| 0/2 |") == 1
    text = p.read_text(encoding="utf-8").replace("| **Deliverable** | D1 |", "| **Deliverable** | D9 |")
    p.write_text(text, encoding="utf-8")
    errs = problems(load(tmp), readme)
    assert any("D9" in e for e in errs), errs
    assert regenerate(regenerate(readme, {}), {}) == regenerate(readme, {})
    print("wfip: self-test ok")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--self-test", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    n = sub.add_parser("new")
    n.add_argument("slug")
    n.add_argument("--title", required=True)
    n.add_argument("--deliverable", default="none: <why>")
    sub.add_parser("index")
    sub.add_parser("check")
    s = sub.add_parser("status")
    s.add_argument("--since", help="a git ref, usually the previous release tag")
    a = ap.parse_args(argv)
    if a.self_test:
        self_test()
        return 0
    readme_path = WFIPS / "README.md"
    if a.cmd == "new":
        print(cmd_new(a.slug, a.title, a.deliverable))
        return 0
    wfips = load()
    readme = readme_path.read_text(encoding="utf-8")
    if a.cmd == "index":
        readme_path.write_text(regenerate(readme, wfips), encoding="utf-8")
        print(f"wfip: {readme_path.relative_to(ROOT)} regenerated from {len(wfips)} WFIP(s)")
        return 0
    if a.cmd == "check":
        errs = problems(wfips, readme)
        for e in errs:
            print(f"wfip: {e}", file=sys.stderr)
        print(f"wfip: {len(wfips)} WFIP(s), {len(errs)} problem(s)")
        return 1 if errs else 0
    if a.cmd == "status":
        print(render_status(wfips, deliverable_ids(readme), a.since, changed_since(a.since) if a.since else None))
        return 0
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
