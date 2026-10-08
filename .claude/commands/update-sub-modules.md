---
description: Bump every submodule (nix-config, WW3, WW4) to its latest, summarise what changed upstream, and open a PR.
argument-hint: "[nix-config|WW3|WW4 ...]  (default: all three)"
---

Bump the submodules named in `$ARGUMENTS` (all when empty) and open one pull request.

**Audience.** The PR body is read by scientists (see `CONTRIBUTING.md`, "Who this repository is
for"): every claim carries `(v)` with the commit, `file:line` or command that checked it, or `⚠`.
Give the command that reproduces each count.

| Submodule | Fork (origin) | Upstream | Branch | Notes |
|---|---|---|---|---|
| `nix-config` | h0ffmann/nix-config | none | `main` | sparse: only `labs/pratico` is checked out |
| `WW3` | h0ffmann/WW3 | NOAA-EMC/WW3 | `develop` | fork may carry its own commits |
| `WW4` | h0ffmann/WW4 | NOAA-EMC/WW4 | `develop` | read-only reference; `docs/ww4-status.json` records the last snapshot |

## 1. Sync and bump

On a machine with push access to the forks (the user's own; a cloud session cannot push to them):

```bash
just sub-sync          # = submodule-update, src-sync, ww4-sync: forks fast-forwarded and pushed, pins staged
```

If a sync stops with "fork has diverged", the fork has commits of its own: report them and stop;
do not pass `--merge` without the user's word.

Without push access, move the pins to the upstream tips in the index and say in the PR that the
forks must be synced (GitHub "Sync fork", or `just src-sync` / `just ww4-sync`) **before merge**, because a
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
- `WW4`: run `just ww4-status` (the `/ww4-status` skill) and quote what it reports; if the new pin
  is past `docs/ww4-status.json`'s `develop.sha`, say so and suggest `/ww4-status` to refresh
  lesson 14.

## 3. Pull request

Branch from the default branch, commit the pins (and `.gitmodules` if it changed) with a message
naming each `path old -> new`, and open the PR with the summaries from step 2. CI does not fetch
`WW3` or `WW4`, so a green CI says nothing about it: say what was and was not built.
