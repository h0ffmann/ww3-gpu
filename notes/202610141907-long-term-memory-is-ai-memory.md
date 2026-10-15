---
id: "202610141907"
title: "The long-term memory is ai-memory"
type: permanent
status: partial
source: "github.com/akitaonrails/ai-memory README.md at 705aba7 (v3.0.0), MIT, Fabio Akita; docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md"
tags: [memory, agents]
created: 2026-10-14
---

# The long-term memory is ai-memory

ai-memory is "Long-term memory for AI coding agents" (its README at `705aba7`) `(v)`: hooks
capture each session, a local server writes them as Markdown pages, and the next session, in any
agent, starts with a handoff. ww3-gpu adopts it in ADR-0009. The status is *partial*: the ADR is
proposed and no session here has been captured yet ⚠.

The notes stay the record a reader sees; ai-memory holds what a session tried. Both serve
[[202610141902-the-repository-is-a-book]].
