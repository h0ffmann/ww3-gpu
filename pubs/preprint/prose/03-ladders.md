# Evidence ladders {#sec:ladders}

Each project gives names to its steps. A step in one ledger is not the same evidence as a step in
the other ledger.

| Step                        | ww3-gpu                                                    | chaceon                                                     |
|:-----------------------|:-------------------------------------|:-------------------------------------|
| Code exists                 | *translated*: 316 routines, 26.2 % of Fortran lines         | *translated*: Rust exists, not fully reached                |
| A run calls it              | *ported*: 72 routines, 99.3 % of profiled run time          | *routine*: every captured call is bit-identical on the host and in SPIR-V |
| Whole runs identical, CPU   | *regtested*: 14 routines, 48.6 % of run time                | *host*: whole runs are `cmp`-identical; 10 080 lines        |
| Identical through a GPU API | none                                                       | *spirv*: the same files from SPIR-V on CPU Vulkan; 5 189 lines |
| Identical on a real GPU     | *gpu*: none                                                | *cuda*: none                                                |

: Evidence steps. ww3-gpu counts are those of `kokkos/PORT_STATUS.md` at `58167d7`
(2026-11-11), written by `scripts/port_score.py --write`. The run-time shares come from one
profile (`bench_small`, 120 × 80 points, 12 h, one thread, gprof, 2026-10-14). On 2026-10-17 the
same ledger had 310, 60 and 10 routines, with the same shares to one decimal: routines were
added to the ledger after that date. chaceon counts come from `just port-summary`. {#tbl:ladders}
