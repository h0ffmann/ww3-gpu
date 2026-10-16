# W3SRCE translated to Kokkos with its three Fortran source terms, not regtested

2026-10-15, the owner with an agent. Issue #42, [`W3SRCE_KOKKOS_PLAN_202610.md`](../W3SRCE_KOKKOS_PLAN_202610.md),
branch `claude/42-port-w3srce`; fork branch `ww3-gpu/kokkos-srce` off `ww3-gpu/kokkos-st4`.

## Question

The owner asked for the next port by wall time while the ST4 checkpoint was still running. Row 1
of `kokkos/PORT_RANKING.md` after ST4 is `W3SRCE` (7.36 s, 5.60 % of the `bench_small` profile
`(v)`). Can it be translated as one unit, with the source terms it still calls in Fortran
(`W3SLN1`, `W3SBT1`, `W3SDB1`), without changing a bit of what WW3's `-O3` build computes?

## What was done

- The owner chose to start this unit before the ST4 checkpoint passed, which ADR-0011 asks for
  first, and waived the account rule of `AGENTS.md` for the session ("TEAM_PROTECTION=off", as on
  2026-10-14 and 2026-10-15: `~/.claude.json` read `claude_max` (v)).
- `W3SRCE` preprocessed for `switch_lab_shrd` (`gfortran -E -cpp` with each switch word) is 392
  statements and calls `W3SPR4`, `W3SIN4`, `W3SDS4`, `W3SNL1`, `W3SNLGQM`, `W3SLN1`, `W3SDB1`,
  `W3SBT1` and `LIU_FORWARD_DISPERSION` (v).
- Three subagents in parallel: the `-O3` inventory of the four routines (GIMPLE, vectoriser report,
  assembly); `W3SLN1`, `W3SBT1`, `W3SDB1` as device functions; the DIA's per-point body moved into a
  team function, `snl1_team`, with the kernel's GIMPLE unchanged in every floating-point
  statement (v, both dumps compared). The main session wrote the `W3SRCE` team kernel, its shim,
  and the fork hunks. Two fresh reviewers compared the C++ object with the Fortran object.
- Gates: `openmp-release` and `serial-debug` built with no warning, ctest 10/10 each;
  `cuda-release` compiled (never run); the fork built and linked `ww3_shel` with `KOKKOS` (never
  run).

## Result

- (v) No libmvec, no power call and no FMA in `W3SRCE`; one vectorised reduction, the time-step
  `MIN` at `:1532`, whose value is the same in any order for positive candidates.
- (v) GCC can swap the operands of a `MIN`/`MAX` after GIMPLE: the reviewers read the order from
  `maxss`/`minss` in the assembly and found 9 sites in `W3SRCE` and 1 in `W3SLN1` where a NaN or a
  `+0`/`-0` tie would have resolved differently. The `W3SLN1` one is reachable (`RFPM = 0` and
  `USTAR = 0` give a NaN `FF1`, and the port returned NaN where WW3 returns 0). All fixed.
- (v) The final `MAX(0., SPEC)` at `:2350` has a scalar remainder whose third element has the other
  operand order; the port copies it for `NSPEC mod 4 = 3` ⚠ (tied to the build's vector width).
- (v) With `TAUWBUG = 0` and `IT > 0`, the Fortran `W3SRCE` reads its local `LLWS` before setting
  it (the `W3SPR4` call at `:1119`, `w3src4md.F90:302`). That case, GQM and Liu's ice dispersion
  stay in the Fortran, call by call.
- ⚠ `W3SDB1` at `B == 1` exactly: the build skips a test the source makes; the port follows the
  build. The owner has not confirmed it.
- Nothing ran inside WW3. Every row is `ported`, `regtested` = `no: waiting for the checkpoint of
  W3SRCE (ADR-0011)`.

## What it changes

Row 1 of the ranking is now `W3XYP3`. The W3SRCE checkpoint needs: the ST4 checkpoint first;
`SRCE` in `WW3GPU_ROUTINES` (`scripts/ww3-gpu.sh`, `regtests/`), with physics words `ST4` and
`NL1` together; `model/src/w3wavemd.F90` in the `kokkos_patch` list of `scripts/ww3-gpu.sh`; the
`WW3/` pin moved to `ww3-gpu/kokkos-srce`; then `just smoke`, `just smoke-ww3-gpu` and
`just full-ww3-gpu` with `WW3GPU_ROUTINES=SRCE`, and `just ww3-gpu-matrix <jobs> all`.
