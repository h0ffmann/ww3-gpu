import importlib.util
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("context", ROOT / "scripts" / "context.py")
context = importlib.util.module_from_spec(spec)
spec.loader.exec_module(context)


def fixture(files):
    d = pathlib.Path(tempfile.mkdtemp())
    for name, text in files.items():
        (d / name).parent.mkdir(parents=True, exist_ok=True)
        (d / name).write_text(text, encoding="utf-8")
    subprocess.run(["git", "init", "-q"], cwd=d, check=True)
    subprocess.run(["git", "add", "-A"], cwd=d, check=True)
    return d


class Pack(unittest.TestCase):
    def test_required_sections_present(self):
        d = fixture({"docs/adr.md": "# ADR\n\nClaim (v) measured.\n"})
        safe, text = context.pack("my-goal", files="docs/adr.md", root=d, budget=1)
        for section in context.PACK_REQUIRED_SECTIONS:
            self.assertIn(f"## {section}", text)
        self.assertEqual(safe, "my-goal")

    def test_evidence_claims_kept(self):
        d = fixture({"docs/adr.md": "# ADR\n\nA number 1.5 s (v) by foo.\nA guess ⚠ maybe.\n"})
        safe, text = context.pack("g", files="docs/adr.md", root=d)
        self.assertIn("(v)", text)
        self.assertIn("⚠", text)

    def test_gate_results_parsed(self):
        d = fixture({
            "gates.json": '[{"command": "just test", "result": "pass", "date": "2026-10-16"}]',
            "docs/adr.md": "# ADR\n\nClaim (v).\n",
        })
        safe, text = context.pack("g", files="docs/adr.md",
                                  gate_results_path=str(d / "gates.json"), root=d)
        self.assertIn("just test", text)
        self.assertIn("pass", text)

    def test_git_status_returns_list(self):
        d = fixture({"docs/adr.md": "# ADR\n\nClaim (v).\n"})
        status = context.git_status(d)
        self.assertIsInstance(status, list)


class Query(unittest.TestCase):
    def test_local_bm25_finds_paragraph(self):
        d = fixture({
            "docs/ADRs/ADR-0001-x.md": "# ADR\n\nWe never widen a tolerance.\n\n"
                                       "Unrelated paragraph about figures.\n",
            "scripts/route.py": "import sys\nsys.exit(1)\n",
        })
        out = context.local_query("widen tolerance", d, budget=2)
        self.assertTrue(any("tolerance" in ln.lower() for ln in out))

    def test_query_uses_route_when_present(self):
        d = fixture({
            "docs/ADRs/ADR-0001-x.md": "# ADR\n\nWe never widen a tolerance.\n",
            "scripts/route.py": "import sys\nprint('docs/ADRs/ADR-0001-x.md:3 routed')\n",
        })
        out = context.query("widen tolerance", d, budget=2)
        self.assertTrue(any("routed" in ln for ln in out))


class Check(unittest.TestCase):
    def test_rejects_email(self):
        bad = (
            "# p\n\n## Goal id\n\ng\n\n## Modified files\n\n(none)\n\n"
            "## Gate results\n\n(none recorded)\n\n## Evidence claims\n\n(none)\n\n"
            "## Related context\n\n(none)\n\n## Summary\n\nmail foo@example.org\n"
        )
        faults = context.check_pack(bad)
        self.assertTrue(any("e-mail" in f for f in faults))

    def test_rejects_home_path(self):
        bad = (
            "# p\n\n## Goal id\n\ng\n\n## Modified files\n\n(none)\n\n"
            "## Gate results\n\n(none recorded)\n\n## Evidence claims\n\n(none)\n\n"
            "## Related context\n\n(none)\n\n## Summary\n\nsee /home/foo/x\n"
        )
        faults = context.check_pack(bad)
        self.assertTrue(any("home path" in f for f in faults))

    def test_accepts_clean_pack(self):
        clean = (
            "# p\n\n## Goal id\n\ng\n\n## Modified files\n\n(none)\n\n"
            "## Gate results\n\n(none recorded)\n\n## Evidence claims\n\n(none)\n\n"
            "## Related context\n\n(none)\n\n## Summary\n\nok\n"
        )
        self.assertFalse(context.check_pack(clean))


class SelfTest(unittest.TestCase):
    def test_self_test_passes(self):
        self.assertEqual(context.self_test(), 0)


class Main(unittest.TestCase):
    def test_pack_command_dry_run(self):
        d = fixture({"docs/adr.md": "# ADR\n\nClaim (v).\n"})
        out = subprocess.run(
            ["python3", str(ROOT / "scripts" / "context.py"), "pack", "--dry-run",
             "--goal", "g", "--files", "docs/adr.md"],
            cwd=d, capture_output=True, text=True)
        self.assertEqual(out.returncode, 0)
        self.assertIn("## Goal id", out.stdout)


if __name__ == "__main__":
    unittest.main()
