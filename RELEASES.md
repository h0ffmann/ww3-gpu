# Releases

Each release of the WW3 GPU Lab, its permanent DOI and what changed. A release is an edition of
the book ([ADR-0004](docs/ADRs/ADR-0004-book-channels-and-first-slice.md)). The concept DOI
[10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351) always resolves to the latest
one; cite a version DOI to pin the exact text and code you read.

Date of record: 2026-10-14. The versions and their dates are DataCite's (`HasVersion` of the
concept DOI, and each version's `Issued` date); the merged pull requests are the generated notes
of each GitHub release. The format follows h0ffmann/closed-source's `RELEASES.md`
([at `5cf9a6a`](https://github.com/h0ffmann/closed-source/blob/5cf9a6a/RELEASES.md)).

| Version | Date | Permanent DOI | Release | PRs merged |
|---|---|---|---|---|
| v0.1.3 | 2026-10-09 | [10.5281/zenodo.23268515](https://doi.org/10.5281/zenodo.23268515) | [v0.1.3](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.3) | 2 |
| v0.1.2 | 2026-10-09 | [10.5281/zenodo.23263632](https://doi.org/10.5281/zenodo.23263632) | [v0.1.2](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.2) | 10 |
| v0.1.1 | 2026-10-08 | [10.5281/zenodo.23233540](https://doi.org/10.5281/zenodo.23233540) | [v0.1.1](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.1) | 20 |
| v0.1.0 | 2026-10-07 | [10.5281/zenodo.23221352](https://doi.org/10.5281/zenodo.23221352) | [v0.1.0](https://github.com/h0ffmann/ww3-gpu/releases/tag/v0.1.0) | 33 |

The next release is the first with the record typed as a book on Zenodo (`publication_type:
book`, [ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md)). v0.1.0 to
v0.1.3 stay typed as software.

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
