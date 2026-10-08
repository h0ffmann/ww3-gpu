#!/usr/bin/env python3
"""figures — the harness around every Mermaid diagram in the repository's Markdown.

    python3 scripts/figures.py check            # CI: fences, cards, renders and gallery agree
    python3 scripts/figures.py render [--force] # re-render changed fences (needs mmdc; `just figures`)
    python3 scripts/figures.py chart TABLE.md --label COL --value COL [--value COL] --id ID --title T
                                                # a measured table -> a bar-chart fence and card stub

A diagram stays a ```mermaid fence in the Markdown it illustrates, so GitHub keeps drawing it in
place. The harness adds three things:

1. Two header comments as the fence's first lines, which Mermaid ignores:
       %% figure: <id>       unique, [a-z0-9-]+; names the rendered files
       %% title: <text>      the question the figure answers; the PDF caption
2. A reading card right after the fence: a <details open> block (open, so the explanation shows
   under the diagram without a click) whose summary is "How to read this figure" ("Como ler esta
   figura" in pt-BR files) and which carries the four bold labels in CARD_LABELS, for readers new
   to the topic.
3. Renders in pubs/figures/mermaid/<id>.pdf (the LaTeX books) and <id>.png (Word, the gallery),
   made by a pinned mermaid-cli with a pinned font. index.json keeps each fence's sha256, so
   `check` can tell a stale render without Chromium. pubs/filters/mermaid.lua swaps each fence
   for its render when pandoc builds a PDF or a .docx.

`render` also rewrites pubs/figures/README.md, the gallery: every figure with its takeaway, a
thumbnail, where it lives, and a link to report a mistake in it.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "pubs" / "figures"
RENDERS = OUT / "mermaid"
INDEX = RENDERS / "index.json"
GALLERY = OUT / "README.md"
CONFIG = OUT / "mermaid-config.json"
REPO_URL = "https://github.com/h0ffmann/ww3-gpu"

ID = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
HEADER = re.compile(r"^%%\s*(figure|title):\s*(.+?)\s*$")
CARD_LABELS = {
    "en": ("How to read this figure", ["Takeaway", "How to read", "Not shown", "Evidence"]),
    "pt": ("Como ler esta figura", ["Em uma frase", "Como ler", "Fora da figura", "Evidência"]),
}


def lang_of(rel: str) -> str:
    return "pt" if ".pt." in rel or "/pt/" in rel else "en"


def markdown_files() -> list[pathlib.Path]:
    """Tracked Markdown, so submodules (WW3, kokkos) and build output are never scanned."""
    out = subprocess.run(["git", "-C", str(ROOT), "ls-files", "-z", "--", "*.md"],
                         capture_output=True, text=True, check=True).stdout
    return sorted(ROOT / p for p in out.split("\0") if p and (ROOT / p).is_file())


def parse(path: pathlib.Path, errors: list[str], root: pathlib.Path = ROOT) -> list[dict]:
    """Every ```mermaid fence in one file, with its header and card; problems go to errors."""
    rel = path.relative_to(root).as_posix()
    lines = path.read_text(encoding="utf-8").splitlines()
    figs, i = [], 0
    while i < len(lines):
        if lines[i].strip() != "```mermaid":
            i += 1
            continue
        start, j = i + 1, i + 1
        while j < len(lines) and lines[j].strip() != "```":
            j += 1
        if j == len(lines):
            errors.append(f"{rel}:{i + 1}: mermaid fence never closed")
            break
        body = lines[start:j]
        where = f"{rel}:{i + 1}"
        head = {}
        for line in body:
            if not line.startswith("%%"):
                break
            m = HEADER.match(line)
            if m:
                head[m.group(1)] = m.group(2)
        fig = {"file": rel, "line": i + 1, "id": head.get("figure", ""), "title": head.get("title", ""),
               "source": "\n".join(body) + "\n", "takeaway": ""}
        if not ID.match(fig["id"]):
            errors.append(f"{where}: first lines need '%% figure: <id>' with id in [a-z0-9-]")
        if not fig["title"]:
            errors.append(f"{where}: first lines need '%% title: <the question the figure answers>'")
        if any(l.strip() == "gantt" for l in body) and not any(l.strip() == "todayMarker off" for l in body):
            errors.append(f"{where}: a gantt needs 'todayMarker off', or the render depends on the day it ran")
        fig["takeaway"] = card(lines, j + 1, lang_of(rel), where, errors)
        figs.append(fig)
        i = j + 1
    return figs


def card(lines: list[str], k: int, lang: str, where: str, errors: list[str]) -> str:
    """Check the <details> card that must follow the fence at line k; return its takeaway."""
    summary, labels = CARD_LABELS[lang]
    while k < len(lines) and not lines[k].strip():
        k += 1
    if k >= len(lines) or lines[k].strip() != "<details open>":
        errors.append(f"{where}: no reading card; put a <details open> block right after the fence")
        return ""
    end = k
    while end < len(lines) and lines[end].strip() != "</details>":
        end += 1
    block = lines[k:end]
    if end == len(lines):
        errors.append(f"{where}: reading card never closed with </details>")
    if len(block) < 2 or block[1].strip() != f"<summary>{summary}</summary>":
        errors.append(f"{where}: card's second line must be <summary>{summary}</summary>")
    text = "\n".join(block)
    takeaway = ""
    for label in labels:
        m = re.search(rf"^\*\*{re.escape(label)}\.\*\*[ \t]*(\S.*)$", text, re.M)
        if not m:
            errors.append(f"{where}: card lacks a non-empty '**{label}.**' paragraph")
        elif not takeaway:
            takeaway = m.group(1).strip()
    return takeaway


def collect() -> tuple[list[dict], list[str]]:
    errors, figs = [], []
    for path in markdown_files():
        figs += parse(path, errors)
    seen = {}
    for f in figs:
        if f["id"] and f["id"] in seen:
            errors.append(f"{f['file']}:{f['line']}: figure id '{f['id']}' already used at {seen[f['id']]}")
        seen.setdefault(f["id"], f"{f['file']}:{f['line']}")
    return figs, errors


def digest(fig: dict) -> str:
    return hashlib.sha256(fig["source"].encode("utf-8")).hexdigest()


def gallery(figs: list[dict]) -> str:
    out = [
        "# Figures",
        "",
        "<!-- generated by `python3 scripts/figures.py render`; edit the diagrams where they live -->",
        "",
        "Every diagram in this repository, generated from the Mermaid source in the page it",
        "illustrates. Each page carries a *How to read this figure* card under the diagram; the PDF",
        "and Word builds use the renders in [`mermaid/`](mermaid/), and",
        "[`mermaid/index.json`](mermaid/index.json) records the source hash and the pinned renderer",
        "of each one. To add or change a figure, follow `.claude/skills/figure/SKILL.md`; `just figures`",
        "re-renders and CI fails when a render no longer matches its source.",
        "",
        "Found something wrong in a figure? Each entry has a link that opens an issue for it.",
    ]
    by_file: dict[str, list[dict]] = {}
    for f in figs:
        by_file.setdefault(f["file"], []).append(f)
    for rel, group in by_file.items():
        out += ["", f"## [`{rel}`](../../{rel})", ""]
        for f in group:
            issue = f"{REPO_URL}/issues/new?title=Figure%20{f['id']}%3A%20"
            out += [
                f"### {f['title']}",
                "",
                f"{f['takeaway']}",
                "",
                f'<img src="mermaid/{f["id"]}.png" alt="{f["takeaway"].replace(chr(34), "&quot;")}" width="560" />',
                "",
                f"`{f['id']}` · [source](../../{rel}) · [PDF](mermaid/{f['id']}.pdf) · [report a mistake]({issue})",
                "",
            ]
    return "\n".join(out).rstrip() + "\n"


def check() -> int:
    figs, errors = collect()
    index = json.loads(INDEX.read_text(encoding="utf-8")) if INDEX.exists() else {"figures": {}}
    known = index.get("figures", {})
    for f in figs:
        check_data(f, errors)
        if not f["id"]:
            continue
        rec = known.get(f["id"])
        where = f"{f['file']}:{f['line']}"
        if not rec or rec.get("sha256") != digest(f):
            errors.append(f"{where}: figure '{f['id']}' has no current render; run `just figures`")
        for ext in ("pdf", "png"):
            if not (RENDERS / f"{f['id']}.{ext}").exists():
                errors.append(f"{where}: missing pubs/figures/mermaid/{f['id']}.{ext}; run `just figures`")
    for stale in sorted(set(known) - {f["id"] for f in figs}):
        errors.append(f"pubs/figures/mermaid/index.json: '{stale}' has no fence any more; run `just figures`")
    if not errors and (not GALLERY.exists() or GALLERY.read_text(encoding="utf-8") != gallery(figs)):
        errors.append("pubs/figures/README.md is out of date; run `just figures`")
    for e in errors:
        print(f"figures: {e}", file=sys.stderr)
    if not errors:
        print(f"figures: {len(figs)} figures, all with cards and current renders")
    return 1 if errors else 0


def nixpkgs_rev() -> str:
    lock = json.loads((ROOT / "flake.lock").read_text(encoding="utf-8"))
    revs = {n["locked"].get("rev") for n in lock["nodes"].values()
            if n.get("locked", {}).get("repo") == "nixpkgs"}
    return ", ".join(sorted(r for r in revs if r))


def pin_pdf_dates(pdf: pathlib.Path) -> None:
    """Chromium stamps the wall clock into the PDF's info dictionary; replace it with a fixed date
    of the same length, so xref offsets hold and an unchanged figure re-renders to the same bytes."""
    data = re.sub(rb"\(D:\d{14}[+-]\d\d'\d\d'\)", b"(D:20000101000000+00'00')", pdf.read_bytes())
    pdf.write_bytes(data)


def render(force: bool) -> int:
    figs, errors = collect()
    if errors:
        for e in errors:
            print(f"figures: {e}", file=sys.stderr)
        return 1
    mmdc = shutil.which("mmdc")
    if not mmdc:
        print("figures: mmdc not on PATH; run `just figures`, which pins it with Nix", file=sys.stderr)
        return 1
    font_dir = os.environ.get("FIGURES_FONT_DIR", "")
    index = json.loads(INDEX.read_text(encoding="utf-8")) if INDEX.exists() else {"figures": {}}
    old = index.get("figures", {})
    RENDERS.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        t = pathlib.Path(tmp)
        env = dict(os.environ)
        if font_dir:  # only this font is visible to Chromium, so label widths match on every host
            (t / "fonts.conf").write_text(
                '<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                f"<dir>{font_dir}</dir><cachedir>{t / 'fc'}</cachedir></fontconfig>\n")
            env["FONTCONFIG_FILE"] = str(t / "fonts.conf")
        (t / "puppeteer.json").write_text('{"args": ["--no-sandbox"]}\n')
        base = [mmdc, "-q", "-p", str(t / "puppeteer.json"), "-c", str(CONFIG)]
        version = subprocess.run([mmdc, "--version"], capture_output=True, text=True, env=env).stdout.strip()
        new = {}
        for f in figs:
            sha = digest(f)
            new[f["id"]] = {"file": f["file"], "title": f["title"], "sha256": sha}
            have = all((RENDERS / f"{f['id']}.{e}").exists() for e in ("pdf", "png"))
            if not force and have and old.get(f["id"], {}).get("sha256") == sha:
                continue
            src = t / f"{f['id']}.mmd"
            src.write_text(f["source"], encoding="utf-8")
            print(f"figures: rendering {f['id']} ({f['file']}:{f['line']})")
            for args in (["-o", str(RENDERS / f"{f['id']}.pdf"), "--pdfFit"],
                         ["-o", str(RENDERS / f"{f['id']}.png"), "-s", "2", "-b", "white"]):
                subprocess.run(base + ["-i", str(src)] + args, check=True, env=env)
            pin_pdf_dates(RENDERS / f"{f['id']}.pdf")
    for gone in set(old) - set(new):
        for e in ("pdf", "png"):
            (RENDERS / f"{gone}.{e}").unlink(missing_ok=True)
    index = {"renderer": {"mermaid-cli": version, "nixpkgs": nixpkgs_rev(),
                          "font": pathlib.Path(font_dir).parent.parent.name.split("-", 1)[-1] if font_dir else "host default",
                          "config": CONFIG.relative_to(ROOT).as_posix()},
             "figures": dict(sorted(new.items()))}
    INDEX.write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    GALLERY.write_text(gallery(figs), encoding="utf-8")
    return check()


NUMBER = re.compile(r"[-+]?\d+(?:[.,]\d+)?")


def table_rows(text: str) -> list[dict]:
    """The first Markdown table in text, as dicts keyed by header (markup stripped)."""
    clean = lambda c: re.sub(r"[`*]", "", c).strip()
    rows, header = [], None
    for line in text.splitlines():
        if not line.lstrip().startswith("|"):
            if header:
                break
            continue
        cells = [clean(c) for c in line.strip().strip("|").split("|")]
        if header is None:
            header = cells
        elif not all(re.fullmatch(r":?-+:?", c) for c in cells if c):
            rows.append(dict(zip(header, cells)))
    return rows


def number(cell: str) -> float | None:
    m = NUMBER.search(cell)
    return float(m.group(0).replace(",", ".")) if m else None


def chart_body(table: pathlib.Path, label: str, values: list[str], y_title: str) -> list[str]:
    """The xychart lines for one measured table: first value column as bars, the rest as lines.
    Rows without a number in every value column, and a row labelled `total`, are skipped."""
    rows = table_rows((ROOT / table).read_text(encoding="utf-8"))
    missing = [c for c in [label] + values if rows and c not in rows[0]]
    if not rows or missing:
        raise ValueError(f"{table}: no table, or no column {missing}; columns: {list(rows[0]) if rows else []}")
    keep = [r for r in rows if r[label].strip().lower() != "total"
            and all(number(r[v]) is not None for v in values)]
    q = lambda t: '"' + t.replace('"', "'") + '"'
    top = max(number(r[v]) for r in keep for v in values)
    # dark bars and a contrasting line: the neutral theme's default greys barely print
    out = ['%%{init: {"themeVariables": {"xyChart": {"plotColorPalette": "#4c6a8c, #c0392b"}}}}%%',
           "xychart-beta", f"    x-axis [{', '.join(q(r[label]) for r in keep)}]",
           f"    y-axis {q(y_title)} 0 --> {top * 1.1:.3g}"]
    for i, v in enumerate(values):
        out.append(f"    {'bar' if i == 0 else 'line'} [{', '.join(f'{number(r[v]):g}' for r in keep)}]")
    return out


def data_args(argv: list[str]) -> tuple:
    ap = argparse.ArgumentParser(prog="%% data:", add_help=False)
    ap.add_argument("table", type=pathlib.Path)
    ap.add_argument("--label", required=True)
    ap.add_argument("--value", action="append", required=True)
    ap.add_argument("--y-title", default="")
    a = ap.parse_args(argv)
    return a.table, a.label, a.value, a.y_title


def check_data(fig: dict, errors: list[str]) -> None:
    """A fence with a `%% data:` line must still be exactly what its table produces."""
    line = next((l for l in fig["source"].splitlines() if l.startswith("%% data:")), None)
    if not line:
        return
    where = f"{fig['file']}:{fig['line']}"
    try:
        want = chart_body(*data_args(shlex.split(line[len("%% data:"):])))
    except (ValueError, SystemExit, OSError) as e:
        errors.append(f"{where}: bad '%% data:' line: {e}")
        return
    body = [l for l in fig["source"].splitlines()
            if not l.startswith(("%% figure:", "%% title:", "%% data:"))]
    if body != want:
        errors.append(f"{where}: chart no longer matches its table; regenerate it with `figures.py chart`")


def chart(table: pathlib.Path, label: str, values: list[str], fid: str, title: str,
          y_title: str, lang: str) -> int:
    """Print a bar-chart fence and a card stub for one measured table, so a profile or benchmark
    table becomes a figure without retyping its numbers; `check` keeps the two in step."""
    try:
        body = chart_body(table, label, values, y_title)
    except ValueError as e:
        print(f"figures: {e}", file=sys.stderr)
        return 1
    argv = [table.as_posix(), "--label", label] + sum((["--value", v] for v in values), [])
    argv += ["--y-title", y_title] if y_title else []
    out = ["```mermaid", f"%% figure: {fid}", f"%% title: {title}", f"%% data: {shlex.join(argv)}"]
    summary, labels = CARD_LABELS[lang]
    out += body + ["```", "", "<details open>", f"<summary>{summary}</summary>", ""]
    for name in labels:
        out += [f"**{name}.** TODO", ""]
    out.append("</details>")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("command", nargs="?", default="check", choices=["check", "render", "chart"])
    ap.add_argument("table", nargs="?", type=pathlib.Path, help="chart: Markdown file holding the table")
    ap.add_argument("--force", action="store_true", help="render: re-render every figure")
    ap.add_argument("--label", help="chart: the column naming each bar")
    ap.add_argument("--value", action="append", default=[], help="chart: numeric column; repeat for lines")
    ap.add_argument("--id", help="chart: figure id")
    ap.add_argument("--title", help="chart: the question the figure answers")
    ap.add_argument("--y-title", default="", help="chart: y-axis title, with its unit")
    ap.add_argument("--lang", choices=["en", "pt"], default="en", help="chart: card language")
    a = ap.parse_args()
    if a.command == "chart":
        if not (a.table and a.label and a.value and a.id and a.title):
            ap.error("chart needs TABLE, --label, --value, --id and --title")
        sys.exit(chart(a.table, a.label, a.value, a.id, a.title, a.y_title, a.lang))
    sys.exit(check() if a.command == "check" else render(a.force))
