# Challenges: the regression tests are large {#sec:challenges}

WW3's regression tests are the only judge of the Kokkos port, and they are large. Table
@tbl:regtests gives their size at the fork commit `80644ba9`, counted on 2026-10-18.

| Set of `matrix.base` entries                                       | Entries           | Share  |
|:-------------------------------------------------------------------|:------------------|:-------|
| All entries                                                        | 758, in 60 tests  | 100 %  |
| Entries with the DIA (`NL1`)                                       | 310               | 41 %   |
| Entries with ST4 physics (`ST4`)                                   | 136               | 18 %   |
| Entries that the resident preset can select                        | 7 (all serial)    | 0.9 %  |
| Selected regtests of ww3-gpu                                       | 5, and the `ww3_ts1` pair | 0.7 % |

: Size of the WW3 regression matrix. Source: ww3-gpu `regtests/README.md` at `58167d7`, counts
from `just ww3-gpu-matrix-list`. The count of all entries is the number of lines that
`matrix.base` writes, and a machine section can disable some of them. {#tbl:regtests}

One run of the 310 `NL1` entries takes days. The selected set takes one to three hours on one
workstation (Intel i9-14900, 32 hardware threads); this time is an estimate, not a measurement.
Thus a full matrix run cannot follow each commit. ww3-gpu runs the selected set as a guardrail,
and it postpones the full matrix to a validation phase. Until then, a static argument supports bit
parity: the `-O3` GIMPLE or the disassembly of both compilers.
