# 2026-10-19: the batched W3SRCE for SEED, FLD2 and no LN1 (goal be5bdbea-3817)

Unit U4 of [the GFS-Wave plan](../plans/2026-10-19-007-gfs-wave-target-plan.md).

## What was tried

The switch of WW3's GFS-Wave regtests (`regtests/ww3_ufs1.2/input/switch_MPI_OMPH` at the pin `aa382393`) has `SEED`
and `FLD2` and no `LN1`. The gate of `W3KOKKOS_SRCE_SETUP` required `LN1` and refused `SEED`, `FLD1`, `FLD2`, so
the batched `W3SRCE` never ran under it. The kernel (`kokkos/src/ww_kokkos/srce_kernel.hpp`) now translates three
branches of `W3SRCE`, each chosen at run time by a flag of the packet (`srce_params.hpp`), and the fork's gate
admits them (`kokkos/src/fortran_iface/PATCH.md`, section 32).

| Branch | Fortran (`WW3-noaa/` at `03adda91`) | Kokkos |
|---|---|---|
| no `LN1` (or `LN0`) | `w3srcemd.F90:884-886` zeroes `VSLN`; no call at `:1222` | `SrcePoint::step_a_sln1`: `if (p.ln1)` |
| `SEED` | `:1970-1982`, after the limiter and before the tail | `step_c_after_sds4`, `if (p.seed)` |
| `FLD2` | `HSTOT` `:1816-1847`; `CALC_FPI` `:2279`; `W3FLD2` `:2291-2299`, `CHARN = FLDALPHA` otherwise | `step_c_after_sds4` (the `HSTOT` sum), `finish()`; [`srce_fld2.hpp`](../../kokkos/src/ww_kokkos/srce_fld2.hpp) holds `W3FLD2` and `APPENDTAIL` |

`FLD1` stays refused. New port reports: [`W3FLD2_KOKKOS.md`](../../kokkos/port_report/W3FLD2_KOKKOS.md),
[`APPENDTAIL_KOKKOS.md`](../../kokkos/port_report/APPENDTAIL_KOKKOS.md).

## Static parity argument (ADR-0016)

Reference: `WW3-noaa/` built with the `ww3_ufs1.2` switch and WW3's CMake Release flags (`-O3 -g -fopenmp`, gfortran
16.2.0, the pinned shell) plus `-fdump-tree-optimized-lineno`; the assembly of `w3srcemd.F90` from the same flags with
`-S` (v, both read 2026-10-19). Command: `cmake -S WW3-noaa -B build/ref-ufs -G Ninja -DSWITCH=<switch_MPI_OMPH>
-DCMAKE_BUILD_TYPE=Release -DCMAKE_Fortran_FLAGS=-fdump-tree-optimized-lineno`, then `cmake --build`.

- (v) **No `LN1`.** `VSLN` is a never-written array after `VSLN = 0.`; the object still adds and scales it
  (`0 + x` is not `x` for `x = -0`), so the kernel keeps `vsln` at 0 and the same `vs` sum and the ice scaling.
- (v) **`SEED`** (GIMPLE `[1972]-[1978]`, assembly `.loc 1 1972`-`1978`): `UC = (FACSD*GRAV)/SIG`, `SLEV =
  (((MIN(MAX(U10ABS/UC-1,0),1)*6.25e-4)/(w*w*w))/SIG)`, `(1-ICE)*SLEV` under `INFLAGS2(4)`, `cosf(U10DIR-TH)`, `MAX(c,0)`,
  squared as `c*c`, `SLEV*c²`, and `MAX` into the spectrum. The `maxss`/`minss` operands were read: `maxss 0, x` and
  `minss 1, m` for `SLEV`; for the spectrum `maxss spec, v`, so the new value is the first operand and a tie keeps the old
  one (the GIMPLE has them the other way). `6.25E-4` is the `float` literal.
- (v) **`HSTOT`** (`[1843]`, `[1847]`): `hstot += factor*spec` in the loop that already sums `WHITECAP(3)`, then
  `sqrtf(hstot)*4`. Summed in every call; it feeds only the `FLD2` branch.
- (v) **`CALC_FPI` and the call** (`[2279]`, `[2292]`-`[2299]`, assembly `.loc 1 2295`): `MIN(FPI/TPI, 2.)` with `FPI/TPI` first
  (`minss`), `WNDX = (COEF*U10ABS)*cos`, `WNDY = (COEF*U10ABS)*sin` from one `sincosf(U10DIR)`, `ZWND = ZZWND`, `DEPTH`
  the local, `DAIR`; `CHARN = FLDALPHA` otherwise; `U10ABS > 10` and `HSTOT > 0.5`. `CALC_FPI` is the unit of
  2026-10-16 (object identical under FLD2); `FPI` is written back to `FPIS(ISEA)`.
- (v) **`W3FLD2`** (`w3fld2md.F90` GIMPLE, 2 495 lines read): `**2.0` is a product, `**3.0` is `powf(x, 3.)` (three
  calls per bin, hoisted out of the direction loop, so the C++ calls `dm::powf` per bin), `/2.0` is `*0.5`, `XFR**2` is
  `XFR*XFR`; `UREFD = atan2f(WNDY,WNDX)` and one `sincosf(UREFD)`; `USTRA = (UREF*0.4)/logf(ZWND/Z01)`; `Z0 =
  10/expf((UREF*0.4)/USTRA)`; the stress integral `acc + ((((SPC2*SCIN)*COS)*SIG2)*DTH)` with one `sincosf(TH(T))` and a
  lone `cosf(UREFD-TH(T))`; `SCIN = (((((A1*E)*|E|)*DAIR)/1000)*WN)/CP`; `TAUXW`, `TAUYW` added in `K` order as
  `tauintx*(dwn*1000)`; the smooth-roughness loop with its peeled first pass (`LOG(10/0.001)` is the constant
  `9.2103404998779296875`, `0.132*NU` the constant `1.386e-6`); `USTRB = sqrtf(sqrtf(TAUY²+TAUX²)/DAIR)`;
  `(USTRB*0.5)*2.0` for the next guess; 19 iterations at most, the 5 % test, the fail-safe to the bulk stress. The C++
  follows these statements in this order (`srce_fld2.hpp`).
- (v) **`APPENDTAIL`** (`w3fld1md.F90` GIMPLE): every `SUM` is an in-order sum; the three `WN2(K)**3.0` divisions are
  vectorised for `NTH >= 4`, and the object calls libmvec's `_ZGVbN4vv_powf` for the first `4*(NTH/4)` directions and
  scalar `powf` for the rest. The port calls the same function for the same directions (`ww::st4::mvec_powf`, the helper
  ST4 uses), because libmvec and scalar `powf` differ in the last bits (`st4_libmvec.hpp`).
- ⚠ Not done: the C++ objects were not dumped and compared with this GIMPLE; the argument is the C++ read against the
  GIMPLE above, and the run below. `MIN(PI/WN, 20.)` and the `MAX`/`MIN` of `APPENDTAIL` take the GIMPLE operand order; the
  assembly of `w3fld2md.o` and `w3fld1md.o` was not read for them (only NaN or a signed-zero tie could differ).
- ⚠ Device: `asinf`, `acosf` (`APPENDTAIL`, rare branch) are CUDA's. The rest goes through `ww::dm`. The headers
  `fld1_sig2wn.hpp` and `fld1_wnd2z0m.hpp` were changed from `Kokkos::tanh` and similar to `dm::` calls; on the host the
  wrappers call the same functions.

## Runs

CPU only, Kokkos OpenMP backend, gfortran and g++ 16.2.0, never `--gpu` (v):

| Command | Result |
|---|---|
| `cmake -S WW3 -DSWITCH=<ufs1.2 switch KOKKOS> -DWW_KOKKOS=ON` and `cmake --build` | the fork compiles and links with the GFS-Wave switch |
| `WW_KOKKOS_STRICT=1 WW3GPU_SWITCH=switch_lab_gfs bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos SRCE` | every model file byte-identical to the Fortran of the same executable; binary files also to `--ww3`; `nccmp-tol` PASS; `CHA` 0.016 (`W3FLD2` ran; `FLDALPHA` is 0.0095) |
| the same with `full` | 9 520 points, 12 hours: byte-identical, PASS; 20.84 s with Kokkos, 27.01 s Fortran (one run each, not a timing) |
| `... smoke ... WW3GPU_SWITCH=switch_lab_ompg` | the unchanged lab switch: byte-identical |
| `WW3GPU_KOKKOS=all WW3GPU_MPI_RANKS=1 WW3GPU_OMP_THREADS=1 WW_KOKKOS_STRICT=1 just ww3-gpu-matrix 1 mpi -R '^323:'` | `ww3_ufs1.1` `grdset_c`, the GFS-Wave switch: `test.comp` 0 of 30 files different, PASS; 33 286 spectra in 24 batched calls through the new path, no fallback |

`switch_lab_gfs` is `switch_lab_ompg` with `SEED FLD2` for `LN1` (`switches/README.md`).

## Not done, left to the validation phase

- `just ww3-gpu-matrix` on the other ufs entries, with `WW3GPU_STRICT=ON` and zero fallbacks.
- Grids with `NTH` not a multiple of 4 (the scalar-`powf` remainder of `APPENDTAIL`), the bulk fail-safe and the
  non-convergence path: written from the GIMPLE, not exercised by the cases above (`NTH` is 24 in the smoke and the
  full, 36 in `ww3_ufs1.1`).
- `TAIL_ID` 1 (the saturation level from `WND2SAT`) is translated and not exercised: the cases use `TAIL_ID` 0.
- No timing: no WW3 regtest has run, so "regtest pending ⚠".
