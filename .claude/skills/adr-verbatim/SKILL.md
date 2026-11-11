---
name: adr-verbatim
description: 'Record concise ADRs that preserve the user''s decisions without conversational noise. Use for "adr-verbatim", "new ADR", "document that as an ADR", "record this decision", or supplied ADR wording.'
---

**In this repository.** The readers of an ADR are scientists (PhD and independent researchers in
wave modelling and HPC; see `CONTRIBUTING.md`, "Who this repository is for"), so keep domain
terms, `file:line` references at the pinned commit and the `(v)`/`⚠` marks exactly as the owner
gave them. Every ADR is a section of the one document `docs/ADRs.md` (ADR-0022), never a file of
its own: append it at the end, take the next number, put `<a id="adr-NNNN"></a>` on the line
before its heading, write the heading as `## ADR-NNNN: <the decision>` (a colon, no dash:
`scripts/adrs.py` and the website read that line), follow it with the header table (at least
**Status** and **Date**), and add its row to the index table at the top. A decision that changes
an older one supersedes it: set the older one's Status to "Superseded by ADR-NNNN" and nothing
else in it. Run `just adrs` to check that the new record is read.

# ADR Verbatim

Preserve the user's meaning, not the conversation transcript. Record only the decision and context needed to understand it.

## Steps

1. Extract the decision the user stated or approved in the conversation. Ask only if the decision is unclear; do not require them to dictate polished wording.
2. Use the project's established ADR location, including a previously specified repository. Do not assume the current checkout owns the ADRs. Name the file by that directory's rule (check its README or AGENTS.md); if it has none, take the next number.
3. Write a short, clear ADR. Remove filler, emotional reactions, repetition, rhetorical questions, and instructions to the agent. Fix grammar and clarify references without changing the meaning, scope, conditions, or priorities.
4. Check every statement against the user's decision. Remove invented requirements and unapproved agent suggestions.
5. Show the saved path and full ADR in a code block.

## Format

Match the directory's filenames, such as `docs/adr/0042-short-slug.md` or `docs/adr/20261030-175900-short-slug.md`. Use the file's ID in the heading.

```markdown
# <ID> — short title

Status: accepted (<user>, YYYY-MM-DD)

<The decision in plain English.>
```

## Rules

- Prefer a short paragraph or list. Add sections only when they improve clarity.
- Do not add speculative rationale, alternatives, consequences, or implementation details.
- Use exact wording only when the user explicitly asks for an exact quote or verbatim passage.
- Newer decisions win. The user's latest decision overrides older ADRs, because they have more information and context now. Don't treat an older ADR as a blocker; record the new decision and name the ADR it supersedes.
- A changed decision needs a new ADR. Correct an existing ADR when the user explicitly requests it, without changing unrelated history.
