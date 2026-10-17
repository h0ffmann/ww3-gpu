# Related work

The ties between this record and other works, as Zenodo and DataCite hold them. A tie is a line
of `related_identifiers` in [`.zenodo.json`](.zenodo.json): Zenodo copies it into the record at
the next release, and DataCite indexes it, so a reader of either record finds the other. The file
follows h0ffmann/closed-source's `RELATED_WORK.md`
([at `5cf9a6a`](https://github.com/h0ffmann/closed-source/blob/5cf9a6a/RELATED_WORK.md)).

**Date of record: 2026-10-14.** Each "points back" cell was read on that day from
`https://api.datacite.org/dois/<doi>` (its `relatedIdentifiers`). Never write a tie or a count that
was not read from the index on the day of record.

## What this record names

The ties in `.zenodo.json` on this branch. The ones marked *next release* are not on Zenodo yet:
v0.1.3, the latest, carries only the first and the marola one.

| Tie | To | What it is | Whose work |
|---|---|---|---|
| `isSupplementTo` | [github.com/h0ffmann/ww3-gpu](https://github.com/h0ffmann/ww3-gpu) (software) | this repository, the source and the code | the author |
| `isVariantFormOf` | [ww3-lab-course.pdf](https://h0ffmann.github.io/ww3-gpu/ww3-lab-course.pdf) (book), *next release* | the book's PDF, rebuilt on every merge | the author |
| `isReferencedBy` | [10.5281/zenodo.23224155](https://doi.org/10.5281/zenodo.23224155) (software) | marola: sea conditions and bathing-water quality at Brazilian beaches | the author and the marola members |
| `requires` | [10.5281/zenodo.23311208](https://doi.org/10.5281/zenodo.23311208) (software), *next release* | picasso: the Nix labs of the toolchain, the paper and the website | the author |
| `isCitedBy` | [10.5281/zenodo.23333656](https://doi.org/10.5281/zenodo.23333656) (book), *next release* | *Closed Source: The God that Failed*, whose citation and paper discipline this repository adopts | the author |
| `isCitedBy` | [10.5281/zenodo.23352633](https://doi.org/10.5281/zenodo.23352633) (proposal), *next release* | *marola-bench: a proposal for a WeatherBench for Brazil* (all rights reserved: read and cite it, do not copy it) | the author |
| `references` | [10.5281/zenodo.23356739](https://doi.org/10.5281/zenodo.23356739) (book), *next release* | *Nashkell: Rhizomatic experiments*, the open book whose site shape this one follows | the author |
| `references` | [10.5281/zenodo.23352630](https://doi.org/10.5281/zenodo.23352630), *next release* | h0ffmann: the author's profile, whose site, notes and translation rules this one follows | the author |
| `references` | [github.com/akitaonrails/ai-memory](https://github.com/akitaonrails/ai-memory) (software), *next release* | ai-memory, the long-term memory this repository adopts ([ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md)) | Fabio Akita, MIT |
| `references` | [10.48550/arXiv.2606.11356](https://doi.org/10.48550/arXiv.2606.11356) (preprint), *next release* | Koldunov et al. 2026, *An Ocean Model Ported by a Large Language Model: Experience and Lessons from FESOM2 (Fortran to C to C++/Kokkos)*, the closest published method to this port, compared in [notes/](notes/index.md) (v, DataCite record read 2026-10-16) | Koldunov, Cheedela, Danilov, Sidorenko, Beyer, Jung |

## What points back at this record

| Record | Its tie to 10.5281/zenodo.23221351 |
|---|---|
| [marola](https://doi.org/10.5281/zenodo.23224155) (software) | `References` |
| [picasso](https://doi.org/10.5281/zenodo.23311208) (software) | `IsRequiredBy` |
| [Closed Source: The God that Failed](https://doi.org/10.5281/zenodo.23333656) (book) | `Cites` |
| [marola-bench](https://doi.org/10.5281/zenodo.23352633) (text, proposal) | `Cites` |
| [Nashkell: Rhizomatic experiments](https://doi.org/10.5281/zenodo.23356739) (book) | none |

Every record that names this one today is the author's own. Nashkell does not name it back; that
tie belongs in nashkell's own metadata.
