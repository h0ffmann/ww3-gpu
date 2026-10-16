# Kokkos ST4 written under the personal account, on the owner's word

2026-10-15, the owner with an agent. AGENTS.md, "Which account writes Kokkos code (hard rule)";
issue #45, `docs/ST4_KOKKOS_PLAN_202610.md`.

## Question

The session that ports ST4 started on `claude_max`. May it write Kokkos code?

## What was done

(v) `~/.claude.json` after the owner's `/login`, 20:30: `organizationType` `claude_max`. The agent
stopped, as AGENTS.md asks. The owner asked for a PR removing the check; the auto-mode classifier
refused the agent's first step of that PR ("Self-Modification"), and nothing was changed. The owner
then answered "TEAM_PROTECTION=off, you can proceed with the Kokkos port I originally requested".

## Result

The port went on in that session, an exception for this session only, as on 2026-10-14
([`2026-10-14-kokkos-account-exception.md`](2026-10-14-kokkos-account-exception.md)). The rule in
AGENTS.md and its hook are unchanged.

## What it changes

Nothing in the rule. Whether to keep, relax or drop it is the owner's call, in its own PR.
