# A Bend 2 tryout on one WW3 kernel
## Plan for porting `W3SNL1` (the DIA) to Bend as a third arm of the existing Fortran / Kokkos parity harness

Prepared 18 September 2026. A proposal, not a build record: nothing below has been run.
`(v)` marks a claim checked on 2026-09-18 against a source named in §11; `⚠` marks one that
was not checked. Bend released 2.0.8 and 2.0.9 on the day this was written `(v)`, so every
version-specific claim here is dated and will go stale; re-check the marked lines before acting.

> **Scope.** One kernel, one week, one written result either way. This is a learning
> exercise about what a proof-checked, GPU-targeting language costs on real wave-model
> arithmetic. It is not a route on the ladder (`course/13`), it does not touch `kokkos/`,
> and it does not propose Bend for the port.

---

## 1. Summary and recommendation

**Do it, small.** Port the per-call body of `W3SNL1` (sections 1–4 of `w3snl1md.F90`) to a
standalone Bend program driven by the committed fixture `kokkos/tests/fixtures/snl1_nk25_nth24.bin`,
compare `S` and `D` against the Fortran answers in that file with the L1 gate the Kokkos port
already uses (1e-5 relative, 1e-30 absolute floor), and time it against the
`kokkos/PORT_STATUS.md` row on the same workstation. Budget: five to seven working days for one
person. Warm up on the dispersion Newton solver of `gpu/01_dispersion.f90` first, because it
exercises the toolchain and the F32 transcendentals with no tables and no shared arrays.

**Why W3SNL1.** The repo already has a verbatim Fortran reference, a captured fixture, a
tolerance-justified L1 test, a benchmark protocol and a ledger row for it `(v)`. A Bend version
becomes a third arm in a harness that exists, so the interesting question, *what does the
language cost for the same answer to the same tolerance*, can be answered with numbers.

**Four findings from the Bend sources that shape the plan** (evidence in §3):

1. **Bend 2 has one float type, `F32`, and no `F64`** `(v)`. WW3's spectral state is default
   `REAL`, four bytes `(v)`, so the width matches the kernel as ported; the precision risk is
   operation-level (contraction, libm), not width. §4.
2. **There is no C ABI for calling a pure Bend definition** `(v)`. A native build emits a whole
   program with a `main` and no exported symbols; a library target is an open request
   (bendlang/bend#813). So there is no `snl1_shim.cpp` equivalent: the experiment is a
   standalone reimplementation, and items 2 (shim) and 4 (L2 replay) of the repo's definition
   of done do not apply. §5.
3. **`Array<T>` is a balanced binary tree with a single owner** `(v)`. Indexing is a tree
   walk, and two parallel branches cannot read one array. A gather kernel with dozens of
   table lookups per bin, which is what the DIA is, has to model the extended spectrum as a
   reusable (`+`) tree instead. What that costs is the experiment's central unknown. §3, §6.
4. **The CPU build is `clang -std=c11 -O3 … -lm` with no `-ffp-contract` flag** `(v)`. The
   Kokkos port measured 1.1e-5 relative drift from exactly one fused multiply-add and turned
   contraction off to reach bit-parity; Bend exposes no such knob, so bit-identity should not
   be expected and the L1 tolerance, not `b4b`, is the gate. §4.

A well-measured negative result is a legitimate outcome: "the DIA costs N× the Kokkos serial
time in Bend because of tree indexing" is a finding this learning repo can use.

---

## 2. What Bend 2 is, and what it is not

The name is overloaded. The Bend of 2024 was a Python-looking language over the HVM
interaction-net runtime, with no proofs. The current `bendlang/bend` is a rewrite: the README
states "Bend 2 is a new language. Bend 1 programs and HVM do not carry over" `(v)`, and the
in-tree benchmark headers refer to further internal generations ("Bend3", "Bend4") `(v)`.
What ships today, checked 2026-09-18 `(v)`:

| Fact | Value |
|---|---|
| Repository | `bendlang/bend`, Apache-2.0, ~21.4k stars, compiler in TypeScript, not archived, pushed 2026-09-18 |
| Releases | v2.0.8 (18:51 UTC) and v2.0.9 (18:59 UTC), both 2026-09-18 |
| Self-description | "a fast language that blocks AI mistakes via proof" |
| Three pillars | laws in `LAWS.bend` proven by defs in `PROOF.bend`, checked by the compiler; one C file compiled by clang for the CPU and by CUDA/Metal for the GPU; parallelism by binary fork-join with no threads, locks or kernels |
| Syntax | Python-shaped, dependently typed, affine by default, almost no inference ("everything is annotated") |
| GPU | a `!` after a call (`pow2!(20n)`) hands it and every parallel call inside it to the GPU |
| Platforms | "works best on the back-end, on Linux or macOS"; Linux GPU needs CUDA 12 and clang ≥ 19 |

Purity, affinity and mandatory termination are the design: a `def` must recurse on a
structurally smaller argument or be marked `@unsafe`, there is no `if` (match on `Bool`),
no mutual recursion, and a `match` can only inspect a parameter or a pattern-bound variable `(v)`.

---

## 3. What the sources say

All rows `(v)` on 2026-09-18 from `guide/GUIDE.md` (607 lines), `bend2/base.bend` (2831 lines),
`bend2/main.ts`, `bend2/comp.ts`, `bend2/bend.ts`, the demos and the open issues, unless marked.

### 3.1 Numeric surface

| Item | Finding |
|---|---|
| Number types | `Nat`, `U32`, `F32`. `F32` is `F32{data: Word(32n)}`. `grep -c F64 base.bend` = 0; `grep -c I32` = 0. No signed integer, no double. **F64 history:** exactly one artifact in the repository's history, PR #795 "F64: 64-bit floats — type, laws, literals, host + CUDA lanes", opened 2026-09-18 00:58 UTC by an outside contributor (`author_association: NONE`, 6 files, +155/−2) and closed unmerged by the project lead at 02:40 UTC the same day with zero comments and zero reviews `(v, GitHub API)`. Its body claims a full mirror of the F32 surface, a `1.5d` literal, C and CUDA lanes, 161 cases against a binary64 oracle and an RTX 3090 run ⚠ (the author's claims, unreviewed). It had to guard its helpers with `#ifndef __METAL_VERSION__` because Metal Shading Language has no fp64 `(v, PR body)`: a 64-bit float breaks the "same C file runs on every chip" uniformity the language is built on. No roadmap statement exists either way; the same day's issues (#797, #801) and a commit tightening `F32.read` show the maintainers hardening 32-bit semantics across lanes, not adding a 64-bit one `(v)`. |
| F32 arithmetic | `add sub mul div mod pow neg abs sqrt`; `exp log log2 log10`; `sin cos tan asin acos atan atan2 sinh cosh tanh`; `floor ceil trunc round`; `min max clamp lerp square hypot`; `is_eq … is_ge`. All the primitives are `law` declarations without a `def` (bendlang/bend#827 explains the convention), lowered by `comp.ts` to `(f32)expf(x)`-style C. |
| Comparison | `cmp` (returning `Cmp`) is not defined on `F32`; the `is_lt` family is. Bit operations are `U32` only. |
| Conversions | `U32.to_f32` is numeric, `(f32)(u32)x`. `F32.to_u32` truncates and returns 0 outside `[0, 2^32)` (`comp.ts:430-432`). `F32.bits` reinterprets an `F32` as its `U32` pattern. **There is no `F32.from_bits`.** |
| Text | `F32.show` prints the shortest decimal that `strtof` reads back to the same value (`comp.ts:461-470`), so output is exact. `F32.read` is `strtof` and rejects strings containing `x`, `X` or `(`, so hex-float input is out (`comp.ts:499-500`). A float literal in source is `Math.fround(Number(text))` (`bend.ts:2253`): decimal → double → float, which can double-round ⚠ in rare cases. |
| Constants | `F32.pi()` is the literal `3.14159265`; WW3's `PI` is the double `3.141592653589793` rounded to `REAL` once (`real.hpp`). The two may differ in the last bit ⚠; the fixture carries `xfr`, `dth`, `lam` and the tables, so the kernel need not compute `PI`. |

**Consequence.** Every function the `W3SNL1` body needs exists: the per-call kernel is
`MAX`, `EXP` and arithmetic `(v, snl1_dia.cpp section 1)`. The transcendentals `INSNL1` needs
(`ACOS`, `ASIN`, `SIN`, `LOG`, `**`) also exist, but `INSNL1` need not be ported at all: its
output is integer tables plus `AF11`, all stored in the fixture and already verified exactly
by `L1_test_snl1_tables` `(v)`.

### 3.2 Data

| Item | Finding |
|---|---|
| `Array<T>` | `ALeaf{value}` / `ANode{xs, ys}`, a balanced binary tree of 2^d slots (`base.bend:67-69`); `[v : T*8n]` or `[v : T^3n]`. It is a `Type`: exactly one owner, a read hands the array back beside the element, `a[i] <- v` rewrites in place. `Array.get`/`Array.swap` descend one node per level (`base.bend:2222-2235`), so an index costs a tree walk, not pointer arithmetic. Indexes wrap. The `a[i]` sugar assumes `Array<U32>`; other element types call `Array.get`/`Array.set`. The guide says this "will be generalized soon". |
| Sharing | Reusable values are `+` and carry a reference count; only `Data`-kinded values can be `+`. `Array<T>` is `Type`, so it can never be `+`: two parallel branches cannot read the same array. The numeric benches in the tree do not use `Array` at all: `nbody` keeps a system's state in 21 scalar parameters, `tree-matmul` represents a matrix as a `Data` quad-tree shared through `+` binders. |
| Flat buffers | None. There is no contiguous numeric buffer type, no slice, no view. `File.read_bytes` returns `List<&2, U32>`, one element per byte. |
| Sizes | Arrays are powers of two. The fixture's working set is `UE` of 792 and eight arrays of 720 floats (`nspecy + nth`, `nspecx + nth`, read from the fixture header `(v)`), so a Bend version pads to 1024 slots each. |

### 3.3 Parallelism and the GPU

| Item | Finding |
|---|---|
| Primitive | `a b = f(x) g(y)` is a parallel let; the compiler is promised the two calls are independent (purity guarantees it) and take roughly equal time (the programmer's job). |
| Scheduler | "a contention-free, binary fork-join machine: every task is handed to a core exactly once and never moved afterwards". No work stealing, so an unbalanced tree wastes cores. |
| GPU dispatch | `f!(x)` runs that call and every parallel call inside it on the GPU. The same C file is the kernel; a binary that uses `!` carries a `.gpu` program beside it. The heap is unified; on discrete cards the copy cost is not documented ⚠. |
| Defaults | The GPU is **on by default** when present; `--gpu off` is the CPU baseline and `--threads N` sets CPU threads (issue #826, from the binary's own `--help`; the guide's line "enables the GPU" is wrong). |
| Tuning | Fork depth for a `!` call changes GPU time by up to 8×, and the same sweep is flat on the CPU (issue #828, Apple M5/Metal, voxel raycaster). The optimum is per program. |
| Guidance | "The GPU shines on uniform numeric work like mandelbrot or nbody; divergent work like n-queens stays faster on the CPU." |
| Toolchain | Linux: CUDA 12 at `$CUDA_HOME` or `/usr/local/cuda` with `nvrtc.h` present, otherwise `!` runs on the cores (`main.ts:333-341`); clang ≥ 19 for a `!` program, ≥ 14 otherwise; `CC` from the environment is tried first (`main.ts:309`). |
| Flags | CPU: `clang -std=c11 -O3 file -lpthread -lm` (`main.ts:344`). GPU adds `-DBEND_CUDA=1 -lcuda -lnvrtc`. No `-ffp-contract`, no `-ffast-math`, no `--fmad` anywhere in `main.ts`. |

### 3.4 Interop and IO

| Item | Finding |
|---|---|
| Bend → C | An effect is a `def` returning `IO(...)` whose body is `import "./x.c"` plus a `.js` twin; only the event loop runs host code, "so proofs, termination and the GPU never touch host code". The C side (signatures, ownership, handles) is undocumented; issue #825 derives it from `comp.ts` and reports a custom handle arriving as 0. |
| C → Bend | Not offered. `bend x.bend -o x.c` emits a whole program; `nm` on the binary shows `main` and nothing else (issue #813's probe on 2.0.5). JavaScript can `import` a `.bend` module and call its non-IO defs; C, Fortran and Rust cannot. |
| Files | `File.open/read/read_bytes/write/close`, `IO.args`, `IO.now` (milliseconds, `Nat`), `IO.print`. Enough to read a text fixture and print results. |
| Termination in IO | A loop bounded by the outside world counts down a `Nat` fuel argument. |

### 3.5 Proofs

A law is a type; a proof is a `def` of that type; `{==}` closes a goal whose two sides
compute to the same term; `%e : P` rewrites; there are no tactics `(v)`. Every demo's laws
are over `Nat`, `Bool`, lists and game states; `demos/proof_numerics` proves commutativity,
associativity, distributivity and Euclid's division **for `Nat`**, not for `F32` `(v)`. Nothing
in the tree states or proves a property of a floating-point computation `(v)`.

### 3.6 Open issues that bear on numeric use (all open, 2026-09-18 `(v)`)

| # | Title | Why it matters here |
|---|---|---|
| 813 | Add a native library target for pure definitions | the missing C ABI (§5) |
| 825 | The guide does not document the native C side of custom effects | the other direction is undocumented too |
| 826 | `--gpu 4GB` "enables the GPU" is wrong; `--gpu off` is the undocumented CPU baseline | every timing table needs `--gpu off` rows |
| 828 | Fork depth for a `!` call changes GPU frame time by up to 8×; flat on the CPU | a fork-depth sweep is part of the benchmark, not a tuning afterthought |
| 827 | O(1) primitives are `law`, invisible to `grep '^def'` | why `grep F32` finds no `def`s: the primitives exist |
| 824 | Metal: U32 remainder wrong for one input | a correctness bug in an arithmetic primitive, on the other GPU backend |
| 822 | Install script fails: no release from bend-lang.com | the documented installer was broken on the day of writing |

---

## 4. Precision: the headline risk, stated correctly

**Correct the premise first.** WW3 is not double-precision Fortran. The spectral state and
the physics run in default `REAL`, four bytes: `constants.F90` uses plain default `REAL`
`(v, KOKKOS_H100_PLAN §3.2)`, `real.hpp` sets `using Real = float` "matching WW3's default
REAL" `(v)`, `03_precision.f90` says "WW3 computes mostly in default 4-byte real" `(v)`, the
fixture is `float32` throughout `(v, fixtures/README.md)`, and `snl1_ref.F90` contains no
`REAL(8)`, `DBLE` or `D0` literal `(v, grep)`. A build with `-r8` is the exception, and the
shim refuses it by design `(v, ww_kokkos_c.hpp rule 2)`.

So Bend's `F32`-only surface is **width-matched** to the DIA as this repo ports it. Where WW3
does use `REAL(8)` (some integral parameters, per `AGENTS_KOKKOS` §1.2 rule 7 ⚠ which routines),
Bend cannot follow, and that alone removes it from consideration for those routines. For this
kernel the risk is elsewhere.

**Design for F32 permanently.** F64 has been implemented once, by an outsider, and closed
without comment (§3.1); there is no public plan, and the Metal backend cannot have it. That
patch is preserved on our fork's `f64` branch (§7.4), which changes nothing here: it is an
optional second arm, not a dependency, and every gate below is a float32 gate. The
experiment therefore does not wait for a wider type: every gate below is a tolerance against
the float32 Fortran reference, and a precision-driven failure is a reportable result, not a
"try again when F64 lands". Even if F64 existed, the workstation's GeForce part runs fp64 at a
small fraction of its fp32 rate (`gpu/03_precision.f90` says about 1/64 for the RTX 4090 ⚠
not measured here), so the GPU row would be the wrong place to want it.

**Three sources of last-bit drift, in order of certainty:**

1. **Contraction.** Bend compiles with clang at `-O3` and passes no `-ffp-contract` `(v,
   main.ts:344)`. clang's default for C is to fuse `a*b + c` within an expression ⚠ (clang
   documentation, not re-read today). The Kokkos port measured exactly this: GCC's default
   contraction fused `AWG1*UE(..) + AWG2*UE(..)` into an FMA, the `openmp-release` build drifted
   **1.1e-5 relative** from the fixture, and `-ffp-contract=off` restored bit-identity on all three
   presets `(v, PORT_STATUS.md, course/12)`. Bend has no documented flag for this. One lever
   exists but is untested ⚠: `CC` is honoured, so a wrapper script that appends
   `-ffp-contract=off` may work for the CPU build; the GPU program goes through NVRTC, whose
   default is `--fmad=true` ⚠.
2. **libm.** Bend's `F32.exp` is `(f32)exp(f32)` in C linked with `-lm` `(v, comp.ts:228)`;
   whether that is `expf` or `exp` on a double is decided by the emitted cast and by clang ⚠.
   gfortran's `EXP` on a `REAL` calls `expf` ⚠. Same host libm, but not necessarily the same
   entry point. On the GPU, CUDA's `expf` is a different implementation with its own ULP
   bound ⚠.
3. **Evaluation order.** Section 4 of the kernel is a sum of eighteen products `(v,
   snl1_dia.cpp)`. The port keeps the Fortran's order; a Bend version must too, and the affine
   discipline makes a "helpful" regrouping easy to introduce while satisfying the type checker.

**What this does to the repo's vocabulary.** `b4b` / "bit-identical" is the criterion of
ladder steps 1–2 and what the `PORT_STATUS.md` L1 column reports `(v, GLOSSARY)`; it was
*achieved* by the Kokkos port but the *gate* is `L1_test_snl1_dia`'s 1e-5 relative with a 1e-30
floor `(v)`. A Bend row reports the measured maximum relative error of `S` and `D`, never
"bit-identical", and passes or fails the same 1e-5 gate. Two honest sub-outcomes:

- CPU (`--gpu off`) within 1e-5: the language reproduces WW3 arithmetic to the gate the port
  set. If the maximum error lands near 1.1e-5, the FMA explanation is testable with the `CC`
  wrapper.
- CPU fails 1e-5 by a small margin with an identified cause (contraction with no knob): a
  clean negative about *control*, not about correctness; report it as such.

**What this does to the experiment's value.** Passing the gate says nothing WW3 does not
already know. The value is the *cost* of reaching it: lines of Bend per line of Fortran, the
tree-array penalty, the fork-depth sweep, and whether a proof-checked language can express the
kernel without `@unsafe`. Those are the numbers to write down.

---

## 5. Interop: there is no shim

The Kokkos arm is called from Fortran through `ww_kokkos_c.hpp` / `snl1_shim.cpp` /
`w3kokkosmd.F90`, a `bind(C)` boundary that owns the runtime, the buffer lifetime and the
error channel `(v, course/12)`. None of that is possible with Bend today `(v, §3.4)`: a
compiled Bend program is a program, and the only foreign direction is Bend calling C for
effects. Bendlang/bend#813 asks for exactly the library target a shim would need and is open.

So the experiment has the shape of `kokkos/tests/L1_test_snl1_dia.cpp`, not of the shim:

```
fixture (.bin) ──dump──► text (decimal, ≤9 sig. digits)
                              │
                     bend snl1.bend  (reads text, computes S and D, prints their F32.bits)
                              │
                  compare (C++, reuses fixture_io) ──► max |Δ|/max(|ref|, 1e-30) per point
```

- **In.** A small C++ tool over `ww::fixture::load()` writes the header, `sig`, the 32 index
  tables, `af11`, `awg`, `swg` and the per-point `kdmean`, `cg`, `a`, `s`, `d` as text. The
  index tables go out 0-based and pre-shifted by `nth` (the port's scratch-slot convention `(v,
  course/12)`), so the Bend side never sees a negative index; `U32` has no sign `(v)`. The
  Bend program reads them with `File.read` and `F32.read` / `U32.read`.
- **Round-trip calibration, first thing.** The Bend program prints `F32.bits` of every input
  it read; the comparator checks them against the fixture bytes. Required: zero mismatches.
  This is what makes `strtof`'s decimal parsing ⚠ a verified fact for this fixture rather than
  an assumption, and it costs an hour.
- **Out.** `S` and `D` printed as `U32.show(F32.bits(x))`, exact `(v)`. The comparator
  reinterprets and applies the L1 formula.
- **Alternative rejected.** Generating a `.bend` module with the data as literals avoids file
  IO but pays the double-rounding ⚠ of literal parsing and a slower check on a large file
  (a `main` returning a value is "normalized by the checker (slow for big work)" `(v)`); a
  `main` returning `IO` is compiled.

---

## 6. Candidate kernels, ranked

| # | Kernel | Divide-and-conquer fit | Harness in the repo | Verdict |
|---|---|---|---|---|
| 1 | `W3SNL1` body (sections 1–4) | **Medium.** Parallel over `nspecx` bins in section 3 and `nspec` bins in section 4 is a balanced tree; parallel over points is another. Each bin performs sixteen indexed reads of `UE` through the `ip`/`im` tables in section 3 and thirty-two reads of `SA*`/`DA*` through `ic` in section 4 `(v, snl1_dia.cpp)`, all from arrays every branch must read, so the arrays must be `+` `Data` trees and every read is a tree walk. Section 2's high-frequency tail reads the row below and stays a sequential fold `(v)`. No reduction anywhere `(v)`. | Fortran reference, fixture, L1 test with justified tolerance, `ww_bench_snl1`, ledger row: all present `(v)`. | **The target.** Agrees with the premise of this proposal, with the tree-array cost as the thing to measure. |
| 2 | Dispersion Newton, `gpu/01_dispersion.f90` | **Best.** Independent over (point, frequency), a handful of scalars per leaf, `tanh`, `sqrt`, division, fixed eight iterations `(v)`. Exactly the "uniform numeric work" the guide says the GPU is for. | None: the program prints a residual and two values; `bench/README.md` says nothing in `gpu/` or `bench/` was run ⚠. A twenty-line gfortran driver printing `TRANSFER(k, 0)` bit patterns would make one. | **Warm-up, day 1.** Cheapest possible answer to "does Bend's `F32.tanh` agree with gfortran's, and does `!` dispatch on the 4090 under nix". It is WW3-shaped (`WAVNU1` in `w3dispmd.F90` ⚠ not opened; the `WW3/` submodule is empty in a fresh clone) but not WW3 code. |
| 3 | `kernel_bench.f90` proxy (`Sin − Sds` sub-stepping) | Good: uniform trip count, `sqrt` only, per-bin independent `(v)`. | A checksum, no reference. | A proxy of a proxy; skip unless 1 and 2 leave time. |
| 4 | `W3SIN4` / `W3SDS4` | Reductions for the integral parameters (`team_reduce` in the Kokkos plan `(v, course/13)`): a fold is natural in Bend but changes summation order. | Not ported, no fixture `(v, PORT_STATUS)`. | Not first. |
| 5 | `W3QCK3` / `W3XYP3` propagation | Poor: 1-D sweeps with sequential dependence, halo stencils, memory-bound `(v, AGENTS_KOKKOS §2.2)`. | None. | No. |

---

## 7. The experiment

### 7.1 Where it lives

A `bend/` directory at the repo root, beside `gpu/` (the nvfortran sandbox) and `kokkos/`,
holding only source and a README; results are appended to this document as a dated §12 so the
plan and its outcome stay in one place. Nothing under `kokkos/` changes except one host tool:

| Path | What |
|---|---|
| `bend/README.md` | how to build and run, the toolchain versions used, the `--gpu off` rule |
| `bend/dispersion/main.bend` | step 1 |
| `bend/snl1/main.bend` | step 2, the `W3SNL1` body; heritage header naming `w3snl1md.F90:115-473`, LGPL-3.0-or-later like `snl1_dia.cpp` `(v, course/13)` |
| `bend/snl1/LAWS.bend`, `PROOF.bend` | step 4 |
| `kokkos/tools/fixture_dump/` | the fixture → text dumper and the bits comparator, C++ over `fixture_io`, built with the tree so `just kokkos-test` keeps them compiling |
| `justfile` | `bend-dispersion`, `bend-snl1`, `bend-snl1-bench` recipes; each fails with a message if `bend` is not on `PATH` |
| `bend-lang/` | the compiler itself: a submodule of `h0ffmann/bend`, tracking `main` (§7.4). Not ours to edit |

Toolchain rule: build Bend **from the `bend-lang/` submodule** (§7.4), or install it on the
host from a pinned release asset by hand, and record `bend --version` in `bend/README.md`. No
script in this repo pipes the installer to a shell; the installer was failing on the day of
writing anyway (issue #822). CI does not run Bend.

Note the two directories are different things and neither is the other: `bend-lang/` is the
compiler, a submodule of a fork we do not write; `bend/` is our own Bend source, the only part
of this experiment that is ours.

### 7.2 Steps

| Step | Deliverable | Gate | Est. |
|---|---|---|---|
| 0 | Bend on the workstation; `clang --version` ≥ 19; `CUDA_HOME` pointing at a CUDA 12 with `nvrtc.h`; `pow2!(20n)` from the guide runs with and without `--gpu off` | prints 1048576 both ways | 0.5 d |
| 1 | Dispersion: `main.bend` over the same `(depth, sigma)` grid as `01_dispersion.f90`; a gfortran driver printing bit patterns; comparator | max relative error of `k` vs gfortran `-O3`, and vs `-O3 -ffp-contract=off`; CPU 1 thread, 32 threads, GPU times | 1 d |
| 2 | Round-trip: dumper, `File.read` in Bend, `F32.bits` echo | 0 mismatches over the whole fixture | 0.5 d |
| 3 | `W3SNL1` body in Bend; `S`, `D` for the three points | max relative error vs the fixture, per point, with the 1e-5 / 1e-30 formula; the two L1 property checks re-run through the same program: zero spectrum → zero `S`, `D`; `A×2` → `S×8`, `D×4` to 1e-4 `(v, L1_test_snl1_dia.cpp)` | 2–3 d |
| 4 | Timing, protocol of `ww_bench_snl1`: 1 000 points (the three tiled), 20 calls after 3 warm-ups, median of three runs, `IO.now` inside the process `(v, PORT_STATUS)`; rows `--gpu off --threads 1`, `--gpu off --threads 32`, GPU; a fork-depth sweep for the GPU row (issue #828) | a table beside the ledger's 24.96 / 5.02 / 0.047 ms kernel-only row, same machine (i9-14900 + RTX 4090) | 1 d |
| 5 | One law and its proof (§8) | `bend PROOF.bend` prints "All terms check." or the reason it cannot | 0.5–1 d |
| 6 | §12 of this file: numbers, versions, what failed | every number `(v)` | 0.5 d |

Reproducibility rules from `bench/README.md` apply: wall time, medians, clocks reported,
"never report a speedup you haven't checked" `(v)`.

### 7.3 What counts as what

**Success:** steps 2 and 3 pass their gates on `--gpu off`, and step 4 produces the three rows.
The GPU row is reported whatever it is.

**Clean negative (a result, written up the same way):** any of
- the kernel cannot be expressed without `@unsafe`, with the construct named;
- the CPU error exceeds 1e-5 with the cause identified (contraction, libm entry point,
  evaluation order), or unidentified after the `CC` wrapper test;
- the GPU row is slower than the CPU row, with the fork-depth sweep attached;
- the tree-array cost makes the CPU row more than an order of magnitude slower than the Kokkos
  serial kernel, with a profile of where the time goes.

**Not a result:** "it did not build" without the compiler's message and the Bend version, or a
timing without the `--gpu off` control (issue #826 documents exactly that mistake).

**What does not change.** `kokkos/PORT_STATUS.md` keeps its rule that a row claims only what a
command in this repo reproduces `(v)`; a Bend row would need Bend in the toolchain, which it
is not, so the numbers live here, not there.

### 7.4 The compiler: a submodule of our own fork

`bend-lang/` is `h0ffmann/bend`, a fork of `bendlang/bend`, wired in the way `WW3/` and
`nix-config/` already are: `branch = main`, `shallow = true` `(v, .gitmodules)`. Initialise it
with `git submodule update --init --depth 1 bend-lang`; it is about 76 MB, mostly the README's
animated media `(v, measured)`.

**Why a fork and not the upstream URL.** Two reasons, and only the second is specific to Bend.
A fork pins a mirror we control, so a force-push or a yanked release upstream cannot take the
toolchain out from under a half-finished experiment — upstream force-pushes `main`, which is
visible in this project's own PR history `(v, a base_ref_force_pushed event on #795)`. And the
fork is where a patch upstream will not take can live.

**What the fork carries.** Branch `f64`, at `ee3832e`, is the head of upstream PR #795 —
64-bit floats, closed unmerged by the project lead 1h42m after it was opened, with no review
and no comment `(v, API, 2026-09-18)`. The implementation is real and is preserved on that
branch: commit `492ea6b` is +413/-3 across `bend2/base.bend`, `bend2/bend.ts` and
`bend2/comp.ts`, and `base.bend` at the branch tip has 136 occurrences of `F64` against 0 on
`main` `(v, checked at both refs)`. The PR's own file list on GitHub shows only the five test
specimens and a gates cap bump, which is an artifact of the base force-push, not the state of
the branch ⚠ (the cause was not traced further; the branch content is what was checked).

**Why the submodule tracks `main`, not `f64`.** The default build must be stock Bend, so that
every number this experiment reports is a number about the language as it actually ships. F64
is one `git -C bend-lang checkout f64` away, and any result produced that way is labelled as
coming from a patched compiler or it is not a result.

**What F64 is and is not for, here.** It is *not* a prerequisite: §4 establishes that WW3's
DIA is float32 throughout, so stock Bend is already width-matched to the kernel this plan
ports. Carrying the patch buys two things — a double-precision reference arm computed inside
Bend, which turns "is this drift ours or the language's?" into a measurement rather than an
argument; and an answer to the F64 question for any *other* WW3 routine that does use
`REAL(8)`. Neither is on the critical path. If the rebase cost ever exceeds that value, the
branch stays where it is as a record and the submodule keeps tracking `main`.

**The rebase burden, measured.** Upstream `main` is 49 commits ahead of the PR's base
(`46df6be`), touching 38 files `(v, compare API, 2026-09-18)`, and the patch edits three of
the hottest files in the compiler. Upstream also shipped v2.0.8 and v2.0.9 eight minutes apart
on that same day, and spent it tightening F32 semantics across lanes (issues #797, #801). A
rebase is therefore expected to conflict, and the branch is not expected to stay green without
work ⚠ — no rebase was attempted here.

---

### 7.5 Porting Bend to F64: what it would actually take

Written down because the fork makes it a live option, not because this experiment needs it
(§4: the DIA is float32). Everything here is from `WONTFIX.txt`, `AGENTS.md` and
`gates/repo.ts` at upstream `main`, read 2026-09-18 `(v)`.

**F64 is not refused.** `WONTFIX.txt` is an explicit list of what the project will not do,
sorted into DESIGN, CAPACITY, OPEN, RUNTIME and SOON. F64 appears nowhere in it `(v, grep)`.
Neither the type nor 64-bit arithmetic is a declared non-goal, which is worth knowing before
reading the close of #795 as a verdict on the idea.

**The likely reason a finished patch was closed anyway** is the OPEN section's own heading:
"no solution we trust yet; *patches would cost us control of the codebase*" `(v, verbatim)`.
That is a statement about who writes the compiler, not about what it should contain. A 413-line
outside patch across the three central files is the exact shape that sentence rejects. Any
attempt to land F64 upstream should assume the constraint is authorship and review bandwidth,
and propose accordingly — smallest possible diff, one file at a time, with the theory
unchanged.

**The hard constraint: `bend2/bend.ts` is off limits.** `AGENTS.md` says it "is the language
(parser, theory, checker) and is human-written: do not edit it" `(v, verbatim)`. PR #795 edited
it — about 45 lines, by its author's own account, purely to add the `d` literal suffix to the
`NUMBER` rule and the two bit-conversion helpers. So the single cheapest change to make an F64
patch acceptable is **to ship no literal syntax at all**: no `2d`, no `1.5e3d`. Values would be
built with `F32.to_f64` and `U32.to_f64`-style conversions, or read from text with `F64.read`,
and the parser would not move. That is uglier for a human writing constants and almost
irrelevant for a generated numeric kernel, which is the use case that wants F64 in the first
place.

**The size gates are real and already conflict.** `gates/repo.ts` caps every file by ttok:
`bend2/base.bend` 24000, `bend2/comp.ts` 61500, `bend2/bend.ts` 41000 `(v)`. PR #795 bumped
base to 25000 and comp.ts from 61000 to 63000; upstream has since moved comp.ts's own cap to
61500 on `main` `(v)`. A rebase therefore conflicts on the gate file itself, in addition to the
three source files — the mechanical part of the 49-commit gap in §7.4.

**Four lanes, and one of them cannot have it.** `comp.ts` carries the C, Metal, CUDA and JS
runtimes from one source `(v, AGENTS.md)`. Metal Shading Language has no fp64, which is why
#795's helpers are `#ifndef __METAL_VERSION__`-guarded, and `WONTFIX.txt` fixes the backend set
at "Metal and CUDA only" `(v)` — so the gap cannot be closed by adding a lane. F64 is therefore
permanently a type that exists on three of four backends, and a program using it is not
portable across the language's own targets. That is a genuine design cost to the maintainer,
not an implementation detail, and it is the strongest technical argument against the feature.

**The JS lane inverts.** A JS number *is* an IEEE binary64, so F64 would be exact there for
free, while F32 needs `Math.fround` and, per `WONTFIX.txt`, still loses NaN payload bits
because a bit-exact F32 measured 7-8x slower `(v)`. F64 is the type that lane represents
natively. Worth stating in any upstream proposal: it removes a documented infidelity rather
than adding one.

**What was not established** ⚠: whether a new primitive type obliges a change to
`bend2/bend.lean`, the Lean mechanization of the core (#795 did not touch it, which is either
correct or an omission — not checked); whether NVRTC compiles the `double` helpers at the
optimization settings Bend uses; and what fp64 costs on the target card beyond the ~1/64 figure
`03_precision.f90` quotes for the RTX 4090.

**If we carry it anyway.** Keep `f64` as a record, not a maintained branch: rebase it only when
an experiment actually needs the arm, accept that `gates/repo.ts` and the three source files
will conflict each time, and never let a number measured on the patched compiler into a table
without saying so. The alternative that costs nothing upstream — compensated summation in F32
(Kahan or two-sum) where a reduction is what drifts — is the first thing to try if the DIA's
error turns out to be accumulation rather than contraction ⚠ (not evaluated here).

---

---

## 8. `LAWS.bend` for a numeric kernel: what is realistic

The pitch is that laws make "implement a function that sorts a list" precise and
machine-checked `(v, guide)`. For `W3SNL1` the candidate laws sort into three bins.

**Held by construction, no law needed `(v)`.** Termination (every non-`@unsafe` def), absence
of data races (affinity: two branches cannot alias a mutable value), no out-of-bounds read in
the C sense (indexes wrap). `L1_test_snl1_dia.IsReproducibleAcrossLaunches` exists to catch a
race `(v)`; in Bend that test is redundant on one device, though not across CPU and GPU, where
the arithmetic differs.

**Statable and provable, but about integers.** "Every pre-shifted table entry of *this*
fixture lies in `[0, 1024)`" is a closed claim over `U32` values that `{==}` decides by
evaluation; "the output has `nspec` entries" likewise. These mirror what `L1_test_snl1_tables`
checks exactly `(v)`. They are cheap, and they are the honest scope of step 5.

**Statable, not provable, and mostly false.** Every property a wave modeller would want:
- *Zero spectrum gives zero source.* True in exact arithmetic; in IEEE, `0 × inf` is NaN, and
  `F32` primitives are opaque `law`s with no definitional unfolding to induct on ⚠ (inferred
  from `base.bend`; the checker can evaluate closed terms, not quantify over 2^32 words).
- *`S` is cubic in `A`.* False to the last bit; the C++ test uses 1e-4 `(v)`. A law is exact.
- *Action conservation.* The DIA is not exactly conservative in discrete float arithmetic, and
  the kernel has no summation to state it over `(v, no reduction)`.
- *Non-negativity of the spectrum.* Not a property of `Snl` at all: `S` is signed by design,
  and positivity of `A` is enforced by the limiter in `W3SRCE`, not here `(v, AGENTS_KOKKOS §2.2)`.

**Verdict.** Do not oversell this. For floating-point physics the proof layer adds
termination and race-freedom, both real, and nothing about the numbers. A law about energy
conservation would be false before it was unprovable. The place proofs could earn their keep
is the integer machinery of `INSNL1` (index windows, direction wrap-around), which is also the
part this tryout deliberately does not port.

---

## 9. Effort and recommendation

**Effort.** Five to seven working days, one person, steps 0–6 above, on the workstation the
ledger was measured on. Add a day if CUDA under nix does not land at a path Bend's build
accepts (`CUDA_HOME` is honoured `(v)`). Add unknown time for version churn: two releases in
eight minutes on the day of writing `(v)`, and the guide's own "will be generalized soon" on
arrays.

**Recommendation: do it, time-boxed, as written.** Reasons for: the harness exists, so the
answer is numbers rather than impressions; the negative outcomes are informative; it is one
week. Reasons it is *only* a tryout, each `(v)`: no C ABI (#813), `F32` only, tree arrays with a
single owner, and a toolchain that changes daily. Any one of those keeps Bend off the port's
route list; together they make the question "could Bend be an arm of the port" already
answered, and the remaining question "what does it cost to say the same thing" worth one week.

**Revisit when** all three hold: a native library target exists (#813 closed), a flat
`Data`-shareable numeric buffer exists, and `F64` exists — the last has been submitted once and
rejected without a stated reason (PR #795 `(v)`), so do not plan around it. Any two are not enough.

---

## 10. Open questions and what was not verified

- ⚠ Whether `CC=<wrapper adding -ffp-contract=off>` is respected end to end, and what NVRTC's
  contraction default is in Bend's GPU build. Step 3 tests the first.
- ⚠ Whether Bend's `F32.exp` lowers to `expf` or to `exp` on a promoted double; `comp.ts:228`
  shows a `(f32)` cast around a `$o` call, the macro expansion was not traced.
- ⚠ `strtof` correctness for 9-digit decimals on the workstation's libc, and the
  double-rounding of source literals. Step 2 makes the first moot for this fixture.
- ⚠ The cost of `Array.get` on a `+` `Data` tree versus the sugar on `Array<U32>`; nothing
  in the tree benchmarks a gather. This is the experiment.
- ⚠ Host↔device movement on a discrete GPU with Bend's "unified heap": documented for Apple
  unified memory only. The Kokkos row's 94 % transfer share `(v)` is the number to compare
  against, and Bend gives no shim/kernel split to measure it with.
- ⚠ Whether `!` builds at all under the pinned nix shell (clang ≥ 19 and CUDA 12 at
  `$CUDA_HOME`); the shell pins gfortran 15.3, not clang `(v, README)`.
- ⚠ `WAVNU1`'s actual algorithm in `w3dispmd.F90`; `gpu/01_dispersion.f90` is the lab's own
  Newton solver, and the `WW3/` submodule was not initialised for this review.
- ⚠ `paper/BendRT.pdf` (the runtime design and its benchmarks) and `paper/BendTT.pdf` were not
  read; all runtime claims here come from the guide, `main.ts` and the issues.
- ⚠ The README's speed charts ("as fast as C on the CPU, as fast as CUDA on the GPU") are the
  project's own; no third-party measurement was found and none was attempted.
- The Bend bench headers refer to "Bend3" and "Bend4" while releases are 2.0.x `(v)`; which
  generation the guide describes was not resolved and does not matter for this plan, but it is
  a sign of how fast the ground moves.

---

## 11. Sources

Bend, all fetched 2026-09-18 from `github.com/bendlang/bend` at `main`:
`README.md`; `guide/GUIDE.md`; `bend2/base.bend`; `bend2/main.ts` (`cc_find`, `cli_build`);
`bend2/comp.ts` (the `u32_to_f32`, `f32_to_u32`, `f32_bits`, `f32_show`, `f32_read` lowerings);
`bend2/bend.ts:2253`; `demos/proof_numerics/{LAWS,PROOF,main}.bend`; `demos/pure_par_sum/`;
`demos/app_ray_tracer_3d/main.bend`; `bench/runtime/nbody/main.bend`;
`bench/runtime/tree-matmul/main.bend`; `tests/io/foreign_types.c`; issues #797, #801, #813,
#822, #824, #825, #826, #827, #828; PR #795 (state, timestamps, body); the repository metadata
and releases from the GitHub API.

This repository, read in the same session: `README.md`, `CONTRIBUTING.md`, `justfile`,
`course/12-porting-a-kernel-w3snl1.md`, `course/13-bulk-porting-with-agents.md`,
`kokkos/PORT_STATUS.md`, `kokkos/src/ww_kokkos/{real.hpp,snl1_dia.cpp,snl1_tables.cpp,spectrum_fixtures.hpp,fixture_io.hpp}`,
`kokkos/src/fortran_iface/{ww_kokkos_c.hpp,snl1_shim.cpp}`, `kokkos/tests/{L1_test_snl1_dia.cpp,bench_snl1.cpp}`,
`kokkos/tests/fixtures/{README.md,snl1_ref.F90,snl1_nk25_nth24.bin}` (header decoded),
`bench/README.md`, `bench/kernel_bench.f90`, `gpu/01_dispersion.f90`, `gpu/03_precision.f90`,
`docs/KOKKOS_H100_PLAN_202609.md`, `docs/AGENTS_KOKKOS_202609.md`, `docs/GLOSSARY.md`.
