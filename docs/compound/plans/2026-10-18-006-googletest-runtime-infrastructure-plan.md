---
title: GoogleTest for the runtime infrastructure of the Kokkos port - Plan
type: feat
date: 2026-10-18
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-10-18 (goal abbb0456): start planning GoogleTest for the most critical parts of the code, given the ww3-gpu goal and the work of the last days"
execution: code
---

# GoogleTest for the runtime infrastructure - Plan

## Goal Capsule

- **Objective:** the C++ that decides which copy of the spectrum is current, what crosses between
  host and device, and whether a run reports Kokkos when the Fortran ran gets tests that fail in a
  fraction of a second, before any regtest, for the defects WW3's regtests see late or never.
- **Authority:** AGENTS.md over this plan; [ADR-0010](../../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)
  unchanged for ported routines; [ADR-0019](../../ADRs/ADR-0019-googletest-for-runtime-infrastructure.md)
  (Proposed with this plan) draws the boundary. Nothing here starts before the owner accepts
  ADR-0019.
- **Stop conditions:** a test that needs a Fortran reference, a captured fixture or a peek at WW3's
  arithmetic is out of scope (ADR-0010); a test that needs a hook in production code is redesigned
  through the public ABI or dropped; nothing runs on a device.
- **Execution profile:** medium (a tool and its tests, no ported routine); one branch from `main`,
  one PR through `just pr`, after the follow-up PR of #191 merges (it changes the defaults the
  suites pin).

---

## Why these parts, ranked by what a defect costs

The goal is speed without changing the Fortran's arithmetic (ADR-0016). Since PR #191 the speed
rests on the resident spectrum: `VA` stays on the device, and every host reader or writer syncs
only what it touches. A defect in that bookkeeping changes no arithmetic and so escapes every
argument of bit parity; it hands a kernel stale bits, and WW3's regtests see it only when those
bits reach a compared file. The ranking follows that cost, and the last days' work:

| Rank | Code | Size at `main` `5cd0140` (v, `wc -l`) | Why now | Silent failure it guards |
|---|---|---|---|---|
| 1 | `kokkos/src/fortran_iface/resident_shim.cpp` with `resident_c.hpp` | 599 + 103 | #191 removed the read at `W3WAVE`'s return and moved every reader and writer to column syncs (`ww3_multi`'s exchanges, `W3IOBC`, `W3IOTR`, `W3CPRT`, the ESMF cap); #196 binds `VA` on unstructured grids; the review found the 8-list cache can thrash | a column read or written in the wrong direction, a state left `BOTH` when the device is newer, a cache eviction that serves another list's columns |
| 2 | `kokkos/src/fortran_iface/part_flags.cpp` | 135 | #191 added `WW_KOKKOS_PRESET`, `WW_KOKKOS_STRICT` and the fallback slots; the follow-up of #191 changes when strict applies; the review found `STRICT` is on only for the literal `1` | a part reported on Kokkos while it ran on the Fortran; a preset that silently drops a part; an exit line the regtest driver no longer parses |
| 3 | `presets.txt` against its three readers | 9-line table | #191 made it the one list for the shim, `scripts/ww3-gpu.sh` and the matrix driver; the script's self-test hard-codes the parts | the three readers disagree on what `resident` means |
| 4 | the installed package config (`kokkos/cmake/ww_kokkosConfig.cmake.in`) | 41 | #191's U6; WW3 builds against it; #196 moves the ABI to 0.11 | WW3 links a `ww_kokkos` built for another Kokkos or backend |

Out of scope by ADR-0010, and why each is still covered:

| Code | Covered by |
|---|---|
| every translated kernel and its shim's arithmetic (`srce`, `st*`, `snl1` with #193's direction halo, `prop3`, `outg`'s fields, `cprt`'s placement, #196's `W3XYPUG`) | WW3's regtests through the matrix; the static parity argument of ADR-0016 |
| the Fortran half on the fork (`KOKKOS_PART`, `KOKKOS_FALLBACK`, the sync calls) | the regtests, and `just ww3-gpu-matrix-selftest` for the driver |
| `scripts/*.sh` | their own `--self-test` (`ww3-gpu.sh`, `restart_pair.sh`, `ww3_fortran_ab.sh` when it lands) |
| the device copies of glibc's functions (`libm_device.hpp`) | `scripts/libm_bits.sh` (host transcriptions, every input) and its `--device` run (the owner's) |

## How the state machine is observed without a hook

The resident shim's public ABI is enough to drive and observe every state (v, `resident_c.hpp` at
`5cd0140`), with a host backend where the "device" copy is a second host array:

- `ww_va_device(g, 1)` marks the device newer (state `DEVICE`), after a host-to-device copy when the
  host was newer.
- Spoiling the host array afterwards and calling `ww_va_host_read` shows whether the device's bits
  came back: the host must hold the values from before the spoil.
- `ww_va_host_read_cols` must restore only the listed columns and leave the state `DEVICE`, so a
  later `ww_va_host_read` still restores the rest.
- `ww_va_host_wrote_cols` after a host write of some columns, then a spoil of the others, then
  `ww_va_host_read`: the listed columns hold the new values, every other column the device's.
- `ww_va_device_zero_cols`, `ww_va_host_update`, and `BAD_SHAPE` for a moved array, another shape,
  a column outside `0..NSEALM`, a negative `ncol` (`resident_shim.cpp:130`, `:287-294`,
  `:345-352`).
- Counters: `ww_va_report` writes its lines to `stderr`; GoogleTest's `CaptureStderr` reads them, so
  "no whole-array copy" is asserted on the line `VA device-to-host copies: N`.

Grids are independent contexts keyed by `IGRID`, so each test uses its own grid number and no reset
is needed. Variables read once per process (`WW_KOKKOS_VA_CHECK`, `_POISON`, `_RESIDENT`,
`WW_KOKKOS_PRESET`, `WW_KOKKOS_STRICT`) get one CTest entry each, the same executable with an
`ENVIRONMENT` and a `--gtest_filter`.

## Units

### T0. Accept ADR-0019, and the harness

- The owner accepts or changes ADR-0019; until then nothing below is built.
- `kokkos/tests/CMakeLists.txt`: a `ww_add_infra_test(<name> [ENV ...] [FILTER ...])` beside
  `ww_add_test`, label `infra`; it reuses `kokkos_env.hpp` (one `Kokkos::initialize` per process,
  as AGENTS.md requires).
- `AGENTS.md` ("The port has no tests of its own"), `CONTRIBUTING.md`, the `ww3-port` skill and
  `docs/GLOSSARY.md` (`infra_test_*`, label `infra`) name the boundary, in the same PR.
- **Done:** `just kokkos-test serial-debug` and `openmp-release` list the new label and still pass.

### T1. `infra_test_resident`: the residency state machine (rank 1)

Test cases, one `TEST` each, every one through the public ABI:

1. Bind on the first call: state `HOST`; a second call with another pointer, `nspec` or `nsealm`
   is `BAD_SHAPE`.
2. `HOST` -> `ww_va_device(g, 0)`: one host-to-device copy, state `BOTH`; `(g, 1)`: `DEVICE`.
3. `DEVICE` -> `ww_va_host_read`: the host gets the device's bits, one device-to-host copy, state
   `BOTH`; a second read copies nothing.
4. `ww_va_host_write` from `DEVICE`: the read first, then `HOST`; the next `ww_va_device` copies.
5. Column read from `DEVICE`: only the listed columns restored, duplicates allowed, the state still
   `DEVICE`; an empty list does nothing; a column `> NSEALM` or `< 0` is `BAD_SHAPE`.
6. Column write from `DEVICE` and from `BOTH`: listed columns reach the device, the others keep the
   device's bits; from `HOST`: nothing crosses.
7. The column cache: nine distinct lists cycled on one grid stay correct (every read returns the
   right columns), and the counters show the reallocation the review predicted for the ninth list
   (a regression test of the cost, not a defect today).
8. `ww_va_device_zero_cols` and `ww_va_host_update` keep both copies in their relation.
9. Two grids in one process do not share state or columns (`ww3_multi`).
10. Under `WW_KOKKOS_VA_POISON=1` (own CTest entry): a host array left stale reads as NaN, and the
    next sync restores the device's bits, so a correct sequence ends bit-identical.
11. Under `WW_KOKKOS_VA_CHECK=1` (own entry): a host write the code did not declare is reported at
    the next sync.
12. Under `WW_KOKKOS_VA_RESIDENT=1` (own entry): a column written and not listed shows in the host
    array after `ww_va_host_wrote_cols`.
13. `ww_va_report` under `WW_KOKKOS_VA_COUNT=1`: the lines `scripts/ww3-gpu.sh` and the matrix
    driver parse are present with the counts the sequence implies.

### T2. `infra_test_part_flags`: flags, presets, strict mode, fallbacks (rank 2)

1. `ww_kokkos_flag` precedence, one CTest entry per environment: an explicit `WW_KOKKOS_<PART>`
   wins both ways; then a preset part counts as on; then `WW_KOKKOS_ALL=1`, except the opt-in list
   (`part_flags.cpp:90`), which `ALL` never turns on.
2. `WW_KOKKOS_PRESET=resident` gives exactly the parts of `presets.txt`; an unknown name sets the
   status `W3INIT` stops on.
3. `WW_KOKKOS_STRICT`: `1` is on; the current behaviour for any other value is pinned, and the
   owner decides in T2 whether `true`/`yes` should also turn it on (the review's finding). The test
   then follows the decision.
4. Fallback slots: a name of 24 characters fits and 25 is refused (`part_flags.cpp:80`, `:123-129`);
   the 65th slot is refused, never written past the array; `ww_kokkos_fallback_add` counts.
5. The exit line: the exact text `scripts/ww3-gpu.sh` and the driver read, `calls left on the
   Fortran (WW_KOKKOS_STRICT=1 stops instead): SRCE=0 CPRT=0`, pinned as a contract.

### T3. `presets.txt` against its readers (rank 3)

A CTest (not GoogleTest) that runs `scripts/ww3-gpu.sh`'s `kokkos_preset resident`, the matrix
driver's `ww3gpu_kokkos_preset`, and a tiny program that prints the shim's compiled preset, and
fails when they differ. It replaces the script self-test's hard-coded part string (the review's
testing gap).

### T4. The package consumer (rank 4)

A CTest that configures a one-file consumer against an install-only build (`WW_KOKKOS_BUILD_TESTS=OFF`)
in a scratch prefix: it must find `ww_kokkos` with the recorded Kokkos version, and refuse it when
the recorded version or backends are edited, as U6 of #191 checked by hand (`kokkos/README.md`,
"Install-only build"). It runs only in `openmp-release`, since it builds a second tree.

### T5. Every suite can fail

On a scratch copy, plant one defect per test (for example: skip the state change in
`host_sync`, swap read and write in `host_wrote_cols`, serve a stale list from the cache, read
`WW_KOKKOS_STRICT` as always on), run the suite, record that it fails, and restore. The record is a
dated `docs/log/` entry with each mutant and the test that caught it; a mutant no test catches is a
missing test, added before the PR.

## Order and dependencies

- After the follow-up of #191 (branch `goal/abbb0456-8241-strict-outg-poison`) merges: it makes
  `WW_KOKKOS_VA_POISON=1` the default for Kokkos runs, adds the `OUTG` fallback slot, and changes
  when strict applies. T1 case 10 and T2 cases 3 and 5 pin those defaults.
- Beside #196 (`W3XYPUG`, unstructured grids): T1's cases use the C ABI with no grid geometry, so an
  unstructured `VA` (`NSEALM` nodes) is the same shape to the shim; #196 adds no case here unless it
  changes `resident_shim.cpp`, and then T1 gains its case in that PR.
- Plan 005 (sea points in vector lanes of `W3SRCE`, another session) changes a kernel, which stays
  under ADR-0010.

## Gates

`just kokkos-test serial-debug` (with ASan and UBSan, which also check the shims' buffers) and
`openmp-release`; CI's `kokkos` job runs both. `python3 scripts/port_score.py --check` is
unaffected (no ported routine changes). Vale on the changed Markdown. Never `ctest` on a CUDA
build (`-LE infra`, and `just kokkos-cuda-test` stays the owner's).

## Risks

- A test pins today's behaviour where today's behaviour is wrong (strict's parsing, the cache
  size): T2 case 3 and T1 case 7 are written against the owner's decision, not the code's state.
- `CaptureStderr` and the counters are printed text: a reworded line breaks a test. That is the
  point (the scripts parse the same text), and the PR that rewords it updates both.
- On the OpenMP backend the device copy is host memory, so a test cannot catch a fault that only a
  real device shows (an asynchronous copy, a missing fence) ⚠; the GPU checklist covers that.
