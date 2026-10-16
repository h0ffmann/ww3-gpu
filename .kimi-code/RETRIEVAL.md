# Retrieval map for Kimi Code

The two auto-loaded files (`.kimi-code/AGENTS.md`, `AGENTS.md`) already pass the 32 KB warning,
so lookup detail lives here, read on demand. One question, one source: open the named file or
run the named command instead of searching the tree. A number or a WW3 fact found this way still
follows the evidence rules of `AGENTS.md` (`(v)` / `⚠`, `file:line` at the pin).

| Question | Answer lives in |
|---|---|
| The rules, where a change goes, which gate it needs | `AGENTS.md` (already loaded) |
| What an abbreviation, switch, routine or tool name means | `docs/GLOSSARY.md` |
| Is this idea already designed or decided | `docs/WFIPs/README.md`, `docs/ADRs/README.md`; `just wfip status --since <tag>` |
| What was tried before, including failures | `docs/log/README.md` (dated journal), `notes/index.md` |
| What is translated, ported, regtested; what is next | `kokkos/PORT_STATUS.md`, `kokkos/PORT_RANKING.md`; the ledger `kokkos/ports.tsv`; `python3 scripts/port_score.py --check` fails when they are stale |
| A measured number, its machine and command | `bench/README.md` and `bench/results/*.json`; `just results table` |
| The WW3 pin this checkout builds against | `just src-st` |
| A WW3 routine or line | `WW3/model/src/<file>.F90:<line>` at the pin (see below) |
| A proposal reference and how it was checked | `pubs/proposal/REFERENCES.md`; `just refs` |
| The numbers the website shows | `site/data/*.tsv` (generated, never edited by hand) |
| What past sessions did, what is open | `ai-memory search "<question>"` and `ai-memory read-page <path>` on the owner's workstation (no binary in cloud sessions); `MEMORY.md` says what lives where |
| A small lookup over the tree | `just ask "<question>"` (the local model) |

## Searching the tree

- Use the Grep and Glob tools, not shell grep. Scope every search: the submodules `WW3/`,
  `WW3-noaa/`, `WW4/` and `picasso/` are read-only references, so exclude them from repo-wide
  searches and search `WW3/model/src` only when hunting a Fortran routine.
- Routine `W3QCK1` lives in `w3qck1md.F90` (the `w3...md` module of the same name); a
  case-insensitive hit for `SUBROUTINE W3QCK1` in `WW3/model/src` gives the file, and the
  citation is
  `model/src/<file>.F90:<line>` at the pin, never a quote from memory.
- Large files: read with a line offset around the `file:line` hit instead of the whole file.
- A generated file (`site/data/`, `kokkos/PORT_STATUS.md`, `docs/WFIPs/README.md`) is refreshed
  by its script (`scripts/port_score.py --write`, `just wfip index`), never edited.
