# 2026-10-19: static and diagnostic checks of the GFS-Wave W3SRCE merge (risks 2, 3, 4 and 7 of PR #206)

Goal be5bdbea-3817. The risks are the numbered ones in the body of PR #206 (`goal/be5bdbea-3817-gfs-wave`, head
`c0021a3`); the entry that made the merge is [`2026-10-19-gfs-srce-seed-fld2.md`](2026-10-19-gfs-srce-seed-fld2.md).
Everything ran on the CPU (OpenMP), never `--gpu`. Nothing here is a regtest verdict: WW3's regtests are postponed
(ADR-0016), and the port has no tests of its own (ADR-0010). The evidence is the objects of both compilers read, and
WW3's own executable run with the Fortran and the Kokkos routine side by side.

## Risk 3: the C++ half of the static argument

Reference: the `-O3` objects of `w3fld1md`, `w3fld2md` and `w3srcemd` that the earlier entry describes (`WW3-noaa/` at
`03adda91`, the `ww3_ufs1.2` switch, gfortran 16.2.0, `-O3 -g -fopenmp`, no `-march`). Port: `kokkos/src/fortran_iface/srce_shim.cpp`
(the translation unit that includes `srce_fld2.hpp` through `srce_kernel.hpp`), g++ 16.2.0 in the pinned `ww3` shell with the
flags of `kokkos/build/openmp-release/build.ninja` for that file: `-O3 -march=x86-64-v3 -DNDEBUG -std=c++20 -ffp-contract=off
-fno-math-errno -fopenmp`, plus `-g1 -c` and `objdump -dl`, and `-fdump-tree-optimized-lineno -S` for the GIMPLE. To read
`W3FLD2` and `APPENDTAIL` as functions, the same flags compiled a scratch file that instantiates
`ww::fld2::w3fld2` and `ww::fld2::appendtail` out of line (not committed: it is a reading aid, not a test). Read 2026-10-19 (v).

| Question | Fortran | C++ | Same? |
|---|---|---|---|
| FMA in the objects | 0 `fmadd`, `fmsub`, `fnmadd`, `fnmsub` in `w3fld1md.o`, `w3fld2md.o`, `w3srcemd.o` (baseline x86-64 has none) | 0 in `srce_shim.o` (whole unit, `-march=x86-64-v3`) and in the scratch objects | yes |
| `MIN(PI/WN(K), 20.)` (`w3fld2md.F90:395`) | `divss` then `minss .LC0,%xmm0`: `x < 20 ? x : 20`; `.LC0` is `0x41a00000`, `.LC28` is `0x40490fdb` | `vdivss` then `vminss %xmm7,%xmm1,%xmm0`: `x < 20 ? x : 20`; `.LC30` is `0x41a00000`, `.LC3` is `0x40490fdb` | yes |
| `APPENDTAIL` `MAXANG` loop (`:131`) | `maxss %xmm0,%xmm1`: `a > maxang ? a : maxang` | `vmaxss %xmm0,%xmm1,%xmm0`: the same | yes |
| `APPENDTAIL` `MAXAN`, `MINAN` loop (`:170`, `:173`) | `minss %xmm4,%xmm2` and `maxss %xmm1,%xmm5`: `a < minan ? a : minan`, `a > maxan ? a : maxan` | `vminss %xmm3,%xmm2,%xmm0`, `vmaxss %xmm1,%xmm2,%xmm1`: the same | yes |
| `MAX(REAL(NTH),1.)` (`:235`, `:257`, `:274`) | `cvtsi2ss`, `maxss .LC52` | folded into integer code; `nth > 1 ? nth : 1`, the same float for every `NTH` | yes (value) |
| `NORMSPC` `c > 0 ? c*c : 0` | no `maxss` in the object for it | no `maxss` for it | yes |
| `SEED`, `w3srcemd.F90:1970-1982` | `divss` for `UC`, `subss`, `maxss 0,x`, `minss 1,m`, `mulss 6.25e-4`, `(w*w)*w`, `divss`, `divss SIG`, `(1-ICE)` product, `cosf`, `maxss 0,c`, `c*c`, `*SLEV`, `maxss old,new` | the same instructions in the same order (`vmaxss mem,x`, `vminss 1,m`, `vmaxss old,new` ...) | yes |
| `HSTOT` (`:1843`, `:1847`) | `mulss spec,factor` then `addss`; `sqrtss`, `* 4` | the same; the `WHITECAP(3)` sum beside it too | yes |
| `MIN(FPI/TPI, 2.)` (`:2295`) | `divss TPI`, `minss 2.` | `vdivss TPI`, `vminss 2.` with `FPI/TPI` first | yes |
| `COEF*U10ABS*COS`, `...*SIN` | one `mulss` for `COEF*U10ABS`, then a product with each of the `sincosf` results | the same | yes |
| `W3FLD2` stress loop (`:395-422`), read in the GIMPLE of the Fortran and in `srce_fld2.hpp` | `wnd_z_mag = (ustra/0.4) * logf(wnd_z/z0)`, `scin = (((((a1*we)*|we|)*dair)/1000)*wn)/cp`, the sum term `dth*(sig2*(cos*(spc2*scin)))`, one `sincosf(TH)`, a lone `cosf(UREFD-TH)`, `a1` kept when `wnd_z_proj` is NaN | the same statements in the same order (commutative swaps of one product only) | yes |
| `W3FLD2` smooth roughness and closure (`:438-484`) | the peeled first pass `ustsm = uref*kappa/9.2103404998779296875`, `z02 = 1.386e-6/ustsm`, `cds/3*(2cds/(cds+cdf)+1)`, `taux = taunux + tauxw`, `b2 = (ustrb+ustra)*0.5`, `ustra = (ustrb*0.5)*2` | the same | yes |
| libm calls in `APPENDTAIL` | `_ZGVbN4vv_powf` at 3 sites, scalar `powf`, `cosf` 3, `sincosf` 1, `asinf` 1, `acosf` 1 | `_ZGVbN4vv_powf` at 3 sites, scalar `powf`, `cosf` 3, `sincosf` 1, `asinf` 1, `acosf` 1 | yes |
| libm calls in `W3FLD2` with `SIG2WN` and `WND2Z0M` | `atan2f` 2, `cosf` 1, `powf` 2, `sincosf` 2, `SIG2WN` 5 calls (`tanhf` 2 and `coshf` 1 each) | the same `atan2f`, `cosf`, `powf`, `sincosf`; `tanhf` 10 and `coshf` 5 from the five inlined `SIG2WN` | yes |

The remaining static counts differ (the Fortran duplicates a loop where gfortran versions it, so its `expf` and `logf` appear
more often in the object than in the source), and no statement above rests on a count. The operand-order question of the
earlier entry is closed: all five sites take the semantics of the Fortran's `minss` and `maxss`, so a NaN or a signed-zero tie
gives the same result.

No mismatch, so no C++ change for risk 3. What this does not show (⚠): it is a site-by-site reading, not a full diff of two
GIMPLE dumps, and the unrolled and vectorised copies of the in-order sums were not each read; their order is the source's
because the build has no `-ffast-math`. The Fortran objects were read from the build of the earlier entry, not rebuilt here. `A1` starts at 0 in the C++ and undefined in the
Fortran; it matters only for a NaN wind, whose result is NaN either way.
The libm bits of the device are risk 7.

## Risk 2: paths no run reached

`scripts/ww3-gpu.sh` copies the smoke case's inputs into each run directory and gave no way to change them. It now has
`WW3GPU_CASE_HOOK=<file>`: `bash <file> <case dir>` runs after each copy, in the Fortran run and the Kokkos run alike
(`--self-test` checks that a missing file gives exit 64). The hooks are scratch files; each was one `sed`:

| Hook | Edit of the copy |
|---|---|
| `nth25`, `nth27`, `nth30` | `SPECTRUM%NTH = 25`, `27`, `30` in `ww3_grid.nml` (remainders 1, 3, 2 of `NTH/4`) |
| `tailid1` | `&FLD2 TAILTYPE=1 /` before `END OF NAMELISTS` in `namelists_ST4_T475.nml` |
| `taillev04` | `&FLD2 TAILLEV=0.04 /` there (0.04 is the upper clamp, `w3gridmd.F90:3234`) |
| `taillev0005` | `&FLD2 TAILLEV=0.0005 /` there (the lower clamp) |
| `wind40`, `wind60`, `wind105` | the five `WND` `VALUE1` of `ww3_shel.nml` from 12 to 40, 60 or 10.5 m/s |
| `wind40_taillev04`, `taillev04_nth25`, `taillev0005_wind105` | the two edits together |

Command, from the root of the checkout, per hook: `JOBS=4 OMP_NUM_THREADS=2 WW_KOKKOS_STRICT=1 WW3GPU_SWITCH=switch_lab_gfs
WW3GPU_CASE_HOOK=<hook> bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos SRCE` (the Fortran of the same executable
against SRCE on Kokkos, strict: a fallback would stop the run). Every row below: rc 0; 2 161 spectra through 2 161 batched
calls; no fallback; `nccmp-tol` PASS (hs); `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` and the NetCDF byte-identical to the
Fortran's; every model file but the NetCDF byte-identical to `--ww3`'s (the pure NOAA build).

| Variant | Path it takes | `Attn` lines, Fortran / Kokkos |
|---|---|---|
| baseline (`NTH` 24) | `CHA` 0.0155, so `W3FLD2` ran | 0 / 0 |
| `NTH` 25, 27, 30 | `APPENDTAIL` splits the directions at `4*(NTH/4)`: libmvec `powf` for 24 and the scalar `powf` for 1, 3 or 2 (`ww3_grid.out` says 25 directions); `CHA` moves | 0 / 0 |
| `TAILTYPE=1` | `TAIL_ID` 1: the saturation level from `WND2SAT` (`CHA` 0.0165 against 0.0155) | 0 / 0 |
| `TAILLEV=0.04` | the non-convergence path, and from it the bulk fail-safe: 522 `Attn: Stress not converged` and 522 `Attn: W3FLD2 failed, using bulk stress`, equal in both runs; `Calculated Wind/Cd` 12 m/s and 3.19e-3 | 1 044 / 1 044 |
| `TAILLEV=0.04`, `NTH` 25 | both paths with the scalar remainder: 514 and 514 | 1 028 / 1 028 |
| 40 and 60 m/s | stronger forcing; converges, `CHA` 0.0225 to 0.025 | 0 / 0 |
| 40 m/s, `TAILLEV=0.04` | converges, `CHA` about 0.04 | 0 / 0 |
| `TAILLEV=0.0005`, 12 m/s and 10.5 m/s | the weakest tail and the weakest wind `W3FLD2` takes; converges, `CHA` about 0.011 | 0 / 0 |

What triggers the paths (`w3fld2md.F90` at `03adda91`): the iteration stops after 20 passes, and if the stress still differs
by 5 % the point is marked not converged (`:484-490`); the bulk fail-safe runs when the drag coefficient leaves
`0.0005 < Cd < 0.01` or when the point did not converge (`:573-581`). At 12 m/s the tail level 0.04 makes the stress
iteration oscillate, which is the only input found that reaches either path. Every bulk event of these runs follows a
non-convergence; a converged point with `Cd` outside the window was not found (⚠, `Cd` at 40 m/s with `TAILLEV` 0.04 stays inside).
The weak side was tried with the lowest tail level (0.0005) at 12 and at 10.5 m/s, just above the 10 m/s that `W3FLD2` needs
(`w3srcemd.F90:2292`): `Cd` stays inside the window, so a drag coefficient below 0.0005 was not reached either (⚠: two
forcings, no search). The capacity status (`nkt_max`) was not reached.

These runs are diagnostics, not the regtest verdict; the grids of the WW3 regtests keep NTH 24 or 36 and the default tail.

## Risk 4: tail parameters per grid

`TAILTYPE`, `TAILLEV`, `TAILT1`, `TAILT2` and `FALPHA` are the names the grid input uses (`NAMELIST /FLD2/` at
`WW3/model/src/w3gridmd.F90:968`); they become `TAIL_ID`, `TAIL_LEV`, `TAIL_TRAN1`, `TAIL_TRAN2` and `FLDALPHA` at `:3234-3239`.
The defaults under the `FLD2` switch are `TAILTYPE = 0`, `TAILLEV = 0.006`, `TAILT1 = 1.25`, `TAILT2 = 3.00`,
`FALPHA = 0.0095` (`:3210-3214`), and `FALPHA` is then set to `AALPHA` under `ST4` (`:3220`). `READNL` reads a namelist only when
the file has a line `&FLD2` (`:7345-7355`).

`grep -rniE "FLD|TAIL"` over `regtests/ww3_ufs1.1`, `ww3_ufs1.2` and `ww3_ufs1.3` at the pin `5dbf6eba` (every `.inp`, `.nml`,
`info`, `switch*` file) finds no `&FLD2` line and no tail parameter. The one hit is `regtests/ww3_ufs1.2/input/ww3_grid_hafsl.inp:17`,
`&FLD1 TAILTYPE=1 /`, the grid of matrix entry `grdset_l`: under the `FLD2` switch the `FLD1` namelist is not compiled and not
read, so that grid also takes the defaults. No `AALPHA` is set either. So every grid of the three cases has `TAIL_ID` 0,
`TAIL_LEV` 0.006, `TAIL_TRAN1` 1.25, `TAIL_TRAN2` 3.00 (v), and the check at `w3kokkosmd.F90:1869-1870` never stops the Kokkos
`W3SRCE` for these cases. It would stop a run whose grids differ in these four values (not tried; the variants above change
all grids alike).

## Risk 7: `asinf` and `acosf` on a device

glibc 2.44 (tarball sha256 `37f600f2bef3c5e8300147059568b2a2e40a7ad6ccc65ce942556d49429cc667`, read from the Nix store) carries
`asinf` and `acosf` from CORE-MATH in `sysdeps/ieee754/flt-32/e_asinf.c`, `e_acosf.c` and `s_asincosf_data.c` (revisions `bc385c2` and
`56dd347`, MIT, correctly rounded; `NEWS` of 2.41). No multiarch variant exists for them (the `fma` files of
`sysdeps/x86_64/fpu/multiarch` are for the double functions), so there is one build, as for `tanhf` and `atan2f`. They need no
table beyond two 12-entry coefficient arrays, so they port in the pattern of `coshf` and `sinhf`:
`kokkos/tools/libm_bits/coremath_f.hpp` has `ww::libm::asinf` and `acosf` with the notice kept, `ww::dm::asinf` and `acosf` in
`libm_device.hpp` call glibc on the host and the copy on a device (with the deleted overloads for other types), and
`srce_fld2.hpp` calls them. `libm_bits.cpp` and `device_check.cu` list the two functions, and `scripts/libm_bits.sh all` runs them.

| Check | Command | Result |
|---|---|---|
| host, all floats | `scripts/libm_bits.sh ww asinf fma 3`; `... ww acosf fma 3` | 4 294 967 296 inputs each, 0 differing, 0 NaN payload differences |
| correct rounding | `scripts/libm_bits.sh cr asinf 3`; `... cr acosf 3` | 4 294 967 296 each, 0 differing, 0 unresolved (2 147 483 648 NaN outputs not judged) |
| device code compiles | `scripts/libm_bits.sh --nvcc` (compile only) | `device_check` built for sm_89 and sm_90; not run |
| g++ output | the scratch `APPENDTAIL` object | still one `call asinf` and one `call acosf` on the host |

So the copies equal glibc's on the host on every input (v). That a device returns the same bits is ⚠ until
`scripts/libm_bits.sh --device` runs under the owner. The comment in `srce_fld2.hpp` (line 35), `docs/GPU_LIBM_BITS_202610.md`
and the two port reports say so.

## Gates

`just kokkos-test serial-debug` and `just kokkos-test openmp-release`: 11 of 11 each. `bash scripts/ww3-gpu.sh --self-test` ok;
shellcheck clean on `scripts/ww3-gpu.sh` and `scripts/libm_bits.sh`; `port_score.py --write` then `--check` current;
vale 0 errors on the changed Markdown. No `regtested` column moved.
