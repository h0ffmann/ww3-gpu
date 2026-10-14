import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import refs_check  # noqa: E402


class RefsCheck(unittest.TestCase):
    def test_self_test(self):
        self.assertEqual(refs_check.self_test(), 0)

    def test_bib_fields(self):
        e = refs_check.bib_entries("@misc{k, title = {T}, url = {https://x}}")
        self.assertEqual(e, {"k": {"title", "url"}})

    def test_proposal_log_is_complete(self):
        bib = refs_check.BIB.read_text(encoding="utf-8")
        refs = refs_check.REFS.read_text(encoding="utf-8")
        problems, _ = refs_check.check(bib, refs)
        self.assertEqual(problems, [])


if __name__ == "__main__":
    unittest.main()
