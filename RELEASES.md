# Releases

Each release of the WW3 GPU Lab, its permanent DOI and what changed. From v0.1.1 to v0.1.5 a
release was an edition of the book ([ADR-0004](docs/ADRs.md#adr-0004)); the book left the
repository with [ADR-0023](docs/ADRs.md#adr-0023), and v0.2.0 is the first release without it. The concept DOI
[10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351) always resolves to the latest
one; cite a version DOI to pin the exact text and code you read.

Date of record: 2026-11-11. The versions and their dates are DataCite's (`HasVersion` of the
concept DOI, and each version's `Issued` date); the merged pull requests are the generated notes
of each GitHub release. The format follows h0ffmann/closed-source's `RELEASES.md`
([at `5cf9a6a`](https://github.com/h0ffmann/closed-source/blob/5cf9a6a/RELEASES.md)).

| Version | Date | Permanent DOI | Release | PRs merged |
|---|---|---|---|---|
| v0.2.0 | 2026-11-11 | minted by Zenodo after the tag ⚠ | [v0.2.0](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.2.0) | 23 |
| v0.1.5 | 2026-10-23 | [10.5281/zenodo.23600920](https://doi.org/10.5281/zenodo.23600920) | [v0.1.5](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.5) | 40 |
| v0.1.4 | 2026-10-16 | [10.5281/zenodo.23422195](https://doi.org/10.5281/zenodo.23422195) | [v0.1.4](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.4) | 58 |
| v0.1.3 | 2026-10-09 | [10.5281/zenodo.23268515](https://doi.org/10.5281/zenodo.23268515) | [v0.1.3](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.3) | 2 |
| v0.1.2 | 2026-10-09 | [10.5281/zenodo.23263632](https://doi.org/10.5281/zenodo.23263632) | [v0.1.2](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.2) | 10 |
| v0.1.1 | 2026-10-08 | [10.5281/zenodo.23233540](https://doi.org/10.5281/zenodo.23233540) | [v0.1.1](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.1) | 20 |
| v0.1.0 | 2026-10-07 | [10.5281/zenodo.23221352](https://doi.org/10.5281/zenodo.23221352) | [v0.1.0](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.0) | 33 |

v0.1.4 and v0.1.5 are typed as a book on Zenodo (`publication_type: book`,
[ADR-0009](docs/ADRs.md#adr-0009)); v0.1.0 to v0.1.3 are typed as software, and so is v0.2.0,
since the book is gone ([ADR-0023](docs/ADRs.md#adr-0023)). The types are DataCite's
`resourceTypeGeneral`, read on 2026-11-11 (v).

## v0.2.0: a new home and a twin in Rust

The website moved to <https://wave.piabrota.art/>, built from piabrota/nix-os, and the Gleam
code left this repository (#231, [ADR-0024](docs/ADRs.md#adr-0024)); GitHub Pages still serves
the progress, ADR and figure pages. chaceon (<https://github.com/h0ffmann/chaceon>), WW3 written
in Rust, is the twin of the Kokkos port: the site presents it beside that port (#214), and it is
a submodule and a runtime of its own, which `--chaceon` builds and compares with `--ww3` (#228).

`just run` runs one case on six runtimes with one command, on MetOcean's WW3 7.14
(`WW3-metocean-7.14/`, tag `7.14.mos-92e2cf6`) as the reference, with fresh IFS wind from
ECMWF Open Data (`just ifs-get`, #220, #228). The WW3 submodules are now that one reference and
the h0ffmann/WW3 fork; NOAA's WW3 is written from the fork's history by `scripts/ww3_base.sh`
(#233). The course and the book left the repository (#222, ADR-0023), and every diagram is a
dark Archify drawing, with no Mermaid (#219, #223, #224, #226, #230, #234). The preprint
"Wave models on GPUs" compares the Kokkos and Rust ports, in English and in pt-BR (#213). MPI
enters the port's plans (ADR-0020), the C++ translated from WW3 carries SPDX headers with
`NOTICE` and `LICENSES/`, and the ondaseventos image builds MetOcean's WW3 (#211).
The WW3 7.14 manual is summarised for agents in `docs/ww3-manual/` (#218); the ADRs are one
document (#217); Vale reports ASD-STE100 checks (#232); `llms.txt` indexes the repository
(#227); the README names UFSC's ondaseventos as the first user (#225); the lineage page joined
the site (#221); Oh My Pi loads the Claude Code setup (#216); the WeatherNext 3 spec and the
Kimi Code folder were removed (#215); the project snapshot was refreshed (#212).

## v0.1.5: the device-resident step

The spectrum `VA` stays on the device through the time step (WFIP-0005), the device computes
glibc's libm bits (WFIP-0006), and the resident preset was validated on the CPU (#173 to #191).
W3SDS4, W3SIN4 and W3SRCE run with sea points in vector lanes (#197 to #201); W3XYPUG and
W3XYPFSN2 run on an unstructured grid, `ww3_tp2.6` byte-identical (#202, #204).

## v0.1.4: the regtest matrix

The Kokkos DIA runs inside WW3 and is checked by WW3's own regtests across the 311 NL1 entries
of `matrix.base` (ADR-0010, WFIP-0003, #119 to #124); ST4 and most of the remaining source
terms, propagation and W3WAVE support were translated, phase 1 (#135 to #159).

## v0.1.3: four decisions accepted

ADR-0001, ADR-0003, ADR-0004 and ADR-0005 were accepted (#94), and CI moved to the current major
versions of GitHub's own Actions (#93).

## v0.1.2: the book on the web

The book became a static website on GitHub Pages (#78). Lesson 19, two agents and one issue
tracker, was added (#84), with the `para:*` messages between Claude projects (#81). `just adrs`
lists the decisions (#88), and ADR-0005 snapshots the Claude project's settings (#89).

## v0.1.1: the repository is a book

The repository was repositioned as a book on agentic coding and agentic research, with a Leanpub
edition built by CI (#74). The README was written by study area, with a Portuguese mirror and the
DOI and citation examples (#48); the author is cited as Hoffmann, Matheus (#50); marola is linked
as related work (#52); Vale gates the prose (#66).

## v0.1.0: the first edition

The toolchain (the nix-config submodule, #1), WW3 as a submodule (#2), the recipes that build WW3
and run a regression test (#3), the Kokkos plans (#4, #5) and the publication pipeline (#8).
