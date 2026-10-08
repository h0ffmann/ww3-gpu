import pathlib, shutil, subprocess, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FILTER = ROOT / "pubs" / "filters" / "cronograma.lua"
SCHEDULE = ROOT / "pubs" / "proposal" / "pt" / "08-schedule.md"


@unittest.skipUnless(shutil.which("pandoc"), "pandoc not on PATH (`nix develop .`)")
class Cronograma(unittest.TestCase):
    def run_filter(self, to, lang="pt-BR", source=None):
        args = ["pandoc", "--lua-filter", str(FILTER), "--metadata", f"lang={lang}", "-t", to]
        args += [] if source else [str(SCHEDULE)]
        return subprocess.run(args, input=source, capture_output=True, text=True, check=True).stdout

    def test_latex_grid(self):
        out = self.run_filter("latex")
        self.assertIn("\\multicolumn{3}{c}{2026}", out)    # out, nov, dez
        self.assertIn("\\multicolumn{5}{c}{2027}", out)    # jan to maio
        self.assertIn("& abr & maio", out)                 # ABNT writes maio in full
        self.assertIn("& out & nov & dez & jan", out)
        self.assertEqual(out.count("\\rule[-0.3ex]{\\linewidth}{1.5ex}"), 7)
        self.assertEqual(out.count("$\\blacklozenge$"), 2)  # the defence and the legend

    def test_word_table(self):
        out = self.run_filter("plain")
        self.assertEqual((out.count("■"), out.count("◆")), (8, 2))  # seven bars plus the legend's
        self.assertIn("out/26", out)

    def test_english_from(self):
        md = "| Activity | Deadline |\n|---|---|\n| Review | 10/2026 |\n| Defence | from 12/2026 |\n"
        out = self.run_filter("plain", "en-US", md)
        self.assertIn("Oct/26", out)
        self.assertEqual(out.count("◆"), 2)

    def test_other_tables_untouched(self):
        md = "| a | b |\n|---|---|\n| x | 3 |\n"
        self.assertNotIn("■", self.run_filter("plain", source=md))


if __name__ == "__main__":
    unittest.main()
