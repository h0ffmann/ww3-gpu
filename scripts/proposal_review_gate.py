#!/usr/bin/env python3
"""proposal_review_gate — the proposal may not change without a fresh review.

    scripts/proposal_review_gate.py --check              # exit 1 when the review is missing or stale
    scripts/proposal_review_gate.py --hash               # content hash of the proposal sources
    scripts/proposal_review_gate.py --record parecer.md  # store that review for the current content
    scripts/proposal_review_gate.py --self-test

The sources are pubs/proposal/{pt,en}/*.md, refs.bib and meta.*.yaml. Their combined sha256 is
recorded in pubs/proposal/.review.json together with the review text. Any later edit changes the
hash, the recorded review no longer matches, and --check fails: a review cannot be inherited by a
text it never saw. Standard library only.
"""
import argparse
import datetime as dt
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROPOSAL = ROOT / "pubs" / "proposal"
RECORD = PROPOSAL / ".review.json"


def sources(base: pathlib.Path = PROPOSAL) -> list:
    files = sorted(base.glob("pt/*.md")) + sorted(base.glob("en/*.md"))
    files += sorted(base.glob("*.yaml")) + sorted(base.glob("refs.bib"))
    return files


def content_hash(base: pathlib.Path = PROPOSAL) -> str:
    h = hashlib.sha256()
    for f in sources(base):
        h.update(f.relative_to(base).as_posix().encode("utf-8"))
        h.update(f.read_bytes())
    return h.hexdigest()


def check(base: pathlib.Path = PROPOSAL, record: pathlib.Path = None) -> tuple:
    record = record or base / ".review.json"
    now = content_hash(base)
    if not record.exists():
        return 1, (f"no review recorded for the proposal ({record.relative_to(ROOT) if record.is_relative_to(ROOT) else record}).\n"
                   "Run the revisor-proposta subagent, then: just proposal-review-record <parecer.md>")
    try:
        data = json.loads(record.read_text())
    except json.JSONDecodeError as error:
        return 1, f"{record.name}: not valid JSON ({error})"
    if data.get("content_hash") != now:
        return 1, ("the proposal changed after the last review "
                   f"(recorded {str(data.get('content_hash'))[:12]}, now {now[:12]}).\n"
                   "Run the revisor-proposta subagent again, then: just proposal-review-record <parecer.md>")
    return 0, f"review of {data.get('reviewed_at', '?')} matches the current proposal ({now[:12]})"


def record_review(parecer: pathlib.Path, base: pathlib.Path = PROPOSAL, record: pathlib.Path = None) -> int:
    record = record or base / ".review.json"
    text = parecer.read_text(encoding="utf-8").strip()
    if len(text) < 200:
        print(f"{parecer}: a review of {len(text)} characters is not a review", file=sys.stderr)
        return 2
    record.write_text(json.dumps({
        "content_hash": content_hash(base),
        "reviewed_at": dt.datetime.now(dt.timezone.utc).date().isoformat(),
        "reviewer": "revisor-proposta (.claude/agents/revisor-proposta.md)",
        "parecer": text,
    }, ensure_ascii=False, indent=2, sort_keys=True) + "\n")
    print(f"recorded review for {content_hash(base)[:12]} in {record.name}")
    return 0


def self_test() -> None:
    import tempfile
    base = pathlib.Path(tempfile.mkdtemp())
    (base / "pt").mkdir()
    (base / "en").mkdir()
    (base / "pt" / "03-theme.md").write_text("# TEMA\ntexto\n")
    (base / "refs.bib").write_text("@misc{x, title={y}}\n")
    rec = base / ".review.json"
    first = content_hash(base)
    assert check(base, rec)[0] == 1, "no record must fail"
    parecer = base / "parecer.md"
    parecer.write_text("PARECER GERAL\n" + "detalhe. " * 40)
    assert record_review(parecer, base, rec) == 0
    assert check(base, rec)[0] == 0, check(base, rec)
    (base / "pt" / "03-theme.md").write_text("# TEMA\ntexto editado\n")
    assert content_hash(base) != first
    code, msg = check(base, rec)
    assert code == 1 and "changed after the last review" in msg, msg
    short = base / "short.md"
    short.write_text("ok")
    assert record_review(short, base, rec) == 2, "a stub must be refused"
    print("proposal_review_gate self-test ok")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--hash", action="store_true")
    ap.add_argument("--record", type=pathlib.Path)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        self_test()
        return 0
    if a.hash:
        print(content_hash())
        return 0
    if a.record:
        return record_review(a.record)
    if a.check:
        code, msg = check()
        print(("proposal review: " if code == 0 else "proposal review gate: ") + msg,
              file=sys.stderr if code else sys.stdout)
        return code
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
