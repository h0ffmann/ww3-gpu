# `tools/profile` — where does `ww3_shel` spend its time?

Two scripts that answer the same question with different samplers and print
the same table: the top routines of one regtest run, each with its self share,
the cumulative share down to that row, and the model phase it belongs to.

```
| # | routine          | self % | cumulative % | phase        |
|---|------------------|--------|--------------|--------------|
| 1 | w3srcemd:w3srce  |  45.00 |        45.00 | source terms |
| 2 | w3snl1md:w3snl1  |  30.00 |        75.00 | other        |
...

| phase        | self % (all routines) |
|--------------|-----------------------|
| source terms | 45.00 |
...
```

The phase comes from the routine name, module prefix stripped (gfortran emits
`__w3srcemd_MOD_w3srce`, shown as `w3srcemd:w3srce`):

| pattern | phase |
|---|---|
| `w3srce*` | source terms |
| `w3pro*`, `w3uqck*` | propagation |
| `w3gath`, `w3scat`, `mpi_*` | communication |
| `w3io*` | I/O |
| anything else | other |

`w3snl1` therefore lands in *other* by design: `W3SRCE` calls it, and the point
of the table is to see how much of the source-term phase the DIA is once the
Kokkos shim takes it over, so the two are kept apart.

## `gprof_table.sh <ww3-dir> <regtest> [switch]`

Rebuilds WW3 with `-pg` into `<ww3-dir>/build-pg` (the timing build in
`build/` is untouched), runs the regtest's programs up to `ww3_shel` in
`regtests/<regtest>/work_pg`, and reduces `gprof -b` to the table in
`work_pg/gprof_table.md` (raw text in `work_pg/gprof.txt`).

- The switch defaults to the one cached in `<ww3-dir>/build/CMakeCache.txt`,
  i.e. what `just rt <regtest>` built with, so the profiled binary is the same
  physics as the validated one.
- `-pg` is passed as `-DCMAKE_Fortran_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg`.
  WW3's CMake honours `CMAKE_Fortran_FLAGS` on top of its own per-build-type
  flags (`scripts/02_build_ww3.sh` passes only `-DCMAKE_BUILD_TYPE`, so the
  script configures the tree itself). `BUILD_TYPE` defaults to `Debug` -- what
  the brief asks for -- and `BUILD_TYPE=RelWithDebInfo` gives a profile of the
  optimised code, which is the one that matters for the port.
- Each WW3 program overwrites `gmon.out` in the cwd, so the script deletes it
  after every preparatory program and reads only `ww3_shel`'s.
- `gprof_table.sh --render gprof.txt` re-renders a saved `gprof -b` output;
  `gprof_table.sh --pairs` renders `"<self %> <symbol>"` lines from stdin,
  which is how `perf_table.sh` shares the phase map.

## `perf_table.sh <ww3-dir> <regtest>`

No rebuild: copies the prepared `work_lab` to `work_perf`, runs
`perf record -g ./ww3_shel`, then `perf report --stdio --no-children -g none`
into the same table (`work_perf/perf_table.md`, raw in `perf_report.txt`).

**`perf` is not in the pratico shell.** It is tied to the host kernel, so
`nix-config/labs/pratico` does not ship it; when `perf` is not on PATH the
script says so and exits 3. On Debian/Ubuntu install `linux-perf` (or
`linux-tools-$(uname -r)`) and run from a host shell; `perf record` may also
need `kernel.perf_event_paranoid` lowered.

## From the repository root

```bash
just profile ww3_tp1.1          # gprof table, then perf if the host has it
TOP=40 BUILD_TYPE=RelWithDebInfo bash kokkos/tools/profile/gprof_table.sh ~/src/WW3 ww3_tp1.1
```

Both scripts take `--dry-run` as the first argument to print the commands they
would run.

SPDX-License-Identifier: MIT
