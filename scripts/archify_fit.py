#!/usr/bin/env python3
"""archify_fit — does a drawing fit the page a paper prints it on (DESIGN.md, "Pictures in a paper")?

    archify_fit.py <dressed.html> <printed width, mm>   # one line; exit 1 when it does not fit
    archify_fit.py --self-test

scripts/archify.sh png and fit call it for each drawing of docs/archify/paper.tsv, on the page
scripts/archify_dress.py dressed. It measures what a reader's eye meets at the printed size: the
proportion of the frame, and the size of the type once the frame is scaled to the printed width.
The balance of the drawing is a person's judgement, on the built page at 100 %.
"""
import re
import sys

MM_TO_PT = 72 / 25.4
# Width over height of the frame: no taller than 5:4, no wider than 2:1 (a strip prints its type too small).
RATIO = (1.25, 2.0)
# Printed type, in points: the text of a box, and every other text (an arrow's or a frame's label).
BOX_PT, ANY_PT = 7.0, 6.0


def measure(page):
    """(width, height, [(font size, is box text)]) of a dressed page, in viewBox units."""
    m = re.search(r'<svg viewBox="([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)"', page)
    if not m:
        raise SystemExit("archify_fit: no <svg viewBox> in the page")
    x0, y0, w, h = map(float, m.groups())
    svg = page[m.start():page.index("</svg>", m.start())]
    texts = []
    for tag in re.findall(r"<text\b[^>]*>", svg):
        attr = dict(re.findall(r'([\w-]+)="([^"]*)"', tag))
        # Archify shows data-detail="fine" text only when the reader zooms in; a print never has it.
        if "font-size" not in attr or attr.get("data-detail") == "fine":
            continue
        x, y = float(attr.get("x", x0)), float(attr.get("y", y0))
        if x0 <= x <= x0 + w and y0 <= y <= y0 + h:  # the legend lies under the frame
            texts.append((float(attr["font-size"]), "data-node-label" in attr or "data-detail" in attr))
    return w, h, texts


def verdict(w, h, texts, width_mm):
    """(facts, failures) of a w x h frame with those texts, printed width_mm wide."""
    k = width_mm * MM_TO_PT / w
    box = min((s for s, in_box in texts if in_box), default=None)
    small = min((s for s, _ in texts), default=None)
    facts = {"ratio": w / h, "height_mm": width_mm * h / w,
             "box_pt": box and box * k, "small_pt": small and small * k}
    bad = []
    if not RATIO[0] <= facts["ratio"] <= RATIO[1]:
        bad.append(f"proportion {facts['ratio']:.2f}:1 is outside {RATIO[0]}:1 to {RATIO[1]}:1")
    if box is not None and facts["box_pt"] < BOX_PT:
        bad.append(f"box text prints at {facts['box_pt']:.1f} pt, under {BOX_PT} pt")
    if small is not None and facts["small_pt"] < ANY_PT:
        bad.append(f"the smallest text prints at {facts['small_pt']:.1f} pt, under {ANY_PT} pt")
    return facts, bad


def line(facts, width_mm):
    def pt(v):
        return "none" if v is None else f"{v:.1f} pt"
    return (f"{width_mm:g} x {facts['height_mm']:.0f} mm, {facts['ratio']:.2f}:1, "
            f"box text {pt(facts['box_pt'])}, smallest text {pt(facts['small_pt'])}")


def self_test():
    page = ('<svg viewBox="8 22 622 354"><text data-node-label="" x="100" y="60" font-size="11">a</text>'
            '<text data-detail="context" x="100" y="80" font-size="9">b</text>'
            '<text x="300" y="200" font-size="8">c</text>'
            '<text data-detail="fine" x="100" y="90" font-size="7">d</text>'
            '<text x="40" y="420" font-size="5">legend</text></svg>')
    w, h, texts = measure(page)
    assert (w, h) == (622.0, 354.0) and sorted(texts) == [(8.0, False), (9.0, True), (11.0, True)], texts
    assert verdict(w, h, texts, 174)[1] == []
    assert len(verdict(1510, 470, texts, 174)[1]) == 3  # a strip: proportion, box text, smallest text
    assert verdict(w, h, texts, 120)[1][0].startswith("box text")
    print("archify_fit: self-test ok", file=sys.stderr)


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    if len(argv) != 2:
        sys.exit(__doc__)
    width_mm = float(argv[1])
    w, h, texts = measure(open(argv[0], encoding="utf-8").read())
    facts, bad = verdict(w, h, texts, width_mm)
    print(line(facts, width_mm))
    if bad:
        sys.exit("archify_fit: " + "; ".join(bad))


if __name__ == "__main__":
    main(sys.argv[1:])
