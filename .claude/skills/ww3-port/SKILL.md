---
name: ww3-port
description: "The WW3-specific traps of translating a WAVEWATCH III Fortran routine to C++/Kokkos and of claiming it regtested, learned on W3SNL1. Use while translating or reviewing a ported routine (after /port-next has scaffolded it), when a Kokkos result differs from the Fortran by a few ULP, when wiring a shim into W3SRCE or W3INIT, under OMPG/OMPH or ww3_multi, or before writing that a routine is 100% regtested."
---

# ww3-port: translating WW3's Fortran without changing a bit

`/port-next` (`.claude/commands/port-next.md`) checks the port order, reads the routine at the pin
and scaffolds the kernel, shim, report and ledger row. This skill is what to check while the body
is translated and before the routine is called regtested. The rules it serves are `AGENTS.md`
("Porting", "Running on the GPU", "Which account writes Kokkos code") and
`docs/AGENTS_KOKKOS_202609.md` §1.2, §1.5, §1.6; they win where this file disagrees.

Readers are scientists: every WW3 fact you write cites `model/src/<file>.F90:<line>` at the pin,
read from the file, and every claim carries `(v)` with what was checked or `⚠`. The port has no
tests of its own (ADR-0010): the evidence is WW3's regtests, Fortran and Kokkos from one
executable, `test.comp` and `nccmp-tol`.

## 1. Arithmetic: what gfortran -O3 actually computes

The target is the executable WW3's CMake builds (gfortran, `-O3`), not the source read naively.
When a result differs by a ULP, read the optimised GIMPLE before suspecting the physics:
compile the module with the flags in `<build>/model/src/CMakeFiles/ww3_lib.dir/flags.make` plus
`-fdump-tree-optimized`, and compare line by line (`docs/log/2026-10-14-snl1-bitwise-af11.md`).

- **Literal integer powers** (`x**11`) become GCC's power tree at `-O3` (`x5*x6` for 11), but a
  call of libgcc's `__powisf2` at `-O0`; the chains differ on 64% of inputs. Reproduce the `-O3`
  chain (`pow11()`, `kokkos/src/ww_kokkos/snl1_tables.cpp:45`).
- **Runtime integer exponents** (`XFR**IFRP`) stay `__powisf2` at `-O3`: use `powi()`
  (`snl1_tables.cpp:34`), never `std::pow`.
- **`REAL` is 4 bytes**: `float` everywhere, constants built as WW3 builds them. `TPIINV` is
  `1/(2*REAL(PI))`, not `1/(2*pi)` narrowed (`constants.F90:72-75`, `kTpiInv` in
  `snl1_config.hpp`).
- **Contraction is off** (`-ffp-contract=off`, `--fmad=false`). An FMA is a physics change.
- Keep the Fortran's operation order, reductions included. A changed order is its own PR with L2
  evidence, asked about first.

## 2. Memory: bases, pointers, per-grid state

- **Array bases.** `SIG` is `SIG(0:MK+1)` and a `POINTER` (`w3gdatmd.F90`), `CG` is
  `CG(0:NK+1,0:NSEA)` (`w3adatmd.F90`). An assumed-size C dummy loses the lower bound, and an
  element of a pointer array cannot be passed to an array dummy: pass a section, `SIG(1:NK)`
  (`kokkos/src/fortran_iface/PATCH.md`).
- **Per-grid state.** WW3 keeps a routine's tables and parameters per grid: `WADATS(IMOD)` via
  `W3SETA`, `MPARS(IMOD)`/`SGRDS(IMOD)` via `W3SETG`, which also sets `IGRID`
  (`w3gdatmd.F90:2278`). A shim mirrors that with one context per grid: initialised with W3INIT's
  `IMOD`, selected at the call by `IGRID` (`snl1_shim.cpp`, `contexts()`). One file-static
  context silently breaks `ww3_multi`.
- **Initialisation follows the Fortran's condition.** `INSNL1` runs only when `IQTPE > 0`
  (`w3iogrmd.F90:1801`); with `IQTYPE <= 0` an `NL1` switch computes GQM and never reaches the port.

## 3. Threads: W3SRCE runs inside an OpenMP region

- Under `OMPG`/`OMPH`, `W3SRCE` is called from `W3WAVE`'s `!$OMP PARALLEL DO`
  (`w3wavemd.F90:2240-2243`). The shim's contexts are unlocked: the caller patch wraps the call in a
  named `!$OMP CRITICAL`.
- Kokkos 5.2's OpenMP backend aborts when it opens a region inside one unless
  `OMP_MAX_ACTIVE_LEVELS > 1`, and so do large HostSpace `deep_copy` and zero-fills. When
  `omp_get_level() > 0`, the shim runs the same kernel template on `Kokkos::Serial`. That path is
  host-only (`WW_SNL1_SERIAL_NESTED`, `snl1_config.hpp`): a CUDA build also enables OpenMP and
  Serial, but its Views live in `CudaSpace`. Check `cuda-release` compiles (`--build-only` level:
  configure and build, never run on the GPU without the owner).
- A kernel without reductions or atomics computes the same bits on any execution space and team
  size; one with them does not. Say which, with the line.
- Fortran `SAVE` variables allocated inside a parallel region race in WW3 itself (`IKN` in
  `W3SWL6`, `w3swldmd.F90:359-377`). A run that fails there is WW3's bug, not the port's; record it
  and rerun, never edit WW3.

## 4. Claiming "regtested": count the denominator

- The denominator is every `matrix.base` entry whose switch has the routine's switch (`NL1` for
  W3SNL1), `just ww3-gpu-matrix-list all`; `selection:count` pins it per WW3 commit. `run_cmake_test`
  keeps the last `-s` (`-s MPI -s PDLIB` is `switch_PDLIB`), and `$(( $np -1 ))` is shell
  arithmetic.
- The verdict is bit for bit (`AGENTS.md`, "Bit for bit with the Fortran"): `test.comp` finds
  no file different. An entry passes only when both runs finished and compared: a run that did
  not finish is a failure, never a silent pass.
- WW3 is not reproducible everywhere: under PDLIB with MPI two Fortran runs write different
  `restart.ww3` files. When `test.comp` finds a difference the matrix reruns the Fortran
  (`work_G_<n>`) and excuses only the files that differ Fortran/Fortran too (`ww3_self`).
- An entry that links the port is not one that exercises it. `WW_KOKKOS_SNL1_COUNT=1` makes the
  shim print the spectra it computed at exit; the matrix sums them per entry (`kokkos_spectra`). A
  row with 0 passed without testing the port and is reported as such (the GQM entries).
- Report coverage as: entries run / entries in the denominator, byte-identical, within tolerance,
  not exercised, failed, with the record in `docs/data/` and the command. Never widen a tolerance.

## 5. Before calling it done

`just kokkos-test serial-debug`, `just kokkos-test openmp-release`, the CUDA preset compiled,
`just smoke-ww3-gpu`, `just full-ww3-gpu`, `just ww3-gpu-matrix <jobs> all`, `just ww3-gpu-matrix-selftest`,
then the port report, `kokkos/ports.tsv` (`regtested`), `python3 scripts/port_score.py --write`
and `--check`, and a dated `docs/log/` entry with what was run and what it showed.
