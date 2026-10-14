# The first published ai-memory page, and `memory/`

2026-10-14, Hoffmann with an agent, ADR-0009 (amended while Proposed).

## Question

Can the ai-memory pages the owner chooses reach every reader of the repository, and the reader's
own ai-memory, without publishing the raw session capture?

## What was done

- The owner asked for a seed page, `HUMAN.md`: who owns the repository and the standing orders
  they gave on 2026-10-14, quoted from `AGENTS.md`. It was written with ai-memory's
  `memory_write_page`, pinned, tagged `publish`.
- The first write naming the project failed: `project 'h0ffmann-ww3-gpu' is ambiguous in
  workspace 'default': it is the name of project 'h0ffmann-ww3-gpu' and the canonical key of
  project 'ww3-gpu'`. A second project, `ww3-gpu`, had been created from the folder name at
  22:38 UTC, before `.ai-memory.toml` existed. It held 0 pages, 0 sessions and 5 observations
  with no content (v) (`ai-memory purge-project` dry run). With the owner's consent it was purged
  after `ai-memory backup`; CLI writes to `h0ffmann-ww3-gpu` then succeeded (v).
- `scripts/memory.py` (`just memory`): `export` copies the wiki pages tagged `publish` into
  `memory/` and writes its index; `import` loads `memory/` into the local store with
  `ai-memory write-page`, skipping pages whose body is unchanged; `check` refuses session pages,
  monthly logs, ai-memory manifests, an untagged page, an e-mail address, a home path and a stale
  index. CI runs `--self-test` and `check`.
- Machine: the owner's Linux workstation, ai-memory 3.0.0.

## Result

- `just memory export` wrote `memory/HUMAN.md` and `memory/README.md`; `just memory check`
  passed (v).
- `just memory import` on the same machine printed `same   HUMAN.md` (v).
- A throwaway page imported from a scratch directory landed in `h0ffmann-ww3-gpu` with its kind,
  tags and pin, and was deleted (v). The CLI cannot set `entities` or `abstract`, so an imported
  page lacks them (v) (`ai-memory write-page --help`).
- Not tested: an import on a second machine with an empty store ⚠.

## What it changes

Readers with the ai-memory plugin run `just memory import` after cloning or pulling; README.md,
README.pt-BR.md and MEMORY.md say so. Anything tagged `publish` is public from the next export.
