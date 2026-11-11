#!/usr/bin/env python3
"""archify_dress — the dark look (DESIGN.md, "Pictures") over one page Archify rendered.

    archify_dress.py <page.html> <out.html> <slug> [--bar <bar.html>]

scripts/archify.sh png, pdf and site call it. It sets docs/archify/ww3-gpu.css and the site's fonts
over the page, cuts the viewBox to the slug's frame in docs/archify/dark.tsv (a height, or a height
with the x, y and width of the frame), and, with --bar, puts the site's slim bar on top (Pages). It
is the manner of piabrota/nix-os's `just figure`.
"""
import html
import pathlib
import re
import sys

ARCHIFY = pathlib.Path(__file__).resolve().parent.parent / "docs" / "archify"
FONTS = ("https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:wght@600;800"
         "&family=JetBrains+Mono&display=block")
BAR_CSS = """
.wg-bar{position:fixed;top:0;left:0;right:0;z-index:10;box-sizing:border-box;min-height:44px;
  display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:6px 16px;
  padding:8px 16px;background:#0A121B;border-bottom:1px solid #1C2B38;
  font:400 13px/1.3 'JetBrains Mono',ui-monospace,monospace;color:#8FA3AD}
.wg-bar,.wg-bar *{visibility:visible!important}
.wg-bar a{color:#36B7FF;text-decoration:none}
.wg-bar a:hover{text-decoration:underline;text-underline-offset:3px}
.wg-bar a:focus-visible{outline:2px solid #36B7FF;outline-offset:3px;border-radius:6px}
.wg-bar b{font:800 15px/1 'Bricolage Grotesque',system-ui,sans-serif;color:#E7F2EF;margin-right:10px}
.wg-bar .k{white-space:nowrap}
.wg-bar i{display:inline-block;width:10px;height:10px;border:2px solid;border-radius:3px;
  margin-right:6px;vertical-align:-1px}
.diagram-container svg{top:var(--wg-bar,44px)!important;height:calc(100vh - var(--wg-bar,44px))!important}
"""
# The bar wraps on a phone: the drawing starts under it, whatever its height.
BAR_JS = ("<script>(()=>{const b=document.querySelector('.wg-bar');const f=()=>document."
          "documentElement.style.setProperty('--wg-bar',b.offsetHeight+'px');f();"
          "addEventListener('resize',f)})()</script>")


def frames():
    """slug -> (height,) or (height, x, y, width), as dark.tsv gives them."""
    rows = (line.rstrip("\n").split("\t") for line in open(ARCHIFY / "dark.tsv", encoding="utf-8")
            if line.strip() and not line.startswith("#"))
    found = {r[0]: tuple(r[1:5]) for r in rows if len(r) in (2, 5)}
    for slug, frame in found.items():
        if not all(re.fullmatch(r"[\d.]+", v) for v in frame):  # it is written into the page
            raise SystemExit(f"archify_dress: dark.tsv: the frame of {slug} is not numbers")
    return found


def view_box(frame, width, height):
    """The viewBox of a drawing Archify drew `width` by `height`, cut to its dark.tsv frame."""
    if not frame:
        return f"0 0 {width} {height}"
    if len(frame) == 1:
        return f"0 0 {width} {frame[0]}"
    h, x, y, w = frame
    return f"{x} {y} {w} {h}"


def dress(src, slug, bar=None):
    m = re.search(r'<svg viewBox="0 0 ([\d.]+) ([\d.]+)"', src)
    if not m:
        raise SystemExit("archify_dress: no <svg viewBox> in the page")
    box = view_box(frames().get(slug), m.group(1), m.group(2))
    src = src.replace(m.group(0), f'<svg viewBox="{box}"', 1)
    css = (ARCHIFY / "ww3-gpu.css").read_text(encoding="utf-8")
    head = f'<link rel="stylesheet" href="{html.escape(FONTS)}"><style>{css}</style>'
    if bar is not None:
        src = re.sub(r'<link[^>]*rel="?icon"?[^>]*>', "", src)
        head += f'<style>{BAR_CSS}</style><link rel="icon" href="../../favicon.svg">'
    if "</head>" not in src:
        raise SystemExit("archify_dress: no </head> in the page")
    src = src.replace("</head>", head + "</head>", 1)
    if bar is not None:
        body = re.search(r"<body[^>]*>", src)
        src = src[:body.end()] + bar + src[body.end():]
        src = src.replace("</body>", BAR_JS + "</body>", 1)
    return src


def main(argv):
    if len(argv) not in (3, 5) or (len(argv) == 5 and argv[3] != "--bar"):
        sys.exit(__doc__)
    page, out, slug = argv[:3]
    bar = pathlib.Path(argv[4]).read_text(encoding="utf-8") if len(argv) == 5 else None
    pathlib.Path(out).write_text(dress(pathlib.Path(page).read_text(encoding="utf-8"), slug, bar),
                                 encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
