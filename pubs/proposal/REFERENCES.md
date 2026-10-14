# References of the proposal: how each was checked

One row for each entry of [`refs.bib`](refs.bib): which record the entry was checked against, on
which day, and what matched. The status is one of three words. `verified`: the source was fetched
and its title, authors, year and venue match the entry. `partial`: the source was fetched and one
detail differs or could not be confirmed; the note says which. `unverified`: the source was not
fetched, and the proposal must not lean on it until it is. This follows h0ffmann/closed-source's
`papers/<slug>/REFERENCES.md` and its `refs-check`, at
[`5cf9a6a`](https://github.com/h0ffmann/closed-source/tree/5cf9a6a); here the check is
`python3 scripts/refs_check.py` (`just refs`). It checks the references, not the text: the
proposal's prose is reviewed by `revisor-proposta` (`AGENTS.md`, "The proposal"). A `partial`
row names a fix for `refs.bib`, which goes through that reviewer like any other change under
`pubs/proposal/`.

All rows were checked on 2026-10-14.

| Key | Status | Checked against | Note |
|---|---|---|---|
| `ww3manual` | partial | the manual's PDF, `https://github.com/NOAA-EMC/WW3/wiki/files/manual.pdf` (466 pages) | Title page: "User manual and system documentation of WAVEWATCH III version 6.07", The WAVEWATCH III Development Group (WW3DG), March 2019, "Tech. Note 333, NOAA/NWS/NCEP/MMAB, College Park, MD". All match. The entry has neither a `doi` nor a `url`; add `url = {https://github.com/NOAA-EMC/WW3/wiki/files/manual.pdf}` |
| `ww3repo` | verified | `https://github.com/NOAA-EMC/WW3` (200); `model/src/w3initmd.F90` on `develop` | Repository "NOAA-EMC/WW3: WAVEWATCH III"; `WWVER = '7.14'` on `develop`, as the title says |
| `ww4repo` | verified | `https://github.com/NOAA-EMC/WW4` (200) | "Home of the WAVEWATCH IV (WW4) third-generation wind wave modeling framework" |
| `on528` | verified | DataCite, `10.25923/0wyp-9f39` | "The WAVEWATCH IV Project: Phase II report", Tolman, Hendrik, 2026, NOAA National Centers for Environmental Prediction |
| `on525` | verified | DataCite, `10.25923/h7j3-1h25` | "The WAVEWATCH III Software Modernization Project: Phase I report", Tolman, Hendrik, 2025, NOAA NCEP |
| `ikuyajolu2023` | verified | Crossref, `10.5194/gmd-16-1445-2023` | Title, six authors in order, GMD 16, 1445–1458, 2023. Crossref also gives issue 4, which the entry leaves out |
| `yuan2024` | verified | Crossref, `10.5194/gmd-17-6123-2024` | Title, eight authors in order, GMD 17(16), 6123–6136, 2024 |
| `trott2022` | verified | Crossref, `10.1109/TPDS.2021.3097283` | "Kokkos 3: Programming Model Extensions for the Exascale Era", first ten authors in order, IEEE TPDS 33(4), 805–817, 2022 |
| `renomo` | verified | `https://labeco.ufsc.br/renomo/` (200) | "A Rede Nacional de Observação e Monitoramento Oceânico (ReNOMO) foi formada a partir da aprovação na chamada pública CNPq/MCTI/Finep 062/2022" |
| `labeco` | verified | `https://emc.ufsc.br/portal/laboratorios-2/labeco/` (200) | "O Laboratório de Engenharia e Ciências Oceânicas (LabECO) do Departamento de Engenharia Mecânica da Universidade Federal de Santa Catarina" |
| `wwlab` | partial | `https://github.com/h0ffmann/ww3-gpu` (200) | The repository exists. Its title is now "WW3 GPU Lab: agentic coding and agentic research on a bit-for-bit GPU port of WAVEWATCH III"; the entry has the earlier "ww3-gpu: an open lab for WAVEWATCH III operational optimisation and GPU kernels". It has a concept DOI now, `10.5281/zenodo.23221351`, which the entry could carry |
| `koldunov2026` | verified | DataCite, `10.48550/arXiv.2606.11356` | Title, six authors in order, 2026, arXiv |
| `wamdi1988` | verified | Crossref, `10.1175/1520-0485(1988)018<1775:TWMTGO>2.0.CO;2` | "The WAM Model—A Third Generation Ocean Wave Prediction Model", The WAMDI Group, JPO 18(12), 1775–1810, 1988 |
| `tolman1991` | verified | Crossref, `10.1175/1520-0485(1991)021<0782:ATGMFW>2.0.CO;2` | Title, Tolman, JPO 21(6), 782–797, 1991 |
| `campos2020` | verified | Crossref, `10.1007/s10236-019-01329-4` | Title, four authors in order, Ocean Dynamics 70(3), 405–419; print March 2020 (online 11 December 2019) |
| `farina2002` | verified | Crossref, `10.3402/tellusa.v54i2.12133`; the article page at tellusjournal.org | "On ensemble prediction of ocean waves", Leandro Farina, Tellus A 54(2), 148–158, 2002 (Crossref gives only the first page; the article page gives 148-158) |
| `emcwaves` | verified | `https://www.emc.ncep.noaa.gov/emc/pages/numerical_forecast_systems/wavemodels.php` (200) | Page title "EMC Wave Models" |
| `bauer2021` | verified | Crossref, `10.1038/s43588-021-00023-0` | Title, six authors in order, Nature Computational Science 1(2), 104–113, 2021 |
| `graham1982` | verified | Crossref, `10.1145/872726.806987` | "Gprof", subtitle "A call graph execution profiler", Graham, Kessler, McKusick, ACM SIGPLAN Notices 17(6), 120–126, 1982 |
| `perf` | partial | `https://perf.wiki.kernel.org/` | The URL answers 301 to `archive.kernel.org/oldwiki/perf.wiki.kernel.org/`, and the wiki now lives at `https://perfwiki.github.io/main/`, whose title is "perf: Linux profiling with performance counters", as the entry says. The author "Linux Kernel Organization" is not stated on either page. Point the `url` at `https://perfwiki.github.io/main/` |
| `hasselmann1973` | partial | EPIC (AWI), `https://epic.awi.de/id/eprint/10163/` (the entry's `https://epic.awi.de/10163/` timed out) | Title and year match; pages 1–95 and volume 12 match. EPIC's record lists only "Hasselmann, K. and Olbers, D." and cites "Ergänzung zur Deut. Hydrogr. Z., Reihe A (8)", where the entry has `number = {12}` and Barnett, Bouws, Carlson, Cartwright and others; the full author list was not checked against the report itself. Point the `url` at the `id/eprint/` address |
