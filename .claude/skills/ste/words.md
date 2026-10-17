# ste: words and examples

Open this file only when a word choice is not clear or you need a model rewrite.

## Common replacements

⚠ From general knowledge of the STE dictionary, not checked against the Issue 9 dictionary.
When the dictionary is available, check a word there before you add it here.

| Do not write | Write |
|---|---|
| utilize, make use of | use |
| perform, carry out | do, run (a command or a regtest) |
| ensure | make sure |
| obtain, acquire | get |
| commence, initiate | start |
| terminate, abort | stop |
| prior to | before |
| subsequent to | after |
| in order to | to |
| due to | because of |
| approximately | about |
| a large number of | many |
| set up | configure, install |
| turn on, switch on | set, enable |
| find out | find, measure |
| go through | read, examine |
| it is necessary to X | X (imperative) |
| it should be noted that | (delete) |

Repository terms keep their names; do not replace them with a general word:
regtest (not "test"), port (not "rewrite"), shim, switch, pin, preset, ledger,
`test.comp`, `nccmp-tol`, pair to pair, bit for bit.

## Examples from this repository

Each "Before" is quoted from the file named, read on 2026-10-17.

**1. Procedure, -ing form as a noun** (`AGENTS.md`, "Setup and commands")

Before: "Run the gates of what you changed before calling it done."

After: "Before you write that a change is complete, run the gates for each file that you changed."

**2. Passive voice and a phrasal verb** (`kokkos/README.md`, "Presets")

Before: "It is an option of the Kokkos *build*, so it is fixed by the pinned Kokkos derivation in
picasso; a consumer project cannot turn it on."

After: "This option is part of the Kokkos build. The pinned Kokkos derivation in picasso sets it.
A project that uses Kokkos cannot set it."

**3. Two topics in one sentence, technical names kept** (`kokkos/README.md`, "Presets")

Before: "OpenMP is the default execution space in `#ww3`, so `serial-debug` is a *build type*, not
a Serial-only build. Code that must run on Serial says `Kokkos::Serial` explicitly"

After: "In the `#ww3` shell, OpenMP is the default execution space. Thus `serial-debug` is a build
type. It is not a build for Serial only. If code must run on Serial, write `Kokkos::Serial` in the
code."

**4. Evidence marks stay** (pattern, not a quote)

Before: "(v) Byte-identical on the first run, every run: ..."

After: keep "(v)" at the start of the same claim. A rewrite can split the sentence; the mark goes
with the claim it verifies, and the command that verified it stays in the text.
