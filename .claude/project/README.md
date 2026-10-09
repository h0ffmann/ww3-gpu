# The Claude project, as configured

A snapshot of the settings of the Claude project "Wave Forecaster" that live on the Claude service
and not in git, so the project can be rebuilt by hand and a change to it shows up in a diff.
[ADR-0005](../../docs/ADRs/ADR-0005-claude-project-settings-in-repo.md) records why and what is
left out.

| File | What it holds | Read with |
|---|---|---|
| [`project.json`](project.json) | name, visibility, instructions, repositories, the default model, memory and routine settings, what a thread session runs with, the cloud environment, the routines | `get_project_settings`, `get_session`, `list_environments`, `list_triggers` |
| [`routines/a2a-wave-forecaster.txt`](routines/a2a-wave-forecaster.txt) | the stored prompt of the A2A routine, verbatim | `get_trigger trig_01JZ1SZK9ZimYALDctnzBRJW` |

Everything the repository already holds is not repeated here: the hooks and permissions are
[`../settings.json`](../settings.json), the skills [`../skills/`](../skills/), the reviewer
[`../agents/`](../agents/), the slash commands [`../commands/`](../commands/), and the rules
`AGENTS.md` and `CLAUDE.md`. A thread session reads those from its clone, so they need no copy.

## Evidence

All values were read on 2026-10-09 `(v)`: the `project` block with `get_project_settings`, which
only the project's coordinator session has, and the rest from a thread session. `project.model` is
the project's default; the project sets no effort default, so `thread_sessions.effort` is what the
service gave the session that wrote this file. The environment is the account's only one; the
project names no default. Memory is on for the project but off in the owner's account settings, so
there was nothing to read.

## Restoring the project

1. Create a private project named as in `project.json` and attach its repositories.
2. Set the default model, and paste `instructions` into the project instructions if it is not empty.
3. Create each routine with its `cron` and the text of its `prompt` file, inside the project so it
   wakes a session with this project's repositories (lesson 19 explains why a routine must belong
   to the project that answers).
4. Reconnect the connectors the work needs and re-enter any environment secret by hand; neither is
   recorded here.

## Keeping it current

The service is the truth and this folder is its dated copy, as lesson 19 says of the routine. When
a setting changes, read it back with the tool in the table and commit the new value with
`read_on` updated, in the same PR as anything that depends on it. `tests/test_claude_project.py`
checks that the JSON parses, that no e-mail address or account id slipped in, and that lesson 19
quotes the routine prompt exactly as this folder stores it. Nothing checks this folder against the
service itself: no command outside a Claude session can read the project.
