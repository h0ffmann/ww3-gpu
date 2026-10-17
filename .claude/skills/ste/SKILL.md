---
name: ste
description: "Write or rewrite English text of ww3-gpu in ASD-STE100 Simplified Technical English (STE): procedures, READMEs, runbooks, port reports, docs/log entries, commit bodies, PR bodies and issue text. Use when the user says STE, Simplified Technical English, ASD-STE100, 'simplify this for non-native readers', or asks for a procedure a reader can follow step by step. English only; never the proposal or pt-BR text."
---

**Audience.** The readers are scientists, many of them not native English speakers. STE makes
the words simpler, never the science: the equation, the routine, the `file:line` at the pin, the
number with its unit, hardware and command, and every `(v)` or `⚠` mark stay as they are.

# ste

Text: $ARGUMENTS

ASD-STE100 is the controlled-language standard of ASD (the European aerospace and defence
industries association): about 53 writing rules and a dictionary of approved words with one
meaning each. The current issue is Issue 9, January 2025 (v, ASD news and Wikipedia
"Simplified Technical English", read 2026-10-17). The rule numbers below are not given because
the text of Issue 9 was not read; the standard is free on request from asd-ste100.org ⚠.

## Scope

| Apply STE to | Do not apply STE to |
|---|---|
| procedures, runbooks, `README.md` steps, port reports, `docs/log/` entries, commit and PR bodies, issue text, `SKILL.md` instructions | `pubs/proposal/` (pt-BR reference, its own reviewer `revisor-proposta`), any `.pt.md` or pt-BR text, verbatim WW3 or upstream quotes, code, equations, command lines |

## Rules (checklist)

Words:
- Use one word for one meaning. Do not use synonyms for variety ("run", then "execute", then "launch").
- Use simple words (`words.md` has a table). Do not use phrasal verbs ("set up" becomes "configure", "turn on" becomes "set" or "enable").
- Technical names are approved: WW3 routines (`W3SNL1`, `W3SRCE`), switches (`ST4`, `NL1`, `OMPG`), Kokkos, CMake presets, file paths, `file:line`, commands, units, regtest names. Technical verbs are approved: build, compile, configure, link, port, regtest, profile, commit, push.
- Keep the articles ("the", "a"). Do not delete them to make a sentence shorter.
- Use an "-ing" form only in a technical name ("bounds checking", "load balancing"). Rewrite "after building" as "after you build".
- Make noun clusters of three words or fewer. Rewrite "WW3 regtest matrix entry count" as "the number of entries in the WW3 regtest matrix".
- Expand an abbreviation at its first use; it must have a row in `docs/GLOSSARY.md`.

Sentences:
- One topic per sentence.
- Procedural sentences: 20 words or fewer. Descriptive sentences: 25 words or fewer. A command, path or `file:line` counts as one word.
- Paragraphs: six sentences or fewer.
- Use simple tenses only: present, past, future. Do not use "would have been run".
- Use the active voice in procedures. In descriptive text, use the passive only when the agent is not known or not important.

Procedures:
- Write instructions in the imperative: "Run `just smoke`."
- One instruction in each sentence, except when the reader must do two actions at the same time.
- Put a condition before the instruction: "If `test.comp` finds a difference, stop."
- Use a numbered list for steps in sequence, a table for data, a bulleted list for items without order.
- Put a warning or caution before the step it applies to. Start it with a clear command or condition: "Do not run `--ww3-gpu --gpu` without the owner."

## Repository rules that STE does not change

- **Meaning is fixed.** A rewrite changes words, not facts. Do not add, remove or soften a claim, a number or a limit. "Translate, don't improve" applies to prose too.
- **Evidence stays.** Keep each `(v)` and `⚠` with the claim it marks. Never remove a `⚠`; never turn "⚠ projected" into a measurement.
- **Numbers stay complete:** value, unit, hardware, the command that reproduces it.
- **No em dash in prose** (`just vale` fails). Use a full stop, a colon or a comma.
- **Third-party issues** stay uncited, as `AGENTS.md` says.
- If a sentence cannot be made shorter without loss of meaning, keep the meaning and split the sentence.

## How to work

1. Read the text. Identify each procedure and each description.
2. Apply the checklist. Open `words.md` only if a word choice is not clear or you need an example.
3. Return only the rewritten text, or a diff if the user asks for one. No preamble, no summary of the changes, no list of the rules applied, unless the user asks.
4. If a fact in the original is ambiguous, keep the original wording for that sentence and mark it with a short question to the user. Do not guess.
5. Run the gates for the files you changed (`just vale` for English Markdown outside `.claude/`).

Be token-efficient in your own output: the shortest correct text, no restatement of the input,
no explanation of STE unless asked.

## STE and humanizer

They are different filters. `humanizer` removes AI tells from prose that a person signs or
publishes (a chapter, a note, a release text). `ste` restricts words and sentences so that a reader
can follow a procedure or a description without doubt. Use `ste` for procedures, runbooks and
reports; use `humanizer` for signed prose. Do not run both on the same text without a reason:
humanizer can add variety that STE removes. For pt-BR text, use `humanizar`; STE is English only.
