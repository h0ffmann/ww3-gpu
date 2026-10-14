# Notes

The second brain of this repository: a slip box of short notes, one idea each, linked to each
other. It is plain Markdown, so Obsidian reads it when `notes/` is opened as a vault. The research
log stays where it is.

## Two records, two jobs

| Record | Holds | Rule |
|---|---|---|
| `notes/` | the ideas the book and the lab stand on, one per note | linked, sourced, checked by `just notes` |
| [`docs/log/`](docs/log/README.md) | what was tried and what happened, negative results included | append, never rewrite |

The log is this repository's journal. A challenge met, a spike, a tolerance that did not hold:
that goes in `docs/log/`, not in a note. A note may link to a log entry as its source.

## Rules for a note

1. **One idea, one note.** Split a text with three ideas into three notes.
2. **Own words.** Quote only what must stay exact, in quotation marks, with its source.
3. **A permanent name.** `notes/<id>-<slug>.md`. The id is the UTC time the note was made,
   `YYYYMMDDHHMM`. The id never changes; the title can.
4. **Front matter:**

   ```yaml
   ---
   id: "202610141900"
   title: "Bit-for-bit is the gate"
   type: permanent
   status: verified
   source: "kokkos/PORT_STATUS.md at da226b2"
   tags: [porting]
   created: 2026-10-14
   ---
   ```

   - `type`: *fleeting* (an idea as it came), *literature* (what one source says),
     *permanent* (a claim of this repository, built from sources and other notes).
   - `status`: *unverified* (no source read yet), *partial* (a source read, one part not
     confirmed), *verified* (the source was read and says it), *open-question*, *approved*.
     Only the owner sets *approved*, and the note says when.
5. **Evidence marks.** Inside a note, `(v)` marks a claim checked against its source and `⚠` one
   that was not, as everywhere in this repository (`AGENTS.md`, "Audience and evidence").
6. **Links.** Every note links to at least one other note with `[[<id>-<slug>]]`, and
   [`notes/index.md`](notes/index.md) links to every note. A link points to a note that exists.
7. **People.** A person is named only when the work is public under that name: an author with an
   ORCID, a DOI or a public profile. Anyone else is an alias.
8. **Truth.** Never invent a source, a quotation or a number. A number keeps its command.

`just notes` (`scripts/notes_check.py`) checks rules 3, 4 and 6: the fields, the id against the
file name, unique ids, the known types and statuses, at least one link, every link resolves, and
the index covers every note. `--self-test` gives it bad notes and expects each fault.

## Sources

- h0ffmann/h0ffmann's `NOTES.md` at `9bfe949`: the split into notes and a journal, and the shape
  of the front matter.
- h0ffmann/closed-source's `SMART_NOTE_POLICY.md` at `5cf9a6a`: the slip box rules, after
  Luhmann's slip box as Sönke Ahrens describes it in *How to Take Smart Notes*.
- This repository's own `docs/log/README.md`: append, never rewrite, negative results included.
