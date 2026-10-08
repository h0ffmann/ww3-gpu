import importlib.util, pathlib, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("wfip", ROOT / "scripts" / "wfip.py")
wfip = importlib.util.module_from_spec(spec); spec.loader.exec_module(wfip)

README = (ROOT / "docs" / "WFIPs" / "README.md").read_text(encoding="utf-8")
TEMPLATE = (ROOT / "docs" / "WFIPs" / "TEMPLATE.md").read_text(encoding="utf-8")


class WfipTooling(unittest.TestCase):
    def setUp(self):
        self.dir = pathlib.Path(tempfile.mkdtemp())
        (self.dir / "TEMPLATE.md").write_text(TEMPLATE, encoding="utf-8")

    def new(self, slug="x", title="X", deliverable="D1"):
        return wfip.cmd_new(slug, title, deliverable, self.dir, today="2026-01-01")

    def test_new_numbers_sequentially_and_fills_the_template(self):
        self.new()
        p = self.new("y", "Y", "D2, D5")
        self.assertEqual(p.name, "WFIP-0002-y.md")
        w = wfip.load(self.dir)[2]
        self.assertEqual((w.title, w.deliverables, w.blocked_by), ("Y", ["D2", "D5"], []))
        self.assertEqual(w.rows["created"], "2026-01-01")
        self.assertEqual(w.missing, [])
        self.assertEqual(w.dod, (0, 2))

    def test_dod_counts_ticked_boxes_in_section_7_only(self):
        p = self.new()
        text = p.read_text(encoding="utf-8").replace("- [ ] <outcome>: `<command>`", "- [x] done: `cmd`", 1)
        text += "\n## 12. Not a section\n- [ ] not counted\n"
        p.write_text(text, encoding="utf-8")
        self.assertEqual(wfip.load(self.dir)[1].dod, (1, 2))

    def test_check_rejects_unknown_deliverable_and_missing_section(self):
        p = self.new(deliverable="D9")
        text = p.read_text(encoding="utf-8").replace("## 9. Alternatives considered", "## 9. Options")
        p.write_text(text, encoding="utf-8")
        errs = wfip.problems(wfip.load(self.dir), README)
        self.assertTrue(any("D9" in e for e in errs), errs)
        self.assertTrue(any("Alternatives considered" in e for e in errs), errs)

    def test_implemented_needs_a_full_dod(self):
        p = self.new()
        p.write_text(p.read_text(encoding="utf-8").replace(
            "| **Status** | Draft / Accepted / Implemented / Rejected / Superseded by WFIP-NNNN |",
            "| **Status** | Implemented (PR #1) |"), encoding="utf-8")
        errs = wfip.problems(wfip.load(self.dir), README)
        self.assertTrue(any("DoD 0/2" in e for e in errs), errs)

    def test_regenerate_is_idempotent_and_reads_only_the_hand_written_ids(self):
        self.new(); self.new("y", "Y", "D6")
        wfips = wfip.load(self.dir)
        once = wfip.regenerate(README, wfips)
        self.assertEqual(wfip.regenerate(once, wfips), once)
        self.assertEqual(wfip.deliverable_ids(once), ["D1", "D2", "D3", "D4", "D5", "D6"])
        self.assertIn("| D6 | [WFIP-0002](WFIP-0002-y.md) | 0/1 |", once)
        self.assertIn("| D3 | none yet | 0/0 |", once)

    def test_graph_from_blocked_by_only(self):
        self.new(); p = self.new("y", "Y", "D2")
        p.write_text(p.read_text(encoding="utf-8").replace(
            "| **Blocked by** | WFIP numbers that must merge first, comma-separated, or `none` (read by `wfip.py`) |",
            "| **Blocked by** | 0001 |"), encoding="utf-8")
        g = wfip.render_graph(wfip.load(self.dir))
        self.assertIn("W0001 --> W0002", g)
        self.assertNotIn("Depends", g)

    def test_status_names_created_and_revised(self):
        self.new()
        wfips = wfip.load(self.dir)
        out = wfip.render_status(wfips, ["D1"], "v0.1.0", {"A": ["WFIP-0001-x.md"], "M": []})
        self.assertIn("**Created:** WFIP-0001 X (Draft", out)
        self.assertIn("github.com/h0ffmann/ww3-gpu/blob/main/docs/WFIPs/WFIP-0001-x.md", out)

    def test_checked_in_index_is_current(self):
        self.assertEqual(wfip.problems(wfip.load(), README), [])


if __name__ == "__main__":
    unittest.main()
