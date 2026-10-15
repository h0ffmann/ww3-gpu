# Graphify on ww3-gpu's own code: build cost, query cost, what it misses

2026-10-14, the owner with an agent. ADR-0007 (revision of 2026-10-14), the `/graph-check` and
`/graph-view` commands.

## Question

The owner asked for a Claude Code command with prepared graphify queries about this repository,
and how efficient the index is. ADR-0007 had measured graphify on WW3 only and set aside indexing
this repository; what does a code-only graph of ww3-gpu cost, and what does it answer?

## What was done

On the owner's workstation (Intel Core i9-14900, 32 threads), graphify 0.9.91 from `uv tool`,
branch `claude/graph-check` at `6b7003b`:

```bash
G=~/.cache/ww3-gpu-graph/self-6b7003b
env -i PATH="$PATH" HOME="$HOME" GRAPHIFY_NO_AUTO_REFRESH=1 OLLAMA_BASE_URL=http://127.0.0.1:9 \
  /usr/bin/time -f '%e s, %M KB' graphify extract . --code-only --no-label --out "$G"   # three runs, $G removed before each
graphify benchmark "$G/graphify-out/graph.json"
graphify god-nodes --graph "$G/graphify-out/graph.json"
graphify explain snl1_dia.hpp --graph "$G/graphify-out/graph.json"            # three runs, timed
graphify query "how does WW3 call the Kokkos DIA kernel" --budget 600 --graph "$G/graphify-out/graph.json"
graphify query "where is the L1 parity test for W3SNL1" --budget 600 --graph "$G/graphify-out/graph.json"
```

A first, unsandboxed run (`graphify extract . --code-only`, graphify 0.9.91) wrote `graphify-out/`
into the checkout; it was moved to the cache before anything was staged, and `graphify-out/` is now
in `.gitignore`.

## Result

- (v) Corpus: 160 code files read (Python, C++, shell, Fortran, Lua, JavaScript), 276 non-code
  files skipped by `--code-only`, 115 unclassified. The `WW3/` submodule is not in the graph;
  `nix-config/` is (91 nodes).
- (v) Graph: 1,228 nodes, 2,198 edges, 124 communities, `graph.json` 892 KB.
- (v) Build: 1.38 s wall, median of three (1.40, 1.38, 1.36), about 81 MB peak resident. The
  first, unsandboxed run took 4.10 s; the difference was not investigated.
- (v) Query: `graphify explain` 0.14 s wall in each of three runs; `god-nodes` 0.13 s, one run.
- ⚠ Token saving: `graphify benchmark` reports 50.1x fewer tokens per query (about 1,635 against
  81,866 for reading the whole corpus). That is graphify's own estimate from three generic
  questions, not tokens counted in a session; nothing here measured a session with and without the
  graph.
- (v) Seeded queries are right: `explain snl1_dia.hpp` lists the shim, the L1 test and the bench
  that include it, each with `file:L<line>`; `path L1_test_snl1_dia.cpp snl1_tables.hpp` finds the
  one-hop include; `affected SpectralGrid` lists the intro lessons, exercise and test that use it.
- (v) Free-text queries are not: "how does WW3 call the Kokkos DIA kernel" seeded on the word
  "kokkos" and started in `scripts/ww4_status.py`; it never reached the shim. The L1-test question
  found the test, but also `scripts/proposal_lint.py` through `parity()`.
- (v) `kokkos/src/ww_kokkos/snl1_dia.hpp`, the ported kernel, yields no symbols ("a data file, or
  source this extractor models no symbols for"), so the graph sees that it is included, not what is
  in it.

## What it changes

`/graph-view` uses seeded presets (`explain`, `affected`, `god-nodes`) and marks a free-text answer
`⚠`; it states the three blind spots above with every answer. A graph result locates a file; the
claim still cites the file. The token saving stays `⚠` until a session is measured with and
without the graph.
