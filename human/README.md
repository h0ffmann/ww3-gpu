# human/

Material a person added to a pull request (screenshots, notes), kept in the repository so the pull
request's body can point at a committed file instead of a GitHub attachment.

| File | What | Pull request |
|---|---|---|
| [`2026-10-14-account-check.png`](2026-10-14-account-check.png) | screenshot, 2026-10-14 21:52: the agent confirms the `claude_team` account before touching the Kokkos build | #116 |
| [`2026-10-14-hook-exception.png`](2026-10-14-hook-exception.png) | screenshot, 2026-10-14 22:47, from #98's body: the owner grants a one-session exception to the Kokkos account rule; the auto-mode classifier still refuses a subagent told to route Kokkos edits through Bash around `kokkos-account.sh`, and the agent restarts it without that instruction ([log](../docs/log/2026-10-14-kokkos-account-exception.md)) | WFIP-0003 implementation |
