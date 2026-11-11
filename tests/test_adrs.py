import importlib.util, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("adrs", ROOT / "scripts" / "adrs.py")
adrs = importlib.util.module_from_spec(spec); spec.loader.exec_module(adrs)


class AdrSummary(unittest.TestCase):
    def test_reads_title_status_and_date(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "ADRs.md").write_text(
            "# Architecture Decision Records\n\n| **Status** | not a record |\n\n"
            "<a id=\"adr-0007\"></a>\n\n## ADR-0007: Do the thing\n\n| | |\n|---|---|\n"
            "| **Status** | Accepted |\n| **Date** | 2026-01-02 |\n\n### Context\n\n"
            "## ADR-0008: The next one\n\n| **Date** | 2026-01-03 |\n", encoding="utf-8")
        self.assertEqual(adrs.load(d / "ADRs.md"), [("ADR-0007", "Accepted", "2026-01-02", "Do the thing"),
                                                    ("ADR-0008", "?", "2026-01-03", "The next one")])

    def test_site_fills_the_placeholder_newest_first(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "ADRs.md").write_text("".join(
            f"## ADR-{n}: Do </script> {n}\n\n| **Status** | Accepted |\n| **Date** | 2026-01-0{n[-1]} |\n\n"
            for n in ("0001", "0002")), encoding="utf-8")
        (d / "t.html").write_text("<script>const ADRS = [];</script>", encoding="utf-8")
        adrs.site(d / "t.html", d / "o.html", d / "ADRs.md")
        out = (d / "o.html").read_text(encoding="utf-8")
        self.assertLess(out.index("ADR-0002"), out.index("ADR-0001"))
        self.assertIn('"file": "ADRs.md#adr-0002"', out)
        self.assertEqual(out.count("</script>"), 1)

    def test_every_committed_adr_parses(self):
        rows = adrs.load()
        self.assertEqual([r[0] for r in rows], sorted(r[0] for r in rows))
        for num, status, date, title in rows:
            self.assertNotIn("?", (status, date), num)
            self.assertFalse(title.startswith("ADR-"), num)


if __name__ == "__main__":
    unittest.main()
