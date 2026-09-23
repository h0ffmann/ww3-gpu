# 12 — Porting a kernel: `W3SNL1`

Step 4 of the ladder, done once, end to end, on the routine lesson 07 nominated: the DIA
nonlinear interactions. Everything here is in `kokkos/`; keep it open. The phase-1 rule is
*translate, do not improve*: same expressions, same order, same float32 arithmetic. The
only thing that changes is who executes them (v, `kokkos/src/ww_kokkos/snl1_dia.cpp`).

## Source map

Source of truth: `WW3/model/src/w3snl1md.F90` at 7.14 `develop`; line numbers (v, checked in
`~/src/WW3` and in `snl1_ref.F90`'s header). The C++ keeps the Fortran's numbered section comments:

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
| `IF3..IF6` clamped to 0 → addresses down to `1-NTH` | entries as low as `-NTH` are legal; `L1_test_snl1_tables` checks the window `[-nth, nspecy)` explicitly | `INSNL1` section 7 (v) |
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
bit-identical; with contraction off all three presets reproduce the Fortran bit for bit, and
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
the inner loop reads a logical; every `REAL` is `REAL(C_FLOAT)` (v). `L1_test_snl1_shim.cpp`
drives the raw C API on the fixture with poisoned output buffers, so a forgotten copy-out
fails loudly; `shim_roundtrip` (`fixtures/shim_driver.F90`) is the same call from Fortran (v, `PORT_STATUS.md`).

## Fixtures

The L1 tests do not compare against numbers a human typed (v, `kokkos/tests/fixtures/README.md`):

| File | What |
|---|---|
| `snl1_ref.F90` | verbatim `W3SNL1`/`INSNL1` bodies; the only edits are the `USE` lines replaced by module variables with the same names, `CALL W3DMNL` replaced by the `ALLOCATE` it performs, and the `#ifdef W3_T*` output removed (v) |
| `gen_snl1_fixture.F90` | `SETUP_REF(25, 24, 1.1, 0.04118)` with ST4/NL1 defaults, a JONSWAP (10 m/s, 100 km, γ = 3.3) × cos² sea state in action form at 1000 m, 50 m and 10 m; streams header, tables and per-point `kdmean, cg, a, s, d` (v) |
| `snl1_nk25_nth24.bin` | the committed result, 107 892 bytes, little-endian, no record markers, tables stored 1-based exactly as Fortran holds them (v) |

The three depths exercise the `KDMEAN` branch: deep water where `EXP(X2)` underflows, and
a 7 % correction at 10 m (v). `just snl1-fixtures` regenerates the file, and a byte-level
diff then needs a reason in the commit message (v). `fixture_io.{hpp,cpp}` is the one reader (v).

## L1 tests and their tolerances

| Suite | Checks | Tolerance and why |
|---|---|---|
| `L1_test_snl1_tables` | all 32 address tables | exact: an address is an integer (v) |
| | `dal1..3`, `awg`, `swg`, `af11` | 1e-6 relative: they come out of `acosf`/`asinf`/`powf`, a ULP apart (v) |
| `L1_test_snl1_dia` | `S` and `D` on three points | 1e-5 relative with a 1e-30 absolute floor for denormal bins (v) |
| | zero spectrum → zero source; `A×2` → `S×8`, `D×4` | properties of the DIA, not of the fixture (v) |
| | four relaunches | bit-identical: a difference is a race, not rounding (v) |

Why 1e-5 for float32: the state has 24 bits, about 6e-8 relative, and section 3 forms
products of four-term interpolations scaled by `AF11` over ten decades, so a handful of
ULPs is the honest floor. 1e-5 is two orders above that and *tight enough that one fused
multiply-add fails it* (the FMA drift measured 1.1e-5). Measured: 0.0 on all three presets (v, `PORT_STATUS.md`).

## The optional `ww3_lib` cross-check

`gen_snl1_ww3lib.F90` and `just l1-crosscheck` would prove `snl1_ref.F90` verbatim by running
the *real* `W3SNL1` from a configured WW3 build and `cmp`-ing the fixtures. **Neither has been
run**: the owner's WW3 build used `NL0`; it needs an `NL1` build such as `just build switches/switch_lab_shrd` (v, `kokkos/README.md`).

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
just l2 ww3_ts1        # kokkos/tests/L2_replay.sh <ww3-dir> <regtest>
```

Requires `just rt <regtest>` done. The script copies `work_lab` to `work_a` and `work_b`,
runs `ww3_shel` with `WW_KOKKOS_SNL1=0` and `=1`, runs `ww3_ounf` in each, then
`nccmp-tol work_a/ww3.nc work_b/ww3.nc` with the default tolerances, and appends the table
to `kokkos/PORT_STATUS.md` under a `## L2 replays` heading it creates (the ledger's own
section is `## L1 / L2`). Until the patch is applied on a fork branch both runs are the
Fortran path and the table shows zeros: that proves the harness, not the kernel, and the
ledger's L2 column reads "pending fork branch" (v).

## The timing line

`kokkos/PORT_STATUS.md` is the ledger, one row per routine: WW3 file and lines, phase,
shim, L1 parity, L2 replay, Serial / OpenMP / CUDA ms per call, notes (v). The `W3SNL1`
row comes from `ww_bench_snl1` (`kokkos/tests/bench_snl1.cpp`, not a test: 1 000 sea
points, 20 timed calls after 3 warm-ups, median of three runs), on an Intel i9-14900 +
RTX 4090, Kokkos 5.2.0, GCC 15.3 (the owner's workstation, not the CI runner) (v):

| backend | through the shim, ms/call | kernel only, ms/call | vs Serial |
|---|---|---|---|
| Serial (OpenMP backend, 1 thread) | 25.88 | 24.96 | 1.0x |
| OpenMP, 32 threads | 5.95 | 5.02 | 4.4x |
| CUDA, RTX 4090 | 0.75 | 0.047 | 34x |

Two things these numbers are not (v). Not the fastest the kernel can go: every row is the
`-ffp-contract=off` parity build, and turning contraction on would invalidate the L1
column. And not what WW3 would see today: the phase-1 call site passes `NPTS = 1`, and on
CUDA 94 % of the 0.75 ms call is host↔device copy: the entire argument for phase 2, and
why "34x" is not the model's speed-up.

## What "done" means

`docs/AGENTS_KOKKOS_202609.md` §1.5, which the course spec adopts: (1) the kernel with a
heritage header naming the WW3 routine, (2) the `bind(C)` shim and Fortran interface with
the argument table, (3) an L1 test with tolerances stated and justified, (4) an L2 replay
of the smallest regtest that exercises it, (5) a timing line in `PORT_STATUS.md`, (6) a
property test where physics allows: for `Snl`, cubic scaling and zero-in/zero-out stand in
until an action-conservation test exists ⚠. Items 1–3 and 5 are in the tree (v); 4 waits on
the fork branch. Sheet: `exercises/ex13_port.md`, solution `exercises/solutions/ex13_compare.sh`.

→ [`13-bulk-porting-with-agents.md`](13-bulk-porting-with-agents.md)
