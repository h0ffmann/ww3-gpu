# The Claude project, as configured

A snapshot of the settings of the Claude project "Wave Forecaster" that live on the Claude service
and not in git, so the project can be rebuilt by hand and a change to it shows up in a diff.
[ADR-0005](../../docs/ADRs/ADR-0005-claude-project-settings-in-repo.md) records why and what is
left out.

| File | What it holds | Read with |
|---|---|---|
| [`project.json`](project.json) | name, visibility, instructions, repositories, the model, effort and permission mode of thread sessions, the cloud environment, the routines | `get_session` (this session), `list_environments`, `list_triggers` |
| [`routines/a2a-wave-forecaster.txt`](routines/a2a-wave-forecaster.txt) | the stored prompt of the A2A routine, verbatim | `get_trigger trig_01JZ1SZK9ZimYALDctnzBRJW` |

Everything the repository already holds is not repeated here: the hooks and permissions are
[`../settings.json`](../settings.json), the skills [`../skills/`](../skills/), the reviewer
[`../agents/`](../agents/), the slash commands [`../commands/`](../commands/), and the rules
`AGENTS.md` and `CLAUDE.md`. A thread session reads those from its clone, so they need no copy.

## Evidence

All values were read on 2026-10-09 from a thread session of the project `(v)`. Two are `⚠`:

- `thread_sessions` is what `get_session` reported for the session that wrote this file; that it is
  the project's default for every thread, and not this one session's, was not checked.
- `instructions: null` means the session context carried no project instructions; the project
  settings page was not opened.

## Restoring the project

1. Create a private project named as in `project.json` and attach its repositories.
2. Paste `instructions` into the project instructions, if it is not `null`.
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
