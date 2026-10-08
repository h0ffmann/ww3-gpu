import importlib.util, pathlib, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "figures.py"
spec = importlib.util.spec_from_file_location("figures", SCRIPT)
figures = importlib.util.module_from_spec(spec); spec.loader.exec_module(figures)

GOOD = """# Page

```mermaid
%% figure: a-b
%% title: What flows where?
flowchart LR
    A --> B
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** A feeds B.

**How to read.** Left to right.

**Not shown.** C.

**Evidence.** `x.md` (v).

</details>
"""


class Figures(unittest.TestCase):
    def parse(self, text, name="page.md"):
        root = pathlib.Path(tempfile.mkdtemp())
        (root / name).parent.mkdir(parents=True, exist_ok=True)
        (root / name).write_text(text, encoding="utf-8")
        errors = []
        return figures.parse(root / name, errors, root), errors

    def test_good_fence(self):
        figs, errors = self.parse(GOOD)
        self.assertEqual(errors, [])
        self.assertEqual((figs[0]["id"], figs[0]["title"], figs[0]["takeaway"]),
                         ("a-b", "What flows where?", "A feeds B."))
        self.assertTrue(figs[0]["source"].startswith("%% figure: a-b\n"))

    def test_missing_header_and_card(self):
        _, errors = self.parse("```mermaid\nflowchart LR\n  A --> B\n```\n")
        self.assertTrue(any("%% figure:" in e for e in errors), errors)
        self.assertTrue(any("%% title:" in e for e in errors), errors)
        self.assertTrue(any("no reading card" in e for e in errors), errors)

    def test_empty_label(self):
        _, errors = self.parse(GOOD.replace("**Not shown.** C.", "**Not shown.**"))
        self.assertEqual(len(errors), 1, errors)
        self.assertIn("Not shown", errors[0])

    def test_portuguese_card(self):
        _, errors = self.parse(GOOD, "mapas.pt.md")
        self.assertTrue(any("Como ler esta figura" in e for e in errors), errors)

    def test_gantt_needs_today_marker_off(self):
        _, errors = self.parse(GOOD.replace("flowchart LR\n    A --> B", "gantt\n    dateFormat YYYY-MM-DD"))
        self.assertTrue(any("todayMarker off" in e for e in errors), errors)

    def test_pdf_dates_pinned_same_length(self):
        pdf = pathlib.Path(tempfile.mkdtemp()) / "x.pdf"
        raw = b"<</CreationDate (D:20261008020829+00'00') /ModDate (D:20261008020829+00'00')>>"
        pdf.write_bytes(raw)
        figures.pin_pdf_dates(pdf)
        self.assertEqual(pdf.read_bytes(), raw.replace(b"20261008020829", b"20000101000000"))

    def chart_fig(self, edit=None):
        table = pathlib.Path(tempfile.mkdtemp()) / "t.md"
        table.write_text("| routine | a | b |\n|---|---|---|\n| X | 2,0 | 1 |\n"
                         "| Y | 0.5 | n/a |\n| total | 2.5 | 1 |\n", encoding="utf-8")
        body = figures.chart_body(table, "routine", ["a", "b"], "s")
        source = "\n".join([f"%% figure: c", "%% title: q?",
                             f"%% data: {table} --label routine --value a --value b --y-title s"]
                            + [edit(l) if edit else l for l in body])
        return body, {"file": "page.md", "line": 1, "source": source}

    def test_chart_from_table(self):
        body, fig = self.chart_fig()
        self.assertIn('    x-axis ["X"]', body)  # Y lacks a number in b; total is skipped
        self.assertIn("    bar [2]", body)       # comma decimal read as a number
        self.assertIn("    line [1]", body)
        errors = []
        figures.check_data(fig, errors)
        self.assertEqual(errors, [])

    def test_chart_edited_by_hand_fails(self):
        _, fig = self.chart_fig(lambda l: l.replace("bar [2]", "bar [3]"))
        errors = []
        figures.check_data(fig, errors)
        self.assertTrue(any("no longer matches" in e for e in errors), errors)

    def schedule_fig(self, defence="2027-05-01"):
        table = pathlib.Path(tempfile.mkdtemp()) / "s.md"
        table.write_text("| Atividade | Prazo |\n|---|---|\n| Revisão | 10/2026 |\n"
                         "| Defesa (a partir de) | 05/2027 |\n", encoding="utf-8")
        source = "\n".join(["%% figure: s", "%% title: q?", f"%% schedule: {table}", "gantt",
                             "    dateFormat YYYY-MM-DD", "    todayMarker off", "    section A",
                             "    Revisão       :a1, 2026-10-01, 2026-10-31",
                             f"    Defesa        :milestone, d2, {defence}, 0d",
                             "    section Marco externo",
                             "    WW4           :milestone, w4, 2027-07-01, 0d"])
        errors = []
        figures.check_schedule({"file": "page.md", "line": 1, "source": source}, errors)
        return errors

    def test_schedule_matches_table(self):
        self.assertEqual(self.schedule_fig(), [])  # a trailing external milestone is allowed

    def test_schedule_drift_fails(self):
        errors = self.schedule_fig(defence="2027-06-01")
        self.assertTrue(any("no longer match the table" in e for e in errors), errors)

    def test_repository_is_current(self):
        p = subprocess.run([sys.executable, SCRIPT, "check"], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr)


if __name__ == "__main__":
    unittest.main()
