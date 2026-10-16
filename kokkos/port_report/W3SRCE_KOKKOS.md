# W3SRCE_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the checkpoint of the W3SRCE
unit (ADR-0011), itself after the ST4 checkpoint. Selected by `WW_KOKKOS_SRCE=1`; the flag
defaults to 0, so WW3 runs the Fortran.

The source-term integration of one sea point: the semi-implicit dynamic time step with its
limiter, the MLIM shallow-water cap, the high-frequency tail, and the flux diagnostics, with every
source term it calls on the device.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SRCE`, `model/src/w3srcemd.F90:190-2412` at NOAA-EMC/WW3 `03adda91`, the `srce_direct` path as `switches/switch_lab_shrd` compiles it (ST4 NL1 LN1 BT1 DB1 MLIM) |
| Kokkos code | [`../src/ww_kokkos/srce_kernel.hpp`](../src/ww_kokkos/srce_kernel.hpp), a team function `w3srce_team`, one team per point; it calls the ST4 point functions, the DIA as a team ([`snl1_team.hpp`](../src/ww_kokkos/snl1_team.hpp)), [`W3SLN1`](W3SLN1_KOKKOS.md), [`W3SBT1`](W3SBT1_KOKKOS.md), [`W3SDB1`](W3SDB1_KOKKOS.md) |
| C shim | [`../src/fortran_iface/srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp) (`ww_srce_init`, `ww_srce`); parameters in [`srce_params.hpp`](../src/ww_kokkos/srce_params.hpp); the DIA and ST4 tables of the same grid through [`shim_state.hpp`](../src/fortran_iface/shim_state.hpp) |
| WW3 side | `w3kokkosmd.F90` (interfaces, `W3KOKKOS_SRCE_SETUP`, `W3KOKKOS_SRCE_ON`, `W3KOKKOS_SRCE`), `w3initmd.F90` (init and log line), `w3wavemd.F90` (the `srce_direct` call) on the h0ffmann/WW3 branch `ww3-gpu/kokkos-srce` (`e062ff0b`), off `ww3-gpu/kokkos-st4`; the `WW3/` pin is not moved |
| Phase | 1: copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_SRCE)`; Kokkos::Serial inside an OpenMP region |
| Plan | [`W3SRCE_KOKKOS_PLAN_202610.md`](../../docs/W3SRCE_KOKKOS_PLAN_202610.md), issue #42 |

Left to the Fortran `W3SRCE`, call by call (`W3KOKKOS_SRCE_ON`): `IQTPE <= 0` (GQM,
`:1259`); `INFLAGS2(4)` with `IICEDISP` (Liu's dispersion, `:2124-2126`); `TAUWBUG = 0`
(`SINTAILPAR(4) <= 0.5`) with `IT > 0`, where the `W3SPR4` call at `:1119` reads the local `LLWS`
before anything sets it (`w3src4md.F90:302`): undefined in the Fortran, so not translatable. Any
other source-term switch makes `ww_srce_init` refuse (error 5).

## Arithmetic

The reference is the `-O3` object of `w3srcemd.F90` (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`), read as GIMPLE and as assembly.

- (v) No libmvec call, no power call, no FMA in W3SRCE; `logf` (`:1427`, `:1937`), `tanhf`
  (`:1932`, `:1958`) and one `sincosf` for `COS`/`SIN(USTDIR)` (`:2099-2100`), all called the
  same way here. `WN1**3` is `w*(w*w)`; `OFFSET*x` is folded to `x`; `/16.` is `*0.0625`;
  `FFXFM + FAGE` keeps the build's `+ 0.0` at `:1178` and the tail keeps `+ 0.` at `:1995`.
- (v) Every sum is serial and in source order in the build except the time-step `MIN` over `IS`
  (`:1532`, `.REDUC_MIN`, 4 lanes). Every candidate is positive or `+inf`, so a serial `MIN`
  gives the same bits; ⚠ only a NaN candidate could differ.
- (v) Each `MAX`/`MIN` takes its operands in the order of the object's `maxss`/`minss`/`maxps`,
  which is not always the GIMPLE order: `:951`, `:956`, `:1427`, `:1436`, `:1523`, `:1549`,
  `:1937`, `:1958` were reordered after the review. They matter for a NaN or a `+0`/`-0` tie only.
- ⚠ `:2350`: the final `MAX(0., SPEC)` is a 4-wide `maxps` with a scalar remainder whose third
  element has the other operand order; the kernel reproduces it for `NSPEC mod 4 = 3`. It is tied
  to the build's vector width.

## Quirks kept

The `DT = MAX(0.5, DT)` floor (`:1539`); the `TAUWX`/`TAUWY` reset that "keeps the bug as an
option" (`:1069`); `W3SDB1`'s early return leaving `VSDB`/`VDDB` of the previous sub-step;
`ECOS(ITH)` (not `ECOS(IS)`) in the momentum sums (`:2073`); the ice block with `ATT = 1.`, kept for
its signed zeros. Dead under this switch and not ported: `PHINL`, `HSTOT`, `COSI`, `TAUSCX/Y`,
`FHIGI`, `FIRST`, `TAUNUX/Y`.

## Static parity

Two fresh reviewers compared the C++ object (`openmp-release` flags, g++ 16.2) with the Fortran
dump and assembly section by section, 2026-10-15: the initialisation, the time-step limiter, the
spectrum updates, the flux sums, `FAGE`/`FHIGH`/`NKH`, MLIM, the tail, and the post-loop sums and
stresses match statement by statement for finite inputs; the `MAX`/`MIN` orders they found were
fixed. Static parity is still ⚠: only the regtests prove it.

## Benchmarks

None. No timing without parity: nothing is timed before the W3SRCE regtest replay passes on the
same commit (`AGENTS.md`, "No timing without parity").

## How it was regtested

Not yet. What ran, 2026-10-15 (v): `openmp-release` and `serial-debug` built with no warning, ctest
10/10 each (the intro programs' and tools' tests; the port has none, ADR-0010); `cuda-release`
compiled, never run; the fork branch built and linked `ww3_shel` with `KOKKOS`, not run. The
checkpoint (plan §7) adds `SRCE` to `WW3GPU_ROUTINES` and runs `just smoke-ww3-gpu`,
`just full-ww3-gpu` and `just ww3-gpu-matrix` on the CPU.
