import importlib.util, json, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("lm", ROOT / "scripts" / "leanpub_manuscript.py")
lm = importlib.util.module_from_spec(spec); spec.loader.exec_module(lm)


class Math(unittest.TestCase):
    def test_inline_math_becomes_markua_span(self):
        self.assertEqual(lm.convert_math(r"where $N = F/\sigma$ is action"), r"where `N = F/\sigma`$ is action")

    def test_prices_shell_variables_and_code_are_left_alone(self):
        for s in ("costs $5 and $10 each", "the shell reads `$WW3/model`", "```sh\necho $HOME $USER\n```",
                  "    indented $code $block"):
            self.assertEqual(lm.convert_math(s), s)

    def test_display_math_becomes_a_dollar_fence(self):
        self.assertEqual(lm.convert_math("$$a\n= b$$\n"), "```$\na\n= b\n```\n")


class Export(unittest.TestCase):
    def test_every_part_lesson_and_figure_lands_in_the_manuscript(self):
        out = pathlib.Path(tempfile.mkdtemp())
        parts = json.loads(lm.PARTS.read_text(encoding="utf-8"))
        self.assertEqual(lm.export(out, parts, "deadbeef", "0000000"), [])
        m = out / "manuscript"
        book = (m / "Book.txt").read_text().split()
        lessons = sorted(p.stem for p in lm.COURSE.glob("[0-9][0-9]-*.md"))
        self.assertEqual([b for b in book if b[:2].isdigit()], [f"{s}.txt" for s in lessons], "every lesson, in order")
        self.assertEqual(book[:2], ["about.txt", "part-1.txt"])
        self.assertEqual(book.count(f"part-{len(parts['parts'])}.txt"), 1)
        sample = (m / "Sample.txt").read_text().split()
        self.assertEqual(sample[0], "about.txt")
        self.assertTrue(all(s in book for s in sample))
        l13 = (m / "13-bulk-porting-with-agents.txt").read_text()
        self.assertNotIn("```mermaid", l13)
        self.assertNotIn("<details", l13)
        self.assertEqual(l13.count("{aside}"), l13.count("{/aside}"))
        for png in {l.split("(images/")[1].split(")")[0] for l in l13.splitlines() if "](images/" in l}:
            self.assertTrue((m / "images" / png).is_file(), png)
        l00 = (m / "00-orientation.txt").read_text()
        self.assertTrue(l00.startswith("# 00. Orientation: what WW3 actually computes {#ch-orientation}"))
        self.assertNotIn("](0", l00, "lesson links point at chapter ids")
        self.assertIn("deadbeef", (m / "about.txt").read_text())

    def test_a_link_to_a_repository_file_becomes_its_github_url(self):
        l01 = next(lm.COURSE.glob("01-*.md"))
        self.assertIn("](../", l01.read_text(encoding="utf-8"), "the fixture lesson links outside course/")
        text = lm.convert(l01, {p.stem for p in lm.COURSE.glob("[0-9][0-9]-*.md")}, pathlib.Path(tempfile.mkdtemp()), [])
        self.assertRegex(text, rf"{lm.REPO_URL}/(blob|tree)/main/")
        self.assertNotIn("](../", text)


if __name__ == "__main__":
    unittest.main()
