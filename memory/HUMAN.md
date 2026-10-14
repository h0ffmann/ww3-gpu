---
kind: rule
abstract: "Seed page and ground truth for this project's memory: who the human owner is, the standing orders they gave in their own words, and how every other memory ranks against the repository's files."
entities:
- h0ffmann
- ww3-gpu
- labeco/ufsc
- los/ufba
- ufrj/del
tags:
- seed
- ground-truth
- owner
- publish
pinned: true
tier: semantic
type: Note
generated:
  by: process:ai-memory/3.0.0
  at: 2026-10-14T23:10:12Z
---
# HUMAN: the owner and their standing orders

Seed page: the first page written on purpose, on 2026-10-14, at the owner's request. Every other page here ranks below it. It ranks below the repository's files: `AGENTS.md`, `CLAUDE.md`, `MEMORY.md`, the ADRs. If this page and those files disagree, the files win, and the owner should be told this page is stale.

## Who

- M. Hoffmann, GitHub `h0ffmann`, owner of `h0ffmann/ww3-gpu`. Pronouns not stated: use they/them.
- The project is a graduation project at UFRJ (Escola Politécnica, DEL), co-advised at LabECO/UFSC. It moves WAVEWATCH III kernels to GPUs (C++/Kokkos) without changing the answer. The repository is also a book on agentic coding and research (ADR-0003).
- Two Claude projects talk through `[A2A]` issues labelled `para:marola` / `para:wave-forecaster`: Marola Agent (`marola-dev/marola`) and Wave Forecaster Agent (this repo). See AGENTS.md, "Messages between Claude projects".

## Standing orders, in the owner's words (2026-10-14, AGENTS.md)

- "never run --hoff --gpu without supervision, only --hoff --cpu must be regtested pair to pair to existing ww3 src."
- "Never write any new Kokkos code under personal Max account of the owner, always under UFSC team on Claude." Check `~/.claude.json` `oauthAccount.organizationType` before editing under `kokkos/`: `claude_team` is allowed, `claude_max` means stop.
- Every ported routine has a port report, `kokkos/port_report/<ROUTINE>_KOKKOS.md`, and `regtested` and `googletest` columns in `kokkos/ports.tsv`.

## How they want the work done

- Readers are scientists. Every claim carries `(v)` (checked) or `⚠` (not checked), and every number comes with the command that reproduces it.
- WW3 is read-only. Translate, don't improve. Never widen a tolerance. No timing without L1 parity.
- Commits carry `Tested:`. Never record a cost.
- No em dash in prose.

## What memory is for

ai-memory is a lead, not evidence (ADR-0009, MEMORY.md). A fact that matters goes into a repository file with its `(v)`/`⚠`. Pages here hold workstation-only knowledge and how the owner likes to work.