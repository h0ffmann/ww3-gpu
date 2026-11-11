@../AGENTS.md

## Oh My Pi (omp)-specific additions

omp loads this file in place of the root `AGENTS.md` and `CLAUDE.md` (the native `.omp/` file
shadows both at the repository root), and the line above imports `AGENTS.md` whole, so the
rulebook is the same one Claude Code reads. `CONTRIBUTING.md`, "Working with Oh My Pi", has the
evidence for each line below.

- **Start in this repository's root.** omp reads `.omp/extensions`, `.claude/commands` and
  `.mcp.json` from the working directory only, so elsewhere the hooks and commands are missing.
  `WW3/`, `WW3-metocean-7.14/`, `chaceon/`, `WW4/` and `picasso/` are submodules you read, not repositories you work in.
- **Skills and slash commands** load from `.claude/skills` and `.claude/commands` as Claude Code
  writes them (`/port-next`, `/adrs`, `/drawings-refresh`, `/graph-check`, `/graph-view`,
  `/update-sub-modules`); edit a vendored skill through its `.patch`, never the copy. A skill step
  that names a Claude Code tool (the Agent tool, `ToolSearch`) is done with omp's equivalent
  (the `task` tool) or by hand.
- **Subagents.** `revisor-proposta` and `security-reviewer` are in `.omp/agents/`, generated from
  `.claude/agents/` by `python3 scripts/omp_sync.py --write`: run them with the `task` tool and
  `agent: "revisor-proposta"`. Edit `.claude/agents/<name>.md`, never the copy; CI runs
  `omp_sync.py --check`.
- **Hooks.** `.omp/extensions/ww3-gpu-hooks.ts` runs the Claude Code hooks' own scripts: the
  reminder after a write or edit under `pubs/proposal/`, and the stop gate that refuses to end the
  session while the proposal changed without a recorded review
  (`python3 scripts/proposal_review_gate.py --record <parecer.md>`). The cloud `SessionStart`
  setup is not carried over: in a fresh container, run `scripts/agent_env.sh` yourself.
- **Subagents do not get this file.** omp drops every context file named `AGENTS.md` from a task
  subagent and keeps `.omp/RULES.md`, which `omp_sync.py` writes from the repo invariants and the
  GPU rule above. Brief a subagent with the section of `AGENTS.md` its task needs.
- **Claude Code plugins are not loaded**: ponytail's skills are vendored and load, its mode does
  not; Context7 and ai-memory are absent. omp's own memory is local to the machine; the files of
  `MEMORY.md` stay the record. The Consensus MCP server loads from `.mcp.json`.
- **Attribution.** No model name or session link in code, comments, PR titles or documents; a
  commit carries `Tested:` and never `Cost:`. omp's `commit` command does not know these rules:
  read its message before you keep it.
