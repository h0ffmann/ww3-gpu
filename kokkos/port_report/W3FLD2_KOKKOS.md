# W3FLD2_KOKKOS

**Status:** ported, phase 1 (translated); wired into the batched `W3SRCE` under `W3_FLD2`
(`WW_KOKKOS_SRCE=1`). Not regtested by WW3's regtest matrix: postponed to the validation phase
(ADR-0016). Goal be5bdbea-3817.

Wind stress vector from the wave spectrum and the wind (Donelan et al. 2012, Reichl's version): it
extends the spectrum to `k` = 366 rad/m with a saturation tail (`APPENDTAIL`, [its report](APPENDTAIL_KOKKOS.md)),
then iterates the friction velocity `u*` until the wave-supported stress plus the viscous stress match,
and returns `u*`, its direction and the Charnock parameter. `W3SRCE` calls it after the time loop when
`U10ABS > 10` m/s and `HSTOT > 0.5` m, with `FPI` from `CALC_FPI`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLD2`, `model/src/w3fld2md.F90:65-590` at NOAA-EMC/WW3 `03adda91` (526 lines); the call is `model/src/w3srcemd.F90:2291-2299` (the pin's `:2747-2755`) |
| Kokkos code | [`../src/ww_kokkos/srce_fld2.hpp`](../src/ww_kokkos/srce_fld2.hpp), `ww::fld2::w3fld2`; it calls the units `sig2wn`, `wnd2z0m`, `wnd2sat` (FLD2's), `infld` and `appendtail` |
| Called from | `SrcePoint::finish()` of [`srce_kernel.hpp`](../src/ww_kokkos/srce_kernel.hpp), after `calc_fpi` ([report](CALC_FPI_KOKKOS.md)); work in the point's `st4w` slice (`ww::fld2::work_reals`) |
| C shim | the batched `W3SRCE` of [`srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp); `FPI` is one more row of `PSC`, the status bits ride in `SHAVE` and are printed and cleared by `report_fld2` |
| Fork | `ww3-gpu/gfs-srce-be5bdbea`, `PATCH.md` section 32 |

Differences from the Fortran that change no result: the `ALLOCATE` of `WN`, `DWN`, `CP`, `SIG2`, `SPC2`,
`TAUINTX`, `TAUINTY` becomes a slice of preallocated work (`nkt_max`, a bound from the deep-water wavenumber;
a longer spectrum sets a capacity status and the shim stops); the two `PRINT*` lines (`:488`, `:575-576`)
become status bits; `Z0` and `TAUNUX`/`TAUNUY` are locals. `FIRST`/`INFLD` copy the grid's tail parameters
every call (see the caveat below).

## Arithmetic

Reference: the `-O3` GIMPLE and assembly of `w3fld2md.F90` and `w3fld1md.F90` from WW3's CMake Release
build with the `ww3_ufs1.2` switch (gfortran 16.2.0, pinned shell), read statement by statement
(`docs/log/2026-10-19-gfs-srce-seed-fld2.md` lists them).

- (v) `**2.0` is `x*x`, `XFR**2` is `XFR*XFR`, `/2.0` is `*0.5`; `**3.0` is the libm call `powf(x, 3.)`, kept.
- (v) `COS(TH(T))` and `SIN(TH(T))` are one `sincosf`; `COS(WND_10_DIR-TH(T))` is a lone `cosf`; `COS/SIN(UREFD)` one `sincosf`.
- (v) The products of the stress integral keep the source order, `((((SPC2*SCIN)*COS)*SIG2)*DTH)`; `TAUXW`, `TAUYW`
  and every `SUM` add in index order (the vector loops of the object add lane 0, 1, 2, 3 in turn).
- (v) The first pass of the smooth-roughness loop is peeled by gfortran and its `LOG(10./0.001)` is the constant
  `9.2103404998779296875`; `0.132*NU` folds to `1.386e-6` (a `static_assert` checks it).
- (v) `USTRB*.5 + USTRB*.5` is `(USTRB*0.5)*2.0` in the object.
- ⚠ `MIN(PI/WN, 20.)` and the `MAX`/`MIN` of `APPENDTAIL` keep the operand order of the GIMPLE; a NaN or a signed-zero tie
  could differ from the object; none arises with finite input. The assembly of `w3fld2md.o` was not read for these sites.
- ⚠ `SAT` (`TAIL_ID` other than 0 or 1) and `ANGLE1(1)` (a NaN spectrum) are undefined in the Fortran; the port uses 0.
- ⚠ Caveat: `INFLD` runs at a thread's first call and its parameters stay for every grid; the port reads the point's grid.
  The fork stops the part when two grids of one run have different `TAIL_ID`, `TAIL_LEV`, `TAIL_TRAN1` or `TAIL_TRAN2`.
- ⚠ On a device `asinf` and `acosf` (in `APPENDTAIL`) are CUDA's, not glibc's; the other calls go through `ww::dm`.
  CUDA was not compiled or run.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The GFS-Wave profile puts `w3fld2` at 11.8 % of the run
(`docs/data/ww3_gfs_gprof.tsv`, goal be5bdbea-3817).

## How it was regtested

WW3's regtest matrix has not run on it (ADR-0016). CPU replay of 2026-10-19 (Kokkos OpenMP backend, never the GPU):
`scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos SRCE` and `... full ...` with `WW_KOKKOS_STRICT=1` and
`WW3GPU_SWITCH=switch_lab_gfs` (`switch_lab_ompg` with `SEED FLD2` in place of `LN1`): every model file byte-identical
to the Fortran of the same executable, the binary ones also to `--ww3`, `nccmp-tol` PASS, no fallback. The smoke wind
is 12 m/s, so `W3FLD2` runs (the output `CHA` is not `FLDALPHA`).
