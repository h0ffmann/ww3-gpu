# Exhaustive and property-based testing

Checked 2026-10-01 with gfortran/g++ 13.3, glibc 2.39, 4 cores. No new tool: the compilers and
GoogleTest the repo already uses. Part of the plan in [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (b).

## What it is

Run the compiled Fortran and the compiled port on every input when the input space is small
enough, and on generated inputs aimed at the edges of IEEE-754 when it is not. When every input is
covered, the result is a proof for that pair of binaries.

## Applied to ww3-gpu

- `W3SNL1` section 1 (`CONS` from `KDMEAN`): done, `proof/snl1_cons/run.sh`. All 2^32 inputs,
  0 differences at the parity flags, 4 632 147 with contraction on `(v, measured)`.
- `INSNL1`'s address tables (`snl1_tables.cpp` against `snl1_ref.F90`): integers, so
  `L1_test_snl1_tables` is already exact for one grid `(v)`. Looping it over every
  `(NK, NTH, XFR, LAMBDA)` the lab's switch files and namelists use makes it exhaustive.
- `ISP = ITH + (IK-1)*NTH` and the `MAPSTA`/`MAPFS`/`MAPSF` maps: bijection and round-trip
  checks over every grid in `examples/` and `bench/`.
- The card-deck distribution, `INIT_GET_ISEA` / `INIT_GET_JSEA_ISPROC` (`w3parall.F90:1152-1504`
  `(v)`): every `ISEA` maps to one `(JSEA, ISPROC)` and back, for each `NSEA × NAPROC` the lab runs.
- CUDA: `expf` on the device against glibc on every float in `[-1e15, -0.625]`, the range
  `X2` can take in section 1 (`X ≥ KDMN = 0.5`, `SNLS3 = -1.25`).
- Spectral loops (600 floats per point at NK=25, NTH=24) cannot be swept. For those: generators
  for ±0, denormals, the largest finite value, values one ULP either side of every `MAX`/`MIN`
  threshold, plus the physics properties `L1_test_snl1_dia` already checks (zero spectrum →
  zero source, `A×2 → S×8, D×4`) `(v)`.

## Wave model and physics fit

Full: it runs the real code with the real libm, module state and all. The limit is the input
space: exhaustive only for one float32 input or a small integer domain.

## Bit for bit

Yes for the exact binaries tested, including FMA, libm entry points and NaN handling, because it
tests what the compiler produced. The pilot found that gfortran's `MAX(NaN, KDMN)` returns a
number at `-O0` and NaN at `-O3` `(v)`. Run on the GPU, the same sweep decides CPU ↔ CUDA. It
says nothing about other compilers, flags or libm versions until rerun.

## Cost of the proof

Hours per function. A sweep takes 17 s on 4 cores here `(v)`; a GPU sweep should take seconds ⚠.
Maintenance is a rerun after a compiler, flag or libm change. Free. Fits any timeline.

## Pros

- Tests the binaries, so nothing is assumed about compilers.
- Counterexamples are concrete inputs a student can debug.
- Measures how often a difference occurs, which a proof tool does not report.

## Cons

- Stops at one float32 input: two inputs are 2^64 cases.
- Proves one build. A new GCC means a new run.
- Random generation is evidence only, never a proof.

## Verdict

**Now.** Every new kernel with a one-input helper or an integer table gets a sweep.
