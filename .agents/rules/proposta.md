---
trigger: glob
globs: "pubs/proposal/**"
---

# Proposta de Projeto de Graduação (pubs/proposal)

Antigravity counterpart of the Claude Code setup in `.claude/` (the reviewer agent and the two
hooks have no Antigravity equivalent, so this rule carries them). CONTRIBUTING.md is the source of
truth; if this file and it disagree, CONTRIBUTING.md wins.

- `pubs/proposal/pt/` is the reference text, `en/` its mirror. Change both by hand, then re-stamp
  `pubs/proposal/.translation-cache.json` so `just translate` does not overwrite the Portuguese.
- The reader is a DEL/UFRJ committee of electronics and computer engineers with no oceanography
  background: gloss a wave term in a few words the first time it appears; do not gloss MPI or GPU.
- Every verifiable claim carries a citation from `refs.bib`. To find one, use the `find-sources`
  skill; never cite from memory or from a search snippet alone.
- Before finishing: run `python3 scripts/proposal_lint.py` and fix its errors. Then review the
  text as a separate, read-only pass in the persona and format of
  `.claude/agents/revisor-proposta.md`, save the parecer to a file, deal with its blockers, review
  again, and record the final parecer with `python3 scripts/proposal_review_gate.py --record
  <parecer.md>`. CI (`.github/workflows/proposal-review.yml`) fails the pull request otherwise.
- Prose: `humanizar` for Portuguese, `humanizer` for English.
