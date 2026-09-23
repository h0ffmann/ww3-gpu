#!/usr/bin/env bash
# uprd_title — shared title-capping helper for scripts/uprd.sh and .github/workflows/pr-body.yml.
cap_title() {
  python3 - "$1" <<'PY'
import sys

s = sys.argv[1]
cap = 70
if len(s) <= cap:
    print(s)
    sys.exit(0)


def try_cut(sep):
    if sep in s:
        candidate = s.split(sep, 1)[0]
        if candidate.strip() and len(candidate) <= cap:
            return candidate
    return None


cut = None
for sep in (" — ", ". ", "; ", ", "):
    cut = try_cut(sep)
    if cut:
        break
if not cut:
    truncated = s[: cap - 1]
    if " " in truncated:
        truncated = truncated.rsplit(" ", 1)[0]
    cut = truncated.rstrip(".,;: ") + "…"

print(cut)
print(f"uprd: title cut from {len(s)} to {len(cut)} chars — check it reads well, retitle if not", file=sys.stderr)
PY
}
