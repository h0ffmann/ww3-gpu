# ADR-0005: The Claude project's settings are snapshotted in the repository, dated, except memory, connectors and secrets

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-09 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The settings of the Claude project "Wave Forecaster" that live on the Claude service: instructions, repositories, session defaults, the cloud environment, routines, memory, connectors. Not the Claude Code configuration already in `.claude/` and `AGENTS.md`, which is in git by construction |
| **Related** | [`.claude/project/`](../../.claude/project/README.md) (the snapshot); [lesson 19](../../course/19-two-agents-one-issue-tracker.md) (the A2A routine and the trap that its prompt is not in git); [`AGENTS.md`, "Messages between Claude projects"](../../AGENTS.md#messages-between-claude-projects); [ADR-0003](ADR-0003-repository-is-a-book.md) (the book, whose subject is the method these settings encode) |

## Context

The agents that work on this repository run in two layers. The first is Claude Code's own
configuration, and it is already in git: hooks and permissions in `.claude/settings.json`, the
skills, the `revisor-proposta` agent, the slash commands, `AGENTS.md` and `CLAUDE.md` `(v)`
`ls .claude/`, 2026-10-09. The second is the Claude project the owner works in, whose settings
are stored on the service and reach a session only through it: the project instructions, the
repositories attached, the model and effort of thread sessions, the cloud environment, the
routines, the project memory and the connectors.

The second layer changes behaviour as much as the first, and nothing records it. Lesson 19 found
this the hard way: the A2A routine's prompt is the receiver's whole program, it lives on the
service, and the lesson's quoted copy goes stale silently when the prompt is edited there
`(v)` [lesson 19](../../course/19-two-agents-one-issue-tracker.md), "The receiver is a routine".
A reader of the book who wants to reproduce the setup cannot see that layer, and neither can a
reviewer of a PR that depends on it. The owner asked on 2026-10-09 whether all of it could be
kept in the repository for reproducibility.

What the project's sessions can read back, checked on 2026-10-09 `(v)` from a thread session and, for `get_project_settings`, from the coordinator session:

| Setting | Readable from a session | Exportable to git |
|---|---|---|
| Name, visibility, repositories | yes, in the session context | yes |
| Project instructions, default model, routine and task settings | yes, `get_project_settings` (coordinator session only); instructions are empty, no effort default is set | yes |
| Model, effort, permission mode of a session | yes, `get_session`, for that session | yes |
| Cloud environment | name, kind and network policy, `list_environments` | yes; its secrets and variables no |
| Routines | name, schedule, stored prompt, `list_triggers` / `get_trigger` | yes, prompt verbatim |
| Project memory | only through the coordinator's memory tools; on 2026-10-09 it was off in the owner's account settings, so empty | in part: content a person reviews, never wholesale |
| Connectors (Gmail, Calendar, Drive, Docs, GitHub) | names only; they belong to the owner's account | no: each is an OAuth sign-in |
| Chats, artifacts, project files | readable, and not settings | no |

The service also returns identifiers that must not be committed: the creator's account id on
each routine and the owner's e-mail address in the session context.

## Decision

**The project's settings are kept in `.claude/project/` as a dated snapshot that a person can
restore from by hand. The service stays the truth; the snapshot is its copy, read back with the
service's own tools and committed with the date it was read.**

1. `project.json` holds what the table marks exportable: name, visibility, instructions,
   repositories, default model, memory and routine settings, what a thread session runs with, environment name and network
   policy, and each routine's name, trigger id, schedule and the path of its prompt file. It
   carries `read_on` and the tools it was read with.
2. Each routine's prompt is a text file under `routines/`, verbatim. Lesson 19 keeps quoting it,
   and `tests/test_claude_project.py` fails if the lesson and the file differ, so the two copies in
   git cannot drift from each other.
3. Only the project's own routines go in. Routines on the same account that belong to another
   project (Marola Agent's inbox, its PR check-ins) stay out: they are that project's settings.
4. Memory, connectors, secrets, chats, artifacts and project files are listed under
   `not_in_this_snapshot` and stay out. A lesson learned in memory that should survive is promoted
   by a person into `AGENTS.md`, a skill or a doc, where review and the evidence rule apply.
5. No e-mail address, account id or credential is committed; the test above checks the folder for
   the first two patterns.
6. A change to the project on the service is followed by a PR that updates the snapshot and its
   `read_on`. No CI job can check the snapshot against the service, since no command outside a
   Claude session can read the project; this is a convention, as the lesson's quoted prompt already
   was.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep nothing; the service is enough | A copy in a lesson goes stale with no signal (the routine's schedule was changed on the service the night it was created); a reader of the book cannot rebuild the setup. |
| Export everything, memory included, on a schedule | Memory is written by sessions and holds notes, partial findings and the owner's preferences; committing it unreviewed bypasses the evidence rule and risks personal data. The memory tools are also not available to every session. |
| Generate the snapshot with a script in CI | There is no API the repository can call: the project is reachable only from inside a Claude session, through its tools. A routine could write it, but a routine that pushes to the repository needs the owner's approval for each push anyway. |
| Put the snapshot in `docs/` | It is configuration for agents, next to the rest of it in `.claude/`; `docs/` holds what is argued, this holds what is set. |
| Make the snapshot authoritative and apply it to the service | The service has no import; restoring is a person following `.claude/project/README.md`. |

## Consequences

- The book's chapter on two agents (lesson 19) and its routine now point at one file in git, and a
  test ties them.
- A PR that edits a routine prompt shows the edit as a diff, which a reviewer can read; the edit on
  the service is still made by a person or a session of this project.
- The snapshot can be stale between a change on the service and the PR that records it; `read_on`
  says how old it is.
- The snapshot is complete only while memory stays off; if the owner turns it on, what it holds is
  read and triaged by the rule in point 4.

## Revisit when

- The service gains an export or import for projects, or an API a CI job can call: then the
  snapshot is generated and checked, and this record is superseded.
- The project gains instructions, a second routine or a second environment.
- Memory turns out to hold settings the work depends on that no file in git carries.
