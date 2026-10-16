#!/usr/bin/env python3
"""context — pack, query and check the context an agent sends to Claude Code or Kimi Code.

    scripts/context.py pack --goal <id> [--files f1,f2] [--gate-results <file>]
    scripts/context.py query "<question>" [--budget N]
    scripts/context.py check <pack.md>
    scripts/context.py --self-test

The packer keeps the invariants this repository requires on compaction (AGENTS.md, CLAUDE.md):
modified files, gate results, and the (v)/⚠ status of every number. Everything else is retrieved
on demand through the routing skill (scripts/route.py) or a small local BM25 index.

Standard library only, so it runs in a fresh container, a jail and Kimi Code alike.
"""
import argparse
import json
import math
import pathlib
import re
import subprocess
import sys
import tempfile
from collections import Counter

ROOT = pathlib.Path(__file__).resolve().parent.parent
SUBMODULES = ("WW3/", "WW3-noaa/", "WW4/", "picasso/")
CODE_AND_DOCS = (".md", ".py", ".sh", ".cpp", ".hpp", ".h", ".F90", ".f90")
QUERY_DIRS = ("docs/", "notes/", "kokkos/port_report/")
STOP = set("a an and are as at be by for from how in is it of on or that the this to was what "
           "when where which who why with does do".split())
LEAKS = (
    (re.compile(r"[\w.+-]+@[\w-]+\.[\w.-]+"), "an e-mail address"),
    (re.compile(r"(/home/|/Users/)[\w.-]+"), "an absolute home path"),
)
PACK_REQUIRED_SECTIONS = (
    "Goal id",
    "Modified files",
    "Gate results",
    "Evidence claims",
    "Related context",
    "Summary",
)


def tracked(root=ROOT):
    """Repo-owned files, excluding the large read-only submodules."""
    try:
        out = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True,
                             check=True).stdout.split("\n")
    except (OSError, subprocess.CalledProcessError):
        out = [str(p.relative_to(root)) for p in root.rglob("*") if p.is_file()]
    return [f for f in dict.fromkeys(out)
            if f and not f.startswith(SUBMODULES) and (root / f).is_file()]


def lines(root, f):
    try:
        return (root / f).read_text(encoding="utf-8", errors="replace").split("\n")
    except OSError:
        return []


def _words(s):
    return [w for w in re.findall(r"[a-z0-9_]+", s.lower()) if w not in STOP and len(w) > 1]


def git_status(root=ROOT):
    """git status --short, including submodule state lines but not their contents."""
    try:
        out = subprocess.run(["git", "status", "--short"], cwd=root, capture_output=True,
                             text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return []
    if not out:
        return []
    return [ln for ln in out.split("\n") if ln.strip()]


def gate_results(path=None):
    """Parse gate results from a JSON file or return an empty list."""
    if not path:
        return []
    p = pathlib.Path(path)
    if not p.is_file():
        return []
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    if isinstance(data, list):
        return data
    if isinstance(data, dict) and "gates" in data:
        return data["gates"]
    return []


def evidence_claims(file_list, root=ROOT):
    """Lines from the listed files that carry (v) or ⚠."""
    out = []
    seen = set()
    for f in file_list:
        f = f.strip()
        if not f or f in seen:
            continue
        seen.add(f)
        for i, ln in enumerate(lines(root, f)):
            if "(v)" in ln or "⚠" in ln:
                out.append(f"{f}:{i + 1}  {ln.strip()}")
    return out


def route_why(question, root=ROOT, budget=8):
    """Bounded call to scripts/route.py why, the preferred retriever."""
    try:
        out = subprocess.run([sys.executable, "scripts/route.py", "why", question,
                              "--budget", str(budget)], cwd=root, capture_output=True,
                             text=True, check=False)
    except OSError:
        return []
    if out.returncode != 0 or not out.stdout.strip():
        return []
    return [ln for ln in out.stdout.strip().split("\n") if ln.strip()][:budget]


def _paragraphs(root, dirs):
    """Paragraphs from repo-owned Markdown under the given directories."""
    paras = []
    for f in tracked(root):
        if not f.endswith(".md"):
            continue
        if not any(f.startswith(d) for d in dirs):
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
    return paras


def _bm25(question, paras, k=8):
    """BM25 over paragraphs; returns (file, start, text) tuples."""
    if not paras:
        return []
    toks = [_words(p[2]) for p in paras]
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
    return [f"{f}:{s + 1}  " + re.sub(r"\s+", " ", p)[:180]
            for _, f, s, p in scored[:k]]


def local_query(question, root=ROOT, budget=8):
    """Fallback retriever when route.py why returns nothing."""
    paras = _paragraphs(root, QUERY_DIRS)
    return _bm25(question, paras, k=budget)


def query(question, root=ROOT, budget=8):
    out = route_why(question, root, budget=budget)
    if not out:
        out = local_query(question, root, budget=budget)
    return out


def pack(goal, files=None, gate_results_path=None, root=ROOT, budget=8):
    """Build a Markdown context pack and return it as a string."""
    if not goal:
        raise ValueError("--goal is required")
    file_list = [f for f in (files or "").split(",") if f.strip()]
    status = git_status(root)
    gates = gate_results(gate_results_path)
    claims = evidence_claims(file_list, root)
    related = query(goal, root, budget=budget)
    safe_goal = re.sub(r"[^a-zA-Z0-9_.-]+", "-", goal).strip("-")
    lines_out = [
        f"# Context pack: {goal}",
        "",
        "This is a handoff summary, not evidence. Every claim still cites `file:line`.",
        "",
        "## Goal id",
        "",
        goal,
        "",
        "## Modified files",
        "",
    ]
    if status:
        lines_out.extend(status)
    else:
        lines_out.append("(none)")
    lines_out.extend(["", "## Gate results", ""])
    if gates:
        for g in gates:
            cmd = g.get("command", "")
            res = g.get("result", "")
            date = g.get("date", "")
            lines_out.append(f"- `{cmd}` → {res}" + (f" ({date})" if date else ""))
    else:
        lines_out.append("(none recorded)")
    lines_out.extend(["", "## Evidence claims", ""])
    if claims:
        lines_out.extend(claims)
    else:
        lines_out.append("(none in the listed files)")
    lines_out.extend(["", "## Related context", ""])
    if related:
        lines_out.extend(related)
    else:
        lines_out.append("(none found)")
    lines_out.extend(["", "## Summary", "", "(to be filled by the writing agent)", ""])
    text = "\n".join(lines_out)
    return safe_goal, text


def check_pack(text):
    """Validate a pack; return a list of fault strings."""
    faults = []
    for section in PACK_REQUIRED_SECTIONS:
        if f"## {section}" not in text:
            faults.append(f"missing section: {section}")
    for pattern, what in LEAKS:
        for m in pattern.finditer(text):
            snippet = text[max(0, m.start() - 20):m.end() + 20].replace("\n", " ")
            faults.append(f"contains {what}: ...{snippet}...")
    if "## Gate results" in text:
        gate_block = text.split("## Gate results", 1)[1].split("## ", 1)[0]
        if "`" in gate_block and "→" not in gate_block:
            faults.append("gate results list commands but no result arrow")
    return faults


def write_pack(goal, files=None, gate_results_path=None, root=ROOT, budget=8, dry_run=False):
    safe_goal, text = pack(goal, files, gate_results_path, root, budget)
    faults = check_pack(text)
    if faults:
        for f in faults:
            print(f"context pack: {f}", file=sys.stderr)
        return 1
    if dry_run:
        print(text)
        return 0
    out_dir = root / "context" / "handoff"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / f"goal-{safe_goal}.md"
    out_file.write_text(text, encoding="utf-8")
    print(out_file)
    return 0


def self_test():
    """Deterministic tests using temporary directories."""
    failed = 0

    # 1. pack produces required sections
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        (root / "docs").mkdir()
        (root / "docs" / "test.md").write_text("# Hello\n\nClaim 1.5 s (v) measured.\n", encoding="utf-8")
        (root / "scripts").mkdir()
        (root / "scripts" / "route.py").write_text(
            "import sys\nsys.exit(1 if sys.argv[1] == 'why' else 0)\n", encoding="utf-8")
        # No git repo, so git_status returns empty.
        safe, text = pack("test-goal", files="docs/test.md", root=root, budget=2)
        for section in PACK_REQUIRED_SECTIONS:
            if f"## {section}" not in text:
                print(f"self-test: pack missing {section}", file=sys.stderr)
                failed += 1
        if "(v)" not in text:
            print("self-test: pack did not keep (v) claim", file=sys.stderr)
            failed += 1

    # 2. query fallback BM25 finds a paragraph
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        (root / "docs").mkdir()
        (root / "docs" / "adr.md").write_text(
            "# ADR\n\nWe never widen a tolerance.\n\nOther text.\n", encoding="utf-8")
        out = local_query("tolerance widening", root, budget=2)
        if not any("tolerance" in ln.lower() for ln in out):
            print(f"self-test: query missed tolerance: {out}", file=sys.stderr)
            failed += 1

    # 3. check refuses leaks
    bad = "# Pack\n\n## Goal id\n\ng\n\n## Modified files\n\n(none)\n\n## Gate results\n\n(none recorded)\n\n## Evidence claims\n\n(none)\n\n## Related context\n\n(none)\n\n## Summary\n\nmail me at foo@example.org\n"
    faults = check_pack(bad)
    if not any("e-mail" in f for f in faults):
        print("self-test: check did not catch e-mail", file=sys.stderr)
        failed += 1

    # 4. check accepts clean pack
    clean, _ = pack("clean-goal", root=pathlib.Path(tmp))
    clean_text = _  # noqa: F841
    # Build a minimal valid pack manually to avoid git dependency.
    clean_text = "# p\n\n## Goal id\n\nclean\n\n## Modified files\n\n(none)\n\n## Gate results\n\n(none recorded)\n\n## Evidence claims\n\n(none)\n\n## Related context\n\n(none)\n\n## Summary\n\nok\n"
    if check_pack(clean_text):
        print(f"self-test: clean pack reported faults: {check_pack(clean_text)}", file=sys.stderr)
        failed += 1

    print("context self-test:", "FAIL" if failed else "ok")
    return 1 if failed else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--self-test", action="store_true", help="run deterministic self-tests")
    sub = ap.add_subparsers(dest="command")

    p_pack = sub.add_parser("pack", help="write a context handoff card")
    p_pack.add_argument("--dry-run", action="store_true",
                        help="print the pack instead of writing it")
    p_pack.add_argument("--goal", required=True, help="goal or session identifier")
    p_pack.add_argument("--files", default="",
                        help="comma-separated list of files that carry evidence claims")
    p_pack.add_argument("--gate-results", default="",
                        help="JSON file with gate results")
    p_pack.add_argument("--budget", type=int, default=8,
                        help="maximum related-context lines (default 8)")

    p_query = sub.add_parser("query", help="bounded question over docs/notes/port reports")
    p_query.add_argument("question", nargs="+", help="the question")
    p_query.add_argument("--budget", type=int, default=8,
                         help="maximum result lines (default 8)")

    p_check = sub.add_parser("check", help="validate a handoff card")
    p_check.add_argument("pack", help="path to the Markdown pack")

    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if a.command == "pack":
        return write_pack(a.goal, files=a.files, gate_results_path=a.gate_results or None,
                          budget=a.budget, dry_run=a.dry_run)
    if a.command == "query":
        out = query(" ".join(a.question), budget=a.budget)
        if not out:
            print("nothing found; narrow the question or widen --budget", file=sys.stderr)
            return 1
        print("\n".join(out))
        return 0
    if a.command == "check":
        text = pathlib.Path(a.pack).read_text(encoding="utf-8")
        faults = check_pack(text)
        if faults:
            for f in faults:
                print(f"context check: {f}", file=sys.stderr)
            return 1
        print("context check: ok")
        return 0
    ap.print_help()
    return 64


if __name__ == "__main__":
    sys.exit(main())
