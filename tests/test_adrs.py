import importlib.util, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("adrs", ROOT / "scripts" / "adrs.py")
adrs = importlib.util.module_from_spec(spec); spec.loader.exec_module(adrs)


class AdrSummary(unittest.TestCase):
    def test_reads_title_status_and_date(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "ADR-0007-x.md").write_text(
            "# ADR-0007: Do the thing\n\n| | |\n|---|---|\n| **Status** | Accepted |\n"
            "| **Date** | 2026-01-02 |\n", encoding="utf-8")
        (d / "README.md").write_text("# not an ADR\n", encoding="utf-8")
        self.assertEqual(adrs.load(d), [("ADR-0007", "Accepted", "2026-01-02", "Do the thing")])

    def test_site_fills_the_placeholder_newest_first(self):
        d = pathlib.Path(tempfile.mkdtemp())
        for n in ("0001", "0002"):
            (d / f"ADR-{n}-x.md").write_text(
                f"# ADR-{n}: Do </script> {n}\n\n| **Status** | Accepted |\n| **Date** | 2026-01-0{n[-1]} |\n",
                encoding="utf-8")
        (d / "t.html").write_text("<script>const ADRS = [];</script>", encoding="utf-8")
        adrs.site(d / "t.html", d / "o.html", d)
        out = (d / "o.html").read_text(encoding="utf-8")
        self.assertLess(out.index("ADR-0002"), out.index("ADR-0001"))
        self.assertIn('"file": "ADR-0002-x.md"', out)
        self.assertEqual(out.count("</script>"), 1)

    def test_every_committed_adr_parses(self):
        for num, status, date, title in adrs.load():
            self.assertNotIn("?", (status, date), num)
            self.assertFalse(title.startswith("ADR-"), num)


if __name__ == "__main__":
    unittest.main()
