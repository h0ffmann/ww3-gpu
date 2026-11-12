#!/usr/bin/env python3
"""archify_index — the figure index of the Pages site, grouped, from docs/archify/figures.tsv.

    archify_index.py <out/index.html>
    archify_index.py --check

scripts/archify.sh site calls it after drawing the pages. Each group of groups.tsv opens with its
paragraph, then lists its drawings: the question each answers, the page it illustrates, its pt-BR
twin, and the WW3 it is drawn from. --check prints what is wrong with the two tables (the drawing
of dark.tsv with no row, a row with no stamp, a page or anchor that does not exist, a group that is
not defined, a twin that does not point back) and exits 1; tests/test_archify_index.py runs it.
"""
import html
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ARCHIFY = ROOT / "docs" / "archify"
BLOB = "https://github.com/h0ffmann/ww3-gpu/blob/main/"


def table(name):
    lines = [line.rstrip("\n").split("\t") for line in open(ARCHIFY / name, encoding="utf-8")
             if line.strip() and not line.startswith("#")]
    head = lines[0]
    return [dict(zip(head, r)) for r in lines[1:]]


def dark():
    return [line.split("\t")[0] for line in open(ARCHIFY / "dark.tsv", encoding="utf-8")
            if line.strip() and not line.startswith("#")]


def anchors(path):
    """GitHub's heading anchors of a Markdown file: lower case, punctuation out, spaces to -."""
    out = set()
    for line in open(path, encoding="utf-8"):
        m = re.match(r"#{1,6}\s+(.*)", line)
        if m:
            text = re.sub(r"[^\w\- ]", "", m.group(1).strip().lower())
            out.add(text.replace(" ", "-"))
    return out


def problems():
    rows, groups = table("figures.tsv"), table("groups.tsv")
    names = {g["group"] for g in groups}
    by = {r["slug"]: r for r in rows}
    bad = []
    for s in dark():
        if s not in by:
            bad.append(f"{s}: in dark.tsv, no row in figures.tsv")
    for r in rows:
        s = r["slug"]
        if not (ARCHIFY / f"{s}.json").is_file():
            bad.append(f"{s}: no docs/archify/{s}.json")
        if not r.get("ww3", "").strip():
            bad.append(f"{s}: no ww3 stamp")
        if r["group"] not in names:
            bad.append(f"{s}: group {r['group']!r} is not in groups.tsv")
        if r["lang"] not in ("en", "pt-BR"):
            bad.append(f"{s}: lang {r['lang']!r}")
        if not r.get("question", "").strip():
            bad.append(f"{s}: no question")
        if not re.fullmatch(r"[A-Za-z0-9_./-]+(#[A-Za-z0-9_-]+)?", r["page"]):
            bad.append(f"{s}: page {r['page']!r} is not a plain path and anchor")  # archify.sh puts it in HTML
            continue
        path, _, anchor = r["page"].partition("#")
        if not (ROOT / path).is_file():
            bad.append(f"{s}: page {path} does not exist")
        elif anchor and anchor not in anchors(ROOT / path):
            bad.append(f"{s}: page {path} has no heading #{anchor}")
        twin = r.get("twin", "")
        if twin and by.get(twin, {}).get("twin") != s:
            bad.append(f"{s}: twin {twin} does not name it back")
    return bad


def title(slug):
    return json.load(open(ARCHIFY / f"{slug}.json", encoding="utf-8"))["meta"]["title"]


def item(r, by):
    s, e = r["slug"], html.escape
    twin = by.get(r["twin"]) if r["twin"] else None
    extra = f' · <a href="{e(twin["slug"])}/" hreflang="pt-BR">pt-BR</a>' if twin else ""
    lang = ' lang="pt-BR"' if r["lang"] == "pt-BR" else ""
    return (f'<li{lang}><a class="t" href="{e(s)}/">{e(r["question"])}</a>'
            f'<span class="m">{e(title(s))} · illustrates <a href="{BLOB}{e(r["page"])}">'
            f'{e(r["page"].split("#")[0])}</a>{extra} · <a href="{BLOB}docs/archify/{e(s)}.json">source</a></span>'
            f'<span class="r">{e(r["ww3"])}</span></li>')


CSS = """
body{margin:0;padding:48px 16px;background:radial-gradient(60rem 24rem at 100% -20%,color-mix(in srgb,#36B7FF 14%,transparent),transparent 70%),#05090E;color:#E7F2EF;font:400 18px/1.5 Newsreader,Georgia,serif}
main{max-width:760px;margin:0 auto}h1{font:800 40px/1.1 'Bricolage Grotesque',system-ui,sans-serif;color:#36B7FF;margin:0 0 8px}
h2{font:800 24px/1.2 'Bricolage Grotesque',system-ui,sans-serif;color:#E7F2EF;margin:40px 0 8px}
p{color:#8FA3AD}ul{list-style:none;padding:0;border-top:1px solid #1C2B38}li{padding:14px 0;border-bottom:1px solid #1C2B38}
a{color:#36B7FF}a.t{font:600 19px/1.3 'Bricolage Grotesque',system-ui,sans-serif;text-decoration:none;display:block}
.m,.r{display:block;font:400 13px/1.5 'JetBrains Mono',monospace;color:#8FA3AD}.r{color:#5E7480}
nav a{margin-right:12px}a:focus-visible{outline:2px solid #36B7FF;outline-offset:3px;border-radius:6px}
"""


def page():
    rows, groups = table("figures.tsv"), table("groups.tsv")
    by = {r["slug"]: r for r in rows}
    shown = {r["twin"] for r in rows if r["lang"] == "en" and r["twin"]}
    nav, body = [], []
    for g in groups:
        mine = [r for r in rows if r["group"] == g["group"] and r["slug"] not in shown]
        if not mine:
            continue
        gid = html.escape(g["group"])
        nav.append(f'<a href="#{gid}">{html.escape(g["title"])}</a>')
        body.append(f'<section id="{gid}"><h2>{html.escape(g["title"])}</h2><p>{html.escape(g["text"])}</p>'
                    f'<ul>{"".join(item(r, by) for r in mine)}</ul></section>')
    fonts = ("https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:wght@600;800"
             "&amp;family=JetBrains+Mono&amp;family=Newsreader&amp;display=swap")
    return (f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" '
            f'content="width=device-width, initial-scale=1"><title>ww3-gpu · figures</title>'
            f'<link rel="icon" href="../favicon.svg"><link rel="stylesheet" href="{fonts}"><style>{CSS}</style>'
            f'</head><body><main><h1>figures</h1><p>Every drawing of ww3-gpu, grouped by the question it '
            f'answers, each with the page it illustrates and the WAVEWATCH III it is drawn from. Drawn '
            f'with <a href="https://github.com/tt-a1i/archify">Archify</a> (MIT) from '
            f'<a href="{BLOB}docs/archify/figures.tsv">docs/archify/figures.tsv</a> and the sources '
            f'beside it. Each page is one drawing you can zoom. <a href="../">ww3-gpu</a></p>'
            f'<nav>{"".join(nav)}</nav>{"".join(body)}</main></body></html>\n')


def main(argv):
    if argv == ["--check"]:
        bad = problems()
        for b in bad:
            print(f"archify_index: {b}", file=sys.stderr)
        sys.exit(1 if bad else 0)
    if len(argv) != 1:
        sys.exit(__doc__)
    bad = problems()
    if bad:
        sys.exit("archify_index: " + "; ".join(bad))
    pathlib.Path(argv[0]).write_text(page(), encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
