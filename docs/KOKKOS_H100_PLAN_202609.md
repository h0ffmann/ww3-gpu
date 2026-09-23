# Porting WAVEWATCH III® to C++/Kokkos on a Single H100
## Revised plan: full repository map, translation coverage, and lessons from comparable Fortran ports

Prepared 9 September 2026. Supersedes the Rust plan of the same date. Figures marked "est." are estimates. Module names are from the WW3 v7.x source tree; verify line counts against the pinned `develop` commit in Task 3.

> **Review notes (2026-09-13, checked against the `WW3/` submodule, 7.14 `develop` @ 761cf79d).** Verified `(v)`: the CPP transition (`#ifdef W3_xxx`, 239 guards in `w3srcemd.F90` alone, no `.ftn` files), `model/src` size (145 files, ~258k lines of `.F90`, ~267k with PDLIB/SCRIP), 64 regtests, `VA(NSPEC, 0:NSEALM)` with spectrum contiguous per point, `SIG(0:NK+1)`, `MAPSF(NSEA,3)`, `ISP = ITH + (IK-1)*NTH`, the `TAUHF`/`TAUHFT` tables in `w3src4md`, and every file and routine name in §3 except those corrected inline below. Corrected: `constants.F90` has no `RTYPE` (plain default `REAL`); `W3GRML`, `SETUGIOBP`, `DIAGNL` do not exist; `w3iogoncmd`, `w3sxxxmd`, `wmiobcmd`, `ww3_multi_esmf` and the `wav_*` NUOPC cap files are not in the pinned tree (only `wmesmfmd`); ST4 has no `SWELLFT` table (swell dissipation is parametric via `SSWELLF`). Not verified `⚠`: line-count estimates for the exercised subset, the ORNL 78 % figure, and the DOE/Omega statements. Kokkos: 5.1.1 is April 2026 as stated, but 5.2.2 (10 Sep 2026) is current; pin deliberately.

---

## 1. Executive Summary

**Decision.** Port the compute core of WW3 to **C++ with Kokkos** (Kokkos 5.1.1, April 2026, CUDA backend on the H100, Serial/OpenMP backends on the host CPUs). SYCL is a credible alternative but loses on three counts for this lab (§2). The port follows the two-stage recipe validated on FESOM2 in June 2026: Fortran → clean single-threaded C reference → C++/Kokkos, with a literal-translation rule and a tiered validation ladder.

**Two findings that change the plan.**

1. **DOE is already doing this.** The E3SM/Omega team (LANL, whose Steven Brus co-authored the 2023 WW3-GPU paper) states it is "working to completely rebuild the wave component of E3SM (WAVEWATCH III) as a part of OMEGA," porting the costly source terms to C++/Kokkos or replacing them with AI surrogates. No public repository was found as of today. **The first action of this project is to contact that team.** If their WW3 source-term kernels in Kokkos become available, months of work disappear and the lab becomes an early adopter rather than a lone fork.

2. **WW3 `develop` is now CPP-preprocessed.** Since v7.14 all code is `*.F90` with switches mapped to `#ifdef W3_xxx`. This means the frozen operational configuration can be *mechanically* collapsed with the C preprocessor before translation. The "pin the configuration" benefit FESOM2 got from its C stage is available on day one, and it removes the agent's single most common failure (following an inactive branch).

**What the port covers** (§4): the single-grid `ww3_shel` time loop (propagation, source-term integration, the enabled physics packages, output-field integration) for one frozen configuration, with the Fortran pre/post-processors kept as-is. What it does not cover: `ww3_multi`, coupling caps, data assimilation, unstructured/PDLIB (deferred unless the operational grid requires it).

**Effort (solo Staff engineer, part-time, Claude Code, est.).** Stage 1 (C reference, validated) 3–5 months; Stage 2 (Kokkos, device-resident, faster than current CPU) a further 4–7 months. 3-month gated pilot as before. Stall risk ~40%, dominated by silent physics divergence and scope creep. If the DOE kernels arrive, subtract 2–4 months from Stage 2.

**Recommendation.** (1) Weeks 1–3: profile and tune the current CPU run on the H100 host; this may already meet the "several runs per day" target. (2) Email the Omega/WW3 team. (3) Run the 3-month pilot to a validated C reference of the frozen configuration. (4) Gate to Stage 2 on the criteria in §8.

---

## 2. Kokkos vs SYCL for this project

| Criterion | Kokkos 5.x | SYCL 2020 (oneAPI DPC++ / AdaptiveCpp) |
|---|---|---|
| Precedent in Earth-system models | E3SM EAMxx/SCREAM (Gordon Bell 2023), HOMMEXX, Omega (2026), LICOM3-Kokkos (2024), FESOM2-Kokkos (2026) — all Fortran-lineage ocean/atmosphere codes | Few production geophysical models; used in some CFD and lattice-Boltzmann codes; performance comparable to Kokkos when tuned per platform (IEEE 2024 LBM study) |
| NVIDIA H100 support | Native CUDA backend via nvcc or clang; mature | Via DPC++ CUDA plugin or AdaptiveCpp; works, second-tier |
| Deterministic CPU reference in the same source | Serial backend gives bit-identical CPU execution of the same kernels — the basis of the FESOM2 validation ladder | Host device / CPU backend exists but bit-identity with a C reference is less commonly exercised |
| Multidimensional arrays with switchable layout | `Kokkos::View` with LayoutLeft/LayoutRight — directly expresses the Fortran column-major → GPU layout question | `sycl::buffer`/USM are flat; layout is by hand |
| Agent (LLM) familiarity | High: large public corpus (Trilinos, LAMMPS, E3SM, ArborX) | Medium |
| Build complexity | CMake, one device backend at a time; C++20 required in 5.x | Compiler-driven (icpx/acpp); simpler in one sense, but toolchain is the vendor's |
| Lock-in | Linux Foundation project, vendor-neutral, HIP/SYCL/OpenMP-target backends | Khronos standard; implementations vary |
| Verdict | **Choose** | Keep as a documented escape hatch; a Kokkos build can target SYCL later if the lab ever buys Intel GPUs |

Kokkos notes that matter for the plan: 5.x requires C++20; only one device backend can be enabled per build; CUDA-aware MPI is assumed but irrelevant for a single GPU; `MDRangePolicy` is convenient, but Omega reported 10–20% gains from hand-mapped `TeamPolicy` kernels, so expect the same for WW3's per-point source terms.

---

## 3. Full Repository Map — `NOAA-EMC/WW3` (`develop`, v7.x)

Legend for the **Port** column: **CORE** = translate to C then Kokkos; **CPU-C** = translate to C, stays on host; **KEEP** = keep the Fortran executable unchanged, interface by file; **DROP** = out of scope for the frozen single-grid, single-process configuration; **COND** = only if the frozen switch file enables it.

### 3.1 Top level

| Path | Contents | Relevance |
|---|---|---|
| `model/src/` | ~150 `.F90` files: modules (`w3*md.F90`, `wm*md.F90`, `wav_*`), programs (`ww3_*.F90`), plus `PDLIB/` (yow* modules from WWM), `SCRIP/` (remapping), `w3macros.h` | Everything below |
| `model/bin/` | `switch_*` files, `w3_setup`, `w3_automake`, legacy `comp`/`link` scripts, `ww3_from_ftp.sh` | The switch file *is* the frozen configuration |
| `model/cmake/`, `CMakeLists.txt` | CMake build; `-DSWITCH=` selects CPP defines; NETCDF/MULTI_ESMF options | Reuse to build `libww3.so` for the reference |
| `model/nml/` | Namelist templates for every program | Source of runtime defaults the agent must not guess |
| `model/inp/` | Legacy `.inp` templates | Ignore; use `.nml` |
| `regtests/` | ~60 test cases (`ww3_tp1.x`, `ww3_tp2.x`, `ww3_ts*`, `ww3_tic*`, `mww3_test_*`, `ww3_ufs*`) with `input/` and reference `matrix` scripts; binary data from FTP | Validation source; pick 2–3 cases |
| `manual/` | LaTeX manual | The numerics chapter is required reading |
| `docs/` | Doxygen config | — |
| `.github/workflows/` | Spack-based CI for Intel/GNU | Template for the port's CI |

### 3.2 `model/src` — data modules (state that becomes C structs)

| File | Role | Key contents | Port | Translation notes |
|---|---|---|---|---|
| `constants.F90` | Physical/numerical constants (default `REAL`, no kind parameter `(v)`) | `GRAV`, `PI`, `TPI`, `DWAT`, `DAIR`, `RADIUS` | CORE | Copy with `file:line` citation for each literal |
| `w3gdatmd.F90` | Grid data (`GRID` type + module pointers) | `NX,NY,NSEA,NSEAL,NK,NTH,NSPEC`, `XGRD,YGRD`, `ZB`, `MAPSTA,MAPST2,MAPFS,MAPSF`, `SIG,DSIP,TH,ESIN,ECOS,ES2,EC2,ESC`, `DTCFL,DTMAX,DTMIN,DTCFLI`, `FLAGLL,GTYPE,ICLOSE`, physics parameters (`SSWELLF`, ST4 `ZWND` ... ) | CORE | The `W3SETG(IMOD)` pointer-swap idiom: module-level pointers re-targeted per grid. In C: one `Grid` struct passed explicitly. Largest single source of hidden global state. |
| `w3wdatmd.F90` | Wave data | `VA(NSPEC,NSEAL)` spectrum, `UST,USTDIR,ASF,FPIS`, `TIME`, ice/level fields | CORE | `VA` layout is `[spec][sea]`, Fortran column-major → spectrum contiguous per point. Keep. |
| `w3adatmd.F90` | Auxiliary data | `CG,WN,DW,UA,UD,U10,U10D,CX,CY,AS,ITIME,IAPPRO,DTDYN,FCUT`, `SPPNT`, MPI bookkeeping (`IAPPRO` rank map, `MPI_COMM_WAVE`) | CORE (non-MPI parts) | Split: numerics fields → struct; MPI fields → DROP |
| `w3odatmd.F90` | Output data | Output flags, field arrays (`HS,WLM,T02,DIR,SPR,...`), point-output locations, unit numbers | CORE (field arrays) / CPU-C (file bookkeeping) | |
| `w3idatmd.F90` | Input fields | Wind/current/level/ice time slices for interpolation | CPU-C | |
| `w3timemd.F90` | Date/time arithmetic | `DSEC21, TICK21, STME21` | CPU-C | Pure; easy |
| `w3servmd.F90` | Service routines | `STRACE`, `EXTCDE`, `NEXTLN`, `WWDATE` (`DIAGNL` does not exist `(v)`) | CPU-C | `STRACE` behind `W3_S` — compile out |
| `w3arrymd.F90` | Array printing helpers | `PRTBLK`, `OUTA2I` | DROP | Debug only |
| `w3dispmd.F90` | Dispersion relation | `WAVNU1`, `WAVNU2`, `WAVNU3`, `DISTAB` lookup tables | CORE | First leaf routines to translate |
| `w3cspcmd.F90` | Spectral conversion | `W3CSPC` (interpolate between spectral grids) | CPU-C | Used for boundary/initial data |
| `w3gsrumd.F90` | Grid search / interpolation | `W3GRMP` (generic over `_R4`/`_R8`) `(v)` | CPU-C | Forcing interpolation setup |
| `w3parall.F90` | Parallel infrastructure | `INIT_GET_ISEA`, `INIT_GET_JSEA_ISPROC`, `SYNCHRONIZE_*`, PDLIB helpers | DROP (single process) | Provide trivial identity mappings `ISEA=JSEA` |
| `w3triamd.F90` | Unstructured grid setup | Triangle connectivity (no `SETUGIOBP` in the tree `(v)`) | COND | Only for unstructured |
| `w3metamd.F90`, `w3ounfmetamd.F90` | Output metadata | NetCDF attribute tables | DROP | Post-processors keep it |
| `w3nml*md.F90` (shel, grid, ounf, ounp, bounc, trnc, prnc, multi) | Namelist readers | | DROP | Replace with TOML/JSON config for the port |
| `w3macros.h` | CPP macros | `CHECK_ALLOC_STATUS`, etc. | — | |

### 3.3 `model/src` — the time loop and drivers

| File | Role | Port | Notes |
|---|---|---|---|
| `w3initmd.F90` | `W3INIT` (model initialisation), `W3MPII/O/P` (MPI setup), `WWVER` | CORE (non-MPI) | Allocation order and derived quantities (`CG`, `WN` tables) live here |
| `w3wavemd.F90` | `W3WAVE` — the main loop: forcing update → propagation (spatial, then intra-spectral) → source terms → output triggers; `W3GATH/W3SCAT` (MPI gathers of spectra for propagation) | CORE | The sequencing is the spec. `W3GATH/W3SCAT` become no-ops in one process |
| `w3updtmd.F90` | `W3UCUR, W3UWND, W3ULEV, W3UICE, W3UINI, W3UTRN` — forcing interpolation, initial spectra, transparency | CPU-C then Kokkos for interpolation onto device | |
| `w3srcemd.F90` | `W3SRCE` — per-point source-term integration with dynamic sub-stepping and limiter; calls every enabled `w3s*md` | **CORE, the hotspot** | 78% of runtime in the ORNL profile (unstructured, ST4). Loop is sequential *within* a point, independent *across* points |
| `w3fldsmd.F90` | Field I/O for forcing (`W3FLDO/G/P`) | CPU-C | Reads `wind.ww3` etc. |
| `w3iogrmd.F90` | `W3IOGR` — read/write `mod_def.ww3` | CPU-C | Versioned record layout; port as a reader only |
| `w3iorsmd.F90` | `W3IORS` — restart read/write | CPU-C | Same layout as `VA` |
| `w3iogomd.F90` | `W3OUTG` (integral parameters from spectra) + `W3IOGO` (write `out_grd.ww3`) | **CORE for `W3OUTG`**, CPU-C for writer | `W3OUTG` must go to the device (WAM6-GPU lesson) |
| `w3iopomd.F90` | Point output (`W3IOPO`, `W3IOPE`) | CPU-C | Small |
| `w3iobcmd.F90` | Boundary condition I/O (`W3IOBC`) | COND | Needed for nested regional domains |
| `w3iotrmd.F90`, `w3iosfmd.F90` | Track output, spectral partition output | DROP | |
| `w3iogoncmd.F90` | Direct NetCDF gridded output (not in the pinned tree `(v)`) | DROP | Write native `out_grd.ww3`; run `ww3_ounf` |
| `w3partmd.F90` | Spectral partitioning (watershed) | COND | Only if partition output is used operationally |
| `w3profsmd.F90`, `w3profsmd_pdlib.F90`, `pdlib_field_vec.F90`, `PDLIB/yow*.F90` | Unstructured propagation (explicit N-scheme and implicit), domain decomposition | COND / DROP | Substantial; defer |
| `w3pro1md.F90` | PR1: first-order propagation | COND | |
| `w3pro2md.F90` | PR2: ULTIMATE QUICKEST with averaging GSE | COND | |
| `w3pro3md.F90` | PR3: ULTIMATE QUICKEST with Booij-Holthuijsen GSE (`W3XYP3`, `W3KTP3`) | **CORE (operational default)** | Calls `W3QCK3` |
| `w3uqckmd.F90` | `W3QCK1/2/3` 1-D ULTIMATE QUICKEST | CORE | Pure per-bin sweeps; ideal first Kokkos kernel |
| `w3wdasmd.F90` | Data assimilation hook | DROP | |
| `w3strkmd.F90` | Storm tracking | DROP | |
| `w3bullmd.F90` | Bulletin output | DROP | |

### 3.4 `model/src` — physics packages (`COND` unless listed in the frozen switch file)

| Switch | File(s) | What it is | Typical operational choice |
|---|---|---|---|
| `LN0/LN1` | `w3sln1md.F90` | Linear wind input (Cavaleri-Malanotte-Rizzoli) | LN1 |
| `ST0/1/2/3/4/6` | `w3src0md`, `w3src1md`, `w3src2md`, `w3src3md`, **`w3src4md`** (Ardhuin et al. 2010: `W3SPR4`, `W3SIN4`, `W3SDS4`, TABU tables), `w3src6md` (BYDRZ: `W3SPR6`, `W3SIN6`, `W3SDS6`) | Wind input + dissipation | ST4 (NOAA, Ifremer, most Brazilian setups) or ST6 |
| `STAB0/2/3` | in `w3src*md` | Stability correction | STAB0 |
| `NL0/1/2/3/4/5` | **`w3snl1md`** (DIA: `W3SNL1`, `INSNL1`), `w3snl2md` (WRT exact — very slow), `w3snl3md` (GMD), `w3snl4md` (TSA), `w3snl5md` (GKE) | Nonlinear 4-wave interactions | NL1 |
| `NLS0/1` | `w3snlsmd.F90` | Nonlinear smoothing | NLS0 |
| `BT0/1/4/8/9` | `w3sbt1md` (JONSWAP), `w3sbt4md` (SHOWEX), `w3sbt8md`, `w3sbt9md` | Bottom friction | BT1 or BT4 |
| `DB0/1` | `w3sdb1md.F90` | Depth-induced breaking (Battjes-Janssen) | DB1 |
| `TR0/1` | `w3str1md.F90` | Triad interactions (LTA) | TR0 offshore, TR1 nearshore |
| `BS0/1` | `w3sbs1md.F90` | Bottom scattering | BS0 |
| `IC0/1/2/3/4/5` | `w3sic1md` … `w3sic5md` | Wave–ice dissipation | IC0 for Brazil |
| `IS0/1/2` | `w3sis1md`, `w3sis2md` | Ice scattering | IS0 |
| `REF0/1` | `w3ref1md.F90` | Shoreline reflection | REF0 |
| `UOST` | `w3uostmd.F90` | Unresolved obstacles (sub-grid islands) | often on for regional grids |
| `IG0/1` | `w3gig1md.F90` | Infragravity waves | IG0 |
| `XX0` … | (no `w3sxxxmd.F90` in the pinned tree `(v)`) | User slot | XX0 |
| `FLX0/1/2/3/4/5` | `w3flx1md` … `w3flx5md` | Air–sea flux / drag | FLX0 (ST4 computes its own) |
| `SEED`, `MLIM`, `WNT*`, `WNX*`, `CRT*`, `CRX*`, `RWND`, `WCOR`, `TIDE` | flags inside `w3srcemd`/`w3updtmd` | Seeding, limiter, interpolation options, wind corrections, tides | Read from the switch file |

### 3.5 `model/src` — multi-grid, coupling, programs

| File(s) | Role | Port |
|---|---|---|
| `wmmdatmd, wminitmd, wmwavemd, wmgridmd, wmfinlmd, wmiopomd, wmunitmd, wmupdtmd, wmscrpmd, wmesmfmd` + `SCRIP/` (`wmiobcmd` is not in the pinned tree `(v)`) | `ww3_multi` mosaic driver, grid-to-grid remapping | DROP |
| `w3oacpmd, w3agcmmd, w3ogcmmd, w3igcmmd` | OASIS coupling | DROP |
| `wav_comp_nuopc, wav_import_export, wav_kind_mod, wav_pio_mod, wav_restart_mod, wav_history_mod, wav_shel_inp, wav_wrapper_mod, wav_shr_mod` (none present in the pinned tree `(v)`; the cap lives outside `model/src` or in a separate repo) | NUOPC cap for UFS/CESM | DROP |
| `ww3_grid, ww3_strt, ww3_prep, ww3_prnc, ww3_bound, ww3_bounc` | Pre-processors | KEEP (Fortran executables) |
| `ww3_shel` | Single-grid driver, namelist parsing | Replace with a C++ `main` + TOML |
| `ww3_multi` (`ww3_multi_esmf` not in the pinned tree `(v)`) | Multi-grid driver | DROP |
| `ww3_outf, ww3_outp, ww3_ounf, ww3_ounp, ww3_trck, ww3_trnc, ww3_grib, ww3_gspl, ww3_gint, ww3_systrk, ww3_uprstr, ww3_prtide` | Post-processors and tools | KEEP |

**Size estimate.** Full `model/src` is roughly 250–300k lines including PDLIB, SCRIP, multi-grid, coupling and all physics variants. The code *exercised* by a typical regular-grid ST4/NL1/PR3/BT1/DB1 forecast is on the order of 25–35k lines (est.), the same order as FESOM2's 74k-line core after configuration collapse, which yielded a 20k-line C port.

---

## 4. What the Translation Must Cover

### 4.1 Fortran language features present in WW3 and their C/Kokkos mapping

| Feature | Where in WW3 | Mapping | Risk |
|---|---|---|---|
| CPP `#ifdef W3_xxx` switches (v7.14+) | Everywhere | Run `cpp -DW3_ST4 -DW3_NL1 …` from the switch file *before* translation; translate the resolved source | Low — this is the big win |
| Module-level `POINTER` state with `W3SETG/W3SETW/W3SETA/W3SETO(IMOD)` re-targeting | All `w3*datmd` | One `Model` struct with `Grid`, `WaveData`, `AuxData`, `OutData` members passed explicitly; `IMOD` is always 1 | Medium — every routine's implicit inputs must be enumerated |
| `ALLOCATABLE`/`POINTER` arrays allocated in `W3DIMx` routines | data modules | `Kokkos::View` allocated once in init; C reference uses malloc'd flat arrays with index macros | Low |
| Column-major, 1-based, multi-dimensional arrays | everywhere | Index macros `A(I,J)` → `a[(J-1)*NX + (I-1)]` in C; `View<double**, LayoutLeft>` in Kokkos preserves Fortran order where wanted | Medium — the #1 source of silent bugs; per-routine tests catch it |
| Assumed-shape / assumed-size dummy arguments | physics routines | Explicit sizes | Low |
| `OPTIONAL` arguments and `PRESENT()` | `W3SRCE`, output routines | Explicit flags | Low |
| `SAVE` locals and first-call initialisation (`FIRST = .TRUE.`) | ST4 tables, DIA setup, dispersion tables | Move to init; make explicit state | Medium — hidden state |
| `GOTO`, computed branches, `CYCLE`/`EXIT` | `W3SRCE` dynamic loop, propagation boundary handling | Structured equivalents, verified by sub-step counts | Medium |
| `REAL` default kind (`RTYPE`) — WW3 is single precision by default | everywhere | Decide FP32 vs FP64 to match the lab's build; C reference uses `float` if the Fortran does | High if mismatched — the reference comparison becomes meaningless |
| Fortran intrinsics with edge semantics: `MOD` vs `MODULO`, `NINT`, `SIGN`, `MAX/MIN` NaN behaviour, `**` with real exponents, `EXP/LOG` accuracy | physics | `fmod`/floor-mod, `lrint`, `copysign`; document each | Medium |
| `WRITE/READ` unformatted sequential and direct-access files | all `w3io*md` | Byte-exact readers/writers with record markers | Medium — versioned layouts |
| Namelists | `w3nml*md`, `ww3_shel` | TOML; defaults copied with `file:line` citation | Medium — wrong default = wrong physics |
| MPI (`W3GATH/W3SCAT`, `IAPPRO`, `MPI_BARRIER`) | `w3wavemd`, `w3parall`, `w3adatmd` | Delete; identity mappings | Low |
| OpenMP `!$OMP` directives | `w3wavemd`, `w3srcemd`, `w3pro3md` | Ignore in C; they mark the loops that become Kokkos `parallel_for` | Informative |
| `STRACE`, `W3_T` test output, `W3_S` | everywhere | Compile out | Low |
| `CHARACTER` handling, `WRITE(*,fmt)` formatted logs | logs | `fprintf`; not compared | Low |

### 4.2 WW3-specific semantics the agent must not get wrong

1. **Index maps.** `MAPSTA(NY,NX)` (status: 0 land, 1 sea, 2 active boundary, negative = excluded), `MAPFS(NY,NX)` → sea-point index, `MAPSF(NSEA,3)` → (IX, IY, IXY). Every loop over `ISEA` uses these. Translate the maps once and test them exhaustively (integer, exact).
2. **Spectral discretisation.** `SIG(0:NK+1)` radian frequencies with two ghost bins; `TH(NTH)`; `NSPEC = NTH*NK` with direction fastest (`ISP = ITH + (IK-1)*NTH`). Off-by-one in the ghost bins corrupts the high-frequency tail.
3. **Action vs. energy.** `VA` stores action density in wavenumber space (`A = E/σ` with Jacobian); source terms are computed on the energy spectrum and converted back. Conversion factors are in `W3SRCE`; do not "simplify".
4. **Dynamic source-term integration.** Per point: compute all source terms, apply the limiter, choose `DTDYN` between `DTMIN` and `DTMAX`, sub-step until the global step is covered. The number of sub-steps per point is part of the reference (assert on it).
5. **Propagation splitting.** Spatial propagation (`W3XYP3`) over `(IK, ITH)` bins with per-bin CFL sub-steps; then intra-spectral (`W3KTP3`) refraction/shifting per point. Order and sub-step counts matter.
6. **GSE alleviation** in PR3 adds a diffusion operator with its own stability limit.
7. **Boundary points** (`MAPSTA = 2`) are set from `nest.ww3` each step; they must not be propagated into.
8. **Output timing.** Output-field integration (`W3OUTG`) happens at output times only, from the current spectrum — on the GPU, this is the only per-output device→host transfer.
9. **The ST4 lookup tables** (`TAUHF`/`TAUHFT` in `w3src4md` `(v)`; there is no `SWELLFT` table, swell dissipation is parametric via the `SSWELLF` coefficients) are built at init from constants — build them in C identically and compare table-to-table.
10. **`FCUT`/tail parametric extension**: the spectral tail beyond `FCUT` is prescribed (`f^-5`); translate the tail-handling exactly.

### 4.3 Verification artefacts required

- `FROZEN_CONFIG.md`: switch file, all namelist values, `file:line` for each effect.
- Fixture set: dumps of inputs/outputs of every CORE routine at ≥ 5 probe points (deep, shallow, land-adjacent, boundary, high-wind) × 3 time steps, from the Fortran build with `-DWW3_DUMP`.
- Integer tables compared exactly: `MAPSTA/MAPFS/MAPSF`, DIA interaction indices, ST4 tables (float, 1e-12).
- Per-routine C-vs-Fortran gates; whole-run C-vs-Fortran field comparison; Kokkos-Serial-vs-C bit-identity; Kokkos-CUDA-vs-Serial statistical gate with per-field ceilings.
- Sub-step count parity for `W3SRCE` and propagation CFL sub-steps.

---

## 5. Comparable Ports: What Others Learned

| Project | From → To | Who / when | Outcome | Lesson for WW3 |
|---|---|---|---|---|
| **FESOM2** | Fortran → C → C++/Kokkos, LLM-driven | AWI, June 2026 | 74k → 20k C → 31k Kokkos lines; C port matches 5-year statistics; Kokkos Serial bit-identical; A100 node 1.6–3.7× CPU node; first GPU run 3.8× *slower* until halo data went device-resident | The recipe: literal translation, config collapse, validation ladder, twin kernels, memory files across sessions. Directly reusable. |
| **WW3 source terms** | Fortran + OpenACC | ORNL/LANL, 2023 | 2–4× per rank, ~1.3× per node; limited by host–device transfer; `!$acc routine` needed because of module-global structure; few collapsible loops | Partial offload fails. `W3SRCE` is the hotspot (78%). Its per-point structure fits one team per point. |
| **WAM6-GPU** | Fortran + OpenACC, deep refactor | NMEFC China, 2024 | 37× on 8 A100 vs 32-core node; *all* physics and post-processing on device; zero runtime host–device copies | Output-parameter integration must be on device; layout refactoring is where the speedup comes from |
| **Omega** | New C++/Kokkos ocean model (MPAS-Ocean successor) | E3SM/DOE, 2023–2026 | Thin abstraction layer over Kokkos; hand-mapped kernels 10–20% faster than `MDRangePolicy`; strong V&V culture; **WW3 wave component being rebuilt in C++/Kokkos** | Contact them. Copy their thin Kokkos wrapper style. |
| **EAMxx / SCREAM / HOMMEXX** | Fortran → C++/Kokkos rewrite | E3SM, 2019–2023 | Same-or-better CPU performance, GPU portable; Gordon Bell 2023 | Full rewrite in Kokkos is production-proven at scale |
| **LICOM3-Kokkos** | Fortran ocean model → Kokkos | IAP China, 2024 | Single source replaced Fortran/OpenMP/OpenACC/HIP/CUDA variants; matched CPU, surpassed on GPU | Portability layer ends the "N variants" maintenance problem |
| **ICON on GPUs** | Fortran + OpenACC, operational | MeteoSwiss/DWD, 2022–2026 | Operational NWP on GPUs; much of the speedup from mixed precision | Directive ports can work operationally, but with a large team and years |
| **natESM FESOM2 OpenACC sprint** | Fortran + OpenACC | DKRZ, 2025 | Large kernel speedups but "difficult to maintain and not straightforward to bring up on other machines" | Why AWI abandoned directives for Kokkos |
| **ecWAM / IFS via Loki** | Fortran source-to-source | ECMWF | In-house transformation toolchain for GPU | Viable only with a compiler team |
| **Fortran2CPP, Ranasinghe et al., LinguistLLM** | LLM fine-tunes for Fortran→C++/Rust | 2024–2026 | Function-level accuracy improvements; not model-scale | Use frontier agents + harness, not fine-tuned small models |
| **TransJAX, land-surface model → JAX** | Claude multi-agent pipeline | 2026 | Static analysis + generated tests + repair loop | Reference design for the harness |
| **f2rust / rsspice** | Fortran → Rust via custom compiler | 2026, one developer | 500k-line port passing full test suite | Test suite is the guarantee; mechanical translation scales |

**Cross-cutting conclusions.** (1) Every successful port collapsed the configuration first. (2) Every GPU success made the state device-resident for the whole step. (3) Every LLM-driven success forbade "improvements" and validated per routine, then per model. (4) Directive ports are fast to start and slow to finish; Kokkos rewrites are slow to start and then portable. (5) Nobody has done WW3 end-to-end on GPU in the open yet; DOE is the closest.

---

## 6. Translation Pipeline

```
NOAA-EMC/WW3 develop (pinned commit)
        │  cpp -D<switches from switch file>   (mechanical, Task 3)
        ▼
resolved Fortran for the frozen configuration      ← the "specification"
        │  literal translation, one routine per session (Tasks 5–11)
        ▼
ww3c/  clean single-threaded C reference           ← accepted statistically vs Fortran
        │  transliterate to C++, wrap loops in Kokkos (Stage 2)
        ▼
ww3k/  C++/Kokkos; Serial backend bit-identical to ww3c; CUDA backend statistically close
```

Both C and Kokkos trees live in one repository; the C code survives inside the Kokkos tree as the Serial reference and per-kernel twin, exactly as in FESOM2.

**Stage 2 kernel decomposition (Kokkos).**

| Kernel | Policy | Data | Notes |
|---|---|---|---|
| `W3SRCE` (all source terms + dynamic integration) | `TeamPolicy(NSEA, AUTO)`: one team per sea point, `TeamThreadRange` over `NSPEC` for the spectral loops, `parallel_reduce` for integrals | `VA` view `[NSEA][NSPEC]` LayoutRight (spectrum contiguous) | The dynamic sub-step loop runs at team level; divergence across points is acceptable (ORNL measured occupancy/register trade-offs — start with `launch_bounds` 128–256) |
| `W3SPR4/W3SIN4/W3SDS4`, `W3SNL1`, `W3SBT1`, `W3SDB1` | device functions called inside the `W3SRCE` team kernel | scratch in team shared memory | Do not launch separately — that recreates the ORNL transfer problem in miniature |
| `W3XYP3` spatial propagation | `RangePolicy(NSPEC)` per bin, or `MDRangePolicy({bin, row})`; per-bin gather into `[NY][NX]` scratch, `W3QCK3` sweeps in x then y, scatter back | second view `[NSPEC][NSEA]` or transposed scratch | Memory-bound; FP32 candidate later |
| `W3KTP3` intra-spectral | `TeamPolicy(NSEA)` | `VA` | Same shape as source terms |
| `W3OUTG` output parameters | `TeamPolicy(NSEA)` reductions per point | 2-D field views | Only these fields cross to host |
| Forcing interpolation (`W3UWND` etc.) | `RangePolicy(NSEA)` | small | Host→device once per forcing slice |

**Memory (FP64):** regional 0.1° (~160k sea points × 1152 bins) ≈ 1.5 GB per spectrum copy; global 0.25° ≈ 6.5 GB. With `VA`, a propagation scratch, source-term work arrays and forcing: 10–35 GB. Fits the 80 GB H100 without tiling.

---

## 7. Comparison of Routes (updated)

| | Route 0: CPU tuning | Route A: Fortran + OpenACC | **Route B: C → C++/Kokkos** | Route B': adopt DOE Omega WW3 kernels |
|---|---|---|---|---|
| First regtest on H100 within tolerance | n/a | 2–4 mo | 6–9 mo | 3–6 mo (if kernels are released and match the lab's physics) |
| Faster than current CPU | possibly immediate | 4–8 mo, ceiling ~1.3–3× | 9–14 mo, ceiling WAM6-class | 6–10 mo |
| Maintainability | unchanged | fork with directives | clean C++ product; Serial backend runs anywhere | shared with a funded team |
| Stall risk | ~10% | ~30% | ~40% | ~35% (dependency on a third party) |

---

## 8. Three-Month Pilot (solo, part-time) — changes from the Rust plan

The task structure is the same as before; the differences are the target language and the CPP collapse step.

- **Task 0 (day 1):** write to the E3SM Omega team (LANL; Steven Brus, Mark Petersen) describing the lab's configuration and asking about the WW3 C++/Kokkos work and any early access. Ask NOAA-EMC (Jessica Meixner, Matthew Masarik) whether any GPU branch exists.
- **Task 1–2:** build WW3, run 2 regtests, profile the operational run on the H100 host CPUs (Route 0). Unchanged.
- **Task 3 (new content):** run the C preprocessor with the frozen switch defines over `model/src` to produce `resolved/`; build the call graph from the resolved source with `-finstrument-functions`; produce `FROZEN_CONFIG.md`. Definition of done: `resolved/` compiles and reproduces the regtest byte-for-byte.
- **Task 4:** `libww3.so` + `bind(C)` shims + fixture capture. Unchanged.
- **Task 5:** `ww3c/io`: readers for `mod_def.ww3`, `restart.ww3`, `wind.ww3`; writer for `out_grd.ww3`; round-trip byte-identical.
- **Task 6–10:** literal C translation in the order of §4.2 / the earlier table (dispersion → BT1/DB1 → DIA → ST4 → `W3SRCE` → `W3QCK3` → `W3XYP3/W3KTP3`), each gated against the Fortran shim at 1e-12 (or exact for integer tables). Add the FESOM2 debugging tools: per-substep dump, identical-input operator diff, range probes, subsystem disable switches.
- **Task 11:** `W3WAVE` driver in C, end-to-end regtest, `out_grd.ww3` consumable by `ww3_ounf`, scientist review.
- **Task 12 (gate memo):** C-vs-Fortran agreement; C timing vs Fortran (expect within ±5%, as FESOM2 saw); profile of the C code; `PLAN_STAGE2.md` with the kernel table of §6 ordered by measured cost; Kokkos skeleton compiled with Serial and CUDA backends (`hello-view` level) to de-risk the toolchain on the H100 host.

**Gate criteria to proceed to Stage 2:** (a) C port matches Fortran on the regtest and a 5-day operational forecast within scientist-approved tolerance; (b) `W3SRCE` + propagation + `W3OUTG` ≥ 80% of C runtime; (c) velocity ≥ 1 verified routine/week in months 2–3; (d) an answer from DOE on the Omega WW3 kernels, whichever way it goes.

---

## 9. CLAUDE.md Skeleton for `ww3-kokkos`

```markdown
# ww3-kokkos — C/C++/Kokkos port of WAVEWATCH III (frozen configuration) for one H100

## Layout
fortran-ref/     NOAA-EMC/WW3 submodule (pinned) + shims/ (bind(C)) + dump hooks (#ifdef WW3_DUMP)
resolved/        CPP-resolved Fortran for the frozen switch file — THE SPECIFICATION. Read-only.
ww3c/            Stage 1: literal single-threaded C reference. Survives as the Serial twin.
ww3k/            Stage 2: C++/Kokkos. One kernel per file under ww3k/kernels/.
fixtures/        Captured routine inputs/outputs (content-addressed).
tools/           capture.py, regtest_diff.py, dump_compare.py, ladder.sh
docs/            FROZEN_CONFIG.md, CALLGRAPH.md, CONSTANTS_*.md, FLAGGED.md, LESSONS.md, HANDOFF.md, PLAN.md

## Session protocol
Read PLAN.md, HANDOFF.md, LESSONS.md first. One routine or one kernel per session.
End by updating HANDOFF.md and LESSONS.md; commit with the routine name.

## Non-negotiable rules
1. LITERAL TRANSLATION from resolved/. Never simplify, refactor, approximate, vectorise, or "improve".
   Allowed: 1-based→0-based; column-major→index macros; module globals→struct fields; GOTO→equivalent
   structured control flow; MPI→identity mappings.
2. CITE THE LINE. Every claim about the Fortran, every constant, every namelist default:
   resolved/<file>.F90:<line> with the literal text. Never from a comment, a paper, or memory.
3. PRESERVE SUMMATION ORDER in ww3c/. In ww3k/, document each reduction that reorders.
4. Real kind: the Fortran build uses <float|double>; ww3c/ uses the same C type. Do not widen.
5. Unknown feature (COMMON, EQUIVALENCE, SAVE, OPTIONAL, assumed-size, #ifdef not in the switch file)?
   Append to docs/FLAGGED.md and stop for review.
6. NEVER widen a tolerance. Report and stop.
7. NEVER edit fortran-ref/ or resolved/ except dump hooks.
8. No raw CUDA in ww3k/. Kokkos only (tools/check_no_cuda.sh runs in CI).

## Build & gates
- Reference:  cmake -S fortran-ref -B build/ref -DSWITCH=switches/ops && cmake --build build/ref
- Resolve:    tools/resolve.sh switches/ops   → resolved/
- C ref:      cmake -S ww3c -B build/c && cmake --build build/c && ctest --test-dir build/c
- Kokkos:     cmake -S ww3k -B build/k -DKokkos_ENABLE_SERIAL=ON [-DKokkos_ENABLE_CUDA=ON -DKokkos_ARCH_HOPPER90=ON]
- Ladder:     tools/ladder.sh {R1 per-routine | R2 whole-run-C | R3 serial-bitident | R4 gpu-gate | R5 ops-5day}
- Twin check: WW3_VERIFY=1 ./ww3k run configs/regtest_tp2.toml   (runs C twin after each kernel, restores GPU result)
- Profile:    perf/flamegraph (C); nsys profile, ncu (Kokkos CUDA); Kokkos Tools kp_kernel_timer

## Tolerances (TOLERANCES.toml; scientist sign-off to change)
- Integer tables: exact. Pure arithmetic routines: 1e-12 rel (FP64) / 1e-6 rel (FP32).
- ww3k Serial vs ww3c: max |Δ| = 0.
- ww3k CUDA vs Serial, 20-step gate: per-field ceilings in TOLERANCES.toml.
- Whole-run fields (Hs, Tp, Dir, Dp, Uabs): thresholds set by scientists.

## Kokkos conventions
- Views: VA as View<Real**, LayoutRight> [NSEA][NSPEC]; grid fields View<Real**> [NY][NX].
- Allocate all Views once in init; no allocation inside the time loop.
- Source terms: one TeamPolicy kernel per step; physics packages are KOKKOS_INLINE_FUNCTION device functions.
- Host↔device traffic per step: forcing slice in; 2-D output fields out at output times. Nothing else.
- Every kernel keeps its C twin; the twin is called under WW3_VERIFY.
```

---

## 10. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Silent physics divergence the developer cannot diagnose | High | FESOM2 tool set (substep dumps, operator diff, range probes, disable switches); scientist review at every gate |
| FP32/FP64 mismatch between Fortran build and C reference | Medium | Decide in Task 1; encode in CLAUDE.md rule 4 |
| Operational grid is unstructured (PDLIB) | Unknown — ask now | Port regular-grid first regardless; PDLIB is a Stage 3 |
| Kokkos/CUDA toolchain on the lab host (C++20, nvcc, CMake) | Low | Task 12 de-risk; Spack |
| Scope creep into `ww3_multi`, coupling, extra physics | High | Frozen config document is the contract |
| DOE releases a WW3 Kokkos port mid-project | Medium (good problem) | Task 0 contact; design the C reference so their kernels can be dropped in as twins |
| Upstream NOAA physics changes | Certain over years | Yearly diff review by a scientist; the C reference makes re-porting a routine a bounded task |

---

## 11. Sources

- Koldunov et al., "An Ocean Model Ported by a Large Language Model: Experience and Lessons from FESOM2 (Fortran to C to C++/Kokkos)", arXiv:2606.11356, June 2026. https://arxiv.org/abs/2606.11356 — code: https://github.com/koldunovn/fesom_kokkos
- Ikuyajolu et al., "Porting the WAVEWATCH III (v6.07) wave action source terms to GPU", GMD 16:1445, 2023. https://gmd.copernicus.org/articles/16/1445/2023/
- Yuan et al., "WAM6-GPU v1.0", GMD 17:6123, 2024. https://gmd.copernicus.org/articles/17/6123/2024/
- Petersen et al., "The ocean model for E3SM global applications: Omega version 0.1.0", GMD 19:3569, 2026. https://gmd.copernicus.org/articles/19/3569/2026/
- E3SM, "Omega, A Next-Generation Ocean Model for Exascale Computing", May 2026. https://e3sm.org/omega-a-next-generation-ocean-model-for-exascale-computing/
- DOE EESM presentation abstract on Omega and the WW3 rebuild in C++/Kokkos. https://eesm.science.energy.gov/presentations/ocean-model-e3sm-global-applications-omega
- Wei et al., "Accelerating LASG/IAP climate system ocean model version 3 for performance portability using Kokkos", FGCS 2024. https://www.sciencedirect.com/science/article/abs/pii/S0167739X24003285
- "Evaluating Performance Portability of SYCL and Kokkos: A Case Study on LBM Simulations", IEEE 2024. https://ieeexplore.ieee.org/document/10491773/
- Kokkos releases (5.1.1, April 2026). https://github.com/kokkos/kokkos/releases ; LAMMPS Kokkos notes (C++20 requirement). https://docs.lammps.org/Speed_kokkos.html
- NOAA-EMC/WW3 About page (v7.13/7.14 CPP transition). https://github.com/NOAA-EMC/WW3/wiki/About-WW3 ; model README (CMake, switches). https://github.com/NOAA-EMC/WW3/blob/develop/model/README.md
- Chen et al., "Fortran2CPP", arXiv:2412.19770, 2024. https://arxiv.org/abs/2412.19770
- Ranasinghe et al., "LLM-Assisted Translation of Legacy FORTRAN Codes to C++", 2025. https://aclanthology.org/2025.aisd-main.6/
- TransJAX. https://pypi.org/project/transjax/ ; f2rust. https://zaynar.co.uk/posts/f2rust-1/
- NVIDIA, "Introducing CUDA Rust", 8 Sept 2026 (context for the earlier Rust plan). https://developer.nvidia.com/blog/introducing-cuda-rust-two-tracks-for-writing-gpu-kernels/
