# `PORT_STATUS.md` — the WAVEWATCH III → Kokkos port ledger

One row per WW3 routine that is being ported, with where it came from, how far it
has got, and what it costs. A row is only allowed to claim a number that a command
in this repository reproduces.

SPDX-License-Identifier: MIT

## The ledger

| Routine | WW3 file:lines | Phase | Shim | L1 parity | L2 replay | Serial ms | OpenMP ms | CUDA ms | Notes |
|---|---|---|---|---|---|---|---|---|---|
| `W3SNL1` + `INSNL1` | `model/src/w3snl1md.F90:115-473`, `:483-786` | 1 (copy-in / kernel / copy-out) | `ww_snl1_init`, `ww_snl1` | **bit-identical** on all three presets (`L1_test_snl1_dia`, `L1_test_snl1_shim`, `shim_roundtrip`) | pending fork branch (`src/fortran_iface/PATCH.md`) | 25.88 | 5.95 | 0.75 | 1 000 points/call, end-to-end through the shim. Kernel alone: 24.96 / 5.02 / **0.047** ms. Timed with `-ffp-contract=off` — see below. |
| `W3SNL2`…`W3SNL5` | `model/src/w3snl{2,3,4,5}md.F90` | not started | — | — | — | — | — | — | Out of scope; the `IQTPE <= 0` branch (`W3SNLGQM`) is not replaced either. |
| `W3SIN4` / `W3SDS4` | `model/src/w3src4md.F90` | not started | — | — | — | — | — | — | The next candidate: same per-point shape as the DIA, so the same shim generalises. |

**Phase** is the memory contract, not the completeness of the physics:
*phase 1* copies the spectrum host→device and back around every call; *phase 2*
keeps `VA` resident on the device and the copies disappear. Every routine starts
at phase 1 because phase 1 is what can be validated against the Fortran one call
at a time.

## Where the numbers come from

```bash
just kokkos-build openmp-release
nix develop ./nix-config/labs/pratico#ww3 --command \
  env OMP_PROC_BIND=false OMP_NUM_THREADS=1 \
      kokkos/build/openmp-release/tests/ww_bench_snl1          # the Serial column
nix develop ./nix-config/labs/pratico#ww3 --command \
  env OMP_PROC_BIND=spread OMP_PLACES=cores OMP_NUM_THREADS=32 \
      kokkos/build/openmp-release/tests/ww_bench_snl1          # the OpenMP column

just kokkos-cuda-test                                          # builds ww_bench_snl1
nix develop ./nix-config/labs/pratico#cuda --command \
  env OMP_PROC_BIND=false kokkos/build/cuda-release/tests/ww_bench_snl1
```

`kokkos/tests/bench_snl1.cpp`: 1 000 sea points (the three fixture points tiled),
20 timed calls after 3 warm-up calls, on the committed `nk=25, nth=24` grid
(`nspec = 600`). It is not a CTest case — it asserts nothing and its timings are
not reproducible enough to gate a build on.

**Machine** (all nine runs, 15 Sep 2026): Intel Core i9-14900 (32 hardware
threads) + NVIDIA GeForce RTX 4090 (Ada, `CMAKE_CUDA_ARCHITECTURES=89`), driver
595.84, the owner's workstation — *not* the CI runner, whose numbers would be
several times worse and are not recorded here. Kokkos 5.2.0, GCC 15.3.0.
Median of three runs; the spread was under 2 % on the CPU rows and under 4 % on
the GPU row.

| backend | shim ms/call | kernel ms/call | points/s (shim) | speed-up vs. Serial |
|---|---|---|---|---|
| Serial (OpenMP backend, 1 thread) | 25.88 | 24.96 | 3.9e4 | 1.0x |
| OpenMP, 32 threads | 5.95 | 5.02 | 1.7e5 | 4.4x |
| CUDA, RTX 4090 | 0.75 | 0.047 | 1.3e6 | 34x |

### Two things these numbers are not

**They are not the fastest this kernel can go.** `ww_kokkos` is compiled with
`-ffp-contract=off` (and `--fmad=false` on CUDA) because the port's contract is
*the Fortran's arithmetic*: with GCC's default `-ffp-contract=fast`,
`-O3 -march=x86-64-v3` fuses `AWG1*UE(..) + AWG2*UE(..)` into an FMA — one
rounding where WW3 does two — and the `openmp-release` build drifted 1.1e-5
relative from the committed fixture while `serial-debug` stayed bit-identical.
With contraction off, all three presets reproduce the Fortran bit for bit, which
is what the "bit-identical" in the L1 column means. Every row in the timing table
is therefore the cost of the **parity build**. Turning contraction back on is a
physics change, not a tuning knob, and it would invalidate the L1 column.

**They are not what WW3 would see today.** `PATCH.md`'s phase-1 call site passes
`NPTS = 1`, one sea point per launch, because that is the shape `W3SRCE` has. The
CUDA row is measured at `NPTS = 1000`. The gap between the shim column (0.75 ms)
and the kernel column (0.047 ms) on CUDA is the phase-1 host↔device transfer:
94 % of the call. That gap is the entire argument for phase 2, and it is why the
CUDA row must not be read as "the model will be 34x faster".

## L1 / L2

**L1** is a unit test against the Fortran: either a committed binary fixture
captured from the verbatim reference (`kokkos/tests/fixtures/snl1_ref.F90`) or the
reference called in the same process.

| test | what it covers |
|---|---|
| `L1_test_snl1_tables` | `make_tables` vs. `INSNL1`: all 32 address tables exactly, weights and `AF11` to 1e-6 relative |
| `L1_test_snl1_dia` | `snl1` vs. `W3SNL1` to 1e-5 relative; zero-in/zero-out; cubic scaling; launch-to-launch bit-reproducibility |
| `L1_test_snl1_shim` | the same parity *through the C ABI*, plus the error paths: call before init, negative `npts`, null pointers, buffer growth, idempotent init |
| `shim_roundtrip` | a Fortran program calling `W3KOKKOSMD` → shim → kernel and comparing with `W3SNL1_REF` in the same process |

All four report a max relative error of exactly **0.0** on all three presets.

**L2** is a whole-regtest replay: run WW3 twice from one executable with
`WW_KOKKOS_SNL1` flipped, and compare the NetCDF output field by field with
`kokkos/tools/nccmp-tol`. It needs the caller patch, so the column reads
*pending fork branch (`src/fortran_iface/PATCH.md`)* and will stay that way until
that branch exists. Nothing in this repository modifies `WW3/`.
