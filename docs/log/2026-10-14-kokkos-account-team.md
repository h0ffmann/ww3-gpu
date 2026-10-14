# First login to the Claude Team account, and the Kokkos account guard

2026-10-14, the owner with an agent. AGENTS.md, "Which account writes Kokkos code (hard rule)".

## Question

The hard rule says Kokkos code is written only from the team account, never from the owner's
personal Max account. Does `.claude/hooks/kokkos-account.sh` deny a personal-account edit, and
which `organizationType` does the team account show?

## What was done

On the personal account, before `/login`:

```bash
python3 -c "import json,os;print(json.load(open(os.path.expanduser('~/.claude.json')))['oauthAccount']['organizationType'])"
# claude_max
```

The owner ran `/login` and chose the team organisation; the same command then printed
`claude_team`. The hook was run on `main` at `31f122e`, on the owner's workstation:

```bash
bash .claude/hooks/kokkos-account.sh --self-test
d=$(mktemp -d); printf '{"oauthAccount":{"organizationType":"claude_max"}}' > "$d/.claude.json"
echo '{"tool_input":{"file_path":"kokkos/src/a.cpp"}}' | HOME="$d" bash .claude/hooks/kokkos-account.sh
echo '{"tool_input":{"file_path":"kokkos/src/a.cpp"}}' | bash .claude/hooks/kokkos-account.sh
```

## Result

- (v) The self-test passed (`kokkos-account self-test ok`).
- (v) With `claude_max` the hook returned `"permissionDecision": "deny"` for `kokkos/src/a.cpp`.
- (v) With the `claude_team` login it printed nothing, so the edit is allowed.
- (v) The team account's `organizationType` is `claude_team`: a Claude Team plan, not
  Enterprise as the rule's wording says. The organisation is the one LOS/UFBA shares with
  LabECO/UFSC members by invitation.

## What it changes

AGENTS.md records `claude_team` as the account that writes Kokkos code. The hook still allows
any value other than `claude_max` silently; making it allow only `claude_team` and remind on
anything else is a separate change to the hook (⚠ not done here).
