#!/usr/bin/env python3
"""memory: the ai-memory pages this repository publishes, in memory/ (MEMORY.md, ADR-0009).

    scripts/memory.py export [--dry-run]  # owner: wiki pages tagged `publish` -> memory/, index rewritten
    scripts/memory.py import [--dry-run]  # reader: memory/ -> published/ in the local store, unpinned
    scripts/memory.py check               # CI: memory/ holds only publishable pages and its index is current
    scripts/memory.py --self-test         # give check bad pages and expect each fault

The wiki lives on one machine; memory/ is the part of it a reader can see. A page is published
only when it carries the tag `publish`. Session pages and the monthly logs never are: they hold raw
prompts and local paths. The project comes from .ai-memory.toml, so export and import name the
same project on every machine.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "memory"
INDEX = "README.md"
TAG = "publish"
# Never published, whatever their tags: raw session capture and ai-memory's own manifests.
NEVER = re.compile(r"(^|/)(sessions/|log-\d{4}-\d{2}\.md$|_meta\.md$)")
# Prefixes ai-memory treats as maintained or cross-project; a page from git must not pose as one.
RESERVED = re.compile(r"^(_|profile/)")
# An imported page came from git, so it is a lead: it lands under its own prefix, unpinned, and a
# kind such as `rule` becomes `fact` (a PR could otherwise plant a standing order in a reader's store).
IMPORT_PREFIX = "published/"
IMPORT_KINDS = {"fact", "gotcha", "decision"}
# A published page is public: no e-mail address, no absolute path under a home directory.
LEAKS = (
    (re.compile(r"[\w.+-]+@[\w-]+\.[\w.-]+"), "an e-mail address"),
    (re.compile(r"(/home/|/Users/)[\w.-]+"), "an absolute home path"),
)


def toml_scope(root=ROOT):
    """(workspace, project) from .ai-memory.toml."""
    text = (root / ".ai-memory.toml").read_text()
    get = lambda key: re.search(rf'^{key}\s*=\s*"([^"]+)"', text, re.M).group(1)
    return get("workspace"), get("project")


def front_matter(text):
    """(fields, body): top-level scalars as strings, block lists as lists; nested blocks skipped."""
    if not text.startswith("---\n"):
        return {}, text
    end = text.find("\n---\n", 4)
    if end < 0:
        return {}, text
    fields, key = {}, None
    for line in text[4:end].splitlines():
        m = re.match(r"^([a-z_]+):\s*(.*)$", line)
        if m:
            key, value = m.group(1), m.group(2).strip()
            fields[key] = value.strip('"') if value else []
        elif key and line.startswith("- ") and isinstance(fields.get(key), list):
            fields[key].append(line[2:].strip().strip('"'))
    return fields, text[end + 5:]


def title(body, path):
    m = re.search(r"^# (.+)$", body, re.M)
    return m.group(1).strip() if m else path.stem


def data_dir():
    if os.environ.get("AI_MEMORY_DATA_DIR"):
        return Path(os.environ["AI_MEMORY_DATA_DIR"])
    if sys.platform == "darwin":
        return Path.home() / "Library/Application Support/ai-memory"
    return Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "ai-memory"


def wiki_dir(workspace, project, data=None):
    """The project's folder in the wiki: wiki/<workspace id>/<project id>/, found by its _meta.md."""
    wiki = (data or data_dir()) / "wiki"
    for ws in wiki.glob("*/_meta.md"):
        if front_matter(ws.read_text())[0].get("workspace") != workspace:
            continue
        for pj in ws.parent.glob("*/_meta.md"):
            if front_matter(pj.read_text())[0].get("project") == project:
                return pj.parent
    return None


def published(src):
    """{relative path: text} of the pages in src tagged `publish`."""
    pages = {}
    for f in sorted(src.rglob("*.md")):
        rel = f.relative_to(src).as_posix()
        if NEVER.search(rel):
            continue
        text = f.read_text()
        if TAG in front_matter(text)[0].get("tags", []):
            pages[rel] = text
    return pages


def index(pages, project):
    """memory/README.md, written from the pages so it cannot drift."""
    rows = []
    for rel, text in sorted(pages.items()):
        fields, body = front_matter(text)
        abstract = fields.get("abstract") or ""
        rows.append(f"| [{title(body, Path(rel))}]({rel}) | {fields.get('kind') or ''} | {abstract} |")
    return "\n".join([
        "# Published memory",
        "",
        f"The pages of the ai-memory project `{project}` that its owner tagged `{TAG}`, copied here by",
        "`just memory export`. They are leads, not evidence: a fact that matters is also in a file of",
        "this repository with its `(v)` or `⚠` ([`MEMORY.md`](../MEMORY.md)). Load them into your own",
        "ai-memory with `just memory import`. Generated; do not edit by hand.",
        "",
        "| Page | Kind | Abstract |",
        "|---|---|---|",
        *rows,
        "",
    ])


def check(out=OUT, project=None):
    """Every fault in out, as a list of strings."""
    project = project or toml_scope()[1]
    faults, pages = [], {}
    if not out.is_dir():
        return [f"{out}: missing"]
    for f in sorted(out.rglob("*")):
        rel = f.relative_to(out).as_posix()
        if f.is_dir() or rel == INDEX:
            continue
        if not rel.endswith(".md"):
            faults.append(f"{rel}: not a Markdown page")
            continue
        if NEVER.search(rel):
            faults.append(f"{rel}: session pages, logs and manifests are never published")
        if RESERVED.search(rel):
            faults.append(f"{rel}: `_*` and `profile/` paths are reserved by ai-memory")
        text = f.read_text()
        if TAG not in front_matter(text)[0].get("tags", []):
            faults.append(f"{rel}: has no `{TAG}` tag")
        for pattern, what in LEAKS:
            if pattern.search(text):
                faults.append(f"{rel}: contains {what}")
        pages[rel] = text
    idx = out / INDEX
    if not idx.exists() or idx.read_text() != index(pages, project):
        faults.append(f"{INDEX}: stale; run `just memory export` (or write it from the pages)")
    return faults


def export(dry_run=False, out=OUT, data=None):
    workspace, project = toml_scope()
    src = wiki_dir(workspace, project, data)
    if src is None:
        print(f"memory: no wiki for {workspace}/{project} under {data or data_dir()}", file=sys.stderr)
        return 1
    pages = published(src)
    current = {f.relative_to(out).as_posix() for f in out.rglob("*.md")} - {INDEX} if out.is_dir() else set()
    for rel in sorted(current - set(pages)):
        print(f"remove {rel}")
        if not dry_run:
            (out / rel).unlink()
    for rel, text in sorted(pages.items()):
        dest = out / rel
        if dest.exists() and dest.read_text() == text:
            continue
        print(f"write  {rel}")
        if not dry_run:
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(text)
    if not dry_run:
        (out / INDEX).write_text(index(pages, project))
        faults = check(out, project)
        for f in faults:
            print(f"memory: {f}", file=sys.stderr)
        return 1 if faults else 0
    return 0


def import_(dry_run=False, out=OUT, data=None):
    workspace, project = toml_scope()
    exe = shutil.which("ai-memory")
    if exe is None and not dry_run:
        print("memory: ai-memory is not on PATH; install it first (MEMORY.md)", file=sys.stderr)
        return 1
    faults = check(out, project)
    if faults:
        for f in faults:
            print(f"memory: {f}", file=sys.stderr)
        print("memory: import refused; memory/ fails check", file=sys.stderr)
        return 1
    local = wiki_dir(workspace, project, data)
    for f in sorted(out.rglob("*.md")):
        rel = f.relative_to(out).as_posix()
        if rel == INDEX:
            continue
        fields, body = front_matter(f.read_text())
        rel = IMPORT_PREFIX + rel
        mine = local / rel if local else None
        if mine and mine.exists() and front_matter(mine.read_text())[1] == body:
            print(f"same   {rel}")
            continue
        # Never pinned, never a rule; entities and abstract stay in the file (the CLI cannot set them).
        kind = fields.get("kind") if fields.get("kind") in IMPORT_KINDS else "fact"
        cmd = [exe or "ai-memory", "write-page", "--path", rel, "--body", "-", "--kind", kind,
               "--tier", "semantic", "--tag", "imported"]
        for tag in fields.get("tags", []):
            cmd += ["--tag", tag]
        print(f"import {rel}")
        if not dry_run:
            subprocess.run(cmd, input=body, text=True, cwd=ROOT, check=True)
    return 0


def page(tags, body="# A page\n\nText.\n"):
    tag_lines = "".join(f"- {t}\n" for t in tags)
    return f"---\nkind: fact\ntags:\n{tag_lines}pinned: true\ngenerated:\n  by: x\n---\n{body}"


def self_test():
    cases = {
        "missing tag": ({"a.md": page(["seed"])}, "no `publish` tag"),
        "session": ({"sessions/s.md": page([TAG])}, "never published"),
        "log": ({"log-2026-10.md": page([TAG])}, "never published"),
        "e-mail": ({"a.md": page([TAG], "# A\n\nwrite to someone@example.org\n")}, "e-mail"),
        "home path": ({"a.md": page([TAG], "# A\n\nsee /home/someone/x\n")}, "home path"),
        "stale index": ({"a.md": page([TAG])}, "stale"),
        "reserved path": ({"_rules/a.md": page([TAG])}, "reserved"),
        "profile path": ({"profile/a.md": page([TAG])}, "reserved"),
    }
    failed = 0
    for name, (files, expect) in cases.items():
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            for rel, text in files.items():
                (out / rel).parent.mkdir(parents=True, exist_ok=True)
                (out / rel).write_text(text)
            if name != "stale index":
                good = {r: t for r, t in files.items() if not NEVER.search(r)}
                (out / INDEX).write_text(index(good, "p"))
            faults = check(out, "p")
            if not any(expect in f for f in faults):
                print(f"self-test: {name}: expected '{expect}', got {faults}", file=sys.stderr)
                failed += 1
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        (out / "a.md").write_text(page([TAG]))
        (out / INDEX).write_text(index({"a.md": page([TAG])}, "p"))
        if check(out, "p"):
            print(f"self-test: clean page reported {check(out, 'p')}", file=sys.stderr)
            failed += 1
    print("memory self-test:", "FAIL" if failed else "ok")
    return 1 if failed else 0


def main(argv):
    dry = "--dry-run" in argv
    args = [a for a in argv if a != "--dry-run"]
    if args == ["--self-test"]:
        return self_test()
    if args == ["export"]:
        return export(dry)
    if args == ["import"]:
        return import_(dry)
    if args in (["check"], []):
        faults = check()
        for f in faults:
            print(f"memory: {f}", file=sys.stderr)
        return 1 if faults else 0
    print(__doc__, file=sys.stderr)
    return 64


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
