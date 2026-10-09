---
name: noaa-notices
description: "List NOAA announcements new since the last snapshot that touch this repository (NWS Public Information Statements and Service Change Notices, NCEP model changes, NOAA-EMC/WW3 tags and production branches) and route each to the file it breaks or dates. Use when the user types /noaa-notices, asks what NOAA announced lately, whether GFS, GFS-Wave or NOMADS changed, or whether example 02's get_gfs.sh is still safe."
---

**Audience.** The result is read by scientists (see `CONTRIBUTING.md`, "Who this repository is
for"): every claim about a notice carries `(v)` with the PDF it was read from and the date, or
`⚠` when only its title was seen. A notice title is not its content: read the PDF before saying
what changes.

# noaa-notices

The snapshot lives in `docs/noaa-notices.json` (`checked` is its date; `null` until the first run
from a machine that can reach weather.gov). Issue #85 is the design; #86 is the first case it
routes (GFS v17 and `get_gfs.sh`).

## 1. List what is new (seconds, no token)

```bash
just noaa-notices          # = python3 scripts/noaa_notices.py; --all shows what the filter dropped
```

It scrapes the PDF links of <https://www.weather.gov/notification/> (PNS, SCN) and
<https://www.nco.ncep.noaa.gov/pmb/changes/> (the SCN/TIN behind each implementation), keeps the
ones not in the snapshot, from this year or last, whose name or link text matches the keyword
filter (`RELEVANT` in the script: GFS, GEFS, wave, WW3, NWPS, GLWU, RTOFS, HAFS, NOMADS, GRIB,
marine, seas, Ocean Prediction Center, parallel, ciphers), and diffs NOAA-EMC/WW3's tags and
`production/*` branches through `git ls-remote`. A page with no notice link warns, and no link on any page exits non-zero: the
layout changed, so fix `LINK`, never record an empty run.

A cloud container may be refused weather.gov and nco.ncep.noaa.gov by its egress proxy (it was on
2026-10-09); then run it from a workstation, or read the two pages with a web fetch and say the
snapshot was not moved.

These feeds announce changes to services, not weather: a hurricane in progress shows up in the
National Hurricane Center's advisories (<https://www.nhc.noaa.gov/>), not here, unless NOAA
suspends or changes a product because of it.

If it prints "Nothing relevant since the snapshot", tell the user so in one line with the
snapshot date and stop: no issue, no PR.

## 2. Read each hit

Open every listed PDF (and, for a moved production branch, its commits) and note: what changes,
the implementation date or comment deadline, and the paths, file names, variables or hosts it
names.

## 3. Route it

| The notice changes | Check | Where it lands |
|---|---|---|
| GFS paths, file names, `filter_gfs_0p25.pl`, the 10 m wind fields, retention, NOMADS TLS | `examples/02-regional-real-forcing/get_gfs.sh` against NOMADS parallel data, before the date | a fix PR to `get_gfs.sh` and `course/04-forcing.md`; the spec in `specs/001-weathernext3-wind/` if its wind file changes |
| GFS-Wave or GEFS-Wave (grid, physics, coupling, products) | what lesson 04 and the proposal say about operational wave guidance | `course/04-forcing.md`, `course/14-ww4-and-the-future.md`; the proposal only for a fact that changed, through `revisor-proposta` |
| A WW3 production branch or tag | which commit of `develop` it starts from, against the `WW3/` pin (`just src-st`) | a dated `docs/log/` entry; a pin bump is its own PR |
| NWPS, GLWU, HAFS, RTOFS | whether the repository cites it | a `docs/log/` entry, or nothing |

Anything that needs work becomes an issue a person files, in the shape of #86: what the notice
says (v, with the PDF), what in this repository depends on it (`file:line`), what to do before
the date.

## 4. Record it

`just noaa-notices --write` moves the snapshot; commit `docs/noaa-notices.json` with the change
the notices led to, or alone with `Tested: just noaa-notices` and `Cost: none` when nothing did.

## Running it on a schedule

The script needs no token, so a weekly Claude routine whose prompt is "/noaa-notices" works where
the routine's network reaches weather.gov; a quiet week ends at step 1.
