---
description: Bump every submodule (nix-config, WW3, bend-lang) to its latest, summarise what changed upstream, re-check Bend's F64 status, and open a PR.
argument-hint: "[nix-config|WW3|bend-lang ...]  (default: all three)"
---

Bump the submodules named in `$ARGUMENTS` (all three when empty) and open one pull request.

**Audience.** The PR body is read by scientists (see `CONTRIBUTING.md`, "Who this repository is
for"): every claim carries `(v)` with the commit, `file:line` or command that checked it, or `⚠`.
Give the command that reproduces each count.

| Submodule | Fork (origin) | Upstream | Branch | Notes |
|---|---|---|---|---|
| `nix-config` | h0ffmann/nix-config | none | `main` | sparse: only `labs/pratico` is checked out |
| `WW3` | h0ffmann/WW3 | NOAA-EMC/WW3 | `develop` | fork may carry its own commits |
| `bend-lang` | h0ffmann/bend | HigherOrderCO/Bend | `main` | fork also carries branch `f64` (PR #795's patch), never the pin |

## 1. Sync and bump

On a machine with push access to the forks (the user's own; a cloud session cannot push to them):

```bash
just sub-sync          # = submodule-update, src-sync, bend-sync: forks fast-forwarded and pushed, pins staged
```

Or one at a time: `just submodule-update`, `just src-sync`, `just bend-sync`. If a sync stops
with "fork has diverged", the fork has commits of its own: report them and stop; do not pass
`--merge` without the user's word.

Without push access, move the pins to the upstream tips in the index and say in the PR that
the forks must be synced (GitHub "Sync fork", or `just sub-sync`) **before merge**, because a
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
  `w3srcemd`, the switches in `switches/`, or the build (`CMakeLists.txt`, `cmake/`), since
  those move the Kokkos parity harness.
- `bend-lang`: `git describe --tags old` and `new`, then read `CHANGELOG.md` for every release in
  between. Lead with **Breaking** items, then what touches the tryout plan
  (`docs/BEND_TRYOUT_202609.md`): number types, `F32` ops and text, `Array` (sharing, atomics),
  `File` IO, CUDA/Metal lanes, and the `!` GPU path. Note any claim in the tryout doc the
  release makes false.

## 3. Bend F64 (every run; the user decides go/no-go on it)

At the new `bend-lang` pin, check and quote:

```bash
grep -n -i 'f64' README.md WONTFIX.txt        # README "Numbers are ..." line; WONTFIX entry (#1120)
grep -c F64 bend2/base.bend                    # 0 means no F64 type in Base
git log --oneline -i --grep='f64\|float64' old..new
git merge-tree --write-tree --merge-base 492ea6b^ new 492ea6b   # does the fork's f64 patch still apply?
```

State plainly in the PR and in the reply: **F64 supported or not**, with the line that says so,
which section of `WONTFIX.txt` it sits in (SOON, DESIGN, ...), and how many conflict hunks the
`f64` patch has against the new pin. If F64 has landed, say so first and point at the commit.

## 4. Pull request

Branch from the default branch, commit the pins (and `.gitmodules` if it changed) with a message
naming each `path old -> new`, update `docs/BEND_TRYOUT_202609.md` only where the new pin made a
`(v)` claim false (dated note, not a rewrite), and open the PR with the summaries from steps 2-3.
CI does not fetch `WW3` or `bend-lang`, so a green CI says nothing about them: say what was and
was not built.
