# The course

Sixteen lessons, in order. The first half is WW3 as a user runs it; the second half is the
proposal's optimisation ladder, from a benchmark to a Kokkos kernel. A weekend for the
first half if you run everything; the second half takes as long as the port you are doing.

| | Lesson | You'll be able to |
|---|---|---|
| 00 | [Orientation](00-orientation.md) | Say what WW3 computes and what it doesn't. Name every program and what it writes. |
| 01 | [Getting it built](01-build.md) | Clone, fetch the data bundle, pick a switch, build with CMake inside the pinned toolchain, pass a regtest. |
| 02 | [Anatomy of a run](02-anatomy-of-a-run.md) | Write every namelist from scratch. Derive your own timesteps from the CFL condition. |
| 03 | [Grids, bathymetry, masks](03-grids.md) | Build a grid that isn't upside down, with the right sign on the depths. |
| 04 | [Forcing](04-forcing.md) | Get real winds in, and know the four ways `ww3_prnc` silently fails. |
| 05 | [Nesting and multi-grid](05-nesting.md) | Choose between one-way and `ww3_multi`, and place boundary points legally. |
| 06 | [Output and post-processing](06-output.md) | Get netCDF out, read it with the netCDF tools, compare two runs with `nccmp-tol`. |
| 07 | [Physics choices](07-physics-choices.md) | Choose a source-term package on purpose, and say why `W3SNL1` is the first kernel to port. |
| 08 | [The Python ecosystem](08-python.md) | Know what pyww3, WW3-tools and wavespectra do, and why this repo does not depend on them. |
| 09 | [Measure first: benchmark, profile, compile and run configuration](09-benchmark-profile-compile-run.md) | Freeze a reference run, measure seconds per forecast hour, profile by phase, and exhaust the compile-option and namelist knobs with WW3's matrix as the bit-for-bit gate. |
| 10 | [Modern Fortran refactoring](10-modern-fortran-refactoring.md) | Rewrite a profile-top routine in standard Fortran without changing its arithmetic, keep both paths in one binary, and prove parity per routine. |
| 11 | [Kokkos and modern C++ for Fortran people](11-kokkos-and-modern-cpp.md) | Read and write a Kokkos kernel: Views, `LayoutLeft`, mirrors, `parallel_for`/`reduce`, team scratch, `bind(C)`; build the presets and run on the 4090. |
| 12 | [Porting a kernel: `W3SNL1`](12-porting-a-kernel-w3snl1.md) | Follow the DIA port line by line from `w3snl1md.F90` to `snl1_dia.cpp`, run the L1 parity tests, replay a regtest through both paths, and say what "done" means. |
| 13 | [Bulk porting with agents](13-bulk-porting-with-agents.md) | Run the FESOM2 recipe on the next forty routines with a coding agent: the ranked list, the residency ladder, the PR contents, and the decision that puts a kernel into operation. |
| 14 | [WW4 and the future](14-ww4-and-the-future.md) | Judge how mature WW4 is, what's changing, and what of WW3 is worth learning anyway. |
| 15 | [SWAN](15-swan.md) | Recognise the coastal problems WW3 is wrong for, and build the model that isn't. |

Do lesson 02 with `examples/01-fetch-limited-growth` open beside it. When an abbreviation,
switch or routine name is unfamiliar, [`docs/GLOSSARY.md`](../docs/GLOSSARY.md) expands it and
says where it comes from.

Lessons 09 to 13 are the proposal's ladder (`pubs/proposal/pt/04-scope.md`), in order:
compile options, run configuration, modern Fortran, C++/Kokkos kernels, bulk porting.
Each is gated by parity with the one before. Do them with `kokkos/` open beside you:
the intro programs, the `W3SNL1` port, its fixtures and tests are the material, and
`exercises/` has one sheet per lesson.

Lessons 14 and 15 stand alone and can be read first. They answer two separate "should I
even be doing this?" questions: *should I wait for WW4* (no, but know it's coming and that
WW3 is scheduled for sunset), and *is WW3 the right model at all* (often, but not
nearshore).
