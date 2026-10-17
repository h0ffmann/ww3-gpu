# W3MPIO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Under `switches/switch_lab_shrd` the
routine compiles to an empty subroutine and nothing calls it, so no lab-switch regtest can
exercise the port; its checkpoint (ADR-0011) needs the `W3_MPI` body ported under an MPI switch
first.

- (v) Fresh review 2026-10-16: the preprocessor structure of `:2045-5384` recounted by an
  independent `awk` depth pass (the same six lines outside every block, the same counts, no
  `#else`, `#elif` or `#include`; the executable body is `W3_S` `:2216-2218` and the one
  top-level `W3_MPI` block `:2222-5326`);
  `gfortran -E` with the defines rebuilt from `switches/switch_lab_shrd` (the same set as the
  CMake `flags.make`) prints the same six lines; `w3initmd.F90` recompiled in the pinned `#ww3`
  shell with WW3's Release flags and `-D__WW3_SWITCHES__` (`.text` disassembly identical to the
  CMake object; the same gfortran wrapper run outside the shell gives a different `W3INIT`, so
  the shell matters): `__w3initmd_MOD_w3mpio` is one `ret`, size 1, GIMPLE `return;`. `nm -u`
  over the 62 objects finds no reference; W3MPIO's lines are identical in the fork; the only
  call, `:1561`, is in the single block `#ifdef W3_MPI` `:1559-1563`. Every `w3initmd.F90`
  line cited here and in the header read at `03adda91`, the section lines `:2220`, `:4411`,
  `:5150`, `:5257`, `:5321` included; the body holds 142 `MPI_SEND_INIT` and 142
  `MPI_RECV_INIT` calls and no other MPI call. A C++ unit calling the header, built in the same
  shell with the `openmp-release` flags plus `-Wconversion -Wshadow` and at `-O0` with
  ASan/UBSan: no warning, the caller one `ret`. One defect fixed: the header put the fork's
  hunks at `:436-833`, a range of neither file (base `:436`, `:473`, `:733`; fork `:436-838`).
  One wording fixed below (each exit follows a `WRITE`, not a FORMAT). ⚠ The "Not ported"
  summary describes code that is not compiled: its lines were read, not built.

In an MPI build, W3MPIO creates the persistent MPI requests (`MPI_SEND_INIT`, `MPI_RECV_INIT`)
with which WW3's output routines gather the gridded fields (`W3IOGO`), the restart (`W3IORS`),
the boundary data (`W3IOBC`) and the track output (`W3IOTR`) of model `IMOD` on the processes
that write them, and sets the tag counters `IT0PRT`, `IT0PNT`, `IT0TRK`
(`model/src/w3initmd.F90:5323-5325`). Under the shared-memory switch there is one process and
no request to set up.

## What was ported

| | |
|---|---|
| WW3 routine | `W3MPIO`, `model/src/w3initmd.F90:2045-5384` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Fork | the `h0ffmann/WW3` fork at the `WW3/` pin `8e433f99` adds 100 lines to `w3initmd.F90` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3initmd.F90`), in three hunks at `:436`, `:473` and `:733` of the base, all inside `W3INIT` and behind `W3_KOKKOS`; `W3MPIO` is unchanged (v) |
| Kokkos code | [`../src/ww_kokkos/init_w3mpio.hpp`](../src/ww_kokkos/init_w3mpio.hpp), `ww::init::w3mpio(std::int32_t imod)`, host, no operation |
| C shim | none |
| Calls not ported here | none on the compiled path: the compiled routine calls nothing, so the header has no hook table |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

The switch is `F90 NOGRB NOPA LRB4 NC4 SHRD PR3 UQ FLX0 LN1 ST4 STAB0 NL1 BT1 DB1 MLIM TR0 BS0 IC0
IS0 REF0 XX0 WNT1 WNX1 RWND CRT1 CRX1 O0 ... O14`; it defines none of `W3_MPI`, `W3_MPIT`
(test output of the MPI set-up) or `W3_S` (`STRACE` subroutine tracing).

## What the switch compiles

The lines of `W3MPIO` that stand outside every `#if` block, listed by an `awk` pass over
`:2045-5384` that tracks the `#ifdef`/`#endif` depth (the range holds 9 `#ifdef W3_MPI`, 301
`#ifdef W3_MPIT`, 3 `#ifdef W3_S`, 313 `#endif`, no `#else`, no `#if defined`) (v):

| Line | Statement |
|---|---|
| `:2045` | `SUBROUTINE W3MPIO ( IMOD )` |
| `:2136` | `USE W3SERVMD, ONLY: EXTCDE` |
| `:2182` | `USE W3GDATMD, ONLY: UNGTYPE` |
| `:2191` | `INTEGER, INTENT(IN) :: IMOD` |
| `:5328` | `RETURN` |
| `:5384` | `END SUBROUTINE W3MPIO` |

- (v) The routine as `gfortran -E` writes it with the defines of WW3's `flags.make` for the lab
  switch (below) is those six lines and comments. `EXTCDE` and `UNGTYPE` are not referenced, and
  `IMOD` is not read.
- (v) Every executable statement of the body, `:2216-5326`, is under `W3_S` (`:2216-2218`, the
  `STRACE` call) or `W3_MPI` (`:2222-5326`), and every local but `IMOD` is declared under
  `W3_MPI`, `W3_MPIT` or `W3_S` (`:2196-2212`). The two exits of the body, `EXTCDE(10)` and
  `EXTCDE(11)`, each after a `WRITE` with FORMAT 1010 or 1011 (`:3330-3333`, `:4404-4407`),
  and the FORMATs themselves (`:5332-5335`) are under `W3_MPI`; the test FORMATs 9010-9043
  under `W3_MPIT`.
- (v) The only call at the pin is `W3INIT`'s `CALL W3MPIO ( IMOD )` (`:1561`), inside `#ifdef
  W3_MPI` (`:1559-1563`) with `W3MPII` and `W3MPIP`; `grep -rn W3MPIO model/src` finds only
  comments elsewhere (`wmiopomd.F90:181`, `w3adatmd.F90:1035`). No object of a WW3 CMake
  Release build under the lab switch has `__w3initmd_MOD_w3mpio` among its undefined symbols
  (`nm -u` over the 62 objects of the scratch build named below). So a lab-switch run never
  enters the routine.

## Arithmetic

None. The reference is the `-O3` object of `w3initmd.F90`, compiled 2026-10-16 in the pinned
`#ww3` shell (gfortran 16.2.0, the wrapper `flags.make` names) with WW3's Release flags (`-O3
-fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none -O3`) and the
lab-switch defines of the `flags.make` of a WW3 CMake configure of the `WW3/` pin under
`switch_lab_shrd`, plus `-fdump-tree-optimized-lineno`. Its `.text` disassembly is identical to
that of the CMake build's `w3initmd.F90.o` (v).

- (v) The GIMPLE of `__w3initmd_MOD_w3mpio` is `void w3mpio (integer(kind=4) & restrict
  imod) { return; }`, and the object's symbol is one `ret` instruction, 1 byte (`readelf -s`:
  `FUNC GLOBAL`, size 1).
- (v) `ww::init::w3mpio` compiles, with the `openmp-release` flags, to nothing: a throwaway
  `extern "C"` wrapper that calls it is one `ret`. The two routines do the same thing, nothing.
- (v) No floating-point operation, no libm call, no reduction, no allocation, no I/O, no
  `EXTCDE` exit on either side; the result does not depend on the execution space.

## Not ported

The whole `W3_MPI` body, `:2222-5326`, and what it would need to be ported:

- section 1, the `W3IOGO` field requests: `FLGRDALL`/`FLGRDARST` from `FLOGRD`, `FLOGR2`,
  `FLOGRR`, the request count `NRQMAX` per output group, the allocation of `IRQGO(NRQMAX)` and
  `IRQGO2(NRQMAX*NAPROC)` in `OUTPTS(IMOD)%OUT1`, one `MPI_SEND_INIT` per field per process and
  on `NAPFLD` the full-grid `X*` arrays (`W3XDMA` with `FLGRDARST` or `FLGRDALL`, then `W3XETA`,
  `:3335-3347`) and the matching `MPI_RECV_INIT`s, then `W3SETA` (`:4400`), with `EXTCDE(10)` or
  `EXTCDE(11)` when `NRQGO` or `NRQGO2` exceeds its allocation (`:3330-3333`, `:4404-4407`);
- the `W3IORS` restart requests from `:4411` (`W3XDMA` when `NAPRST` differs from `NAPFLD`,
  `W3XETA`, `W3SETA`, `:4775-4776`, `:5064`; `NRQRS`, `IRQRS`, `NBLKRS`, `RSBLKS`, `IRQRSS`,
  `VAAUX`);
- the `W3IOBC` boundary requests (`NRQBP`, `NRQBP2`, `IRQBP1`, `IRQBP2`, `ABPOS`), the
  `W3IOTR` track requests (`NRQTR`, `IRQTR`), and the tag counters at `:5323-5325`;
- the `W3_MPIT` test output and the `W3_S` `STRACE` call.

Its callees are partly translated: `W3XDMA` ([`W3XDMA_KOKKOS.md`](W3XDMA_KOKKOS.md)), `W3XETA`
([`W3XETA_KOKKOS.md`](W3XETA_KOKKOS.md)), `W3SETA` ([`W3SETA_KOKKOS.md`](W3SETA_KOKKOS.md)) and
`INIT_GET_JSEA_ISPROC` ([`INIT_GET_JSEA_ISPROC_KOKKOS.md`](INIT_GET_JSEA_ISPROC_KOKKOS.md)). The
MPI requests themselves are MPI calls on Fortran `mpi_f08` handles, not a computation a Kokkos
kernel does; how a port would hold them is a design question for the owner ⚠.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and there is nothing to time under the lab switch.

## How it was regtested

Not yet, and nothing of the port was run. What was done, 2026-10-16 (v):

- the Fortran object and GIMPLE above, built from `WW3/model/src/w3initmd.F90` with the module
  files of the scratch CMake build of the `WW3/` pin under `switch_lab_shrd`;
- a throwaway translation unit that includes `init_w3mpio.hpp`, takes `&ww::init::w3mpio` and
  calls it from an `extern "C"` wrapper, compiled in the pinned `#ww3` shell against
  `kokkos-openmp-5.2.2` with `g++ -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra
  -Wpedantic -Wconversion -Wshadow -ffp-contract=off`: no warning, the wrapper is one `ret`;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- the same unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89 --fmad=false
  --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against
  `kokkos-cuda-5.2.2`: no warning, compile only;
- a copy of `wave_support.cpp` with the header's `#include` and `(void)&ww::init::w3mpio;`
  added, compiled with the `openmp-release` flags: the same 16 warnings as the unmodified file,
  none from this header, and the same instructions (the objects differ only in the name of the
  translation unit's static initialiser).

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
