#!/usr/bin/env python3
"""Every .claude/agents/*.md and .claude/skills/*/SKILL.md must carry parseable front matter.

An unquoted YAML value containing ": " is read as a nested mapping, the file fails to load and the
agent silently does not exist — which is how revisor-proposta was broken on 2026-09-16 by
"Somente leitura: aponta, não corrige." in its description. Checked without PyYAML (not every
environment has it) and with it when available.
"""
import pathlib
import sys

CLAUDE = pathlib.Path(__file__).resolve().parents[1]
DEFINITIONS = sorted(CLAUDE.glob("agents/*.md")) + sorted(CLAUDE.glob("skills/*/SKILL.md"))
REQUIRED = ("name", "description")


def label(path: pathlib.Path) -> str:
    return path.name if path.parent.name == "agents" else f"{path.parent.name}/{path.name}"


def check(path: pathlib.Path) -> list:
    parts = path.read_text(encoding="utf-8").split("---")
    if len(parts) < 3:
        return [f"{label(path)}: no YAML front matter"]
    body, errors, seen = parts[1], [], set()
    for line in body.strip().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line[0].isspace():  # continuation of a block scalar (| or >) or a nested mapping
            continue
        key, sep, value = line.partition(":")
        if not sep:
            errors.append(f"{label(path)}: front-matter line without a key: {line.strip()[:40]!r}")
            continue
        seen.add(key.strip())
        v = value.strip()
        quoted = len(v) >= 2 and v[0] in "\"'" and v[-1] == v[0]
        block = v[:1] in ("|", ">")
        if ": " in v and not quoted and not block:
            errors.append(f"{label(path)}: {key.strip()}: value contains ': ' and is not quoted — "
                          "YAML reads it as a nested mapping")
    errors += [f"{label(path)}: missing '{k}' in front matter" for k in REQUIRED if k not in seen]
    try:
        import yaml
    except ImportError:
        return errors
    try:
        yaml.safe_load(body)
    except Exception as error:  # noqa: BLE001 - any parse failure is the failure we report
        errors.append(f"{label(path)}: front matter does not parse: {str(error).splitlines()[0]}")
    return errors


def main() -> int:
    if not DEFINITIONS:
        print(f"no agent or skill definitions under {CLAUDE}", file=sys.stderr)
        return 0
    errors = [e for f in DEFINITIONS for e in check(f)]
    for e in errors:
        print(e, file=sys.stderr)
    if not errors:
        print(f"agent and skill front matter ok ({len(DEFINITIONS)} file(s))")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
