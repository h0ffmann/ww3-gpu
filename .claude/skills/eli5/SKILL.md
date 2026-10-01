---
name: eli5
description: "Explain a wave-modelling, Fortran or HPC topic to someone who knows nothing about it. Use when the user types /eli5 <topic>, asks to explain something simply, or asks for a picture explainer of how part of WW3, Kokkos or the forecast chain works."
---

# eli5

Explain like the reader knows nothing about this topic: short sentences, one idea at a time, a
picture before the prose. Adapted for this repository from the community `eli5` skill by Thariq
Shihipar (MIT, `anthropics/claude-plugins-community`), which produces an HTML picture explainer.

Topic: $ARGUMENTS

## Ground it in this repository first

This is a teaching repository, so an explanation that contradicts the course is worse than no
explanation. Before writing:

- `docs/GLOSSARY.md`: the term as this repo defines it.
- `course/*.md`: the lesson that already teaches it, if there is one. Point the reader there at
  the end ("the long version is in `course/03-…`").
- `pubs/proposal/pt|en`: for anything about the project's own plan.

If the repository and your memory disagree, the repository wins. If neither covers it, say so
plainly rather than filling the gap with a plausible-sounding invention.

## How to explain

1. **One-sentence answer first.** What it is, in words a first-year student knows. No jargon in
   this sentence, not even *spectrum* or *kernel*.
2. **A picture.** A Mermaid diagram or a small ASCII sketch that shows the thing working: the
   grid and the spectrum at a point, the time loop, the CPU–GPU hop. Label the parts with the
   words you just used. Keep it under ten nodes; a diagram nobody can read is decoration.
3. **The words the field uses.** Now name it properly: *action balance equation*, *source terms*,
   *CFL condition*, *bit-for-bit parity*, *backend*. One line each, tied back to the picture, so
   the reader can follow a paper or a code comment afterwards.
4. **Why it matters here.** One or two lines connecting it to what this repository does: the
   forecast cycle at LabECO, the run time, the rungs of the optimisation ladder.
5. **Where to read more.** The lesson, the manual section, or the paper, with the repo's
   verification markers: `(v)` for a claim checked against a source you actually opened, `⚠` for
   one you did not.

## Analogies

Use physical ones: waves, water, queues, kitchens, post. Say where the analogy breaks before the
reader finds out: "the ocean does not actually sort waves into bins; the model does, to compute".
Never let an analogy replace the real definition; it buys attention for the definition that follows.

## Audience and language

Default to a curious engineering student. Adjust if asked: `for my advisor` earns a compact,
precise register with the real terms; `for my mother` drops every acronym. Answer in the language
the user wrote in. This project is discussed in Portuguese as often as in English, and pt-BR
answers should use the vocabulary in `docs/GLOSSARY.md`.

## Output

The default is a chat answer: short paragraphs and the diagram inline. If the user asks for a page,
a slide, something printable or something to send to someone, write a standalone HTML file with
large type and few words into `build/eli5/<topic>.html` (`build/` is gitignored) and give them the
path. Do not add files anywhere else in the tree. Explanations that belong in the course go
through a lesson, not through this skill.

## Do not

- Do not invent numbers. Speed-ups, resolutions, model versions and quotas come from a source, or
  they are marked `⚠`.
- Do not explain by pasting code. A ten-line excerpt is fine when the code *is* the point; a file
  dump is not an explanation.
- Do not flatter the question or apologise for simplifying. Just explain.
