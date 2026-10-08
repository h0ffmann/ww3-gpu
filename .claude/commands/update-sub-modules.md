---
description: Bump every submodule (nix-config, WW3) to its latest, summarise what changed upstream, and open a PR.
argument-hint: "[nix-config|WW3 ...]  (default: both)"
---

Bump the submodules named in `$ARGUMENTS` (all when empty) and open one pull request.

**Audience.** The PR body is read by scientists (see `CONTRIBUTING.md`, "Who this repository is
for"): every claim carries `(v)` with the commit, `file:line` or command that checked it, or `⚠`.
Give the command that reproduces each count.

| Submodule | Fork (origin) | Upstream | Branch | Notes |
|---|---|---|---|---|
| `nix-config` | h0ffmann/nix-config | none | `main` | sparse: only `labs/pratico` is checked out |
| `WW3` | h0ffmann/WW3 | NOAA-EMC/WW3 | `develop` | fork may carry its own commits |

## 1. Sync and bump

On a machine with push access to the forks (the user's own; a cloud session cannot push to them):

```bash
just sub-sync          # = submodule-update, src-sync: WW3 fork fast-forwarded and pushed, pins staged
```

If a sync stops with "fork has diverged", the fork has commits of its own: report them and stop;
do not pass `--merge` without the user's word.

Without push access, move the pins to the upstream tips in the index and say in the PR that the
fork must be synced (GitHub "Sync fork", or `just src-sync`) **before merge**, because a
`--recurse-submodules` clone fetches the pin from the fork:

```bash
git update-index --cacheinfo 160000,<sha>,<path>   # per submodule; nothing is checked out
git diff --cached --submodule=short
```

## 2. What is new (per submodule)

For each moved pin, with `old..new` from `git diff --cached --submodule=short`:

- `git log --format='%h %ad %s' --date=short old..new` and `git rev-list --count old..new`.
- `nix-config`: does the range touch `labs/pratico`? (`git diff --stat old new -- labs/pratico`).
  If not, the toolchain this repo builds with is unchanged; say so.
- `WW3`: list each upstream PR in the range; flag any touching `model/src/w3snl1md.F90`,
  `w3srcemd`, other source terms, the switches in `switches/`, or the build (`CMakeLists.txt`,
  `cmake/`), since those move the Kokkos parity harness and its fixtures.

## 3. Pull request

Branch from the default branch, commit the pins (and `.gitmodules` if it changed) with a message
naming each `path old -> new`, and open the PR with the summaries from step 2. CI does not fetch
`WW3`, so a green CI says nothing about it: say what was and was not built.
