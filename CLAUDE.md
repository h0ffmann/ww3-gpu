@AGENTS.md

Read `AGENTS.md` before doing anything here: it holds the repo invariants (write for scientists
with `(v)`/`⚠` evidence, WW3 is read-only, translate don't improve, no timing without parity, the
proposal goes through its reviewer, `Tested:`/`Cost:` trailers), where a change belongs, the gates
CI runs, and the skills and agents. The line above imports it for Claude Code; a tool that reads
this file as plain text must open `AGENTS.md` itself. `CONTRIBUTING.md` is the longer form.

## Claude Code-specific additions

- **Hooks** (`.claude/settings.json`): a `PostToolUse` hook on Write/Edit reminds you to run the
  `revisor-proposta` subagent when a file under `pubs/proposal/` changes, and a `Stop` hook refuses
  to end a session that changed the proposal without a recorded review
  (`python3 scripts/proposal_review_gate.py --record <parecer.md>`). Both have `--self-test`.
- **The reviewer is a subagent**: run it with the Agent tool, `subagent_type: "revisor-proposta"`,
  on the files you changed; `just proposal-review` does the same from a terminal in plan mode. It
  reports; you fix; it does not rewrite.
- **Skills are invoked by name** (`/eli5 <topic>`, `/ww4-status`, `figure`, `release`, `humanizar`,
  `humanizer`, `ponytail*`); each one's `SKILL.md` restates the audience rule for its job, and the
  vendored ones are edited through their `.patch`, never in the copy.
- **Attribution**: the harness appends `Co-Authored-By` and a `Claude-Session` link to commits and a
  "Generated with" line to PR bodies. They stay in the trailer block and the PR footer; never put a
  model name or session link in code, comments, PR titles or documents.
- **On compaction, preserve**: which files were modified, which gates were run and their result,
  and the `(v)`/`⚠` status of any number you wrote. Everything else can go.
- **Start in this repository's root** so `.claude/settings.json`, the hooks and the skills load;
  `WW3/`, `nix-config/` and `bend-lang/` are submodules you read, not repositories you work in.
