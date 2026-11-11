import pathlib, subprocess, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import archify_dress  # noqa: E402
import archify_fit  # noqa: E402

PAGE = ('<svg viewBox="8 22 622 354">'
        '<text data-node-label="" x="100" y="60" font-size="11">--ww3</text>'
        '<text data-detail="context" x="100" y="80" font-size="9">the reference</text>'
        '<text x="300" y="200" font-size="8">same bits as</text>'
        '<text data-detail="fine" x="100" y="90" font-size="7">zoom only</text>'
        '<text x="40" y="420" font-size="5">Legend</text></svg>')


class Fit(unittest.TestCase):
    def test_measure_skips_what_a_print_does_not_show(self):
        w, h, texts = archify_fit.measure(PAGE)
        self.assertEqual((w, h), (622.0, 354.0))
        self.assertEqual(sorted(texts), [(8.0, False), (9.0, True), (11.0, True)])

    def test_a_drawing_made_for_its_column_fits(self):
        facts, bad = archify_fit.verdict(*archify_fit.measure(PAGE), 174)
        self.assertEqual(bad, [])
        self.assertAlmostEqual(facts["box_pt"], 9 * 174 * 72 / 25.4 / 622)

    def test_a_screen_drawing_scaled_down_fails(self):
        _, _, texts = archify_fit.measure(PAGE)
        bad = archify_fit.verdict(1510, 470, texts, 174)[1]  # the preprint's first Figure 1
        self.assertEqual([b.split()[0] for b in bad], ["proportion", "box", "the"])

    def test_tall_and_narrow_fail_alone(self):
        _, _, texts = archify_fit.measure(PAGE)
        self.assertEqual(len(archify_fit.verdict(600, 600, texts, 174)[1]), 1)   # 1:1
        self.assertIn("box text", archify_fit.verdict(622, 354, texts, 120)[1][0])

    def test_cli_exit_code(self):
        tool = [sys.executable, str(ROOT / "scripts" / "archify_fit.py")]
        self.assertEqual(subprocess.run(tool + ["--self-test"], capture_output=True).returncode, 0)


class Frame(unittest.TestCase):
    def test_view_box(self):
        self.assertEqual(archify_dress.view_box(None, "656", "432"), "0 0 656 432")
        self.assertEqual(archify_dress.view_box(("354",), "656", "432"), "0 0 656 354")
        self.assertEqual(archify_dress.view_box(("354", "8", "22", "622"), "656", "432"), "8 22 622 354")

    def test_every_paper_drawing_is_framed_in_dark_tsv(self):
        frames = archify_dress.frames()
        rows = [line.split("\t") for line in (ROOT / "docs/archify/paper.tsv").read_text().splitlines()
                if line and not line.startswith("#")]
        self.assertTrue(rows)
        for slug, mm, _paper in rows:
            self.assertEqual(len(frames[slug]), 4, slug)  # height, x, y, width: equal margins
            self.assertGreater(float(mm), 0)
            self.assertTrue((ROOT / "docs/archify" / f"{slug}.pdf").is_file(), slug)


if __name__ == "__main__":
    unittest.main()
