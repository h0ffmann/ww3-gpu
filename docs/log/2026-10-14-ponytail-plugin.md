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

## Update, 2026-10-14

- (v) Installed at user scope as well (`claude plugin install ponytail@ponytail --scope user`), as
  ai-memory's `setup` wires every project. Its hooks need `node`, which is on the
  non-interactive PATH (`/usr/bin/node`, v18.19.1). Outside this repository the default stays
  `full`; here the project's `PONYTAIL_DEFAULT_MODE=off` still applies.
- ⚠ Open from the security review of this branch: the `ref: v5.2.0` pin is not in force on this
  workstation. `~/.claude/plugins/known_marketplaces.json` already held a `ponytail` marketplace
  without a ref, from the earlier install for another repository, and its checkout follows
  `main`. Today `main` and `v5.2.0` are the same commit, `2961b52`. Removing that entry and adding
  it again with the ref would also touch the other repository's install, so it is the owner's
  call. After each update, compare `gitCommitSha` in `installed_plugins.json` with `2961b52`.
- (v) The same review read the three hooks: with `off`, SessionStart clears the mode and exits
  before the code map and the statusline code, SubagentStart exits with no mode flag, and
  UserPromptSubmit acts only on a prompt that starts with `/ponytail`. None of them uses the
  network.
