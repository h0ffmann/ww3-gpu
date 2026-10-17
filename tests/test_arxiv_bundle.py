import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import arxiv_bundle  # noqa: E402


class ArxivBundle(unittest.TestCase):
    def test_self_test(self):
        self.assertEqual(arxiv_bundle.self_test(), 0)

    def test_every_key_of_the_paper_is_cited(self):
        md = (arxiv_bundle.PAPER / "paper.md").read_text(encoding="utf-8")
        bib = (arxiv_bundle.PAPER / "refs.bib").read_text(encoding="utf-8")
        cited = set(arxiv_bundle.CITE.findall(md.split("\n---", 1)[1]))
        self.assertEqual(arxiv_bundle.bib_keys(bib) - cited, set())


if __name__ == "__main__":
    unittest.main()
