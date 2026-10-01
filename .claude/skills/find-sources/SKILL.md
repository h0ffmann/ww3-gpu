---
name: find-sources
description: "Find and verify a scholarly source for a claim in pubs/proposal or course/, using the Consensus MCP search, then add it to refs.bib. Use when a sentence needs a citation, a placeholder such as (CITAR ...) or (REFERÊNCIA) appears, the reviewer flags a claim without a source, or the user asks for papers on a topic."
---

# find-sources

A search hit is a lead, not a source. This skill turns one into the other.

## 1. Search

Use the `consensus` MCP server's `search` tool (configured in `.mcp.json`; sign in once with
`/mcp`). Query in English, one claim per query, and narrow with `year_min`/`year_max` when the
claim is about the current state of something. The free plan allows 30 searches a month, so write
the query well instead of retrying variations.

If the server is not connected (a cloud session, no sign-in, egress blocked), say so in one line
and search Crossref or the publisher directly with web search. Do not pretend the search ran.

## 2. Verify before citing

For each candidate:

1. Open the DOI (`https://doi.org/<doi>`), the publisher page or the arXiv page. Confirm authors,
   title, year, venue, volume and pages there, not from the search result.
2. Read at least the abstract and the passage that supports the claim. Note which part you read;
   a claim backed only by an abstract is worded as cautiously as the abstract.
3. Copy numbers exactly as the source states them, with their unit and conditions.
4. A preprint is cited as a preprint ("em pré-publicação"), as the proposal already does for
   Koldunov et al. (2026).

If nothing verifiable turns up, leave the claim unsupported, tell the user, and suggest weakening
or cutting it. Never invent a DOI, a page number or an author list.

## 3. Add it

- `pubs/proposal/refs.bib`: follow the existing entries (key `firstauthorYEAR`, `doi` for papers,
  `url` + `urldate` for web pages and repositories).
- Cite with `[@key]` in both `pt/` and `en/`; `just proposal-lint` checks they match.
- In `course/` and `docs/`, mark the claim `(v)` when you opened the source and `⚠` when you did
  not (CONTRIBUTING.md).
- Build (`just proposal pt`) to see it render in ABNT.
