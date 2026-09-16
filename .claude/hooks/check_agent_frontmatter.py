#!/usr/bin/env python3
"""Every .claude/agents/*.md must carry front matter Claude Code can parse.

An unquoted YAML value containing ": " is read as a nested mapping, the file fails to load and the
agent silently does not exist — which is how revisor-proposta was broken on 2026-09-16 by
"Somente leitura: aponta, não corrige." in its description. Checked without PyYAML (not every
environment has it) and with it when available.
"""
import pathlib
import sys

AGENTS = pathlib.Path(__file__).resolve().parents[1] / "agents"
REQUIRED = ("name", "description")


def check(path: pathlib.Path) -> list:
    parts = path.read_text(encoding="utf-8").split("---")
    if len(parts) < 3:
        return [f"{path.name}: no YAML front matter"]
    body, errors, seen = parts[1], [], set()
    for line in body.strip().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        key, sep, value = line.partition(":")
        if not sep:
            errors.append(f"{path.name}: front-matter line without a key: {line.strip()[:40]!r}")
            continue
        seen.add(key.strip())
        v = value.strip()
        quoted = len(v) >= 2 and v[0] in "\"'" and v[-1] == v[0]
        if ": " in v and not quoted:
            errors.append(f"{path.name}: {key.strip()}: value contains ': ' and is not quoted — "
                          "YAML reads it as a nested mapping")
    errors += [f"{path.name}: missing '{k}' in front matter" for k in REQUIRED if k not in seen]
    try:
        import yaml
    except ImportError:
        return errors
    try:
        yaml.safe_load(body)
    except Exception as error:  # noqa: BLE001 - any parse failure is the failure we report
        errors.append(f"{path.name}: front matter does not parse: {str(error).splitlines()[0]}")
    return errors


def main() -> int:
    if not AGENTS.is_dir():
        print(f"no {AGENTS}", file=sys.stderr)
        return 0
    errors = [e for f in sorted(AGENTS.glob("*.md")) for e in check(f)]
    for e in errors:
        print(e, file=sys.stderr)
    if not errors:
        print(f"agent front matter ok ({len(list(AGENTS.glob('*.md')))} file(s))")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
