import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("notes_check", ROOT / "scripts" / "notes_check.py")
notes_check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notes_check)


class NotesCheck(unittest.TestCase):
    def test_the_repository_slip_box_is_clean(self):
        self.assertEqual(notes_check.check(ROOT / "notes"), [])

    def test_the_self_test_passes(self):
        self.assertEqual(notes_check.self_test(), 0)

    def test_a_broken_link_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / "202610141900-a.md").write_text(notes_check.note("202610141900", "a", ["202610141999-gone"]))
            (d / "index.md").write_text("- [[202610141900-a]]\n")
            self.assertTrue(any("does not resolve" in f for f in notes_check.check(d)))


if __name__ == "__main__":
    unittest.main()
