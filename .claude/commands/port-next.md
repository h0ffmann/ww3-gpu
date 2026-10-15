---
description: Start a Kokkos port, phase 1 (translate), from one WW3 F90 routine. Checks the port order, reads the Fortran at the pin, scaffolds the kernel, shim, L1 test, port report and ledger row.
argument-hint: "<module:routine, as in kokkos/PORT_RANKING.md, e.g. w3src4md:w3sds4>"
---

Start the port of `$ARGUMENTS`. Phase 1 only: reproduce the Fortran's arithmetic. The rules are
`AGENTS.md` ("Porting", "Running on the GPU", "Which account writes Kokkos code") and
`docs/AGENTS_KOKKOS_202609.md` §1.2, §1.5 and §1.6; where this file and those disagree, they win.
Every WW3 fact you write cites `model/src/<file>.F90:<line>` at the pin, read from the file.

**0. Stop unless all of these hold.** Say which one failed.

- `python3 -c "import json;print(json.load(open('$HOME/.claude.json'))['oauthAccount']['organizationType'])"`
  prints `claude_team`. `claude_max` or anything else: stop, Kokkos code is not written here.
- `$ARGUMENTS` is `module:routine`, and it has no row in `kokkos/ports.tsv`.
- `git status` is clean, and `WW3/` is initialised at its pin (`just src-st`).

**1. The order.** Run `python3 scripts/port_score.py --check`, then read `kokkos/PORT_RANKING.md`.
If `$ARGUMENTS` is not row 1, stop and ask the owner. Name row 1 and both shares; the order is
wall time (ADR-0002), and choosing another routine is the owner's call. A row below the
10 ms line has no measured cost: say so.

**2. The issue.** Find the issue the port answers (`gh issue list --search "<ROUTINE>"`), and any
plan for it (`ls docs/ | grep -i <routine>`; `W3SDS4` has #45 and
`docs/W3SDS4_TRITON_PLANO_202610.pt.md`). Read them; they may already decide the interface or the
fixture. With no issue, stop: a port with no issue is not work yet, and filing it is a person's
act. Branch `claude/<issue>-port-<routine>` from `main`.

**3. Read the Fortran.** In `WW3/model/src/<module>.F90` at the pin, write down, each with `file:line`:

- the routine's lines (`SUBROUTINE` to `END SUBROUTINE`) and its arguments: name, type, `INTENT`,
  shape, units;
- the module variables it reads and writes (`USE ..., ONLY:`), and the `#ifdef` switches inside it.
  `switches/switch_lab_shrd` decides which branches the lab compiles; preprocess as ADR-0007 does
  rather than guess;
- its callers (`grep -n -i "call *<routine>" WW3/model/src/*.F90`) and what it calls;
- which reductions it does, and in which order: the kernel must keep that order.

**4. Scaffold.** Files in lower case after the routine. Each one is a skeleton, not a guess at the
physics; a body you have not translated says so and fails its test.

| File | What goes in |
|---|---|
| `kokkos/src/ww_kokkos/<r>.hpp`, `.cpp` | heritage header (WW3 module, `file:lines`, commit), Doxygen, the interface; Views in explicit memory spaces, `float` for `REAL`, no allocation in a kernel |
| `kokkos/src/fortran_iface/<r>_shim.cpp`, `ww_kokkos_c.hpp` | the `bind(C)` entry, with the argument table (name, WW3 name, type, intent, shape, units) |
| `kokkos/tests/fixtures/` | a fixture generator from the verbatim Fortran, like `gen_snl1_ww3lib.F90` |
| `kokkos/tests/L1_test_<r>.cpp` | GoogleTest against the fixture, tolerance stated and justified in a comment; never widened |
| `kokkos/port_report/<ROUTINE>_KOKKOS.md` | the four sections of `W3SNL1_KOKKOS.md`: what was ported, benchmarks, how it was regtested, GoogleTest; "not yet" where nothing was measured |
| `kokkos/ports.tsv` | the row: `file` and `lines` from step 3, `status` `ported` only once the kernel builds, `regtested` `no: <why>`, `googletest` `yes: <tests>` or `no: <why>`, `kokkos` the new `.cpp` files |
| `docs/GLOSSARY.md` | the routine and any new abbreviation, with its first use |

The WW3 caller patch is not written here: it goes on the `h0ffmann/WW3` fork branch behind the
`KOKKOS` switch (`kokkos/src/fortran_iface/PATCH.md`), when the owner asks.

**5. Measure and record.** `just kokkos-test serial-debug`, `just kokkos-test openmp-release`,
`just kokkos-coverage`, then `python3 scripts/port_score.py --write` and `--check`, so
`kokkos/PORT_RANKING.md` shows the new row's coverage. A timing enters the report only after its L1
passed on the same build. Never `--hoff --gpu`; `just smoke-hoff` (CPU) is the L2 step, after the
caller patch exists.

**6. Stop and ask** before changing a reduction order, a limiter or the integration order, before
turning contraction on, and when a test fails at its stated tolerance. Report it; never widen it.

**7. Report.** Which files were scaffolded, what is translated and what is not, the L1 result with
its command, the coverage row, and the next step with who takes it.
