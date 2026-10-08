# Architecture Decision Records

A decision record states one choice the lab has made, the context that forced it, the
alternatives weighed and what follows from it. It is shorter than a plan and it does not schedule
work: a WFIP ([`../WFIPs/`](../WFIPs/README.md)) designs a change and carries its definition of
done, while an ADR records a decision that several WFIPs and plans then follow.

Each ADR is `ADR-NNNN-<kebab-slug>.md`, numbered in order, with a header table (Status, Date,
Deciders, Written by, Scope, Related) and the sections *Context*, *Decision*, *Alternatives
considered*, *Consequences* and *Revisit when*. Statuses: Proposed → Accepted, or Rejected, or
Superseded by ADR-NNNN. An accepted ADR is not edited; a new one supersedes it. The evidence rule of
`CONTRIBUTING.md` applies: `(v)` with what was checked, `⚠` when it was not.

| ADR | Decision | Status | Date |
|---|---|---|---|
| [ADR-0001](ADR-0001-proof-language.md) | No proof language for the GPU port; bit-for-bit claims are settled by exhaustive sweeps, GIMPLE diffs and SMT `QF_FP` queries (Kokkos, Triton and WeatherNext 3 scenarios) | Proposed | 2026-10-08 |
