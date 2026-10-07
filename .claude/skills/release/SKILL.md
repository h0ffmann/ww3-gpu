---
name: release
description: "Cut a citable ww3-gpu release and keep its Zenodo/citation metadata right. Use when the user asks to release, tag a version, mint or update a DOI, or change CITATION.cff, .zenodo.json or the README citation badge."
---

**Audience.** Releases are cited by scientists: PhD and independent researchers (see
`CONTRIBUTING.md`, "Who this repository is for"). Write release metadata for academic discovery:
the title, description and keywords a wave modeller or HPC researcher would search for, the
author's ORCID, and references by DOI.

# release

Every published GitHub release of this repo is archived by Zenodo, which mints a version DOI
under one concept DOI. The chain has no secrets in it:

1. `just release X.Y.Z` (`scripts/release.sh`) tags `main` as `vX.Y.Z` and pushes the tag. It
   refuses unless `main` is checked out, clean and level with `origin/main`, and the tag is new.
   `--dry-run` shows what it would do.
2. `.github/workflows/release.yml` turns the tag into a GitHub release with generated notes.
3. Zenodo's GitHub webhook (enabled by the owner at zenodo.org → GitHub) archives the release
   using `.zenodo.json` and mints the DOI a few minutes later.

`just releases` lists past tags.

## Before tagging

- A DOI cannot be deleted. Confirm the version number with the user before pushing a tag.
- Versioning is SemVer: bump PATCH for fixes and docs, MINOR for new lessons, kernels or tools,
  MAJOR only when the user says so.
- Metadata changes (authors, ORCID, title, keywords) must land on `main` before the tag:
  Zenodo reads `.zenodo.json` from the tagged commit, and only the next release picks up an edit.

## Metadata rules

- `.zenodo.json` wins over `CITATION.cff` on Zenodo; keep both saying the same thing.
- Licences: MIT, plus `LGPL-3.0-or-later` for kernels translated from WW3. `CITATION.cff` lists
  both; Zenodo takes one licence id (`mit`), so `.zenodo.json` names the LGPL files in `notes`.
  A newly ported kernel adds its files there.
- Concept DOI: 10.5281/zenodo.23221351 (v0.1.0 is 10.5281/zenodo.23221352). DataCite's API
  (`api.datacite.org/dois?query=ww3-gpu`) shows a new version DOI when zenodo.org is unreachable.
- The author is Hoffmann, Matheus (Poli/UFRJ). Supervisors go under
  `contributors` with `"type": "Supervisor"` in `.zenodo.json`, never as creators.
- An ORCID goes in `.zenodo.json` as `"orcid": "0000-0000-0000-0000"` (bare iD) and in
  `CITATION.cff` as `orcid: "https://orcid.org/0000-0000-0000-0000"` (full URL).
- Validate `CITATION.cff` against the CFF 1.2.0 schema and parse `.zenodo.json` before pushing.
- The README "How to cite" badge uses the concept DOI (it always resolves to the latest
  version), not a version DOI.
