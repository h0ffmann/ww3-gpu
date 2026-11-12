# W3FLD1_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Wind stress vector from the wave spectrum and the wind vector at height `ZWND`, after Reichl,
Hara and Ginis (2014): the spectrum is extended to `k` = 366 rad/m with a saturation tail
(`APPENDTAIL`, [its report](APPENDTAIL_KOKKOS.md)); the wave-supported stress is integrated up the
extended spectrum; the viscous stress `TAUNU` is found by a two-component Newton iteration
(finite differences of the wind at `ZWND`, step `DTX` = 5e-5) so that the computed wind equals the
input wind; the output is the friction velocity `UST`, its direction `USTD`, the roughness length
`Z0` and the Charnock parameter `CHARN`. When the result fails a drag-coefficient and an alignment
test it falls back to the bulk formula of `WND2Z0M`. `W3SRCE` calls it under `W3_FLD1` when
`U10ABS` > 10 m/s and `HSTOT` > 0.5 m, with `FPI` from `CALC_FPI`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLD1`, `model/src/w3fld1md.F90:93-779` at h0ffmann/WW3 `96892c95` (687 lines), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff --quiet 03adda91 HEAD -- model/src/w3fld1md.F90` exits 0, v), as `ww3_ts3/input/switch_ST4_PR1_FLD1` compiles it |
| Kokkos code | [`../src/ww_kokkos/fld1_w3fld1.hpp`](../src/ww_kokkos/fld1_w3fld1.hpp), `ww::fld1::w3fld1`: a device function for one sea point, called inside a team body like `ww::fld2::w3fld2` (no launcher of its own); it calls the units `sig2wn`, `wnd2z0m`, `wnd2sat` and `infld` of `fld1_*.hpp` and `ww::fld2::appendtail` of [`srce_fld2.hpp`](../src/ww_kokkos/srce_fld2.hpp) |
| Caller | `W3SRCE`, `model/src/w3srcemd.F90:2740-2742` at the pin, after `CALC_FPI` (`:2735`); the Fortran passes `RIB` = 0., which `W3FLD1` never reads, so the port has no `RIB` |
| C shim | none yet; the batched `W3SRCE` of the fork refuses `FLD1` |
| Not ported | `W3_S` (`STRACE`), not compiled; the `WRITE(*,*)` lines (`:733-734`, `:766`) are status bits |

`ww::fld1::work_reals(nth, nktmax)` is the work the twenty `ALLOCATE`d arrays of `:294-298` and
`APPENDTAIL`'s `BT`, `ANG`, `NORMSPC`, `ANGLE1` need. `ww::fld2::nkt_max` bounds `NKT` for it. The port
returns `kStatusCapacity` (nothing computed) where `NKT` > the bound or `NKT` < 6; the Fortran reads
`WN` beyond its bounds in the `KA` loops (`:358`) for `NKT` < 6. `TAUNUX` and `TAUNUY` are
`INTENT(INOUT)` as in the Fortran and hold the converged viscous stress on return; `CHARN` is a
pointer, null for the `OPTIONAL` absent.

## Arithmetic

Reference: the `-O3` GIMPLE of `w3fld1md.F90` from WW3's CMake Release build under
`ww3_ts3/input/switch_ST4_PR1_FLD1` (gfortran 16.2.0, `-O3 -g -fno-second-underscore`), recompiled
with `-fdump-tree-optimized`, read statement by statement for the whole routine; `SIG2WN`,
`WND2SAT`, `INFLD` are inlined in it and `WND2Z0M`, `APPENDTAIL` are calls.

- (v) `**2.0`, `**2` and `XFR**2` are `x*x`; `**(1.5)` is the libm call `powf(x, 1.5)`; `**3.0` is
  `powf` (kept in the tail extension, `:335`, one call per bin, hoisted out of the direction loop).
- (v) `SIN(TH(T))` and `COS(TH(T))` are one `sincosf` (`__builtin_cexpif`); `COS(ANGDIF)` is a lone
  `cosf`, computed once per direction and squared.
- (v) The integrands keep the source order, left to right: `((((SIN*COS**2)*CBETA)*SPC2)*SQRT(...))*WN**2)*DTH`;
  `BETAG = ((((BP*CBETA)*SQRT(...))/DWAT)*SIG2)/CP**2` (the `/DWAT` stays a division, 1000.);
  `FTILDE` adds `((BETAG*DWAT)*GRAV)*SPC2*DTH`. The sums over `T` are accumulators started at 0.0,
  added in index order.
- (v) `DWN(NKT) = WN(NKT)*XFR**2 - WN(NKT)` and `DWN(K) = WN(K+1) - WN(K)` (differences, no `/2.0` as in
  `W3FLD2`); `TLTNA(2)` is `(TLTND*DWN)*0.5` and the others `((TLTND(ZI)+TLTND(ZI-1))*0.5)*DWN`.
- (v) The smooth-law loop (`:379-388`) has the folded `0.132*NU` = 1.3860000080967438e-6 over `USTSM`, the
  test `ABS(Z0SM-Z1) < 10.0**(-6)` with the REAL 9.99999997e-7, and stops after the sixth pass
  (`ITERATION > 5`); `KAPPA*WND_IN_MAG` is one product shared with the bulk branch (`:768`).
- (v) `ZNU = 0.1*1.45E-5/SQRT(...)` folds `0.1*1.45E-5` in REAL to 1.4500000133921275e-6.
  `static_assert`s hold these three constants and `DTX`.
- (v) `MAX(ABS(WNDY),CK)` is `maxss` with `ABS(WNDY)` first: a NaN gives `CK` (4.).
- (v) The `ITERATION .LT. 2` branch (`:709`) cannot be taken (`ITERATION` is at least 2 after `:689`) and is
  kept as written; the `ITERATION .GE. 30` branch sets `UST = -999` (`:736`), which `:754` overwrites, so
  the port does not store it.
- (v) The search of `KB` (`:564-569`) and `TAUT` (`:543`), `UPROFV`, `VPROFV` (`:579-580`), `CD` after the
  bulk branch (`:770`) are dead: the object computes none of them
  and the port does not either. The `KB` loop has no lower bound on `KB` in the source (`TLTN(0)` is
  read if the ratio stays above .99 down to 1); the object does not run it.
- Where the object commutes a `+` or a `*` (`UITV**2 + VITV**2`, `TAUY**2 + TAUX**2`, `TLTN(...)` and
  `TLTE(...)` in a sum), the port keeps the source order: only the payload of a NaN can differ ⚠.
- Every libm call, `APPENDTAIL`'s `asinf` and `acosf` included, goes through `ww::dm`: glibc's on the
  host. ⚠ CUDA was not compiled or run.
- (v) The unit compiles with no warning at `-O3 -march=x86-64-v3 -std=c++20 -fopenmp -ffp-contract=off
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion` (Kokkos headers as `-isystem`), 0 `vfmadd` in the
  object, and at `-O0 -g -fsanitize=address,undefined`.

## Deviations from the Fortran that change no result

- `FIRST`/`INFLD`: a threadprivate `SAVE` in the Fortran (`:240-261`); the first call of a thread fixes the
  tail parameters for every grid. The port reads the point's grid on every call, as `W3FLD2`'s port does.
  The two agree unless two grids of one run have different `FLD1` namelists ⚠ (not checked here).
- ⚠ `SAT` stays 0 where the Fortran leaves it undefined (`TAIL_ID` not 0 or 1); `CBETA` is 0 where the
  Fortran leaves it undefined (the first `WAGE` a NaN, for which the object keeps the previous `CBETA`,
  which the port also does).
- The `ALLOCATE`s become slices of preallocated work (above).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested by WW3's regtest matrix: postponed to the validation phase (ADR-0016). Not wired: no shim,
no caller hunk.

A throwaway differential program (not kept, ADR-0010) linked `libww3.a` of the build above, with
`W3GDATMD` set up through `W3NMOD`/`W3DIMS`/`W3SETG`, and called the Fortran `W3FLD1` and the C++
`w3fld1` on the same random inputs: NK 20 to 31, NTH 4 to 28, wind 1 to 90 m/s, depth 2 to 6000 m,
spectra of random level and direction, `TAIL_ID` 0 and 1, `CHARN` present and absent. Because `FIRST`
fixes the tail parameters and `W3DIMS` fixes `NK`/`NTH`, those are constant within a process and vary
across seeds. At `-O3`, 72 runs of 500 cases (3 modes x 24 seeds): 216 000 output words compared
(`UST`, `USTD`, `Z0`, `TAUNUX`, `TAUNUY`, `CHARN`) bit for bit, 0 failures; the bulk fail-safe ran in 146
cases and the not-converged exit in 201 (C++ status bits; in the one run whose output was kept, seed 5, 300 cases,
the Fortran printed 8 and 10 messages for them). With the
C++ at `-march=x86-64-v3`, 18 runs, 54 000 words, 0 failures; under AddressSanitizer and
UBSan, 6 runs of 100 cases, 0 reports. ⚠ No command in the repository reproduces these counts.

## WW3 defects found

- ⚠ `CRIT2` (`:549`) is `ABS(TAUDIR-TAUDIRB)*100.0/(TAUDIR+TAUDIRB)*0.5`: left to right, the factor 0.5
  multiplies, where `CRIT1` (`:547`) divides by `(USTAR+USTRB)*0.5`. It looks like a slip; not
  reproduced numerically. The port keeps the source.
- `KB` (`:564-569`) can index `TLTN(0)`, and the loop is dead (see above).
- `UST = -999` (`:736`) is always overwritten by `:754`.
