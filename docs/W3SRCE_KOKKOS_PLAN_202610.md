# W3SRCE on Kokkos: the source-term integrator as one unit (phase 1, translation only)

Status: **Tasks 1-6, 8, 9 done; Task 7 written on the fork, not pushed**, 2026-10-15 ([log](log/2026-10-15-w3srce-kokkos-translation.md)), the checkpoint of §7 has not run; under [ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md).
It answers the owner's request of 2026-10-15 ("do the next port, by wall time, most expensive
routines first"), with the port planner issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42)
as its issue: no issue names `W3SRCE` alone. [ADR-0002](ADRs/ADR-0002-port-order-wall-time.md) sets
the order, [ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md) what "tested" means,
and ADR-0011 makes this a unit with its regtests at a checkpoint after it. The owner started this
unit while the ST4 checkpoint was still running (ADR-0011 asks for the checkpoint first); the log
entry records it.

**Goal.** Translate `W3SRCE` (`model/src/w3srcemd.F90`), as the lab switch compiles it, to one
C++/Kokkos team kernel per sea point that calls the ST4 and `W3SNL1` device code already ported
and the three Fortran source terms it still calls (`W3SLN1`, `W3SDB1`, `W3SBT1`), translated here.
Wire it into `W3WAVE` behind the `KOKKOS` switch and one run-time flag, `WW_KOKKOS_SRCE`, off by
default. Phase 1: copy in, kernel, copy out, one point per call.

**Architecture.** [`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md):239-240: one team per
sea point; the source terms are device functions inside the `W3SRCE` kernel, never separate
launches. Phase 1 keeps every sum serial, in the Fortran's order: no `parallel_reduce` (it
reorders a sum; `AGENTS.md`, "Bit for bit"). The serial parts run in `Kokkos::single(PerTeam)`;
only the `W3SNL1` body keeps its `TeamThreadRange` loops, which are per element and already
byte-identical to the Fortran (`kokkos/port_report/W3SNL1_KOKKOS.md`).

## 1. Scope

The pin of the Fortran read here is NOAA-EMC/WW3 `03adda91` (`WW3-noaa/`), the base of the fork;
the fork adds only the `KOKKOS` hunks. Every `file:line` below is at that commit (v, read
2026-10-15). Shares are from `kokkos/PORT_RANKING.md` (gprof on `bench_small`, `WW3@6d1bcdfe`,
2026-10-14, `just profile-case full`) (v).

| Rank | Routine | Lines | Self time | Share |
|---|---|---|---|---|
| 1 | `W3SRCE` | `w3srcemd.F90:190-2412` (2223; 973 after preprocessing for `switch_lab_shrd`, 392 statements) | 7.36 s | 5.60 % |
| 6 | `W3SDB1` | `w3sdb1md.F90:96-382` (101 statements preprocessed) | 0.51 s | 0.39 % |
| 8 | `W3SBT1` | `w3sbt1md.F90:88-285` (32 statements) | 0.28 s | 0.21 % |
| 10 | `W3SLN1` | `w3sln1md.F90:56-211` (36 statements) | 0.17 s | 0.13 % |

The unit is 6.33 % of the profiled run time ⚠ (a sum of gprof rows). With ST4 and `W3SNL1` it is
the whole source-term integration of the lab switch.

The preprocessed `W3SRCE` calls, under `switch_lab_shrd` (v, `gfortran -E -cpp` with
`-DW3_<word>` for each word of the switch, 2026-10-15): `W3SPR4` (4 sites), `W3SIN4` (3),
`W3SDS4`, `W3SNL1`, `W3SNLGQM`, `W3SLN1`, `W3SDB1`, `W3SBT1`, `LIU_FORWARD_DISPERSION`.

Out of scope, run by the Fortran `W3SRCE` through the dispatch of §4.3:

- `srce_call` other than `srce_direct`: the implicit calls at `w3wavemd.F90:1618` and `:2285`
  (unstructured grids, `FSSOURCE`). The regular-grid call is `:2311`.
- `IQTPE <= 0` (`w3srcemd.F90:1259`): `W3SNLGQM` (`w3snl1md.F90:789-1182`), the GQM option of
  NL1. Not in the profile.
- `INFLAGS2(4) .AND. ICE > 0 .AND. IICEDISP` (`:2124-2126`): `LIU_FORWARD_DISPERSION`
  (`w3dispmd.F90:687-918`).
- Every switch other than those of `switch_lab_shrd`: the kernel is written for that switch's
  branches. A `KOKKOS` build with another source-term switch (ST3, ST6, NL2, BT4, DB0, ...)
  refuses `WW_KOKKOS_SRCE=1` at init with an error code.
- Regtests (the checkpoint, §8), timings, GPU runs, phase 2 (device-resident spectrum).

Under the hard rules this means: status `ported` at most; `regtested` = `no: waiting for the
checkpoint of W3SRCE (ADR-0011)`; the flag defaults to 0; every parity claim ⚠ until the
checkpoint.

## 2. Global constraints

The constraints of [`ST4_KOKKOS_PLAN_202610.md`](ST4_KOKKOS_PLAN_202610.md) §2 hold unchanged,
except the account line: the owner waived it for this session (log entry). In short: `WW3/` is
read-only, the Fortran changes go to a fork branch behind `W3_KOKKOS`; translate, do not improve;
ask before a reduction order, a limiter or the integration order changes; contraction off; `float`
for `REAL`, `double` for `REAL*8`/`DOUBLE PRECISION`; no allocation in a kernel; explicit memory
spaces; no test of the port's own; never widen a tolerance; no `--gpu` run.

## 3. Tasks

| # | Task | Who | Output |
|---|---|---|---|
| 1 | Reference arithmetic: the `-O3` GIMPLE of the four routines | subagent, read-only | inventory (powers, folded constants, vectorised loops, libm calls) |
| 2 | Common layer: `srce_params.hpp`, the C ABI, CMake | main | parameters, signatures, stubs |
| 3 | `W3SLN1`, `W3SBT1`, `W3SDB1` device functions | subagent | `srce_ln1.hpp`, `srce_bt1.hpp`, `srce_db1.hpp` |
| 4 | `W3SNL1` as a team device function, the existing kernel calling it | subagent | `snl1_dia.hpp/.cpp`; GIMPLE of the kernel unchanged |
| 5 | `W3SRCE` team kernel, launcher and shim | main | `srce_kernel.hpp`, `srce.cpp`, `srce_shim.cpp` |
| 6 | Translation review, one fresh reviewer per routine | subagents, read-only | findings fixed before commit |
| 7 | Fortran wiring on the fork, branch `ww3-gpu/kokkos-srce` from `ww3-gpu/kokkos-st4` | main | `w3kokkosmd.F90`, `w3initmd.F90`, `w3wavemd.F90` hunks |
| 8 | Build gates (serial-debug, openmp-release, cuda-release compiled), the fork built and linked | main | |
| 9 | Ledger, port reports, glossary, log, PR | main | |

## 4. Design

### 4.1 The kernel

```cpp
// kokkos/src/ww_kokkos/srce_kernel.hpp
namespace ww::srce {
template <class Team>
KOKKOS_INLINE_FUNCTION void w3srce_team(const Team& team, const Params& p, const Fields& f,
                                        const snl1::Config& nc, const snl1::Tables& nt,
                                        Work& w, PointIn& in, PointInOut& io, PointOut& out);
}
```

One team per point, `npts` = 1 in phase 1. The integration loop (`:1210` to the `END DO` of the
dynamic loop) runs on one thread of the team; at each `W3SNL1` call the team synchronises, the
DIA body runs over the team, and the team synchronises again. Every other callee is a per-point
device function: `w3spr4_point`, `w3sin4_point`, `w3sds4_point` (ST4, as ported), and the three of
Task 3. The local arrays of `W3SRCE` (`DAM`, `WN2`, `VS*`, `VD*`, `SPECINIT`, `SPEC2`, `BRLAMBDA`,
`LLWS`, about 20 arrays of `NSPEC`) are preallocated per grid at init and passed as `Work`.

### 4.2 Parameters

`W3SRCE` and its three callees read these scalars and arrays of `W3GDATMD` (v, the preprocessed
`USE` lists): `DMIN`, `DTMAX`, `DTMIN`, `FACTI1`, `FACTI2`, `FACSD`, `FACHFA`, `FACP`, `XFLT`, `XREL`,
`FHMAX`, `ZZWND`, `FFXFM`, `FFXPM`, `FFXFA`, `SINTAILPAR`, `IQTPE`, `ICESCALES(1:4)`, `IICEDISP`,
`IICESMOOTH`, `IC_NUMERICS`, `SLNC1`, `FSPM`, `FSHF`, `SBTC1`, `SDBC1`, `SDBC2`, `FDONLY`, and the
grid arrays `SIG`, `DDEN`, `ECOS`, `ESIN`, `MAPWN`; `INFLAGS2(4)` of `W3IDATMD`; the constants
`DWAT`, `GRAV`, `TPI`, `TPIINV` and `OFFSET` (`w3srcemd.F90:69`, `= 1.`); `THR = TINY(1.0)`
(`w3parall.F90:97`). They cross as `RPAR`/`IPAR` with named indices, as ST4 does.

### 4.3 Run-time selection and dispatch

`WW_KOKKOS_SRCE=1` replaces the `CALL W3SRCE(srce_direct, ...)` at `w3wavemd.F90:2311` with the
Kokkos call when every condition holds: `IQTPE > 0`, and not (`INFLAGS2(4)` and `IICEDISP`).
Otherwise the Fortran `W3SRCE` runs for that call. The conditions are read in the caller hunk from
the same module variables `W3SRCE` reads, so the Fortran runs exactly where the kernel has no
translation. Inside the Kokkos `W3SRCE` every callee is Kokkos, whatever the ST4 and `SNL1`
flags say; the init of `WW_KOKKOS_SRCE` therefore initialises the ST4 and `W3SNL1` contexts of the
grid too. `WW_KOKKOS_SRCE_COUNT=1` prints the spectra integrated, as the other routines do.

### 4.4 Quirks the port keeps

Each gets one comment line in the kernel citing the line.

1. `W3SDB1` returns early when `EMEAN < TINY(1.d0)` (`w3sdb1md.F90`, first executable line) and
   leaves `S`, `D` and `LBREAK` unset. They are `W3SRCE`'s `VSDB`/`VDDB`, so they keep the values
   of the previous sub-step (zero on the first, `:1159-1160`). The kernel passes the same buffers.
2. `W3SDB1` computes in `REAL*8` and casts `D = REAL(-CBJ, 4)`; `QB = EXP((QB-1.)/BB)` mixes a
   `REAL` literal into a `REAL*8` expression. `THR` is `REAL` (`TINY(1.0)`) compared with
   `REAL*8` values.
3. `W3SBT1`: after the `EXIT` at `WN(IK)*DEPTH > 6.`, `NSCUT = (IK-1)*NTH`, and `IK = NK+1` when
   the loop runs to its end.
4. `W3SLN1`: `MAX(0., ...)**4`, `USTAR**4` and `RFR**(-4)` are integer powers; their form comes
   from the `-O3` dump (Task 1), not from the source.
5. `W3SRCE`: `DT = MAX(0.5, DT)` at `:1539` is a hard-coded floor (the comment there calls it a
   problem for laboratory scales). Keep it.
6. `W3SRCE`: the `TAUWX`/`TAUWY` reset under `SINTAILPAR(4) > 0.5` "keeps the bug as an option"
   (source comment). Keep it.
7. `W3SRCE`: `FIRST` (`:723`) is written and never read under this switch; `TAUNUX`, `TAUNUY`
   (`:726`, `threadprivate` at `:737`) are not used. Not ported.
8. `W3SRCE`: the sums `PHIAW`, `PHIBBL`, `PHINL`, `WHITECAP(3)`, `HSTOT`, `EFINISH`, `MWXFINISH`,
   `MWYFINISH` accumulate over `IK`, then `ITH`, into one scalar. Serial, in that order.
9. `W3SRCE`: the time-step minimum over `IS` is a sequential `MIN`. Serial.
10. `W3SRCE`: `IF (IT.EQ.0) SPEC = SPECINIT` then `SPEC = MAX(0., SPEC)` (`:2350`) at the end.

### 4.5 Threads

`W3SRCE` is called inside `!$OMP PARALLEL` (`w3wavemd.F90:2241-2354`). As for ST4, the Kokkos
call goes inside `!$OMP CRITICAL (WW_KOKKOS_SRCE)` and the shim runs on `Kokkos::Serial` when
`omp_get_level() > 0`. Phase 1 serialises the points; that is its accepted cost.

## 5. Files

| Path | Status |
|---|---|
| `kokkos/src/ww_kokkos/srce_params.hpp` | create |
| `kokkos/src/ww_kokkos/srce_ln1.hpp`, `srce_bt1.hpp`, `srce_db1.hpp` | create |
| `kokkos/src/ww_kokkos/srce_kernel.hpp`, `srce.cpp` | create |
| `kokkos/src/ww_kokkos/snl1_dia.hpp`, `snl1_dia.cpp` | the team body as a device function |
| `kokkos/src/fortran_iface/srce_shim.cpp`, `ww_kokkos_c.hpp` | create, extend |
| `kokkos/port_report/W3SRCE_KOKKOS.md`, `W3SLN1_KOKKOS.md`, `W3SBT1_KOKKOS.md`, `W3SDB1_KOKKOS.md` | create |
| `kokkos/ports.tsv`, `docs/GLOSSARY.md` | rows, entries |
| fork: `model/src/w3kokkosmd.F90`, `w3initmd.F90`, `w3wavemd.F90` | hunks behind `W3_KOKKOS` |

## 6. Stop and ask the owner

Before a change of a reduction order, a limiter (`MLIM`, `SHAVE`) or the integration order; before
contraction is turned on; when Task 1 finds a vectorised libm call in `W3SRCE` (ST4's libmvec
case); when the `W3SNL1` refactor of Task 4 changes the kernel's GIMPLE.

## 7. Next: the checkpoint

`WW3GPU_ROUTINES` gains `SRCE` (physics word `ST4` and `NL1` together); then `just smoke-ww3-gpu`
and `just full-ww3-gpu` with `WW3GPU_ROUTINES=SRCE`, and `just ww3-gpu-matrix <jobs> all` with
`WW3GPU_STRICT=ON`. Status L2 only after that.
