import importlib.util, pathlib, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("route", ROOT / "scripts" / "route.py")
route = importlib.util.module_from_spec(spec); spec.loader.exec_module(route)


def fixture(files):
    d = pathlib.Path(tempfile.mkdtemp())
    for name, text in files.items():
        (d / name).parent.mkdir(parents=True, exist_ok=True)
        (d / name).write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=d, check=True)
    subprocess.run(["git", "add", "-A"], cwd=d, check=True)
    return d


GLOSSARY = """# Glossary

## WW3 switches

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| NL0–NL5 | quadruplet interactions | one module each | — |
| `NLS` | nonlinear smoother | builds w3snlsmd | — |

## Routines

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| W3SNL1MD / w3snl1md.F90 | the module | DIA | — |
| W3SNL1 | the DIA | kernel | — |
"""


class Term(unittest.TestCase):
    def test_exact_name_first_then_substring(self):
        d = fixture({"docs/GLOSSARY.md": GLOSSARY})
        out = route.term("W3SNL1", d)
        self.assertIn("| W3SNL1 |", out[0])
        self.assertIn("[Routines]", out[0])
        self.assertEqual(len(out), 2)

    def test_family_row_names_its_members(self):
        d = fixture({"docs/GLOSSARY.md": GLOSSARY})
        self.assertIn("NL0–NL5", route.term("NL1", d)[0])

    def test_backticks_ignored(self):
        d = fixture({"docs/GLOSSARY.md": GLOSSARY})
        self.assertIn("NLS", route.term("nls", d)[0])


class Where(unittest.TestCase):
    def test_definitions_not_calls(self):
        d = fixture({
            "k/a.hpp": "void snl1(const Config& c,\n    int n);\nint g() { return snl1(c, 1); }\n",
            "k/b.cpp": "void snl1(const Config& c, int n) {\n  snl1(c, n);\n}\n",
            "s/x.sh": "snl1() {\n  :\n}\nsnl1\n",
            "justfile": "snl1 jobs=\"8\" *args:\n    echo\nother := snl1\n",
            "p/y.py": "def snl1():\n    snl1()\n",
            "f/z.F90": "  SUBROUTINE SNL1 (A)\n  CALL SNL1 (A)\n  END SUBROUTINE SNL1\n",
        })
        out = "\n".join(route.where("snl1", d))
        for hit in ("k/a.hpp:1", "k/b.cpp:1", "s/x.sh:1", "justfile:1", "p/y.py:1", "f/z.F90:1"):
            self.assertIn(hit, out)
        for miss in ("k/a.hpp:3", "k/b.cpp:2", "s/x.sh:4", "justfile:3", "p/y.py:2", "f/z.F90:2"):
            self.assertNotIn(miss, out)

    def test_file_stem_and_missing_ww3_said(self):
        d = fixture({"k/snl1_dia.hpp": "// x\n"})
        out = route.where("snl1_dia", d)
        self.assertEqual(out[0], "k/snl1_dia.hpp  (file)")
        self.assertIn("not checked out", out[-1])

    def test_submodules_skipped(self):
        d = fixture({"WW4/a.cpp": "void snl1(int n) {\n}\n"})
        self.assertFalse(any("WW4/" in x for x in route.where("snl1", d)))


class Docs(unittest.TestCase):
    def test_backtick_and_link_with_section(self):
        d = fixture({
            "a.md": "# A\n\n## Shim\n\nSee `kokkos/src/snl1_shim.cpp:40`.\n",
            "b.md": "# B\n\n[the shim](../kokkos/src/snl1_shim.cpp)\n",
            "c.md": "# C\n\nsnl1_shim.cpp without backticks\n",
        })
        out = route.docs("snl1_shim.cpp", d)
        self.assertEqual(out, ["a.md:5  [Shim]", "b.md:3  [B]"])


class Why(unittest.TestCase):
    def test_ranks_the_matching_paragraph(self):
        d = fixture({
            "docs/ADRs/ADR-0001-x.md": "# ADR\n\nWe never widen a tolerance to pass.\n\n"
                                       "Unrelated paragraph about figures.\n",
            "docs/log/2026-01-01-y.md": "A tolerance was discussed once among many other words "
                                        "here and there.\n",
            "README.md": "never widen a tolerance\n",
        })
        out = route.why("widen tolerance", d)
        self.assertTrue(out[0].startswith("docs/ADRs/ADR-0001-x.md:3"))
        self.assertFalse(any(x.startswith("README.md") for x in out))


class Section(unittest.TestCase):
    def test_numbered_section_to_next_of_its_level(self):
        d = fixture({"p.md": "# P\n\n### 1.5 Done\n\ntext\n\n#### detail\n\nmore\n\n### 1.6 Next\n"})
        out = route.section("p.md", "1.5", d)
        self.assertEqual(out[0], "p.md:3-10")
        self.assertIn("more", out)
        self.assertNotIn("### 1.6 Next", out)


class Main(unittest.TestCase):
    def test_budget_cuts_and_says_so(self):
        out = subprocess.run(["python3", str(ROOT / "scripts" / "route.py"), "docs", "AGENTS.md",
                              "--budget", "2"], capture_output=True, text=True)
        self.assertEqual(out.returncode, 0)
        self.assertEqual(len(out.stdout.strip().split("\n")), 3)
        self.assertIn("--budget to widen", out.stdout)


if __name__ == "__main__":
    unittest.main()
