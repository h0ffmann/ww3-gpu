# 12. Porting a kernel: `W3SNL1`

Step 4 of the ladder, done once, end to end, on the routine lesson 07 nominated: the DIA
nonlinear interactions. Everything here is in `kokkos/`; keep it open. The phase-1 rule is
*translate, do not improve*: same expressions, same order, same float32 arithmetic. The
only thing that changes is who executes them (v, `kokkos/src/ww_kokkos/snl1_dia.cpp`).

## Source map

Source of truth: `WW3/model/src/w3snl1md.F90` at 7.14 `develop`; line numbers (v, checked in
`~/src/WW3`, and in the header of the lab's standalone copy before ADR-0010 removed it). The C++ keeps the Fortran's numbered section comments:

| Fortran | Lines | Sections | Port |
|---|---|---|---|
| `INSNL1` | 483–786; body 602–774 | 1 quadruplet angles · 2 lambda weights · 3 directional indices · 4 frequency indices · 5 ranges · 6 allocate (`W3DMNL`) · 7 spectral addresses · 8 `f**11` scaling · 9 interpolation weights | `snl1_tables.cpp`, `make_tables()`: host, once per grid |
| `W3SNL1` | 115–473; locals 306–328; body 338–440 | 1 propagation constant · 2 auxiliary spectrum and arrays · 3 interactions on the extended spectrum · 4 source and diagonal | `snl1_dia.cpp`, `snl1()`: device, per call |

## `Config` and `Tables`

`ww::snl1::Config` (v, `snl1_config.hpp`) is exactly the `W3GDATMD` inputs, under WW3's
names: `nk, nth, xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, snls3, fachfe`, plus
`nspec()`; `SIG` travels separately (host for `INSNL1`, device for `W3SNL1`). `PI`, `TPI`,
`TPIINV` are built as `constants.F90` builds them, `PI` rounded to `REAL` first (v).
`ww::snl1::Tables` (v, `snl1_tables.hpp`) is `INSNL1`'s output: `nfr, nfrhgh, nfrchg,
nspecx, nspecy, nspec`, `dal1..3`, `awg[8]`, `swg[8]`, and 33 device Views: `ip[2][4]`
and `im[2][4]` of length `NSPECX`, `ic[8][2]` of length `NSPEC`, `af11` of length `NSPECX`.
Arrays of Views because that is how `W3SNL1` reads them, and because a View is a copyable
handle, so the whole struct is captured by value into the kernel (v).

## Index conventions

| Fortran | Port | Why |
|---|---|---|
| `ISP = ITH + (IFR-1)*NTH`, all 1-based | `isp = ith + ifr*nth`, all 0-based | every table entry is the Fortran value minus one (v) |
| `IF3..IF6` clamped to 0 → addresses down to `1-NTH` | entries as low as `-NTH` are legal; the removed `L1_test_snl1_tables` checked the window `[-nth, nspecy)` explicitly | `INSNL1` section 7 (v) |
| `UE(1-NTH:NSPECY)`, `SA1(1-NTH:NSPECX)` … | scratch of length `upper + NTH`; Fortran index `J` at slot `J-1+NTH`; a table entry `j` read at slot `j+NTH` | the `1-NTH` lower bound becomes slot 0 (v) |
| `A(NSPEC)` one point per call | `a(nspec, npts)` `LayoutLeft`, column `ip` is one contiguous spectrum | a batch of points per launch, memory a Fortran caller owns (v, `snl1_dia.hpp`) |

## Kernel structure

One **team per sea point** (`TeamPolicy(npts, Kokkos::AUTO)`), because the working set,
an extended spectrum plus nine helper arrays, is per point and belongs in team scratch.
The budget `(NSPECY + NTH) + 8·(NSPECX + NTH) + NSPEC` floats is checked against
`policy.scratch_size_max(0)` before the launch; too big throws `std::runtime_error` (v). Inside the team (v):

1. **Section 1**: `CONS` from `KDMEAN`: three flops, recomputed per thread; cheaper than a barrier.
2. **Section 2**: `TeamThreadRange` over `nfr` fills `ue` and `con`; a second range
   zeroes slots `[0, NTH)` (the Fortran `DO ISP=1-NTH,0`); `team_barrier()`. The tail
   `DO IFR=NFR+1,NFRHGH` stays **sequential** in `ifr`, a barrier per row: row `IFR` reads row `IFR-1`.
3. **Section 3**: `TeamThreadRange` over `nspecx`: `EP1, EM1, EP2, EM2`, then `SA1, SA2, DA1C..DA2M`; `team_barrier()`.
4. **Section 4**: `TeamThreadRange` over `nspec` writes `s(isp, ipt)` and `d(isp, ipt)`.

No reduction anywhere: every output element is written by one thread from inputs no thread
modifies, so `WW_DETERMINISTIC` changes nothing and the result is bit-identical across
backends and thread counts; the launch is labelled `"srce.snl1.dia"` (v).

Two build facts decide whether "bit-identical" is true (v, `kokkos/src/ww_kokkos/CMakeLists.txt`,
`kokkos/README.md`). **Floating-point contraction is off** for `ww_kokkos`:
`-ffp-contract=off`, and `--fmad=false` for the device half under nvcc. GCC's default fuses
`AWG1*UE(..) + AWG2*UE(..)` into an FMA (one rounding where the Fortran does two), and
`openmp-release` drifted 1.1e-5 relative from the fixture while `serial-debug` was
bit-identical; with contraction off all three presets reproduced the Fortran bit for bit in the
L1 tests of the day, and
removing that line is a physics change, not an optimisation. And **`x**n` is not `std::pow`**:
`powi()` and `pow11()` reproduce gfortran's multiplication chains.

## The shim and `W3KOKKOSMD`

`kokkos/src/fortran_iface/` is the boundary: `ww_kokkos_c.hpp` declares the C ABI,
`snl1_shim.cpp` implements it, `w3kokkosmd.F90` mirrors it in `ISO_C_BINDING` blocks (v).

| Entry point | Does |
|---|---|
| `int ww_kokkos_init(int comm_f)` | starts Kokkos if nobody has; idempotent; `comm_f` accepted and ignored in phase 1 (device from `WW_KOKKOS_DEVICE_ID`); reads `WW_KOKKOS_SNL1` |
| `void ww_kokkos_finalize(void)` | releases buffers; finalises Kokkos only if it started it |
| `int ww_snl1_init(nk, nth, xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, snls3, fachfe, sig)` | `make_tables` once per grid; `sig` copied, not borrowed |
| `void ww_snl1(npts, a, cg, kdmean, s, d)` | unmanaged `LayoutLeft` host views over the pointers, `deep_copy` into persistent device buffers grown on demand, kernel, `fence`, copy out; `npts == 0` is a legal no-op |
| `int ww_snl1_enabled(void)` | 1 iff `WW_KOKKOS_SNL1=1` at init |
| `int ww_snl1_last_error(void)` | status of the last call, 0 on success |

Three things a naive `extern "C"` wrapper gets wrong and this one does not (v,
`snl1_shim.cpp`): ownership of the runtime (a test may own it); lifetime (a
`push_finalize_hook` drops the Views inside `finalize`, never at process exit); errors
(nothing throws across the boundary: every entry is a `try`/`catch` that records a
code). `W3KOKKOS_SETUP` turns `ww_snl1_enabled()` into `LOGICAL :: KOKKOS_SNL1` once, so
the inner loop reads a logical; every `REAL` is `REAL(C_FLOAT)` (v). Until 2026-10-14
`L1_test_snl1_shim.cpp` drove the raw C API on the fixture with poisoned output buffers, so a
forgotten copy-out failed loudly, and `shim_roundtrip` made the same call from Fortran (v,
`PORT_STATUS.md`, "L1 / L2"); both were removed with the port's other tests (next section). The
shim is now checked only by the WW3 run that calls it.

## The port has no tests of its own

Since 14 October 2026 the Kokkos code is validated only by WW3's regression tests, run inside WW3
with the Fortran routine and with the Kokkos routine, on the CPU, and compared within
`kokkos/tools/nccmp-tol/tolerances.txt` ([ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md);
the owner: "kokkos code should NOT have its own tests, hard rule") (v). If WW3's regtests agree for
the Fortran `W3SNL1` and the Kokkos `snl1`, the routine is ported; nothing else is asked of it. The
reason is that one reference is enough: the Fortran WW3 ships, through the harness NOAA runs. A
second copy of the reference kept next to it can drift away unseen (ADR-0010, *Alternatives*).

Earlier versions of this lesson taught the L1 tests the port had until that day. They are history
now, kept here because the numbers they produced are still quoted:

| Removed on 2026-10-14 | What it was | What it showed |
|---|---|---|
| `snl1_ref.F90`, `gen_snl1_fixture.F90`, `snl1_sea_state.F90` | a verbatim standalone copy of `W3SNL1`/`INSNL1`, and the generator that wrote a JONSWAP (10 m/s, 100 km, γ = 3.3) × cos² sea state at 1000, 50 and 10 m with the Fortran's answers into `snl1_nk25_nth24.bin` | the fixture, 107 892 bytes (v, `kokkos/tests/fixtures/README.md`) |
| `L1_test_snl1_tables` | all 32 address tables exactly; `dal1..3`, `awg`, `swg`, `af11` to 1e-6 relative | exact |
| `L1_test_snl1_dia` | `S` and `D` on three points to 1e-5 relative; zero in, zero out; `A×2` → `S×8`, `D×4`; four relaunches bit-identical | max relative error 0.0 on all three presets, 2026-09-15 (v, `kokkos/PORT_STATUS.md`) |
| `L1_test_snl1_shim`, `shim_roundtrip` | the same parity through the C ABI and from Fortran, plus the error paths | 0.0 |
| `gen_snl1_ww3lib.F90`, `just l1-crosscheck` | a cross-check against the real `W3SNL1` from a WW3 library | never run |

The tests and their sources are in git history (`git show f721736:kokkos/tests/L1_test_snl1_dia.cpp`).
The fixture stays, frozen and no longer regenerable, as the input of the kernel benchmark
`ww_bench_snl1` and of `proof/snl1_cons`; `fixture_io.{hpp,cpp}` is still its one reader (v,
`kokkos/tests/fixtures/README.md`). What the change costs is said in the ADR: a defect now shows up
in a model run, minutes long, instead of in one call; and the CUDA kernel has no check until a
regtest runs on the GPU, which only the owner may supervise.

## `PATCH.md`: what changes in the WW3 caller

`w3kokkosmd.F90` is written to be dropped into `WW3/model/src` unchanged (v, its header).
The caller side is `kokkos/src/fortran_iface/PATCH.md`: a hunk-by-hunk recipe for a fork
branch, with real line numbers, not applied in this repo (v). Four hunks (v):

1. **`w3srcemd.F90`**: inside the `W3_KOKKOS` guard, `USE W3KOKKOSMD`, `USE W3SERVMD,
   ONLY: EXTCDE` and `USE W3ODATMD, ONLY: NDSE` (neither is otherwise in scope in `W3SRCE`);
   a one-element `KDM_K(1)` local, because `KDMEAN` there is the expression `WNMEAN*DEPTH`;
   and at the single call site (lines 1258–1264) `IF (KOKKOS_SNL1) THEN CALL WW_SNL1(1,
   SPEC, CG1, KDM_K, VSNL, VDNL) … ELSE CALL W3SNL1(…) END IF`. The call sits in a named
   `!$OMP CRITICAL`: `W3SRCE` runs inside an `!$OMP PARALLEL` region and the shim has one
   unlocked context, so phase 1 serialises it, slower than the Fortran when threaded; the
   correctness step, not the fast one. The `WW_SNL1_LAST_ERROR() /= 0` check followed by
   `EXTCDE` is **not optional**: a `void` C call has no other channel, and a silent failure
   is a plausible-looking wrong forecast. `CG1` is already `CG(1:NK,ISEA)`, sliced by
   `W3WAVE`, so it passes straight through.
2. **`w3initmd.F90`**, after `W3IOGR('READ')`: `WW_KOKKOS_INIT(-1)`, `W3KOKKOS_SETUP`,
   `EXTCDE` when `IMOD > 1` (one spectral grid per process: `ww_snl1_init` *replaces* the
   tables), then `WW_SNL1_INIT(…, SIG(1))`. `SIG(1)`, not `SIG`: `W3GDATMD` allocates
   `SIG(0:MK+1)` and the assumed-size C dummy would start at bin 0: every quadruplet one
   bin low, smooth, plausible and wrong; the lab's `SIG(NK)` reference cannot catch it.
3. **`switches.json`** gains a `KOKKOS` category with `"build_files": ["w3kokkosmd.F90"]`
   and `"requires": ["NL1"]`. `src_list.cmake` needs **no** change: listing the file
   there would compile it unconditionally and break every non-Kokkos build.
4. **`model/src/CMakeLists.txt`** links `ww_kokkos` under `-DWW_KOKKOS=ON` and refuses
   the option without the switch.

Why the fork carries it: WW3's physics is frozen except for shims (`AGENTS_KOKKOS` §5), and
the lab pins WW3 as a submodule fork (`just src-init`, `src-pr`, `src-sync` (v, `justfile`)),
so the patch lives on a branch the pin can point at, and upstream stays untouched.

## L2: replay a regtest through both paths

```bash
just smoke-hoff        # scripts/hoff.sh --hoff smoke --cpu: regtest ww3_ts1
just full-hoff         # scripts/hoff.sh --hoff full --cpu: bench_small, 9 520 sea points
just hoff-matrix       # scripts/hoff_matrix.sh: every serial NL1 entry of matrix.base (WFIP-0003)
```

This is the port's only test (ADR-0010). `scripts/hoff.sh --hoff` builds WW3 from the `WW3/`
submodule, the h0ffmann/WW3 branch `hoff/kokkos-snl1` where the patch below is applied, with the
`KOKKOS` switch, and runs the case twice from the same executable, `WW_KOKKOS_SNL1=0` (the Fortran
DIA) and `=1` (the Kokkos DIA); `nccmp-tol` compares the gridded output field by field. The Kokkos
run is then compared pair to pair with `--ww3`, WW3 built from the same source without the switch
(v, `scripts/hoff.sh` header). It runs on the CPU, Kokkos on OpenMP: a GPU run of WW3 is the
owner's, supervised, and no agent starts one. Measured on 2026-10-14: `hs` within 9.879e-8
relative on `ww3_ts1` and within 1.832e-7 on `bench_small`, both PASS at the 1e-4 tolerance, not
byte-identical (v, `kokkos/port_report/W3SNL1_KOKKOS.md`, "How it was regtested").
`just hoff-matrix` takes the same comparison to WW3's own harness, `run_cmake_test`, with WW3's
`test.comp` beside `nccmp-tol` ⚠ (written on 2026-10-14; no result from it is quoted yet).

The older harness, `just l2 ww3_ts1` (`kokkos/tests/L2_replay.sh`), does the same pairing on a run
prepared by `just rt`, in `work_a` and `work_b`, and appends the table to `kokkos/PORT_STATUS.md`
(v). It needs a WW3 build with the patch; on an upstream build both runs take the Fortran path and
the table shows zeros, which proves the harness and not the kernel.

## The timing line

`kokkos/PORT_STATUS.md` is the ledger, one row per routine: WW3 file and lines, phase,
shim, L1 parity (history), L2 replay, Serial / OpenMP / CUDA ms per call, notes (v). The `W3SNL1`
row comes from `ww_bench_snl1` (`kokkos/tests/bench_snl1.cpp`, not a test: 1 000 sea
points, 20 timed calls after 3 warm-ups, median of three runs), on an Intel i9-14900 +
RTX 4090, Kokkos 5.2.0, GCC 15.3 (the owner's workstation, not the CI runner) (v):

| backend | through the shim, ms/call | kernel only, ms/call | vs Serial |
|---|---|---|---|
| Serial (OpenMP backend, 1 thread) | 25.88 | 24.96 | 1.0x |
| OpenMP, 32 threads | 5.95 | 5.02 | 4.4x |
| CUDA, RTX 4090 | 0.75 | 0.047 | 34x |

Two things these numbers are not (v). Not the fastest the kernel can go: every row is the
`-ffp-contract=off` parity build, and turning contraction on would invalidate every regtest
verdict. These timings were taken on 2026-09-15, when the parity that licensed them was the L1
test of the same build (v, `bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`); a new timing
needs a passed WW3 regtest replay on the same commit. And not what WW3 would see today: the phase-1 call site passes `NPTS = 1`, and on
CUDA 94 % of the 0.75 ms call is host↔device copy: the entire argument for phase 2, and
why "34x" is not the model's speed-up.

## What "done" means

`docs/AGENTS_KOKKOS_202609.md` §1.5, as amended by ADR-0010: (1) the kernel with a heritage
header naming the WW3 routine, (2) the `bind(C)` shim and Fortran interface with the argument
table, (3) withdrawn: it was an L1 test with tolerances stated and justified, and the port no
longer has tests of its own, (4) the WW3 regtest replay and the regtest matrix on the CPU within
`tolerances.txt`, (5) a timing line in `PORT_STATUS.md`, (6) a property checked inside a regtest
run where physics allows: for `Snl`, action conservation ⚠ (not yet checked in a run; the
zero-in/zero-out and cubic-scaling checks that stood in for it went with the L1 tests). Items 1, 2
and 5 are in the tree, and 4 passed on `ww3_ts1` and `bench_small` (v); the matrix has not run ⚠.
Sheet: `exercises/ex13_port.md`, solution `exercises/solutions/ex13_compare.sh`.

→ [`13-bulk-porting-with-agents.md`](13-bulk-porting-with-agents.md)
