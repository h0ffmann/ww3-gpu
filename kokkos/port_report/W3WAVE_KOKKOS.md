# W3WAVE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim that
fills the hook table and the Views, and the W3WAVE hunk on the fork, first.

W3WAVE is WW3's time-step driver for one grid, from the current `TIME` to `TEND`: it updates the
forcing, builds the propagation maps, runs the intra-spectral, spatial and intra-spectral
propagation in that order with the CFL sub-step count, integrates the source terms, and decides
the output and restart times from WW3's integer dates. The port is a host-side sequencer in C++
that calls the Kokkos launchers already translated and reaches every other callee through a table
of C function pointers.

## What was ported

| | |
|---|---|
| WW3 routine | `W3WAVE`, `model/src/w3wavemd.F90:229-2997` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it: `SHRD`, `PR3`, `UQ`, the lab's source terms |
| Kokkos code | [`../src/ww_kokkos/wave_w3wave.hpp`](../src/ww_kokkos/wave_w3wave.hpp) (the driver, `ww::wave::w3wave::w3wave<ExecSpace>`), [`../src/ww_kokkos/wave_w3wave_state.hpp`](../src/ww_kokkos/wave_w3wave_state.hpp) (`Params`, `Module`, `Arrays`, `Callees`), [`../src/ww_kokkos/wave_w3wave_hooks.hpp`](../src/ww_kokkos/wave_w3wave_hooks.hpp) (the `extern "C"` hook table `ww_w3wave_hooks`) |
| Translated callees it launches | `W3DZXY` (`updt_dzxy.hpp`), `W3UWND` (`updt_uwnd.hpp`), `W3MAP3` (`prop_map3.hpp`), `W3UTRN` (`updt_utrn.hpp`), `W3MAPT` (`prop_mapt.hpp`), `W3CFLXY` (`prop_cflxy.hpp`), `W3KTP3` (`prop_ktp3.hpp`), `W3GATH`, `W3SCAT` (`wave_gath_scat.hpp`), `W3XYP3` (`prop_xyp3.hpp`), `W3SRCE` (`srce_kernel.hpp`, `w3srce_team`) |
| Callees through hooks | `DSEC21`, `TICK21`, `W3SETO`/`W3SETG`/`W3SETW`/`W3SETA`/`W3SETI`, `EXTCDE`, the `NDSE`/`NDSO`/`SCREEN` writes (formats 900-951, 1000-1008, `STME21`, `WWTIME`, `BACKSPACE`), `PRINT_MEMCHECK`, `W3UCUR`, `UG_GRADIENTS`, `W3UINI`, `W3UTAU`, `W3URHO`, `W3IOBC`, `W3UBPT`, `W3UICE`, `W3UIC1`, `W3ULEV`, `W3CFLUG`, `W3XYPUG`, the Fortran `W3SRCE`, `W3CPRT`, `W3OUTG`, `W3IOGO`, `W3IOPE`, `W3IOPO`, `W3IOTR`, `W3IORS`, `W3IOSF` |
| C shim | none yet; the hook table and the `Arrays` Views are what it fills |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

The memory contract is explicit (`wave_w3wave_hooks.hpp`): the module arrays live in Views owned by
the shim; before every hook that may touch arrays the driver fences and calls
`sync_to_host(ctx, id)`, and after it `sync_to_device(ctx, id)`. Module scalars and dates are
passed by address (`Module`), so a hook that changes `TICE`, `TBPIN` or `FLBPI` is seen at once.
W3WAVE's own locals `FIELD`, `TAUWX`, `TAUWY` are Views the driver allocates per call, as the
Fortran allocates them (`:708`, `:763`).

## Arithmetic

The reference is the `-O3` object of `w3wavemd.F90` (WW3's CMake Release flags, gfortran 16.2,
switch defines `W3_SHRD W3_PR3 W3_UQ ...` from the CMake `flags.make`), whose disassembly is
identical to the CMake build's, read as GIMPLE (`-fdump-tree-optimized-lineno`), as `objdump -dl`
and as its relocations, 2026-10-16. The C++ side is the `-O3` GIMPLE and object of a throwaway
unit with the `openmp-release` flags.

- (v) The relocations of `__w3wavemd_MOD_w3wave` list every external call: `DSEC21` (45 sites),
  `TICK21` (3), `STME21` (3), `EXTCDE` (8), `PRINT_MEMCHECK` (37 call sites for the 30 source
  calls, the rest duplicated by the optimiser), `lroundf` (2), `sincosf` (1), and each callee
  above once or twice. Every one has its place in the driver; no other libm call exists.
- (v) `NT = 1 + INT(DTTST/DTMAX - 0.001)` (`:1094`) and `NTLOC = 1 + INT(DTG/DTCFLI - 0.001)`
  (`:1771`): a division, a subtraction of `0x3a83126f`, `cvttss2si`; `st4::fint` gives the same
  integer, `INT_MIN` for NaN or out of range included.
- (v) `:1147-1148`: the GIMPLE forms one sum for `DTGA+DTRES` (`:1147`) and `DTRES+DTGA`
  (`:1148`), `DTG = REAL(lroundf(s + 0.0001))` truncated to `INTEGER(4)`, then `DTRES = s - DTG`.
  The C++ writes the sum in the order of `:1147`, `dtga + dtres` (IEEE addition commutes, so
  either order gives the same bits), and its GIMPLE has the same three operations; `st4::nint` is
  `roundf` then a 64-bit conversion, which equals `lroundf` for every input, and both give 0 for
  NaN or out of range.
- (v) `:1167` `MAX(1., DTTST2)` is `maxss DTTST2,%xmm1` with `%xmm1 = 1.`: `1. > DTTST2 ? 1. :
  DTTST2`, so a NaN gives `DTTST2`; the C++ GIMPLE is `DTTST2 < 1. ? 1. : DTTST2`, the same
  selection. `:1168-1169` are one `sincosf` of `FAC*GD0 + (1-FAC)*GDN` and two products, as in
  the C++ (`sincosf` on glibc), which keeps the source's `FAC*GA0` and `FAC*GD0` operand order.
- (v) `:1819` and `:2131` `MAX(DMIN, DW(ISEA))` is `maxss (DMIN),%xmm0` with `%xmm0 = DW`:
  `DW > DMIN ? DW : DMIN`. The C++ kernel computes `DMIN < DW ? DW : DMIN`, the same selection.
- (v) `:753` `FCUT = SIG(NK) * TPIINV` with `TPIINV` folded to `1.5915493667125701904296875e-1`,
  the value of `st4::kTpiInv` (a `static_assert`); `:769` `FACX` is the folded
  `9.0000003183376975357532501220703125e-6` (`:1894` store). `FACX` is read only by `W3CFLUG`
  and `W3XYPUG` here.
- (v) `:1784` `ITLOCH = (NTLOC + 1 - MOD(NINT(REAL(DTTEST)/DTG), 2)) / 2`, `DTTEST` an `INTEGER`
  (`:601`), so `DSEC21`'s result is truncated first; `MOD` and `/2` are C++ `%` and `/` on `int`.
- (v) `0.5*DTI0 >= DTTST` (`:1343`, `:1373`, `:1426`) is the form of both GIMPLEs. No FMA in the
  C++ object (`objdump`, 0 `vfmadd`/`vfmsub`), and W3WAVE has no vectorised loop of its own.
- (v) The order of every call, flag update and date assignment follows the preprocessed source
  line by line: the forcing block (`:1162-1443`), the maps (`:1454-1478`), `DCDX = DCDY = 0.`
  (`:1499-1500`), the `FLZERO` exit (`:1675`), the CFL estimate (`:1689-1731`), the half-step
  split `ITLOC = 1..ITLOCH`, the spatial bins, then `ITLOCH+1..NTLOC` (`:1794-2201`), the source
  terms (`:2216-2379`), the step update and log (`:2395-2410`), and the output types `J =
  1..NOTYPE` then `J = 8` (`:2626-2826`).
- (v) Parallel launches give the serial bits. `W3CFLXY`, `W3KTP3` and `W3SRCE` are called once per
  point and each writes only its own `JSEA` column and scalars while reading arrays the loop does
  not write; WW3 runs the same three loops as `!$OMP PARALLEL DO` under `OMPG` (`:1697`, `:1798`,
  `:2241`). The spatial bins keep the Fortran's serial `ISPEC` loop: one `W3GATH`, one serial
  `W3XYP3` body, one `W3SCAT` per bin.
- (v) `W3XYP3` calls `DSEC21` itself (`w3pro3md.F90:928-930`, `:1396-1398`); the driver calls the
  hook once per step and passes the three values in `prop::Step`, the same values for every bin of
  the step because `TIME`, `TBPI0` and `TBPIN` do not change inside the `ISPEC` loop and `DSEC21`
  is a pure function of its two dates and `CALTYPE` (`w3timemd.F90:330-568`). A device kernel
  cannot call the hook; fewer hook calls, same values.
- (v) The source-term seam `srce_on_kokkos` is not W3WAVE's: it is the fork's
  `W3KOKKOS_SRCE_ON(IT)` (`WW3/model/src/w3kokkosmd.F90:1400-1408` at `8e433f99`), which the fork
  calls per point inside the `JSEA` loop and which depends on `IT` and grid constants only. When it
  says no, the `w3srce_fortran` hook runs W3WAVE's loop `:2246-2349` unchanged. It is a required
  hook: `srce_kernel.hpp` relies on it to refuse the branches it does not port.
- ⚠ The C++ object was compared with the Fortran object for the statements above only; the device
  callees are covered by their own port reports, not re-read here.

## Not ported

- `GTYPE = SMCTYPE` is a run-time value even without `W3_SMC` (`ww3_grid` sets it for `'SMCG'`,
  `w3gridmd.F90:3594-3596`). Every SMC body is under `W3_SMC`, not compiled, and `FIELD` is never
  allocated (`:756-764`). The driver translates the empty branches (the forcing, `W3CFLXY` and the
  source terms still run) and returns `kErrUnsupported` at `:1942`, where the Fortran assigns the
  unallocated `FIELD`, which is undefined ⚠ (not executed on either side).
- `W3_PDLIB` bodies (`:1900-1929`, `IOBP_LOC`), `W3_MPI`, `W3_OMPG`/`W3_OMPH`, `W3_SEC1`,
  `W3_REFRX`, `W3_IC3`, `W3_REF1`, `W3_BT4`, `W3_OASIS`, `W3_SBS`, `W3_PR1`/`W3_PR2`, `W3_FLX5`,
  `W3_DEBUG*`, `W3_TIMINGS`, `W3_T`, `W3_S`: not compiled under the lab switch. `LPDLIB` and
  `GTYPE = UNGTYPE` remain run-time branches and are translated (the unstructured calls go
  through hooks).
- Dead stores kept out: `JSEA = 1; ISEA = JSEA` (`:1008`, `:1023`), `IK = IX = IY = 1`
  (`:2020-2022`), `ITEST = RSTYPE` (`:2709`, `:2796`), and `TMP2 = BEDFORMS(JSEA,1:3)` with its
  copy back (`:2280`, `:2339`): `W3SRCE`'s `BEDFORM` is `INTENT(INOUT)` (`w3srcemd.F90:684-688`)
  and appears nowhere else in `:190-2412` once preprocessed under the lab switch (v), so the copy
  back writes the values it read.
- `ODAT` is read only under `W3_OASIS`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit that
includes `wave_w3wave.hpp` and instantiates `w3wave<Kokkos::DefaultExecutionSpace>` (OpenMP in the
pinned `ww3` shell, Kokkos 5.2.2) compiled with g++ 16.2 and the `openmp-release` flags
(`-O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wno-unused-parameter -ffp-contract=off -fopenmp`) with no warning and no FMA.

- (v) Fresh review 2026-10-16: the preprocessed W3WAVE (fork `8e433f99`, `switch_lab_shrd`, the
  same text as NOAA `03adda91` outside the KOKKOS hunk) read line by line against the driver; a
  fresh `-O3` object of `w3wavemd.F90` with WW3's CMake flags (disassembly identical to an
  out-of-tree CMake build's) read as GIMPLE, `objdump -dl` and relocations; every launcher's
  signature and index base; every line citation. Defects found and fixed: `GTYPE = SMCTYPE` was
  refused at entry, where the Fortran runs until `:1942`; the null `srce_on_kokkos` silently ran
  the Kokkos source terms on branches `srce_kernel.hpp` leaves to the Fortran (now required); the
  `TMP2`/`BEDFORMS` omission was argued from the fork's shim instead of `W3SRCE`; commuted
  `DTRES+DTGA`, `GA0*FAC`, `GD0*FAC` restored to the source's order (same bits). The hoisted
  `DSEC21` of `W3XYP3` is kept: pure, same values. Recompiled with no warning:
  `w3wave<Kokkos::OpenMP>` and `w3wave<Kokkos::Serial>` with the `openmp-release` flags (0
  `vfmadd`/`vfmsub`) and at `-O0 -fsanitize=address,undefined` (the `serial-debug` instrumentation,
  in a throwaway unit, not the preset), and `w3wave<Kokkos::Cuda>` with `nvcc_wrapper -O3
  -std=c++20 -arch=sm_89 --fmad=false -extended-lambda -fopenmp` in the `#cuda` shell (Kokkos
  5.2.2, compile only): host code 0 FMA; a probe kernel shows `--fmad=false` removes the
  contracted `FFMA`, and the `FFMA` left in the device code come from CUDA's own division, square
  root and transcendental sequences.

Open ⚠: nothing was run or regtested; the C++ object was compared with the Fortran object for the
statements above only, and the device callees rest on their own port reports; the CUDA build's
transcendental functions are not shown to return glibc's bits (`AGENTS.md`, "Bit for bit"); the
`SMCTYPE` refusal at `:1942` is not executed.
