import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import archify_dress  # noqa: E402
import archify_index  # noqa: E402


class Index(unittest.TestCase):
    def test_every_drawing_has_a_row_a_stamp_a_page_and_a_group(self):
        self.assertEqual(archify_index.problems(), [])

    def test_every_stamp_names_the_reference_and_its_version(self):
        for r in archify_index.table("figures.tsv"):
            self.assertIn("7.14", r["ww3"], r["slug"])
            self.assertIn("92e2cf6", r["ww3"], r["slug"])

    def test_the_index_lists_each_group_and_drawing_once(self):
        page = archify_index.page()
        for g in archify_index.table("groups.tsv"):
            self.assertEqual(page.count(f'<section id="{g["group"]}">'), 1)
        for r in archify_index.table("figures.tsv"):
            self.assertEqual(page.count(f'href="{r["slug"]}/"'), 1, r["slug"])

    def test_the_stamp_grows_the_frame_and_keeps_the_type_size(self):
        box, label = archify_dress.stamp("8 22 622 354", "WW3 7.14")
        self.assertEqual(box, "8 22 622 372")
        self.assertIn('font-size="9"', label)
        wide, label = archify_dress.stamp("0 0 1380 900", "WW3 7.14")
        self.assertEqual(wide, "0 0 1380 940")
        self.assertIn('font-size="20"', label)

    def test_the_dresser_prints_the_stamp_inside_the_drawing(self):
        page = '<html><head></head><body><svg viewBox="0 0 622 400"><g/></svg></body></html>'
        out = archify_dress.dress(page, "ww3-gpu-runtimes")
        self.assertIn('class="wg-ref"', out)
        self.assertLess(out.index('class="wg-ref"'), out.index("</svg>"))


if __name__ == "__main__":
    unittest.main()
