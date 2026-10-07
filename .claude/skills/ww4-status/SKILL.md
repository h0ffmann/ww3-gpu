---
name: ww4-status
description: "Check NOAA's WAVEWATCH IV (WW4) for changes since the last recorded snapshot and update this repo's research to match. Use when the user types /ww4-status, asks what WW4 has done lately, whether WW4 ported a source term or solver, adopted Kokkos, changed its spectral grid or released, or asks to refresh lesson 14 or the proposal's State of WW4."
---

**Audience.** The result is read by scientists (see `CONTRIBUTING.md`, "Who this repository is
for"): every claim about WW4 carries `(v)` with what was checked, or `⚠`, the date it was checked,
and the command or URL that reproduces it. The proposal paragraph is the exception: it is read by
the DEL committee and goes through `revisor-proposta`.

# ww4-status

The last snapshot lives in `docs/ww4-status.json` (`checked` is its date). Issue #49 is the
worked example of a full update and PR #53 the matching repo change; copy their shape.

## 1. Diff what git can see (seconds, no token)

```bash
just ww4-status          # = python3 scripts/ww4_status.py; clone cached in $WW4_CACHE
```

It fetches NOAA-EMC/WW4 (`develop`, tags, `refs/pull/*/head`) and prints only what changed since
the snapshot: new commits on `develop`, `VERSION` and tags, test levels (L1..L4) present, the
default `spectral_space` in `templates/ww4_run_config.yaml`, files mentioning Kokkos, code-line
counts of every `src/ww4_core/source_terms/ww4_*` and `solver_*` `.cpp` (more than 60 lines is
flagged **IMPLEMENTED**: a placeholder is 7 or 29), and every PR whose head is new or moved, with
what it implements and whether it mentions Kokkos.

If it prints "No change in anything git can see", still do step 2's page checks (issues and
announcements move without commits). If those are quiet too, tell the user so in one line, with
the snapshot date, and stop: no issue, no PR.

## 2. Read what git cannot see

GitHub's list pages are blocked to fetchers (robots.txt) and the WW4 repo is outside this
repo's API scope, so read single pages:

- each PR the script listed: `https://github.com/NOAA-EMC/WW4/pull/<n>`: state (open, merged,
  closed), description, review comments, linked issues;
- issues those PRs name, and issue #43 (CPU–GPU architecture) whenever Kokkos moved;
- `https://github.com/NOAA-EMC/WW4/discussions/categories/announcements` (newest date);
- `https://github.com/NOAA-EMC/WW4/wiki` (last edit, any phase dates or release date);
- a web search for a new NCEP Office Note after ON 528 (doi:10.25923/0wyp-9f39), a WW4 workshop
  or a release announcement.

Open and closed issue counts come only from the repo page and are approximate; mark them `⚠`.

## 3. Decide what it means for ww3-gpu

Check each, and say which ones changed:

1. **`ww4_NL1` implemented** (the DIA): read the body. Is it a port of WW3's `W3SNL1`? Does it
   have parity tests against WW3, and with what tolerance? Compare with `kokkos/PORT_STATUS.md`
   (`L1_test_snl1_dia`, bit-identical on three presets). Any other source term or solver
   implemented: name it and its WW3 counterpart.
2. **`ISourceTerm::calculate` signature** (`src/ww4_core/source_term.h`, `std::span<double>` on
   2026-10-07): a batch of points, a Kokkos `View` or a device memory space decides whether our
   kernel could be handed over without a copy.
3. **Kokkos**: in `CMakeLists.txt` or `externals/` means adopted; mentioned only in docs is not.
4. **Spectral grid defaults**: rerun the scratch formula in `kokkos/PORT_STATUS.md`
   ("Level-0 scratch at WW4's default grid") with the new NK/NTH/XFR and update that table.
5. **Tags, releases, `VERSION`**: against the January 2027 / summer 2027 dates in lesson 14.
6. **L3/L4 tests**: their arrival is when WW4 starts replacing WW3's regression matrix.

## 4. Record it

Branch per `CONTRIBUTING.md`, then:

1. Open an issue "WW4 status update: what changed upstream since <snapshot date>" in the shape of
   #49: short version, what changed (merged, open PRs, issues, elsewhere), timeline against
   ON 525, what this means for ww3-gpu, a Reproduce block, Sources. Refer to WW4 items as
   `NOAA-EMC/WW4#N` so they do not link to this repo's issues.
2. Update `course/14-ww4-and-the-future.md` (snapshot table with the new and previous dates, the
   "Since <date>" list, the timeline column), the date and commit count in `README.md` ("Two
   things worth knowing") and the snapshot date in `CONTRIBUTING.md` item 5.
3. If a fact in the proposal changed (`pubs/proposal/pt/05-justification.md` "Estado do WW4",
   mirrored in `en/`), edit pt and en by hand, bump `urldate` of `ww4repo` in `refs.bib`, re-stamp
   `.translation-cache.json`, run `just proposal-lint`, run the `revisor-proposta` subagent until
   it reports no blockers, then `just proposal-review-record <parecer.md>`. Change the proposal
   only for a fact that changed, never to refresh a date.
4. `just ww4-status --write` to move the snapshot, and commit `docs/ww4-status.json` in the same
   PR, so the next run diffs from here.
5. One PR, `Closes #<issue>`, commit trailers `Tested:` and `Cost:`.

## Running it on a schedule

The script is cheap and needs no token, so it can run as a recurring Claude routine (for example
weekly) whose prompt is "/ww4-status": a quiet week ends at step 1 and 2 with a one-line "no
change since <date>".
