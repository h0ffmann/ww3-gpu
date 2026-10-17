# The compound loop's plans and learnings

Compound Engineering's skills write here (`docs_root: docs/compound` in
[`.compound-engineering/config.yaml`](../../.compound-engineering/config.yaml); the decision is
[ADR-0016](../ADRs/ADR-0016-speed-first-regtests-postponed-one-person-loop.md), the rules are
[`AGENTS.md`, "Engineering work: the compound loop"](../../AGENTS.md#engineering-work-the-compound-loop)).

- `plans/`: one plan per medium or large change, written by `/ce-plan` (and `/ce-brainstorm` for
  the requirements of a large one). A plan names the routine or cost it attacks, the profile row
  that ranks it, the static parity argument it will make and the regtest it leaves for the
  validation phase. The plan is read-only once `/ce-work` starts; git says whether it shipped.
- `solutions/`: one learning per non-obvious, durable lesson, written by `/ce-compound`, read by
  the next `/ce-plan` and by `/ce-code-review`. A dated record of what was tried still goes to
  [`docs/log/`](../log/README.md); a WW3 porting trap also goes into the `ww3-port` skill.

Both follow the evidence rule of `CONTRIBUTING.md`: `(v)` with what was checked, `⚠` when it was
not, and every number with the command that reproduces it.
