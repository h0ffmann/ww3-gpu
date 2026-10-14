# ponytail enabled as a Claude Code plugin, off by default

2026-10-14, the owner with an agent. AGENTS.md, "Skills and agents".

## Question

The owner asked to add ponytail as a plugin and said it was already installed on the workstation.
Is it, and can it be enabled here without its rules reaching every session?

## What was done

On the owner's workstation, branch `claude/ponytail-plugin` from `main` at `4c8c5c4`:

```bash
claude plugin list                     # ponytail@ponytail 5.2.0, scope project, disabled here
python3 -c "import json; print(json.load(open('$HOME/.claude/plugins/installed_plugins.json'))['plugins']['ponytail@ponytail'])"
gh api repos/DietrichGebert/ponytail/tags --jq '.[0].name+" "+.[0].commit.sha'
claude plugin install ponytail@ponytail --scope project
# the SessionStart hook, against a throwaway config dir
P=~/.claude/plugins/cache/ponytail/ponytail/5.2.0
echo '{}' | CLAUDE_CONFIG_DIR=$(mktemp -d) CLAUDE_PLUGIN_ROOT=$P PONYTAIL_DEFAULT_MODE=off  node $P/hooks/ponytail-activate.js
echo '{}' | CLAUDE_CONFIG_DIR=$(mktemp -d) CLAUDE_PLUGIN_ROOT=$P PONYTAIL_DEFAULT_MODE=full node $P/hooks/ponytail-activate.js
```

## Result

- (v) It was installed, but for another repository: scope project, `projectPath`
  `marola-research`, version 5.2.0 at `2961b52`, which is the tag `v5.2.0` (released 2026-10-13).
  In this repository it was listed as disabled.
- (v) The plugin is MIT, "Copyright (c) 2026 DietrichGebert". Its Claude Code hooks run `node` on
  SessionStart, SubagentStart and UserPromptSubmit. Its skills (`ponytail`, `-audit`, `-review`,
  `-debt`, `-gain`, `-help`) load namespaced as `ponytail:<name>`, beside the vendored copies.
- (v) Its default mode is `full` (`hooks/ponytail-config.js:16`). With `PONYTAIL_DEFAULT_MODE=off`
  the SessionStart hook printed nothing; with `full` it printed 3534 characters starting "PONYTAIL
  MODE ACTIVE — level: full", the rules AGENTS.md's "Translate, don't improve" would be fighting.
- (v) After the install, `claude plugin list` shows it enabled at project scope, 5.2.0.

## What it changes

`.claude/settings.json` enables `ponytail@ponytail`, pins its marketplace to `v5.2.0` and sets
`PONYTAIL_DEFAULT_MODE=off`. AGENTS.md says why. ⚠ Not checked: whether `/ponytail` still turns
the mode on for one session with the default off, and whether the vendored and plugin skills both
trigger on the same request; to look at after a restart.
