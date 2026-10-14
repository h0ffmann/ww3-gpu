import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("memory", ROOT / "scripts" / "memory.py")
memory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(memory)


class Memory(unittest.TestCase):
    def test_the_repository_memory_is_clean(self):
        self.assertEqual(memory.check(), [])

    def test_the_self_test_passes(self):
        self.assertEqual(memory.self_test(), 0)

    def test_export_takes_only_tagged_pages_and_never_sessions(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp)
            (src / "sessions").mkdir()
            (src / "a.md").write_text(memory.page([memory.TAG]))
            (src / "b.md").write_text(memory.page(["seed"]))
            (src / "sessions" / "s.md").write_text(memory.page([memory.TAG]))
            (src / "log-2026-10.md").write_text(memory.page([memory.TAG]))
            self.assertEqual(list(memory.published(src)), ["a.md"])

    def test_import_refuses_a_page_posing_as_a_rule(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "_rules").mkdir()
            (out / "_rules" / "x.md").write_text(memory.page([memory.TAG]))
            (out / memory.INDEX).write_text(memory.index({"_rules/x.md": memory.page([memory.TAG])}, "p"))
            self.assertEqual(memory.import_(dry_run=True, out=out), 1)

    def test_front_matter_reads_block_lists(self):
        fields, body = memory.front_matter(memory.page(["seed", memory.TAG]))
        self.assertEqual(fields["tags"], ["seed", memory.TAG])
        self.assertEqual(fields["pinned"], "true")
        self.assertTrue(body.startswith("# A page"))


if __name__ == "__main__":
    unittest.main()
