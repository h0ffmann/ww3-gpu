#!/usr/bin/env python3
"""leanpub_manuscript — export course/*.md as a Leanpub manuscript (Markua) for the free edition.

    python3 scripts/leanpub_manuscript.py [--out build/leanpub]   # writes <out>/manuscript/
    python3 scripts/leanpub_manuscript.py --self-test

Leanpub reads `manuscript/Book.txt` (the files of the book, in order), `manuscript/Sample.txt`
(the free sample) and `manuscript/images/` (v, https://leanpub.com/read/lfm/leanpub-auto-booktxt-sampletxt-and-manuscript-files
and .../leanpub-auto-images, 2026-10-08). Parts and the sample come from pubs/book/parts.json,
the same file the PDF build will use (WFIP-0002 §5.2). What is translated, and nothing else:

- the lesson's first `# Title` gets a stable id, `{#ch-<slug>}`, so `](NN-slug.md)` links become
  `](#ch-<slug>)` and `](NN-slug.md#a)` becomes `](#a)` (as scripts/book_prep.py does for the PDF);
- a link to another file of the repository becomes its GitHub URL at `main`;
- a Mermaid fence with a `%% figure: <id>` header becomes its committed render,
  `pubs/figures/mermaid/<id>.png`, copied to `images/`, and the reading card that follows it
  (`<details>` … `</details>`) becomes a Markua aside;
- `$$ … $$` becomes a Markua `$` code fence and `$x$` in prose becomes `` `x`$ ``
  (v, https://leanpub.com/read/markua/resources, 2026-10-08); `$` inside code is left alone.

A preface names the commit the export is from. Standard library only; no network.
"""
import argparse
import datetime as dt
import json
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
COURSE = ROOT / "course"
RENDERS = ROOT / "pubs" / "figures" / "mermaid"
PARTS = ROOT / "pubs" / "book" / "parts.json"
REPO_URL = "https://github.com/h0ffmann/ww3-gpu"

LESSON_LINK = re.compile(r"\]\((?:\./)?(\d{2}-[a-z0-9-]+)\.md(#[A-Za-z0-9_-]+)?\)")
REPO_LINK = re.compile(r"\]\(((?:\.\./|(?!https?://|#|mailto:)[A-Za-z0-9_./-]+)(?:[A-Za-z0-9_./-]*))(#[A-Za-z0-9_-]+)?\)")
FIGURE = re.compile(r"^```mermaid\n%% figure: ([a-z0-9-]+)\n(?:%% title: ([^\n]*)\n)?.*?^```\n", re.M | re.S)
CARD = re.compile(r"\n<details(?: open)?>\n<summary>([^<]*)</summary>\n(.*?)\n</details>\n", re.S)
DISPLAY_MATH = re.compile(r"^\$\$(.+?)\$\$[ \t]*$", re.M | re.S)
INLINE_MATH = re.compile(r"(?<![\w$`\\])\$(?!\s)((?:[^$`\n\\]|\\.)+?)(?<!\s)\$(?![\w$])")
CODE_SPAN = re.compile(r"`[^`\n]*`")


def slug(stem: str) -> str:
    return stem[3:]


def lessons():
    return {p.stem[:2]: p for p in sorted(COURSE.glob("[0-9][0-9]-*.md"))}


def repo_url(path: pathlib.Path) -> str:
    rel = path.resolve().relative_to(ROOT).as_posix()
    return f"{REPO_URL}/{'tree' if path.is_dir() else 'blob'}/main/{rel}"


def convert_math(text: str) -> str:
    """Display math first; inline math only outside code spans and fences."""
    text = DISPLAY_MATH.sub(lambda m: "```$\n" + m.group(1).strip() + "\n```", text)
    out, in_fence = [], False
    for line in text.split("\n"):
        if line.startswith("```"):
            in_fence = not in_fence
        if not in_fence and not line.startswith(("    ", "\t")):
            spans = {}
            def hide(m):  # keep `$WW3`-style code spans out of the math regex
                key = f"\x00{len(spans)}\x00"
                spans[key] = m.group(0)
                return key
            masked = CODE_SPAN.sub(hide, line)
            masked = INLINE_MATH.sub(lambda m: "`" + m.group(1) + "`$", masked)
            for key, span in spans.items():
                masked = masked.replace(key, span)
            line = masked
        out.append(line)
    return "\n".join(out)


def convert(path: pathlib.Path, known: set, images: pathlib.Path, missing: list) -> str:
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"^# (.+?)\s*$", rf"# \1 {{#ch-{slug(path.stem)}}}", text, count=1, flags=re.M)

    def lesson_link(m):
        target, anchor = m.group(1), m.group(2)
        if target not in known:
            missing.append(f"{path.name}: link to {target}.md, which is not a lesson")
            return m.group(0)
        return f"]({anchor})" if anchor else f"](#ch-{slug(target)})"
    text = LESSON_LINK.sub(lesson_link, text)

    def repo_link(m):
        target, anchor = m.group(1), m.group(2) or ""
        p = (path.parent / target)
        if not p.exists():
            missing.append(f"{path.name}: link to {target}, which does not exist")
            return m.group(0)
        return f"]({repo_url(p)}{anchor})"
    text = REPO_LINK.sub(repo_link, text)

    def figure(m):
        fig, title = m.group(1), (m.group(2) or fig).strip()
        png = RENDERS / f"{fig}.png"
        if not png.is_file():
            missing.append(f"{path.name}: figure {fig} has no render in {RENDERS.relative_to(ROOT)}")
            return m.group(0)
        shutil.copy2(png, images / png.name)
        return f"![{title}](images/{png.name})\n"
    text = FIGURE.sub(figure, text)
    text = CARD.sub(lambda m: f"\n{{aside}}\n**{m.group(1).strip()}**\n\n{m.group(2).strip()}\n{{/aside}}\n", text)
    return convert_math(text)


def preface(commit: str, ww3: str, parts: dict) -> str:
    return f"""# About this book {{#ch-about}}

*{parts['title']}* is compiled from the repository [h0ffmann/ww3-gpu]({REPO_URL}), which is
its source and its evidence. This edition was exported from commit `{commit}` on
{dt.date.today().isoformat()}; the WAVEWATCH III® source it quotes is the fork pinned at `{ww3}`.
The chapters are the repository's lessons; the code, measurements, plans and decisions they
describe are versioned next to them, and the newest edition is always the repository itself.

Two marks run through the text. `(v)` after a claim means it was checked against the named source
when it was written; `⚠` means it was not, and the claim is a thing to verify before relying on it.
Every number comes with the command that reproduces it.

The book is free to read. If you use it, please cite the repository's concept DOI,
[10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351), which resolves to the latest
release. Corrections are welcome as issues or pull requests; the most useful one is confirming or
correcting anything marked `⚠`.

WAVEWATCH III® is a registered trademark of NOAA's National Weather Service, named here only to
refer to that software; this is an independent project, not endorsed by NOAA.
"""


def export(out: pathlib.Path, parts: dict, commit: str, ww3: str) -> list:
    manuscript = out / "manuscript"
    if manuscript.exists():
        shutil.rmtree(manuscript)
    images = manuscript / "images"
    images.mkdir(parents=True)
    by_num = lessons()
    known = {p.stem for p in by_num.values()}
    missing, book = [], ["about.txt"]
    (manuscript / "about.txt").write_text(preface(commit, ww3, parts), encoding="utf-8")
    for i, part in enumerate(parts["parts"], 1):
        name = f"part-{i}.txt"
        (manuscript / name).write_text(f"{{class: part}}\n# {part['title']}\n", encoding="utf-8")
        book.append(name)
        for num in part["lessons"]:
            path = by_num[num]
            name = f"{path.stem}.txt"
            (manuscript / name).write_text(convert(path, known, images, missing), encoding="utf-8")
            book.append(name)
    (manuscript / "Book.txt").write_text("\n".join(book) + "\n", encoding="utf-8")
    sample = ["about.txt"] + [f"{by_num[n].stem}.txt" for n in parts.get("sample", [])]
    (manuscript / "Sample.txt").write_text("\n".join(sample) + "\n", encoding="utf-8")
    return missing


def git(*args):
    try:
        return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def self_test():
    import tempfile
    assert convert_math("where $N = F/\\sigma$ is action") == "where `N = F/\\sigma`$ is action"
    assert convert_math("costs $5 and $10 each") == "costs $5 and $10 each", convert_math("costs $5 and $10 each")
    assert convert_math("the shell reads `$WW3/model`") == "the shell reads `$WW3/model`"
    assert convert_math("```sh\necho $HOME $USER\n```") == "```sh\necho $HOME $USER\n```"
    assert convert_math("$$a\n= b$$\n") == "```$\na\n= b\n```\n"
    out = pathlib.Path(tempfile.mkdtemp())
    parts = json.loads(PARTS.read_text(encoding="utf-8"))
    missing = export(out, parts, "deadbeef", "0000000")
    assert not missing, missing
    book = (out / "manuscript" / "Book.txt").read_text().split()
    assert book[0] == "about.txt" and book[1] == "part-1.txt" and "13-bulk-porting-with-agents.txt" in book, book
    l13 = (out / "manuscript" / "13-bulk-porting-with-agents.txt").read_text()
    assert "```mermaid" not in l13 and l13.count("![") == 4 and l13.count("{aside}") == 4 and "<details" not in l13, (l13.count("!["), l13.count("{aside}"))
    assert (out / "manuscript" / "images" / "porting-validation-ladder.png").is_file()
    l00 = (out / "manuscript" / "00-orientation.txt").read_text()
    assert l00.startswith("# 00. Orientation: what WW3 actually computes {#ch-orientation}"), l00[:80]
    assert "](#ch-" in l00 and "](0" not in l00, "lesson links rewritten"
    l01 = (out / "manuscript" / "01-build.txt").read_text()
    assert f"{REPO_URL}/blob/main/" in l01 or f"{REPO_URL}/tree/main/" in l01, "repo links become GitHub URLs"
    assert "](../" not in l01
    print("leanpub_manuscript: self-test ok")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=str(ROOT / "build" / "leanpub"))
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        self_test()
        return 0
    parts = json.loads(PARTS.read_text(encoding="utf-8"))
    commit, ww3 = git("rev-parse", "--short", "HEAD"), git("rev-parse", "--short", "HEAD:WW3")
    missing = export(pathlib.Path(a.out), parts, commit, ww3)
    for m in missing:
        print(f"leanpub_manuscript: {m}", file=sys.stderr)
    n = len((pathlib.Path(a.out) / "manuscript" / "Book.txt").read_text().split())
    print(f"leanpub_manuscript: {pathlib.Path(a.out) / 'manuscript'}: {n} files in Book.txt, {len(missing)} problem(s)")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
