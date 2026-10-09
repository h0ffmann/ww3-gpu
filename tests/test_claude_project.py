import json, pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SNAP = ROOT / ".claude" / "project"


class ClaudeProjectSnapshot(unittest.TestCase):
    def test_json_parses_and_points_at_files(self):
        data = json.loads((SNAP / "project.json").read_text(encoding="utf-8"))
        for r in data["routines"]:
            self.assertTrue((SNAP / r["prompt"]).is_file(), r["prompt"])

    def test_no_email_or_account_id(self):
        # ADR-0005: the service returns account UUIDs and the owner's address; neither is committed.
        bad = re.compile(r"[\w.+-]+@[\w-]+\.[\w.]+|[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
        for p in SNAP.rglob("*"):
            if p.is_file():
                self.assertIsNone(bad.search(p.read_text(encoding="utf-8")), p)

    def test_lesson_19_quotes_the_stored_prompt(self):
        lesson = (ROOT / "course" / "19-two-agents-one-issue-tracker.md").read_text(encoding="utf-8")
        prompt = (SNAP / "routines" / "a2a-wave-forecaster.txt").read_text(encoding="utf-8")
        self.assertIn("```text\n" + prompt + "```", lesson)


if __name__ == "__main__":
    unittest.main()
