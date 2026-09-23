# Glossary and remissive index

Every abbreviation, switch, routine, file and tool name used in this repository, with its
expansion, a one-sentence description and where it appears. Each expansion is tagged `(v)`
when it was checked against the named source (a WW3 source header, `switches.json`, the WW3
manual, or a file in this repo) and `⚠` when it is inferred or general knowledge; a `⚠` row is
a claim to verify, not a fact. The [index](#remissive-index) at the end lists every term
alphabetically with the section it lives in.

**WW3 naming patterns.** A file `w3xxxxmd.F90` is a Fortran *module* (`W3XXXXMD`) and the
routines inside it drop the `md`: `w3snl1md.F90` holds `W3SNL1` and `INSNL1`; `wm*` files are
the multi-grid (`ww3_multi`) counterparts and `ww3_*.F90` are programs. Source-term switches
come in numbered families that select one module each: `ST{n}` input/dissipation, `NL{n}`
nonlinear, `BT{n}` bottom friction, `DB{n}` depth breaking, `TR{n}` triads, `BS{n}` bottom
scattering, `IC{n}`/`IS{n}` ice, `LN{n}` linear input, `FLX{n}` stress, `REF{n}` reflection,
`PR{n}` propagation with `UQ`/`UNO` as the scheme; `0` always means "off". A *switch* is a
compile-time CPP key: the CMake build reads a whitespace-separated switch file, turns each
known key into `-DW3_<KEY>` and adds that key's `build_files` to the library
(`WW3/model/src/cmake/check_switches.cmake` `(v)`), so changing a switch means a full rebuild.
The manual chapter behind most switch rows is `WW3/manual/impl/switch.tex`.

## Models, projects and institutions

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| WW3 | WAVEWATCH III® `(v)` [README.md](../README.md) | NOAA/NCEP's third-generation spectral wind-wave model, the subject of this repo; `\ws` = "WAVEWATCH III" in `WW3/manual/defs.tex`. | everywhere; [course/00-orientation.md](../course/00-orientation.md) |
| WW4 | WAVEWATCH IV™ `(v)` [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) | NOAA's ground-up C++ (with Rust) rewrite of WW3, pre-alpha in 2026, whose L1–L4 test structure this repo copies. | [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| SWAN | Simulating WAves Nearshore `(v)` [course/15-swan.md](../course/15-swan.md) | TU Delft's implicit, unconditionally stable nearshore spectral model, the "other half" of the coastal toolkit. | [course/15-swan.md](../course/15-swan.md) |
| SWASH | ⚠ not expanded in the repo; TU Delft's non-hydrostatic, phase-resolving sibling of SWAN | Phase-resolving model for runup and overtopping, where neither WW3 nor SWAN applies. | [course/15-swan.md](../course/15-swan.md) |
| WAM | ⚠ likely "WAve Model" (the WAMDI Group, 1988) | The original third-generation model that WW3 and ECWAM descend from; `ST1`/`ST3` are its Cycle 3 / Cycle 4 physics (`WW3/model/src/w3src1md.F90`, `w3src3md.F90` headers `(v)`). | [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md), [course/07-physics-choices.md](../course/07-physics-choices.md) |
| ECWAM | ⚠ ECMWF's WAM | WAM's operational descendant at ECMWF, source of ERA5's wave fields. | [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md) |
| WAM6-GPU | WAM6-GPU v1.0, Yuan et al. (2024), *GMD* 17 `(v)` [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) | A whole-model GPU refactor of WAM that keeps fields device-resident and reports an order-of-magnitude gain; the counter-example to directive offloading. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md), [pubs/proposal/pt/05-justification.md](../pubs/proposal/pt/05-justification.md) |
| FESOM2 | ⚠ Finite-volumE Sea ice–Ocean Model 2; not expanded in the repo | The ocean model Koldunov et al. (2026) took from Fortran to C to C++/Kokkos with an LLM assistant; the "FESOM2 recipe" is this project's porting method `(v)`. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md), [pubs/proposal/pt/05-justification.md](../pubs/proposal/pt/05-justification.md) |
| Omega | ⚠ E3SM's C++/Kokkos ocean model, the MPAS-Ocean successor | DOE project said to be rebuilding WW3's source terms in C++/Kokkos; the H100 plan's first action is to contact them. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| NOAA | National Oceanic and Atmospheric Administration `(v)` `WW3/regtests/bin/matrix.comp` header | US agency that distributes WW3 and owns the WAVEWATCH trademarks. | [README.md](../README.md) |
| NWS | National Weather Service `(v)` `WW3/regtests/bin/matrix.comp` header | NOAA's weather service, the trademark holder and primary funder of WW4. | [README.md](../README.md), [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) |
| NCEP | National Centers for Environmental Prediction `(v)` `WW3/manual/start.tex` | The NOAA centre where WW3 was written and runs operationally. | [course/00-orientation.md](../course/00-orientation.md) |
| EMC | Environmental Modeling Center `(v)` `WW3/manual/start.tex` | NCEP's modelling centre; the GitHub organisation `NOAA-EMC` hosts WW3, WW4 and WW3-tools. | [README.md](../README.md) |
| NCO (NCEP) | NCEP Central Operations `(v)` `WW3/manual/impl/switch.tex` | The operations centre whose file-naming conventions the `NCO` switch enables; not the netCDF Operators of the same acronym. | [WW3 switches](#ww3-switches) |
| ON 525 / ON 528 | NCEP Office Note 525 (Phase I report, Tolman 2025) and 528 (Phase II report, 2026) `(v)` [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) | The WW4 planning documents: language choice, governance, and the commitment to sunset WW3 once WW4 matures. | [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| UFS | ⚠ Unified Forecast System, NOAA's coupled forecasting system | The coupled system in which WW3 is the wave component via a NUOPC cap; regtests `ww3_ufs1.*` are its configurations. | [examples/README.md](../examples/README.md), [course/05-nesting.md](../course/05-nesting.md) |
| NUOPC / ESMF | National Unified Operational Prediction Capability / Earth System Modeling Framework `(v)` `WW3/manual/app/nuopc.tex` | The coupling layer and framework used by UFS; WW3's output type 7 ("coupling fields") serves them. | [course/06-output.md](../course/06-output.md) |
| OASIS | Ocean Atmosphere Sea Ice Soil `(v)` `WW3/manual/app/oasis.tex` | The CERFACS/CNRS coupler selected by the `OASIS` switch (`w3oacpmd.F90`); OASIS-MCT is its usual version. | [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md) |
| LabECO | Laboratório de Engenharia e Ciências Oceânicas `(v)` [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) | The UFSC laboratory that runs WW3 operationally for ReNOMO and whose configuration the proposal optimises. | [pubs/proposal/pt/](../pubs/proposal/pt/), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| UFSC | Universidade Federal de Santa Catarina `(v)` [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) | Home of LabECO and of the co-advisor. | [pubs/proposal/meta.pt.yaml](../pubs/proposal/meta.pt.yaml) |
| UFRJ | Universidade Federal do Rio de Janeiro `(v)` [pubs/proposal/meta.pt.yaml](../pubs/proposal/meta.pt.yaml) | The university whose Escola Politécnica receives the graduation-project proposal. | [README.md](../README.md), [course/00-orientation.md](../course/00-orientation.md) |
| DEL | Departamento de Engenharia Eletrônica e de Computação `(v)` [pubs/proposal/meta.pt.yaml](../pubs/proposal/meta.pt.yaml) | The UFRJ/Poli department whose proposal template and fixed section names `pubs/proposal/` follows. | [README.md](../README.md), [pubs/proposal/template.tex](../pubs/proposal/template.tex) |
| ReNOMO | Rede Nacional de Observação e Monitoramento Oceânico `(v)` [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) | The national ocean-observation network whose forecast cycle LabECO's WW3 feeds. | [pubs/proposal/pt/01-title.md](../pubs/proposal/pt/01-title.md) |
| CNPq / MCTI / Finep | ⚠ Conselho Nacional de Desenvolvimento Científico e Tecnológico / Ministério da Ciência, Tecnologia e Inovação / Financiadora de Estudos e Projetos; only the call number "062/2022" is `(v)` in [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) | The funding call behind ReNOMO. | [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) |
| ECMWF | ⚠ European Centre for Medium-Range Weather Forecasts | Producer of ERA5 and of the `ST3` (WAM4+/Bidlot) lineage. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| IFREMER / SHOM | ⚠ Institut Français de Recherche pour l'Exploitation de la Mer / Service Hydrographique et Océanographique de la Marine | The French institutions behind the "SHOM/Ifremer" `ST4` package (`WW3/model/src/w3src4md.F90` header `(v)`) and the `switch_Ifremer2` reference switch file. | [course/04-forcing.md](../course/04-forcing.md), [switches/README.md](../switches/README.md) |
| DOE / E3SM / ORNL / LANL | ⚠ US Department of Energy, Energy Exascale Earth System Model, Oak Ridge and Los Alamos National Laboratories | The institutions behind the 2023 OpenACC port of `W3SRCEMD` (Ikuyajolu et al.) and the Omega project. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| GMD (journal) | ⚠ *Geoscientific Model Development* | Journal of Ikuyajolu et al. (2023) and Yuan et al. (2024); not the `NL3` GMD of the physics section. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |

## Wave physics and the spectrum

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| N | action density N(k, θ; x, t) = F/σ `(v)` [course/00-orientation.md](../course/00-orientation.md) | The quantity WW3 transports, conserved in the presence of currents where energy is not; stored per point as `VA` ("storage array for spectra", `WW3/model/src/w3wdatmd.F90` `(v)`). | [course/00-orientation.md](../course/00-orientation.md) |
| F, E | energy density spectrum F(k, θ) or E(σ, θ) `(v)` [course/00-orientation.md](../course/00-orientation.md), [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) | Wave energy per spectral bin; the DIA is applied to the energy spectrum and converted from action as A = E/σ. | [course/00-orientation.md](../course/00-orientation.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| σ (SIG) | intrinsic (relative) radian frequency; `SIG` = "relative frequencies" array `(v)` `WW3/model/src/w3gdatmd.F90` | The frequency axis of the spectrum, allocated `SIG(0:NK+1)` with a zeroth bin: the reason the shim must receive `SIG(1)`. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| θ (TH) | direction; `TH` = "directions (radians)" `(v)` `WW3/model/src/w3gdatmd.F90` | The directional axis, `NTH` bins of width `DTH`. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| k (WN) | wavenumber; `WN` = "wavenumbers" for all sea points `(v)` `WW3/model/src/w3adatmd.F90` | The spatial wavenumber from the dispersion relation σ² = gk tanh(kd), solved by `WAVNU1`/`WAVNU3` or 20 Newton steps in the fixtures. | [course/00-orientation.md](../course/00-orientation.md), [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) |
| CG | group velocity; `CG` = "group velocities for all wave model grid points" `(v)` `WW3/model/src/w3adatmd.F90` | The speed at which energy travels, cg = g/(4πf) in deep water; the lowest frequency's CG sets the CFL limit. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| KDMEAN | mean relative depth k·d, "band-energy-weighted mean of kd" `(v)` [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) | The shallow-water scaling argument of the DIA, passed to `W3SNL1` as the expression `WNMEAN*DEPTH` `(v)` [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md). | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| NK, NTH, NSPEC | number of discrete wavenumbers, directions and spectral bins, `NSPEC = NK*NTH` `(v)` `WW3/model/src/w3gdatmd.F90` | The size of one spectrum; the spectral index is `ISP = ITH + (IK-1)*NTH` (θ fastest). | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| FREQ1 (FR1) | "first frequency (Hz)" `(v)` `WW3/model/nml/ww3_grid.nml`; `FR1` = "lowest frequency" `(v)` `w3gdatmd.F90` | The lowest frequency of the spectral grid; energy below it does not exist in the model. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| XFR | "frequency increment" factor σ(k+1)/σ(k) `(v)` `WW3/model/nml/ww3_grid.nml`, `w3gdatmd.F90` | The geometric ratio between frequency bins, 1.1 by default, so f_max = FREQ1·XFR^(NK−1). | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| DTH | "directional increments (radians)" `(v)` `WW3/model/src/w3gdatmd.F90` | 2π/NTH, the directional bin width. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| GRAV | acceleration of gravity, 9.806 m/s² `(v)` `WW3/model/src/constants.F90` | WW3's `g`, a default-`REAL` parameter. | [kokkos/tests/fixtures/snl1_sea_state.F90](../kokkos/tests/fixtures/snl1_sea_state.F90) |
| PI, TPI, TPIINV | π, 2π and 1/(2π) `(v)` `WW3/model/src/constants.F90` | Built as `REAL` parameters in that order, which the port reproduces to keep bit parity. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [kokkos/src/ww_kokkos/snl1_config.hpp](../kokkos/src/ww_kokkos/snl1_config.hpp) |
| Hs (HS) | significant wave height, 4√(total energy) `(v)` [course/06-output.md](../course/06-output.md) | The single most used output field, `hs` in the netCDF and the first row of the tolerances file. | [course/06-output.md](../course/06-output.md), [kokkos/tools/nccmp-tol/tolerances.txt](../kokkos/tools/nccmp-tol/tolerances.txt) |
| fp (FP) | peak frequency `(v)` [course/06-output.md](../course/06-output.md) | The frequency of the spectral maximum; a noisy statistic. | [course/06-output.md](../course/06-output.md) |
| Tp | peak period `(v)` [course/00-orientation.md](../course/00-orientation.md) | 1/fp; comparing it point-to-point against a buoy is discouraged in lesson 06. | [course/06-output.md](../course/06-output.md) |
| Tm, T01, T02, T0M1 | mean periods from different spectral moments `(v)` [course/06-output.md](../course/06-output.md) | Not interchangeable; `T0M1` (energy period) is what engineering work wants and is a judged field of `nccmp-tol`. | [course/06-output.md](../course/06-output.md) |
| dir, dp (DIR, DP) | mean direction and peak direction `(v)` [course/06-output.md](../course/06-output.md) | Directional output fields in degrees, given a 1e-2 absolute tolerance because they are quantised by the bin width. | [kokkos/tools/nccmp-tol/README.md](../kokkos/tools/nccmp-tol/README.md) |
| SPR, EF, WND, DPT, USS, TUS, SXY, UST, CHA, PHS/PTP/PDIR | spread, 1-D spectrum, wind, depth, Stokes drift, radiation stress, friction velocity, Charnock, partitioned fields `(v)` [course/06-output.md](../course/06-output.md) | The other output fields the course reaches for; the authoritative list is in `WW3/model/nml/ww3_ounf.nml`. | [course/06-output.md](../course/06-output.md), [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| efth | 2-D spectrum `efth(time, station, frequency, direction)` in m²/Hz/rad `(v)` [course/06-output.md](../course/06-output.md) | What `ww3_ounp` writes; integrating it recovers `Hs`. | [course/06-output.md](../course/06-output.md) |
| S, D (VS, VD) | source term S and the diagonal term D of its derivative `(v)` `WW3/model/src/w3snl1md.F90` (W3SNL1 purpose) | Every source-term routine returns both, `D` for the semi-implicit integration in `W3SRCE`. | [course/07-physics-choices.md](../course/07-physics-choices.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| Sin, Snl, Sds, Sbot/Sbt, Sdb, Sice, Str, Sln | wind input, nonlinear four-wave interaction, dissipation (whitecapping), bottom friction, depth breaking, ice, triads, linear input `(v)` [course/00-orientation.md](../course/00-orientation.md) | The terms of S = Sin + Snl + Sds + Sbot + Sdb + Sice + …, each selected by a switch family (`ST`, `NL`, `BT`, `DB`, `IC`, `TR`, `LN`). | [course/00-orientation.md](../course/00-orientation.md), [course/07-physics-choices.md](../course/07-physics-choices.md) |
| DIA | Discrete Interaction Approximation, Hasselmann et al. (1985) `(v)` `WW3/model/src/w3snl1md.F90` header | The `NL1` approximation of Snl with two mirror-image quadruplets; the first kernel ported. | [course/07-physics-choices.md](../course/07-physics-choices.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| quadruplet | four resonant wave components, one representative configuration in the DIA `(v)` [course/07-physics-choices.md](../course/07-physics-choices.md) | Defined by `LAMBDA` (`LAM`, default 0.25); `INSNL1` precomputes its address tables and weights. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| GMD (NL3) | Generalized Multiple DIA `(v)` `WW3/model/src/w3snl3md.F90` | Several quadruplets with fitted coefficients (`genes_gmd` fits them). | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| WRT / XNL (NL2) | Webb–Resio–Tracy exact interactions `(v)` `WW3/manual/eqs/NL2.tex`, `\xnl` = "WRT" in `WW3/manual/defs.tex` | The "exact" Snl, ~1000× the cost of the DIA, research only. | [switches/README.md](../switches/README.md) |
| TSA / FBI (NL4) | Two-Scale Approximation / "generic shallow-water Boltzmann integral" `(v)` `WW3/manual/impl/switch.tex`, `WW3/model/src/w3snl4md.F90` | Resio & Perrie's alternative Snl. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| GKE (NL5) | Generalized Kinetic Equation `(v)` `WW3/manual/eqs/NL5.tex` | Resonant and quasi-resonant four-wave interactions (`w3snl5md.F90`, `w3gkemd.F90`). | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| GQM | Gaussian Quadrature Method (Lavrenov 2001) `(v)` `WW3/model/src/w3snl1md.F90` (`W3SNLGQM`) | The `IQTPE <= 0` branch of `NL1` that the port does not replace. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| JONSWAP | ⚠ Joint North Sea Wave Project (not expanded in the repo; `\js` = "JONSWAP" in `WW3/manual/defs.tex`) | The empirical fetch-limited spectrum (γ = 3.3) used as the L1 fixture sea state, and the `BT1` bottom-friction formulation named after the same project. | [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md), [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| PM | Pierson–Moskowitz fully developed limit, Hs = 0.0246·U10² `(v)` [course/06-output.md](../course/06-output.md) | The ceiling `ww_fetch_analyse` prints for a fully developed sea. | [course/06-output.md](../course/06-output.md) |
| Kahma & Calkoen (K&C92) | Kahma & Calkoen (1992) fetch law, ê = 5.2e-7·x̂^0.9 `(v)` [course/06-output.md](../course/06-output.md) | The empirical growth curve example 01 is compared against. | [course/06-output.md](../course/06-output.md), [examples/01-fetch-limited-growth/](../examples/01-fetch-limited-growth/) |
| fetch | distance from the upwind coastline over which the wind acts `(v)` [course/06-output.md](../course/06-output.md) | The independent variable of fetch-limited growth; example 01's box has its coast at i = 1. | [course/03-grids.md](../course/03-grids.md) |
| CFL | ⚠ Courant–Friedrichs–Lewy condition (the manual only defines the macro `\cfl` = "CFL") | The explicit-scheme stability limit DTXY ≲ Δx/cg_max that ties the timestep to the grid spacing and lowest frequency. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/15-swan.md](../course/15-swan.md) |
| GSE | Garden Sprinkler Effect `(v)` `WW3/manual/num/GSE.tex` | The break-up of a distant swell field into discrete beams because each direction bin travels as its own ray; `PR3` corrects it. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/07-physics-choices.md](../course/07-physics-choices.md) |
| UQ | ULTIMATE QUICKEST `(v)` `WW3/manual/defs.tex`, `WW3/model/src/w3uqckmd.F90` | The third-order explicit propagation scheme (`W3QCK1/2/3`), WW3's standard. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| UNO | ⚠ not expanded in WW3 ("UNO2 scheme", `WW3/model/src/w3uno2md.F90` header `(v)`) | The second-order alternative to `UQ`. | [WW3 switches](#ww3-switches) |
| SHOWEX | ⚠ Shoaling Waves Experiment (Ardhuin et al. 2003, `\showex` = "SHOWEX" in `WW3/manual/defs.tex`) | The movable-bed bottom-friction formulation of `BT4`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| LTA | Lumped Triad Approximation / Interaction (Eldeberky 1996) `(v)` `WW3/model/src/w3str1md.F90` | The `TR1` triad source term. | [WW3 switches](#ww3-switches) |
| BYDRZ | ⚠ Babanin–Young–Donelan–Rogers–Zieger (`ST6` is "BYDRZ source term package" in `WW3/manual/impl/switch.tex` `(v)`, not expanded there) | The observation-based `ST6` input/dissipation package (`w3src6md.F90`). | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| WAM4+ | WAM Cycle 4 physics by Janssen with Bidlot's extensions `(v)` `WW3/model/src/w3src3md.F90` | The `ST3` package and the wind-input lineage of `ST4`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| BETAMAX | master gain on wind input in `&SIN4` `(v)` [course/07-physics-choices.md](../course/07-physics-choices.md) | The `ST4` tuning knob to resist reaching for before checking winds and bathymetry. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| MIZ | ⚠ marginal ice zone | Where the `IC*`/`IS*` ice physics act; `IS2` is "floe-size dependant scattering of waves in the marginal ice zone" (`w3sis2md.F90` `(v)`). | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| SMC | Spherical Multiple-Cell grid `(v)` [course/03-grids.md](../course/03-grids.md) | Quad-tree-refined grid type used by the UK Met Office (`w3smcomd.F90`, `w3psmcmd.F90`). | [course/03-grids.md](../course/03-grids.md) |
| RECT / CURV / UNST | rectilinear, curvilinear and unstructured `GRID%TYPE` `(v)` [course/03-grids.md](../course/03-grids.md) | The three explicit grid types; `UNST` needs `PDLIB` for parallel runs. | [course/03-grids.md](../course/03-grids.md) |
| SMPL / TRPL / NONE | simple periodic, tripole and no closure `GRID%CLOS` `(v)` [course/03-grids.md](../course/03-grids.md) | Index closure of global grids; tripole folds the top row to avoid the pole singularity. | [course/03-grids.md](../course/03-grids.md) |
| ZLIM, DMIN | coastline limit depth and absolute minimum water depth `(v)` `WW3/model/nml/ww3_grid.nml` | The "which points are land" threshold versus the floor applied to depth in the physics. | [course/03-grids.md](../course/03-grids.md) |
| FLAGTR | obstruction/transparency flag in `&MISC` `(v)` [course/03-grids.md](../course/03-grids.md) | 0 = no subgrid obstruction file, 1–4 = transparencies at cell boundaries/centres with or without ice. | [course/03-grids.md](../course/03-grids.md), [course/01-build.md](../course/01-build.md) |
| shuffle / card deck | WW3's MPI decomposition of sea points across ranks `(v)` `WW3/manual/num/space_tri.tex` ("Card Deck approach"), [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) | Deals sea points to ranks like cards for source terms and transposes (`W3GATH`/`W3SCAT`) for propagation; balances well, does not scale to exascale. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| phase-averaged | statistics of the spectrum, not individual crests `(v)` [course/00-orientation.md](../course/00-orientation.md) | What WW3 and SWAN are; phase-resolving models (SWASH, XBeach) are a different class. | [course/00-orientation.md](../course/00-orientation.md) |
| partition (PHS, PTP, PDIR, NOSWLL) | watershed partitioning of the 2-D spectrum into wind sea + swell systems `(v)` [course/06-output.md](../course/06-output.md), `WW3/model/src/w3partmd.F90` | Output fields `PHS(n)` etc. per system, `FIELD%PARTITION` selects which. | [course/06-output.md](../course/06-output.md) |

## WW3 switches

Sources: `WW3/model/src/cmake/switches.json` (the CMake build's list; it has a description
per *category*, not per switch, so `(v json)` below cites the category) and the manual chapter
`WW3/manual/impl/switch.tex` (`(v man)`). The lab's own files are
[switches/switch_lab_shrd](../switches/switch_lab_shrd), [switch_lab_mpi](../switches/switch_lab_mpi)
and [switch_lab_st6](../switches/switch_lab_st6), explained in [switches/README.md](../switches/README.md).
A key that is in neither `switches.json` nor any `#ifdef W3_<KEY>` in `model/src` is **inert**
in the 7.14 CMake build (`check_switches.cmake` only looks for keys it knows); five keys in the
lab files are in that state and are flagged below.

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| F90 | ⚠ legacy "Fortran 90 style / system calls" key ([switches/README.md](../switches/README.md)); **not in `switches.json`, no `W3_F90` guard, absent from all 558 upstream switch files** `(v)` | Inert in the CMake build; kept in the lab files for compatibility with old tutorials. | [course/01-build.md](../course/01-build.md), [switches/README.md](../switches/README.md) |
| NOGRB | "No package included" for GRIB `(v man)`; category "GRIB package" `(v json)` | No GRIB output; the alternative `NCEP2` needs NCEP's GRIB2 library. | [course/01-build.md](../course/01-build.md) |
| NCEP2 | NCEP GRIB2 package for IBM SP `(v man)` | GRIB output via NCEP libraries; not used here. | [switches/README.md](../switches/README.md) |
| NOPA | ⚠ legacy "no coupling to an external driver" key ([course/01-build.md](../course/01-build.md)); **not in `switches.json`, no `W3_NOPA` guard, in 3 of 558 upstream switch files** `(v)` | Inert in the CMake build; coupling is opt-in through `COU`/`OASIS` instead. | [course/01-build.md](../course/01-build.md) |
| LRB4 | ⚠ legacy "4-byte record-length units for binary files" key ([course/01-build.md](../course/01-build.md)); **not in `switches.json`, no `W3_LRB4` guard, in 2 of 558 upstream switch files** `(v)` | Inert in the CMake build (record length is handled elsewhere); harmless to keep. | [course/01-build.md](../course/01-build.md) |
| NC4 | legacy "enable netCDF-4 output" key, **inert in 7.14** `(v)`: absent from `model/src/cmake/switches.json` and `model/bin/all_switches`, no `W3_NC4` guard anywhere in `model/src`, present in 1 of 558 upstream switch files (`switch_NWS_rwps`). CMake emits `-DW3_NC4` for it, as it does `W3_<switch>` for every token in the file, but no source reads it; the classic `w3_make` scripts do not know it either | `ww3_ounf`/`ww3_ounp` are built whenever CMake finds netCDF (`NetCDF_Fortran_FOUND`); netCDF-3 vs -4 is the runtime `FILE%NETCDF` key (3 or 4) of `ww3_ounf.nml` (`NCTYPE` in the program, `ww3_ounf.F90:362`). The manual (`intro/about.tex`) records netCDF-3 support removed and NC4 made the default; `nc-config --has-nc4` is what `impl/compile.tex` asks you to check. Kept in `switches/switch_lab_*` as harmless. | [course/01-build.md](../course/01-build.md), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| SHRD | "Shared memory model, no message passing" `(v man)`; categories "shared / distributed memory" and "message passing protocol" `(v json)` | The serial build; `SHRD` appears in both mandatory groups. | [switches/switch_lab_shrd](../switches/switch_lab_shrd), [course/01-build.md](../course/01-build.md) |
| DIST | "Distributed memory model" `(v man)` | Must be paired with `MPI`. | [switches/switch_lab_mpi](../switches/switch_lab_mpi) |
| MPI | "Message Passing Interface (MPI)" `(v man)` | The distributed-memory build, run with `mpirun -np N ww3_shel`. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| MPIBDI | "Experimental parallelization of multi-grid model initialization" `(v man)` | Experimental MPI option (`mpiexp` category `(v json)`). | — |
| OMPG | "General loop parallelization directives used for both exclusive OpenMP parallelization and hybrid MPI-OpenMP" `(v man)`; category "directive controlled threading" `(v json)` | Turns on the `!$OMP` regions such as the `W3SRCE` loop in `W3WAVE`; requires care with the unlocked phase-1 shim. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| OMPH | "Idem, but for directives used only for hybrid MPI-OpenMP parallelization" `(v man)`; requires `MPI` and `OMPG` `(v json)` | The hybrid layer on top of `OMPG`. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| OMP0 | ⚠ not in `switches.json` or the manual; a `W3_OMP0` guard wraps an `!$OMP PARALLEL DO` around the source-term loop in `WW3/model/src/w3wavemd.F90:1551` `(v)` | An older OpenMP path the shim's threading note also has to respect. | [kokkos/README.md](../kokkos/README.md), [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| B4B | "Enforce bit-for-bit reproducibility of OpenMP enabled code … currently only affects code compiled with the `SMC` switch" `(v man)`; category "bit-for-bit reproducability" `(v json)` | A switch, distinct from the `*_b4b` matrix variants below. | [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests) |
| PR0 / PR1 / PR2 / PR3 | no propagation / first-order / higher-order with the `art:BH87` dispersion correction (⚠ Booij & Holthuijsen 1987) / higher-order with the `tol:OMOD02b` averaging technique (⚠ Tolman 2002) `(v man)`; category "GSE aleviation" `(v json)` | The GSE-alleviation group; `PR3` selects `w3pro3md.F90` (`W3XYP3`, `W3KTP3`). | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| UQ / UNO | "Third-order (UQ) propagation scheme" / "Second-order (UNO) propagation scheme" `(v man)`; category "propagation scheme" `(v json)` | The scheme paired with `PR{n}`; `UQ` builds `w3uqckmd.F90`, `UNO` builds `w3uno2md.F90`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| SMC | "Activate SMC grid" `(v man)` | Builds `w3smcomd.F90`/`w3psmcmd.F90`. | [course/03-grids.md](../course/03-grids.md) |
| FLX0–FLX5 | stress computation: none (in source terms) / Wu (1980) / Tolman & Chalikov / idem with cap / Hwang (2011) / from atmospheric stress `(v man)`, module headers `w3flx1md`–`w3flx4md` `(v)` | Which friction-velocity formula feeds the input term; `ST4` conflicts with `FLX1–4` in `switches.json` `(v)`, so upstream pairs it with `FLX0`. See the `FLX2` caveat in [course/01-build.md](../course/01-build.md). | [course/01-build.md](../course/01-build.md), [switches/README.md](../switches/README.md) |
| FLD0 / FLD1 / FLD2 | "Diagnostic stress comp" `(v json)`; sea-state dependent τ after `art:Rei14` / `art:Don12` (⚠ Reichl 2014 / Donelan 2012) `(v man)` | Optional diagnostic stress. | — |
| LN0 / LN1 / SEED | "No linear input" / "Cavaleri and Malanotte-Rizzoli with filter" / "Spectral seeding" `(v man)`; category "linear input" `(v json)` | Seeds a spectrum from calm; `LN1` builds `w3sln1md.F90`. | [course/01-build.md](../course/01-build.md) |
| ST0–ST6 | input/whitecapping: none / WAM-3 / Tolman & Chalikov (1996) / WAM-4 and variants / Ardhuin et al. (2010) / BYDRZ `(v man)`; module headers `w3src0md`–`w3src6md` `(v)` | The matched input–dissipation packages; there is no `ST5` in the tree. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| STAB0 / STAB2 / STAB3 | no stability correction (no effect) / correction compatible with `ST2` only / `rep:AB02` correction (⚠ Abdalla & Bidlot 2002) for `ST3`/`ST4` `(v man)`; `STAB2` requires `ST2` `(v json)` | Air–sea stability correction on the wind input; `STAB0` has no `W3_STAB0` guard at all `(v)`. | [course/04-forcing.md](../course/04-forcing.md) |
| NL0–NL5 | quadruplet interactions: none / DIA or GQM / WRT exact / GMD / TSA / GKE `(v man)` (`NL5` from `w3snl5md.F90` `(v)`); category "quadruplet interactions" `(v json)` | `NL2` conflicts with `OMPG`/`OMPH` `(v json)`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| NLS | "Activate nonlinear smoother" `(v man)` | Builds `w3snlsmd.F90`. | — |
| BT0 / BT1 / BT4 / BT8 / BT9 | bottom friction: none / JONSWAP / SHOWEX / Dalrymple & Liu fluid mud / Ng fluid mud `(v man)`, module headers `(v)` | `BT4` needs a `D50` sediment map. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| DB0 / DB1 | depth-induced breaking: none / Battjes–Janssen `(v man)`, `w3sdb1md.F90` `(v)` | Surf-zone dissipation. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| MLIM | "Use Miche-style shallow water limiter" `(v man)`; category "Miche style limiter" `(v json)` | Caps `Hs` in shallow water inside `W3SRCE` (`W3_MLIM` block `(v)`). | [course/01-build.md](../course/01-build.md) |
| TR0 / TR1 | triad interactions: none / Lumped Triad Interaction (LTA) `(v man)` | `w3str1md.F90`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| BS0 / BS1 | bottom scattering: none / Magne and Ardhuin `(v man)` | `w3sbs1md.F90`. | [switches/README.md](../switches/README.md) |
| IC0–IC5 | ice sink term: none / simple / Liu et al. / Wang & Shen viscoelastic / frequency-dependent / extended Fox–Squire and other models `(v man)`, `w3sic5md.F90` `(v)` | Ice dissipation family; `IC5` arrived in v6.06. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| IS0 / IS1 / IS2 | ice scattering: none / diffusive / floe-size dependent scattering and dissipation `(v man)` | `w3sis1md.F90`, `w3sis2md.F90`. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| REF0 / REF1 | "No reflection" / "Enables reflection of shorelines and icebergs" `(v man)` | `REF1` needs a slope map. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| IG1 | "Second-order spectrum and free infragravity waves" `(v man)` | Builds `w3gig1md.F90`, `w3canomd.F90`. | — |
| UOST | "Enable the unresolved obstacles source term" `(v man)` | `w3uostmd.F90`. | — |
| XX0 | ⚠ legacy "no user-defined/experimental source term" key ([course/01-build.md](../course/01-build.md)); **not in `switches.json`, no `W3_XX0` guard, absent from all 558 upstream switch files** `(v)`; the regtest `info` files still say `!/XXn` | Inert; the placeholder module `w3sxx0md` is not in the pinned tree either (`docs/KOKKOS_H100_PLAN_202609.md`). | [course/01-build.md](../course/01-build.md) |
| WNT0 / WNT1 / WNT2 | wind interpolation in time: none / linear / approximately quadratic `(v man)` | Compile-time choice of forcing interpolation. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| WNX0 / WNX1 / WNX2 | wind interpolation in space: vector / approximately linear speed / approximately quadratic speed `(v man)` | Idem in space. | [course/01-build.md](../course/01-build.md) |
| CRT0 / CRT1 / CRT2, CRX0 / CRX1 / CRX2 | current interpolation in time and space, same scheme as `WNT`/`WNX` `(v man)` | Current forcing interpolation. | [course/01-build.md](../course/01-build.md) |
| RWND | "Correct wind speed for current velocity" `(v man)`; category "wind vs. current definition" `(v json)` | Wind taken relative to the current (`RWINDC` in `w3updtmd.F90` `(v)`). | [course/01-build.md](../course/01-build.md) |
| WCOR | "wind speed correction" `(v json)` | Optional wind-speed correction. | — |
| WRST | "Save wind in restart and use in first time step in wmesmf" `(v man)` | Coupled-restart detail. | — |
| MGP / MGW (MGWIND) / MGG | moving-grid propagation, wind and GSE corrections `(v man)`; `MGWIND` is the JSON name `(v json)` | Continuously moving grid options. | — |
| SEC1 / TDYN / DSS0 / XW0 / XW1 / TIDE / REFRX | sub-second time steps / dynamic swell age / no frequency dispersion in diffusion / swell-only or growth-only diffusion in `UQ` / tidal analysis / refraction from phase-velocity gradients `(v man)` | Miscellaneous numerical options. | — |
| RTD | "Rotated grid option" `(v man)`; category "rotated grid" `(v json)` | Rotated-pole grids (`PoLat`, `PoLon` in `w3gdatmd.F90` `(v)`). | — |
| NNT | "Generate file test_data_nnn.ww3 with spectra and nonlinear interactions for training and testing of NNIA" `(v man)`; "NN training/test data generation" `(v json)` | Neural-network training dumps; also the only default importer of `NDSE` in `W3SRCE`, which is why `PATCH.md` adds its own. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| COU | "Activates the calculation of variables required for coupling" `(v man)`; requires `MPI` `(v json)` | Coupling fields on. | [course/06-output.md](../course/06-output.md) |
| OASIS / OASACM / OASOCM / OASICM | initialises the OASIS coupler / atmospheric / oceanic / sea-ice coupling fields `(v man)` | Builds `w3oacpmd.F90`, `w3agcmmd.F90`, `w3ogcmmd.F90`, `w3igcmmd.F90` `(v json)`. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| PDLIB | "Domain Decomposition for Explicit and Implicit Solver on triangular unstructured grids (ParMetis is required)" `(v man)`; PDLIB = Parallel Decomposition Library `(v)` `WW3/model/src/w3profsmd_pdlib.F90` | Unstructured-grid parallelism, out of the proposal's scope. | [course/03-grids.md](../course/03-grids.md), [pubs/proposal/pt/04-scope.md](../pubs/proposal/pt/04-scope.md) |
| METIS / SCOTCH | "domain decomposition library" `(v json)`, both require `PDLIB` | The graph partitioner behind `PDLIB`; METIS/ParMETIS are pinned in the toolchain. | [course/01-build.md](../course/01-build.md) |
| SCRIP / SCRIPNC / SCRIPMPI | "Grid to grid interpolation" `(v json)`; SCRIP = Spherical Coordinate Remapping and Interpolation Package `(v)` `WW3/manual/manual.bib`; `SCRIPNC` stores remapping weights in netCDF `(v man)` | Remapping between grids in `ww3_multi` (`wmscrpmd.F90`). | [switches/README.md](../switches/README.md) |
| TRKNC | "Activates the NetCDF API in the wave system tracking post-processing program" `(v man)` | Used by `ww3_systrk`. | — |
| NCO | "Code modifications for operational implementation at NCO (NCEP Central Operations)" `(v man)` | In `switch_NCEP_st4`; not recommended for general use. | [Models, projects and institutions](#models-projects-and-institutions) |
| MPRF / MEMCHECK / SETUP / BIN2NC / ASCII | multi-grid profiling / memory check / zeta setup / netCDF instead of binary model output / ASCII output for `.ww3` files `(v json)` | Diagnostic and I/O options in `switches.json`. | — |
| O0–O16 | optional output: namelists (`O0`), boundary points (`O1`), status map and `mask.ww3` (`O2`), field preprocessor (`O3`), 1-D/2-D spectrum plots (`O4`/`O5`), wave-height map (`O6`), echo of homogeneous input and point-output diagnostics (`O7`), … buoy log (`O14`) `(v man)` | Verbosity keys; harmless to include, and `O2` is what writes `mask.ww3`. | [course/01-build.md](../course/01-build.md), [switches/README.md](../switches/README.md) |
| T, T0, T1, S | "Enable test output throughout the program(s)" (`T`, `Tn`) and "Enable subroutine tracing … `strace`" (`S`) `(v man)`; `W3_T0`/`W3_T1` guards exist in 20/32 source files `(v)` | Debug keys; the `#ifdef W3_T` `WRITE` is what blocks `PURE` on `W3SNL1`. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| KOKKOS (proposed) | new category "C++/Kokkos port of the source terms", `build_files: w3kokkosmd.F90`, requires `NL1` `(v)` [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) | The switch the fork branch would add so `W3_KOKKOS` guards the shim calls; not applied in this repo. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| switch_default, switch_Ifremer2, switch_NCEP_st4 | upstream reference switch files in `WW3/model/bin/` `(v)` | 30 upstream files exist; `NCEP_st4` and `Ifremer2` both pair `ST4` with `FLX0`. | [switches/README.md](../switches/README.md), [course/01-build.md](../course/01-build.md) |
| switch_PR3_UQ, switch_MPI | per-regtest switch files under `WW3/regtests/<test>/input/` `(v)` | What `just rt ww3_tp1.1 PR3_UQ` builds with. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |

## WW3 programs and files

Program purposes are from the `1. Purpose` header of `WW3/model/src/<program>.F90` `(v)`;
namelist keys from the annotated templates in `WW3/model/nml/` `(v)`.

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| ww3_grid | "Grid preprocessing program, which writes a model definition file containing the model parameter settings and grid data" `(v)` | Reads `ww3_grid.nml` plus ASCII bathymetry/mask and writes `mod_def.ww3`; the most important program. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| ww3_strt | "Generation of initial conditions for a cold start of WAVEWATCH III" `(v)` | Writes `restart.ww3`; optional, `ww3_shel` cold-starts from calm without it. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| ww3_prnc | "Pre-processing of the input water level, current, wind, ice fields, momentum and air density, as well as assimilation data … from NetCDF input" `(v)` | netCDF forcing → `wind.ww3`, `current.ww3`, `ice.ww3`, `level.ww3`; the program with the four silent failure modes. | [course/04-forcing.md](../course/04-forcing.md) |
| ww3_prep | "Pre-processing of the input … fields for the generic shell W3SHEL" `(v)` | The legacy ASCII/binary counterpart of `ww3_prnc`. | [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) |
| ww3_shel | "A generic shell for WAVEWATCH III, using preformatted input fields" `(v)` | The single-grid model driver, the proposal's whole scope and the thing the benchmark times. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [pubs/proposal/pt/04-scope.md](../pubs/proposal/pt/04-scope.md) |
| ww3_multi | "Program shell or driver to run the multi-grid wave model (uncoupled)" `(v)` | The two-way mosaic driver (`wm*` modules); out of the proposal's scope and refused by the phase-1 shim. | [course/05-nesting.md](../course/05-nesting.md), [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| ww3_ounf | "Post-processing of grid output to NetCDF files" from `out_grd.ww3` `(v)` | Gridded fields → `ww3.*.nc`; what `nccmp-tol` and `L2_replay.sh` compare. | [course/06-output.md](../course/06-output.md) |
| ww3_ounp | "Post-processing of point output" from `out_pnt.ww3` `(v)` | Point spectra → `ww3.*_spec.nc`; also feeds `ww3_bounc`. | [course/06-output.md](../course/06-output.md) |
| ww3_outf / ww3_outp | legacy "Post-processing of grid output" / "of point output" to ASCII `(v)` | Avoid; use the netCDF pair. | [course/00-orientation.md](../course/00-orientation.md) |
| ww3_bounc / ww3_bound | "Combines spectra files into a nest.ww3 file for boundary conditions" `(v)` (netCDF and legacy variants) | Nearest-point interpolation of parent spectra onto the child boundary. | [course/04-forcing.md](../course/04-forcing.md), [course/05-nesting.md](../course/05-nesting.md) |
| ww3_trnc / ww3_trck | "Convert direct access track output file to netCDF file" / "… to free-format readable sequential file" `(v)` | Track output post-processing (`track_o.ww3` → `track.nc`). | [course/06-output.md](../course/06-output.md) |
| ww3_gint | "Re-gridding binary output (out_grd.* files) to another grid" `(v)` | Grid interpolation of binary output. | [course/00-orientation.md](../course/00-orientation.md) |
| ww3_systrk | "Perform spatial and temporal tracking of wave systems, based on spectral partition (bulletin) output" `(v)` | Output type 6, "separated wave fields". | [course/06-output.md](../course/06-output.md) |
| ww3_uprstr | "Update the WAVEWATCH III restart files based on the significant wave height analysis from any data assimilation system" `(v)` | Restart update from an `Hs` analysis. | [course/00-orientation.md](../course/00-orientation.md) |
| ww3_gspl / ww3_grib / ww3_prtide | grid splitting for hybrid `ww3_multi` parallelisation / GRIB post-processing / tide prediction `(v)` | Programs in the tree the course does not use. | — |
| mod_def.ww3 | model definition file, written by `ww3_grid` and read/written by `W3IOGR` `(v)` `WW3/model/src/w3iogrmd.F90` | Opaque binary holding grid, spectral discretisation, timesteps and physics namelists; every other program reads it, and a stale one explains most confusing behaviour. | [course/00-orientation.md](../course/00-orientation.md) |
| restart.ww3 | restart file, read/written by `W3IORS` `(v)` `WW3/model/src/w3iorsmd.F90` | The complete model state (`VA`) for continuing a run. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| out_grd.ww3 / out_pnt.ww3 | raw gridded / point output read by `ww3_ounf` / `ww3_ounp` `(v)` | Binary intermediates between the model and the netCDF post-processors. | [course/06-output.md](../course/06-output.md) |
| nest.ww3 | boundary-condition file written by `ww3_bounc` `(v)` | Picked up automatically by `ww3_shel` if present; there is no flag. | [course/04-forcing.md](../course/04-forcing.md) |
| wind.ww3, current.ww3, ice.ww3, level.ww3 | binary forcing files written by `ww3_prnc` `(v)` [course/04-forcing.md](../course/04-forcing.md) | Interpolated forcing on the model grid. | [course/04-forcing.md](../course/04-forcing.md) |
| log.ww3 | the run log with the per-timestep input table `(v)` [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) | Where you see whether forcing ever arrived. | [course/01-build.md](../course/01-build.md) |
| mask.ww3 / mapsta.ww3 | ASCII land-sea mask (written under `O2`) / status map from `ww3_grid` `(v)` `WW3/manual/impl/switch.tex`, [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) | Open `mask.ww3` and look at it to check `IDLA`. | [course/03-grids.md](../course/03-grids.md) |
| param.scratch | the namelists `ww3_grid` actually used `(v)` [course/07-physics-choices.md](../course/07-physics-choices.md) | What the model is really running, as opposed to what you configured. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| .nml / .inp | Fortran namelist input (order-independent) / legacy fixed-format input (`$` comments) `(v)` [course/00-orientation.md](../course/00-orientation.md) | Both work in v7; everything here uses `.nml`, annotated templates live in `WW3/model/nml/`. | [course/00-orientation.md](../course/00-orientation.md) |
| namelists.nml | the physics-tuning file pointed at by `GRID%NML` (`&MISC`, `&SIN4`, `&SDS4`, `&PRO3`, …) `(v)` `WW3/model/nml/namelists.nml`, [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) | Read by `ww3_grid`; blocks irrelevant to the switch are skipped silently. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| &SPECTRUM_NML, &RUN_NML, &TIMESTEPS_NML, &GRID_NML, &DEPTH_NML, &MASK_NML, &OBST_NML | the `ww3_grid.nml` blocks in dependency order `(v)` [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) | Spectral grid, term flags (`FLCX/FLCY/FLCTH/FLCK/FLSOU`), timesteps, grid, bathymetry, mask, obstructions. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| DTMAX / DTXY / DTKTH / DTMIN | "maximum global time step" / "maximum CFL time step for x-y" / "maximum CFL time step for k-th" / "minimum source term time step" (s) `(v)` `WW3/model/nml/ww3_grid.nml` | The four timesteps; the template's rule of thumb is `DTXY ≈ 0.9·Tcfl`, `DTMAX ≈ 3·DTXY`, `DTKTH ≈ DTMAX/2`, `DTMIN ≈ 10`. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| IDLA / IDFM | layout indicator (1 = line-by-line bottom to top, 3 = top to bottom, 2/4 single read) / format indicator (free, fixed, unformatted) `(v)` `WW3/model/nml/ww3_grid.nml` | Which way is up in an ASCII array; the classic upside-down-continent error. | [course/03-grids.md](../course/03-grids.md) |
| SF | scale factor multiplied into the values read (`value <= scale_fac * value_read + add_offset`) `(v)` `WW3/model/nml/ww3_grid.nml` | Set `DEPTH%SF = -1.` for a file of positive depths. | [course/03-grids.md](../course/03-grids.md) |
| INBND_COUNT / INBND_POINT | input boundary points, `INBND_POINT(n) = i j CONNECT` `(v)` `WW3/model/nml/ww3_grid.nml` | Where boundary spectra enter; points must be strictly inside the grid and `CONNECT = T` fills the segment. | [course/05-nesting.md](../course/05-nesting.md) |
| UGOBCFILE | open-boundary list file for unstructured grids `(v)` [course/03-grids.md](../course/03-grids.md) | Used by regtest `ww3_tp2.7`. | [examples/README.md](../examples/README.md) |
| &DOMAIN_NML, &INPUT_NML, &OUTPUT_TYPE_NML, &OUTPUT_DATE_NML | the `ww3_shel.nml` blocks `(v)` [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) | Run window, forcing flags (`'T'` file, `'H'` homogeneous, `'C'` coupler), what and when to output. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md) |
| HOMOG_COUNT / HOMOG_INPUT | homogeneous (constant) forcing without data files, e.g. `WND` "defined by speed, direction and airseatemp" `(v)` `WW3/model/nml/ww3_shel.nml` | How example 01 gets its 10 m/s wind. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/06-output.md](../course/06-output.md) |
| FIELD%TYPE | netCDF packing, `[2 = SHORT, 3 = it depends, 4 = REAL]`, template default 3 `(v)` `WW3/model/nml/ww3_ounf.nml` | Use 4 while learning; `ncdump` never unpacks type 2. | [course/06-output.md](../course/06-output.md) |
| FIELD%TIMESPLIT / FIELD%PARTITION / FIELD%LIST | file chunking `[0,4,6,8,10]` / partitions to write `'0 1 2 3'` / field names `(v)` `WW3/model/nml/ww3_ounf.nml` | Output-file layout controls. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/06-output.md](../course/06-output.md) |
| NFGRIDS / WINDLINE / ICELINE / CURRLINE / UNIPOINTS / WW3GRIDLINE | `ww3_multi` configuration lines in the UFS documentation `(v)` [course/05-nesting.md](../course/05-nesting.md) | The multi-grid input structure; `UNIPOINTS` unifies point output across grids. | [course/05-nesting.md](../course/05-nesting.md) |
| WW3_DATA / ww3_from_ftp.sh | the NOAA FTP data bundle and the script that fetches it `(v)` [course/01-build.md](../course/01-build.md), `WW3/model/bin/ww3_from_ftp.sh` | The binary half of the package needed by most regtests. | [course/01-build.md](../course/01-build.md) |

## WW3 modules, routines and regtests

Expansions are the `@brief` or `1. Purpose` header of the routine in `WW3/model/src/` at
7.14 `develop` `(v)`; line numbers where given are from [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md)
and [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md).

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| W3SNL1MD / w3snl1md.F90 | "Bundles routines to calculate nonlinear wave-wave interactions according to the Discrete Interaction Approximation (DIA) of Hasselmann et al. (JPO, 1985)" `(v)` | The `NL1` module: `INSNL1`, `W3SNL1` and `W3SNLGQM`. | [course/07-physics-choices.md](../course/07-physics-choices.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| W3SNL1 | "Calculate nonlinear interactions and the diagonal term of its derivative" `(v)`, `w3snl1md.F90:115-473` | The DIA kernel, `(A, CG, KDMEAN, S, D)` per sea point; the first routine ported. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| INSNL1 | "Preprocessing for nonlinear interactions (weights)" `(v)`, `w3snl1md.F90:483-786` | Builds the quadruplet address tables (`IP11…IM42`, `IC11…IC82`), weights (`AWG1..8`, `SWG1..8`) and `AF11` ("scaling array f**11", `w3adatmd.F90` `(v)`) once per grid. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| W3SNLGQM | Snl by the Gaussian Quadrature Method of Lavrenov (2001), adapted from TOMAWAC `(v)` | The `IQTPE <= 0` branch at the same call site; not replaced by the port. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3SNL2 / W3SNL3 / W3SNL4 / W3SNL5 | interface to exact interactions / multiple DIA for arbitrary depths / TSA / GKE (`CalcQRSNL`) `(v)` | The other Snl modules, out of scope. | [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| W3SRCEMD / W3SRCE | "Source term integration routine" `(v)`, `w3srcemd.F90:190` | The per-point driver that sums Sin + Snl + Sds + …, sub-steps dynamically and applies the limiter; the profile's top routine and the OpenACC target of Ikuyajolu et al. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3SRC4MD: W3SPR4 / W3SIN4 / W3SDS4 | "SHOM/Ifremer source terms" after Janssen and Ardhuin et al. (2009, 2010): mean parameters / "diagonal and input source term for WAM4+ approach" / "wave dissipation source term" `(v)` | The `ST4` package, the next port candidate. | [course/07-physics-choices.md](../course/07-physics-choices.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| W3SRC6MD: W3SIN6 / W3SDS6 | observation-based input after Donelan et al. (2006) and dissipation after Babanin et al. (2010), Rogers et al. (2012) `(v)` | The `ST6` package. | [course/07-physics-choices.md](../course/07-physics-choices.md) |
| W3SLN1, W3SBT1, W3SDB1, W3STR1, W3SIC1…5, W3REF1 | linear input / JONSWAP bottom friction / Battjes–Janssen breaking / LTA triads / ice source terms / shoreline reflection `(v)` module headers | The remaining per-point source-term routines, "trivial once `W3SRCE` exists" in the ranked list. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| W3FLX2MD / W3FLX2 | "Flux/stress computations according Tolman and Chalikov (1996)" `(v)` | The `FLX2` friction-velocity routine, `ST2`'s companion. | [course/01-build.md](../course/01-build.md) |
| W3PRO2MD / W3PRO3MD | "third order propagation scheme" modules (`PR2` / `PR3`) `(v)` | Hold `W3XYP2`/`W3KTP2` and `W3XYP3`/`W3KTP3`. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| W3XYP2 / W3XYP3 | "Propagation in physical space for a given spectral component" `(v)` | Spatial advection per spectral bin, the memory-bound bucket. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3KTP2 / W3KTP3 | "Propagation in spectral space" `(v)` | Refraction and frequency shift per point over (θ, k). | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| W3UQCKMD / W3QCK1 / W3QCK2 / W3QCK3 (W3UQCK*) | "Portable ULTIMATE QUICKEST schemes": 1-D propagation on a regular grid / with variable spacing / with cell transparencies `(v)` | The `UQ` sweeps called by `W3XYP*`. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| W3WAVEMD / W3WAVE | "Run WAVEWATCH III for a given time interval" `(v)`, `w3wavemd.F90:229` | The main time loop: forcing update → propagation → source terms → output; slices `CG(1:NK,ISEA)` into `W3SRCE`. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3GATH / W3SCAT | "Gather spectral bin information into a propagation field array" / "Scatter data back to spectral storage after propagation" `(v)` | The MPI transposes between the point and spectral decompositions; the communication bucket of the profile. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3INITMD / W3INIT | "Initialize WAVEWATCH III" `(v)`, `w3initmd.F90:163` | Where the Kokkos runtime and DIA tables would be set up (after `W3IOGR('READ')` at line 735). | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3MPII | "Perform initializations for MPI version of model. Data transpose only" `(v)` | MPI set-up in `w3initmd.F90`. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| W3IOGRMD / W3IOGR | "Reading and writing of the model definition file" `(v)` | Reads `mod_def.ww3`; also calls `INSNL1` (`w3iogrmd.F90:1802`). | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3IOGOMD / W3OUTG / W3IOGO | "Gridded output of mean wave parameters": fill output arrays / read-write `out_grd.ww3` `(v)` | Integral parameters (`Hs`, `Tp`, …) from the spectrum; ranked #8 for the port. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3IOPOMD / W3IOPO, W3IORSMD / W3IORS, W3IOBCMD / W3IOBC, W3IOTRMD / W3IOTR | point output / restart files / boundary conditions / track output read-write `(v)` | The I/O bucket (`w3io*`) of the profile. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| W3PARTMD / W3PART | "Spectral partitioning according to the watershed method" / "Interface to watershed partitioning routines" `(v)` | The wind-sea/swell partition behind `PHS`, `PTP`, `PDIR`. | [course/06-output.md](../course/06-output.md) |
| W3UPDTMD (W3UPDT*): W3UWND / W3UCUR / W3ULEV / W3UICE / W3UINI / W3UTRN | "Bundles all input updating routines": interpolate wind / current / water level / ice map / fetch-limited initial field / cell transparencies `(v)` | Forcing interpolation, cheap but the trigger of host→device copies. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3GDATMD | "Define data structures to set up wave model grids and aliases to use individual grids transparently" `(v)` | Grid data: `NK`, `NTH`, `NSPEC`, `XFR`, `SIG(0:NK+1)`, `DTH`, the DIA constants `LAM`, `SNLC1`, `KDCON`, `KDMN`, `SNLS1..3`, `FACHFE`, `IQTPE`; the largest source of hidden global state. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| W3NMOD / W3DIMS / W3SETG | "Set up the number of grids to be used" / "Initialize an individual spatial grid at the proper dimensions" / "Select one of the WAVEWATCH III grids / models" `(v)` `w3gdatmd.F90` | The pointer-swap idiom that retargets module-level `SIG`, `NK`, … per grid (`IMOD`). | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| W3ADATMD | "Define data structures to set up wave model auxiliary data for several models simultaneously" `(v)` | Holds `CG(0:NK+1,0:NSEA)`, `WN`, and the DIA tables `IP11..AF11`. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3NAUX / W3DIMA / W3SETA / W3DMNL | number of grids / dimension an auxiliary grid / select a grid / "Initialize an individual data grid at the proper dimensions (DIA)" `(v)` `w3adatmd.F90` | `W3DMNL` is the allocation `INSNL1` section 6 calls; `snl1_ref.F90` replaces it with the `ALLOCATE` it performs. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| W3WDATMD (W3NDAT / W3SETW) | "wave model dynamic data": the spectra `VA` and `TIME` `(v)` | The model state proper. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3ODATMD (W3NOUT / W3SETO) | data structures for model output, including the unit numbers `NDSE`/`NDSO` `(v)` | Where `PATCH.md` imports `NDSE` from. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3IDATMD | "wave model input data for several models" `(v)` | Forcing input arrays. | — |
| CONSTANTS / constants.F90 | "Define some much-used constants for global use (all defined as PARAMETER)" `(v)` | `GRAV`, `PI`, `TPI`, `TPIINV`, `DWAT`, `DAIR`, `RADIUS`, … all default `REAL`. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| W3SERVMD / EXTCDE / STRACE | service routines: "Perform a program stop with an exit code" / "Keep track of entered subroutines" `(v)` | `EXTCDE(1)` is the abort the patched caller must issue on a shim error; `STRACE` is the `S`-switch tracer. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| W3DISPMD / DISTAB / WAVNU1 / WAVNU3 | dispersion-relation routines: fill interpolation tables / "wavenumber and group velocity from the interpolation array" / "from the improved Eckard's formula by Beji (2003)" `(v)` | How WW3 gets `WN` and `CG` from σ and depth. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| W3PROFSMD / w3profsmd_pdlib.F90 | "Propagation schemes for unstructured grids using fluctuation splitting" / its PDLIB, implicit version `(v)` | Unstructured propagation, deferred. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| W3PARALL / W3TRIAMD / W3GSRUMD / W3CSPCMD / W3FLDSMD / W3TIMEMD | parallel routines for the implicit solver / unstructured grid reader / grid search & regrid utilities / spectral-grid conversion / forcing field I/O / time utilities `(v)` | Supporting modules named in the H100 plan's repository map. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| wm*md.F90 (WMESMFMD, WMSCRPMD, …) | `ww3_multi` counterparts: ESMF interface module, SCRIP remapping, … `(v)` | Dropped from the port's scope. | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md) |
| VA, VS/VD, SPEC, VSNL/VDNL, CG1, WNMEAN, DEPTH, ISEA/JSEA, IMOD | storage array for spectra / source and diagonal / the point's spectrum inside `W3SRCE` / DIA outputs / `CG(1:NK,ISEA)` / mean wavenumber / depth / global and local sea-point index / grid number `(v)` `w3wdatmd.F90`, [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) | The variable names that appear in the patch hunks. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| regtests/ | WW3's 62 regression cases (`ww3_tp1.*`, `ww3_tp2.*`, `ww3_ts*`, `ww3_tc*`, `ww3_tic*`, `ww3_ta*`, `mww3_test_*`, `ww3_ufs*`) `(v)` [examples/README.md](../examples/README.md), `WW3/regtests/` | The best example collection there is; each has an `info` file and `input/`. | [examples/README.md](../examples/README.md), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| ww3_tp1.1 | "one-dimensional propagation, full global propagation along the equator" `(v)` `WW3/regtests/ww3_tp1.1/info` | The ~30 s sanity regtest `just rt` runs by default; no source terms. | [README.md](../README.md), [course/01-build.md](../course/01-build.md) |
| ww3_tp2.2 | "two-dimensional propagation, propagation over half the globe" `(v)` | The standard "did my build work" test. | [examples/README.md](../examples/README.md) |
| ww3_tp2.5 | "two-dimensional propagation, Arctic region on polar stereographic grid" `(v)` | Named in `AGENTS_KOKKOS` as an L2 candidate family (`ww3_tp2.x`). | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| ww3_tp2.7 / ww3_tp2.17 | unstructured grid with reflection / unstructured grid, card-deck vs domain decomposition, explicit vs implicit `(v)` | The unstructured and open-boundary references. | [course/03-grids.md](../course/03-grids.md), [course/05-nesting.md](../course/05-nesting.md) |
| ww3_ts1 | "source term integration in homogeneous conditions (1-point model)" `(v)` | The smallest regtest that exercises `W3SNL1`; the default of `just l2`. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [kokkos/tests/L2_replay.sh](../kokkos/tests/L2_replay.sh) |
| ww3_tc1 | "365_day and 360_day calendar" test with minimal homogeneous forcing `(v)` | Its `ww3_shel.nml` is the minimal example worth reading. | [examples/README.md](../examples/README.md) |
| ww3_ufs1.1 | "1deg global structured" UFS configuration `(v)` | The profiling target in `AGENTS_KOKKOS` §1.6. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| mww3_test_01…09 | multi-grid (`ww3_multi`) tests, e.g. "expanded status map (wetting and drying)" `(v)` | Reference material for two-way nesting. | [course/05-nesting.md](../course/05-nesting.md) |
| regtests/unittests | five I/O unit-test programs run via CTest (since 2023) `(v)` `WW3/regtests/unittests/` | The only unit tests WW3 has. | [pubs/proposal/pt/05-justification.md](../pubs/proposal/pt/05-justification.md) |
| matrix.base | generator of the regression-matrix run script from a list of options `(v)` `WW3/regtests/bin/matrix.base` | Lists `rstrt_b4b`, `nth_b4b`, `npl_b4b` among its options. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| matrix.comp | "Compare output of matrix for two model versions" file by file with `cmp`/`diff` `(v)` `WW3/regtests/bin/matrix.comp` | Sorts cases into identical / non-identical; no tolerance anywhere. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| test.comp | "Compares output of two runs to check b4b reproducibility (i.e. for mpi, thread, restart) with the same version of the code" `(v)` `WW3/regtests/bin/test.comp` | The per-test comparator behind the `*_b4b` variants. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| rstrt_b4b / nth_b4b / npl_b4b | "Restart Reproducibility" / "Thread Reproducibility" / "MPI task Reproducibility" `(v)` `WW3/regtests/bin/matrix.base` | Matrix variants that run a case twice (restarted, different thread count, different rank count) and demand identical output. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| b4b | bit-for-bit `(v)` `WW3/regtests/bin/test.comp` | The reproducibility criterion of steps 1–2 of the ladder; "bit-identical" in the L1 column means the same thing. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| develop / 7.14 | WW3's development branch and the version string of the pinned `WW3/` submodule `(v)` [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) | Where real work happens; the 6.07.1 release tarball is from 2019. | [course/01-build.md](../course/01-build.md) |

## HPC and software

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| MPI | Message Passing Interface `(v)` `WW3/manual/impl/switch.tex` | Distributed-memory parallelism; Open MPI 5.0.10 is pinned, and Fortran owns `MPI_Init`. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| OpenMP | ⚠ Open Multi-Processing (`\omp` = "OpenMP" in `WW3/manual/defs.tex`) | Directive-based threading (`OMPG`/`OMPH`) and one of the Kokkos backends; `OMP_NUM_THREADS`/`OMP_PROC_BIND` must be set explicitly. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [kokkos/CMakeLists.txt](../kokkos/CMakeLists.txt) |
| OpenACC | ⚠ Open Accelerators directive standard | `!$acc` offload directives, used by Ikuyajolu et al. (2023) on `W3SRCEMD` and by `gpu/00_hello_acc.f90`; the contrast that motivates Kokkos. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md), [gpu/README.md](../gpu/README.md) |
| CUDA / CUDA Fortran | ⚠ NVIDIA's GPU programming platform | The Kokkos device backend in `#cuda`; CUDA Fortran (`-cuda`, `attributes(global)`) is the `nvfortran` route. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md), [gpu/README.md](../gpu/README.md) |
| nvcc / nvcc_wrapper | NVIDIA's CUDA C++ compiler and Kokkos' wrapper script that CMake must use as `CMAKE_CXX_COMPILER` `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | Compiles the C++ kernels for the GPU; rejects an extended lambda inside a private member function. | [kokkos/README.md](../kokkos/README.md) |
| nvfortran | NVIDIA's Fortran compiler in the free HPC SDK `(v)` [README.md](../README.md) | Does CUDA Fortran, OpenACC, OpenMP target and `do concurrent` offload; used as a CPU stepping stone here. | [course/01-build.md](../course/01-build.md), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| cc89 / sm_89 / compute capability 8.9 | the RTX 4090's (Ada) architecture target, `-gpu=cc89` for `nvfortran` `(v)` [README.md](../README.md), `CMAKE_CUDA_ARCHITECTURES=89` for Kokkos `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | `sm_89` is ⚠ nvcc's spelling of the same target (not used in the repo); `cc90` is the H100. | [gpu/README.md](../gpu/README.md) |
| ADA89 / HOPPER90 | `Kokkos_ARCH_ADA89` (RTX 4090) / `HOPPER90` (H100) Kokkos architecture keys `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | The pinned CUDA Kokkos is built for ADA89 only; an H100 needs a Kokkos rebuild in `nix-config`. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| H100 / RTX 4090 | NVIDIA data-centre (Hopper) and consumer (Ada) GPUs `(v)` [pubs/proposal/pt/04-scope.md](../pubs/proposal/pt/04-scope.md), [README.md](../README.md) | The proposal's target and the owner's workstation card; the 4090 runs FP64 at 1/64 of FP32. | [bench/README.md](../bench/README.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| PCIe / NVLink | ⚠ Peripheral Component Interconnect Express / NVIDIA's GPU interconnect (not expanded in the repo) | The host↔device link whose bandwidth bounds every phase-1 port; consumer cards have PCIe only. | [README.md](../README.md), [bench/README.md](../bench/README.md) |
| NVMe | ⚠ Non-Volatile Memory Express (not expanded in the repo) | Fast local storage named among today's lab-machine features. | [pubs/proposal/pt/05-justification.md](../pubs/proposal/pt/05-justification.md) |
| FP32 / FP64 / float32 | single / double precision; WW3's default `REAL` is 4 bytes, `ww::Real` is `float` `(v)` [kokkos/src/ww_kokkos/real.hpp](../kokkos/src/ww_kokkos/real.hpp) | The port keeps `float` state and accumulates in `double`. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| FMA / `-ffp-contract` / `--fmad` | fused multiply-add and the GCC / nvcc flags that control contraction `(v)` [kokkos/README.md](../kokkos/README.md) | One rounding where the Fortran does two; `ww_kokkos` builds with `-ffp-contract=off` and `--fmad=false` for bit parity. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| ULP | ⚠ unit in the last place (not expanded in the repo) | The granularity of float rounding; the budget of a bit-parity test. | [kokkos/src/ww_kokkos/snl1_config.hpp](../kokkos/src/ww_kokkos/snl1_config.hpp) |
| `powi()` / `pow11()` | integer-power helpers that reproduce gfortran's multiplication chain for `x**n` `(v)` [kokkos/README.md](../kokkos/README.md) | Why the port is bit-exact rather than "close": `x**n` is not `std::pow`. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| Kokkos | ⚠ the Sandia-led C++ performance-portability library (not expanded; 5.2.0 pinned `(v)` [kokkos/README.md](../kokkos/README.md)) | One kernel source runs on Serial, OpenMP or CUDA backends chosen at build time; the abstraction WW4 proposes too. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| View | `Kokkos::View<T*, Layout, MemorySpace>`, a reference-counted multidimensional array handle with a label `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | The Kokkos equivalent of `REAL, ALLOCATABLE :: A(:,:)`; an *unmanaged* View wraps caller-owned memory. | [kokkos/intro/01_views.cpp](../kokkos/intro/01_views.cpp) |
| LayoutLeft / LayoutRight | column-major (Fortran) / row-major (C) index order as part of the View type `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | `LayoutLeft` at the Fortran boundary; `deep_copy` between layouts is a silent transpose. | [kokkos/intro/04_layouts_and_mirrors.cpp](../kokkos/intro/04_layouts_and_mirrors.cpp) |
| execution space / memory space | where a kernel runs (Serial, OpenMP, CUDA) / where memory lives (`HostSpace`, `CudaSpace`) `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | `DeviceSpace = DefaultExecutionSpace::memory_space` is spelled out on every View. | [kokkos/src/ww_kokkos/snl1_config.hpp](../kokkos/src/ww_kokkos/snl1_config.hpp) |
| mirror / `deep_copy` / `create_mirror_view` | a host View with the same layout as a device View, and the explicit copy between them `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | A no-op on a host backend, a PCIe transfer on CUDA. | [kokkos/intro/01_views.cpp](../kokkos/intro/01_views.cpp) |
| parallel_for / parallel_reduce / parallel_scan | Kokkos' loop, reduction and prefix-sum patterns `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | The equivalents of `DO`, `SUM`/`MAXLOC` and a prefix sum; reductions use reducers such as `Kokkos::MaxLoc`, never atomics. | [kokkos/intro/02_parallel_for.cpp](../kokkos/intro/02_parallel_for.cpp), [kokkos/intro/03_reduce_and_scan.cpp](../kokkos/intro/03_reduce_and_scan.cpp) |
| RangePolicy / MDRangePolicy / TeamPolicy | 1-D, multi-dimensional (`Rank<2>`) and team-of-threads execution policies `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | The DIA uses `TeamPolicy(npts, Kokkos::AUTO)`: one team per sea point. | [kokkos/src/ww_kokkos/snl1_dia.cpp](../kokkos/src/ww_kokkos/snl1_dia.cpp) |
| scratch / `team_scratch(0)` / `TeamThreadRange` / `team_barrier` | per-team level-0 scratch memory (shared memory on a GPU), the intra-team loop, and the barrier between writing and reading scratch `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | Replaces `W3SNL1`'s ten automatic arrays; ~28 KB for `NK=25, NTH=24`. | [kokkos/intro/05_team_scratch.cpp](../kokkos/intro/05_team_scratch.cpp) |
| fence / `Kokkos::fence()` | wait for all outstanding device work `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | Required before the host reads a result, before a mirror dies, before a timer; not to be sprinkled. | [kokkos/intro/06_interop_bindc.cpp](../kokkos/intro/06_interop_bindc.cpp) |
| KOKKOS_LAMBDA / KOKKOS_INLINE_FUNCTION | the capture-by-value lambda macro and the host+device function annotation `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | What a kernel body may be made of: Views, scalars and inline helpers, no host memory. | [kokkos/src/ww_kokkos/spectrum_fixtures.hpp](../kokkos/src/ww_kokkos/spectrum_fixtures.hpp) |
| ScopeGuard / `Kokkos::initialize` / `finalize` / `push_finalize_hook` | RAII runtime lifetime, once per process; a hook that drops Views before `finalize` `(v)` [kokkos/README.md](../kokkos/README.md) | The shim finalises Kokkos only if it started it. | [kokkos/src/fortran_iface/snl1_shim.cpp](../kokkos/src/fortran_iface/snl1_shim.cpp) |
| `Kokkos_ENABLE_DEBUG_BOUNDS_CHECK` | a Kokkos *build* option, fixed by the pinned derivation `(v)` [kokkos/README.md](../kokkos/README.md) | Cannot be switched on from the presets; sanitizers are the net. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| RAII | ⚠ Resource Acquisition Is Initialization (not expanded in the repo) | The C++ ownership discipline both WW4's `AGENTS.md` and this repo mandate: no raw `new`/`delete`. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md), [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `bind(C)` / ISO_C_BINDING / `extern "C"` | Fortran 2003 C interoperability (`BIND(C, NAME=…)`, `c_int`, `c_float`, `VALUE`) and its C++ side `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md), [kokkos/src/fortran_iface/w3kokkosmd.F90](../kokkos/src/fortran_iface/w3kokkosmd.F90) | The only boundary between WW3 and the kernels: assumed-size arrays, no mangling, nothing throws. | [kokkos/src/fortran_iface/ww_kokkos_c.hpp](../kokkos/src/fortran_iface/ww_kokkos_c.hpp) |
| `do concurrent` | Fortran 2008 loop whose iterations are independent, with Fortran 2018 `LOCAL()` locality `(v)` [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) | One source, three targets with `nvfortran -stdpar=gpu`; the standard-Fortran route to parallelism. | [gpu/README.md](../gpu/README.md) |
| `CONTIGUOUS` / `INTENT` / `PURE` / explicit interface | modern-Fortran attributes of the refactoring checklist `(v)` [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) | What makes a routine replaceable by a kernel; `PURE` is blocked on `W3SNL1` by the `W3_T` `WRITE`. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| automatic array | a local array sized at run time from module variables, e.g. `UE(1-NTH:NSPECY)` `(v)` [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) | The per-call stack workspace the port moves into team scratch. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `-pg` / gprof | GCC profiling instrumentation and the GNU profiler `(v)` [kokkos/tools/profile/README.md](../kokkos/tools/profile/README.md) | `gprof_table.sh` rebuilds WW3 with `-pg` into `build-pg` and reduces `gprof -b` to a phase table. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| perf | the Linux kernel's sampling profiler (`perf record -g`) `(v)` [kokkos/tools/profile/README.md](../kokkos/tools/profile/README.md) | Not in the pratico shell; `perf_table.sh` exits 3 without it. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| wall-clock | elapsed real time, "seconds per forecast hour of `ww3_shel`" `(v)` [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) | The proposal's metric (*tempo de execução*); measured with `SYSTEM_CLOCK`, never `CPU_TIME`. | [bench/README.md](../bench/README.md), [pubs/proposal/pt/03-theme.md](../pubs/proposal/pt/03-theme.md) |
| ASan / UBSan | AddressSanitizer / UndefinedBehaviorSanitizer, `-fsanitize=address,undefined` `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | Turned on by the `serial-debug` preset. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| GoogleTest / GTest | Google's C++ test framework, 1.18.0 pinned `(v)` [kokkos/README.md](../kokkos/README.md) | The framework of the `L1_test_*` suites and of WW4's tests. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) |
| L1 / L2 / L3 / L4 | WW4's test levels: unit tests of single functions (`L1_test_*`) / integration tests of modules and interfaces (`L2_test_*`) / functional physics cases / full-model regression `(v)` [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) | This repo's `L1_*` are per-kernel tests against captured Fortran and `L2_*` replay a whole regtest `(v)` [kokkos/README.md](../kokkos/README.md); L3/L4 do not exist yet in WW4. | [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| CTest | CMake's test runner (`ctest --test-dir …`) `(v)` [justfile](../justfile) | Registers every intro program and test suite. | [kokkos/README.md](../kokkos/README.md) |
| CMake presets | `CMakePresets.json` configure/build/test presets `serial-debug`, `openmp-release`, `cuda-release` (schema 10, CMake ≥ 4.1) `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | Choose build type and instrumentation; the backend comes from the shell. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| Ninja | the build generator the presets use `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | Also recommended by SWAN's implementation manual. | [course/15-swan.md](../course/15-swan.md) |
| Nix / flake / devShell / `nix develop` | the reproducible toolchain: a locked nixpkgs revision exposed as development shells `(v)` [README.md](../README.md), [flake.nix](../flake.nix) | `nix develop ./nix-config/labs/pratico#ww3` (Serial+OpenMP Kokkos) and `#cuda` (CUDA); the root `flake.nix` builds the PDFs. | [course/01-build.md](../course/01-build.md), [kokkos/README.md](../kokkos/README.md) |
| pratico | `nix-config/labs/pratico`, the pinned WW3 toolchain flake (gfortran 15.3, Open MPI 5.0.10, netCDF, Kokkos, GoogleTest, CMake 4.4.2) `(v)` [README.md](../README.md) | A sparse git submodule; `just ww3` enters its `#ww3` shell. | [justfile](../justfile), [course/01-build.md](../course/01-build.md) |
| publisher | `nix-config/labs/publisher`, the pandoc/xelatex toolchain flake for the course book and proposal PDFs `(v)` [flake.nix](../flake.nix) | What `just book`, `just proposal` and `just pubs` run inside. | [README.md](../README.md) |
| just / justfile | the task runner and its recipe file; `just` lists every task `(v)` [README.md](../README.md) | Thin wrappers over `scripts/`; `just rt`, `just build`, `just kokkos-test`, `just l2`, `just profile`, `just nccmp`. | [justfile](../justfile) |
| CI | `.github/workflows/ci.yml` `(v)` [README.md](../README.md) | Syntax checks, the Fortran sandbox, markdown link check, the Kokkos serial/OpenMP presets; does not build WW3. | [README.md](../README.md) |
| NetCDF / netCDF-4 / HDF5 | ⚠ Network Common Data Form (not expanded in the repo); netcdf-c 4.10.1, netcdf-fortran 4.4.6, HDF5 1.14.6 pinned `(v)` [course/01-build.md](../course/01-build.md) | The output format of `ww3_ounf`/`ww3_ounp` and the input of `ww3_prnc`; the Fortran bindings (`netcdf.mod`) are compiler-specific. | [course/06-output.md](../course/06-output.md) |
| nc-config / nf-config | the netCDF-C and netCDF-Fortran configuration scripts; `nc-config --has-nc4` `(v)` `WW3/model/bin/README.md`, `nf-config --prefix` `(v)` [course/01-build.md](../course/01-build.md) | How to find the library when CMake cannot (`NetCDF_ROOT`). | [course/01-build.md](../course/01-build.md) |
| ncdump | netCDF's header/data dumper, ships with netcdf-c `(v)` [course/06-output.md](../course/06-output.md) | The zero-dependency way to read output; it never unpacks scaled shorts. | [course/04-forcing.md](../course/04-forcing.md) |
| `_FillValue` | the netCDF attribute marking never-written cells `(v)` [kokkos/tools/nccmp-tol/README.md](../kokkos/tools/nccmp-tol/README.md) | Cells equal to it (or NaN) are excluded by `nccmp-tol`. | [course/06-output.md](../course/06-output.md) |
| NCO / ncks / ncap2 / ncdiff | the netCDF Operators: subsetting (`ncks -v hs -d time,-1`), arithmetic (`ncap2`), differencing (`ncdiff`) `(v)` [course/06-output.md](../course/06-output.md), [course/04-forcing.md](../course/04-forcing.md) | In the pinned toolchain (nco 5.3.2) `(v)`; `get_gfs.sh` requires `ncap2`. `just toolchain` does not print it. | [course/03-grids.md](../course/03-grids.md) |
| nccmp | the C tool that compares netCDF files bit for bit `(v)` [course/06-output.md](../course/06-output.md) | Not `nccmp-tol`: it knows nothing about tolerances. | [course/06-output.md](../course/06-output.md) |
| cdo | Climate Data Operators; cdo 2.5.1 in the pinned toolchain `(v)` | `cdo mergetime` is how `get_gfs.sh` concatenates the GRIB forecast hours before `grib_to_netcdf`. | [course/04-forcing.md](../course/04-forcing.md) |
| ecCodes / grib_to_netcdf / grib_copy / grib_ls | ECMWF's GRIB library (2.48.0 pinned) and its command-line tools `(v)` [course/01-build.md](../course/01-build.md), [course/04-forcing.md](../course/04-forcing.md) | GRIB → netCDF for the GFS winds; renames `10u`/`10v` to `u10`/`v10` ⚠. | [course/04-forcing.md](../course/04-forcing.md) |
| GRIB / GRIB2 | ⚠ GRIdded Binary, the WMO meteorological format (not expanded in the repo) | What NOMADS serves; the `NOGRB`/`NCEP2` switches concern GRIB *output*. | [course/04-forcing.md](../course/04-forcing.md) |
| GFS / GEFS | ⚠ Global Forecast System / Global Ensemble Forecast System (NOAA; not expanded in the repo) | NOAA's operational 0.25° winds used by example 02; 3-hourly forecast output. | [course/04-forcing.md](../course/04-forcing.md), [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md) |
| NOMADS | ⚠ NOAA Operational Model Archive and Distribution System (not expanded in the repo) | Serves GFS GRIB2; `filter_gfs_0p25.pl` subsets it by box and variable. | [course/04-forcing.md](../course/04-forcing.md) |
| ERA5 / CDS | ECMWF's reanalysis and the Copernicus Climate Data Store that serves it `(v)` [course/04-forcing.md](../course/04-forcing.md) | The better hindcast product, not used because fetching it needs the Python `cdsapi` client. | [course/08-python.md](../course/08-python.md) |
| GEBCO | ⚠ General Bathymetric Chart of the Oceans (not expanded in the repo) | 15-arc-second global bathymetry, `elevation` negative below sea level, sampled by example 02. | [course/03-grids.md](../course/03-grids.md) |
| gridgen / genes_gmd / OceanMesh2D / GMSH / SMS | NOAA-EMC's MATLAB grid and obstruction generator / GMD coefficient fitter / unstructured mesh generators `(v)` [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md) | Grid-preparation tools outside this repo. | [course/03-grids.md](../course/03-grids.md), [course/07-physics-choices.md](../course/07-physics-choices.md) |
| pyww3 / WW3-tools / wavespectra / ww3tool / bmi-wavewatch3 / rompy | the Python ecosystem: namelist dataclasses + runner / NOAA's validation toolkit / xarray spectral library / … `(v)` [course/08-python.md](../course/08-python.md) | Known and deliberately not depended on; `WW3-tools` remains the right validation tool. | [course/08-python.md](../course/08-python.md) |
| Ollama / llm / `TRANSLATE_*` | the local model shell (`just ask`, `just pull`) and the translation backend variables `(v)` [justfile](../justfile), [README.md](../README.md) | Used only by `scripts/translate_md.py`, never by lab code. | [README.md](../README.md) |
| pandoc / xelatex / ABNT / IEEE | the PDF pipeline and its two citation styles (`just book abnt`) `(v)` [justfile](../justfile) | Builds `pubs/` into `pdf/`. | [README.md](../README.md) |
| Mermaid | the diagram syntax GitHub renders in Markdown `(v)` [pubs/proposal/mapas-mentais.pt.md](../pubs/proposal/mapas-mentais.pt.md) | The proposal's mind maps and the lesson-13 flowcharts. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| SPDX | ⚠ Software Package Data Exchange licence identifiers | `SPDX-License-Identifier: MIT` on the tooling and `LGPL-3.0-or-later` on the translated kernels `(v)` [kokkos/README.md](../kokkos/README.md). | [kokkos/README.md](../kokkos/README.md) |
| LLM / coding agent / Jules / Copilot | large-language-model assistants; WW4's `AGENTS.md` names Jules and Copilot `(v)` [course/14-ww4-and-the-future.md](../course/14-ww4-and-the-future.md) | The typist of lesson 12 and the subject of lesson 13; the FESOM2 port used one under expert direction. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| P-cores / E-cores | Intel's performance and efficiency cores on an i9 `(v)` [bench/README.md](../bench/README.md) | Why `--bind-to core --cpu-set 0-15` beats unpinned ranks on a memory-bound model. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| `SYSTEM_CLOCK` / `CPU_TIME` | Fortran intrinsics for wall time / summed CPU time `(v)` [bench/README.md](../bench/README.md) | Use the first; the second sums across threads and makes threading look like a slowdown. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| Amdahl | ⚠ Amdahl's law (not expanded in the repo) | The serial-fraction ceiling that bounds per-member GPU gains and why `ww3_grid` is excluded from timings. | [bench/README.md](../bench/README.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| Unified / Managed Memory | CUDA memory that migrates between host and device implicitly `(v)` [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) | What `-stdpar=gpu` uses and what the agent rules forbid as a way of avoiding transfer design. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |

## This repository's own names

| Term | Expansion | What it is | Where it appears |
|---|---|---|---|
| ww3-gpu (ex ww-lab) | the GitHub repository `h0ffmann/ww3-gpu` `(v)` [README.md](../README.md); the `justfile` header and `flake.nix` still say `ww3-lab` / `ww-lab` `(v)` | This repo: course, examples, switches, the Kokkos port and the proposal, cited as `[@wwlab]` in the proposal. | [pubs/proposal/pt/07-methodology.md](../pubs/proposal/pt/07-methodology.md) |
| `(v)` / `⚠` | "verified against a source I actually fetched" / "I could not verify this; check it before trusting it" `(v)` [README.md](../README.md) | The repo-wide honesty convention, used in this glossary too; confirming a `⚠` is the most useful contribution. | [CONTRIBUTING.md](../CONTRIBUTING.md) |
| the ladder (*escada*) | the proposal's four steps in fixed order: 1 compile options, 2 run configuration, 3 modern Fortran, 4 C++/Kokkos kernels for the GPU `(v)` [pubs/proposal/pt/04-scope.md](../pubs/proposal/pt/04-scope.md) ("Quatro etapas … executadas nesta ordem"), *rungs* in the English proposal `(v)` [pubs/proposal/en/06-objective.md](../pubs/proposal/en/06-objective.md) | Lessons 09–13 follow it; each rung is gated by parity with the one before. | [course/README.md](../course/README.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| etapas 1–4 / steps 1–4 | *Opções de compilação* / *Configuração da execução* / *Refatoração dirigida em Fortran moderno* / *Reescrita de kernels em C++/Kokkos e execução em GPU* `(v)` [pubs/proposal/pt/04-scope.md](../pubs/proposal/pt/04-scope.md) | Step 1–2 are lesson 09, step 3 lesson 10, step 4 lessons 11–13. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md), [pubs/proposal/pt/08-schedule.md](../pubs/proposal/pt/08-schedule.md) |
| critérios de paridade / parity gates | bit-for-bit via WW3's matrix for steps 1–2; per-field tolerances plus per-routine tests for steps 3–4 `(v)` [pubs/proposal/pt/07-methodology.md](../pubs/proposal/pt/07-methodology.md); "Parity gates" in [pubs/proposal/en/07-methodology.md](../pubs/proposal/en/07-methodology.md) `(v)` | A change that fails the criterion is not merged. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) ("The ladder and its gates") |
| comparador por campo / per-field comparator | the netCDF field-by-field comparator with versioned tolerances the proposal builds `(v)` [pubs/proposal/pt/07-methodology.md](../pubs/proposal/pt/07-methodology.md) | Implemented as `nccmp-tol`. | [course/06-output.md](../course/06-output.md) |
| execução de referência congelada / frozen reference run | code revision, switch file, namelists, grid and forcing frozen and documented at the start `(v)` [pubs/proposal/mapas-mentais.pt.md](../pubs/proposal/mapas-mentais.pt.md), [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) | Every number in the course is relative to it. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| escada de residência de dados / data-residency ladder | the four steps by which host↔device copies disappear: 1 copy `VA` in/out per call, 2 `VA` + tables resident, 3 propagation and halos on device, 4 member dimension `(v)` [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) ("The data-residency ladder", from `AGENTS_KOKKOS` §3.4). ⚠ The Portuguese phrase itself does not occur in `pubs/`; the proposal speaks of the state "resident on the device" (*residentes no dispositivo*) | The plan for making the phase-1 copy disappear; step 2 is where the OpenACC port stalled. | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md), [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| validação em escada / validation ladder (FESOM2 recipe) | Fortran → reference C → Kokkos serial (bit-identical to the C) → Kokkos CUDA (statistical comparison), each rung checked against the one below `(v)` [pubs/proposal/pt/07-methodology.md](../pubs/proposal/pt/07-methodology.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) | Collapsed to "bit-identical to the Fortran fixture" for `W3SNL1`. | [pubs/proposal/mapas-mentais.pt.md](../pubs/proposal/mapas-mentais.pt.md) |
| chave de execução / runtime switch | one binary, two paths, selected by a flag read once at start-up `(v)` [pubs/proposal/pt/07-methodology.md](../pubs/proposal/pt/07-methodology.md), [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) | `WW_KOKKOS_SNL1` → `KOKKOS_SNL1` is the implementation. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| phase 1 / phase 2 (port phases) | the memory contract of a ported routine: phase 1 copies the spectrum host→device and back per call, phase 2 keeps `VA` resident `(v)` [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md); `AGENTS_KOKKOS` §3.6 numbers phases 0–5 `(v)` | Every routine starts at phase 1 because that is what can be validated one call at a time. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md), [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| "translate, do not improve" | the phase-1 rule: same expressions, order and float32 arithmetic `(v)` [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md), [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) §1.5 | Any algorithmic change is a separate PR with its own L2 evidence. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| definition of done (six items) | kernel with heritage header; `bind(C)` shim + Fortran interface; L1 test with justified tolerances; L2 replay of the smallest regtest; timing line in `PORT_STATUS.md`; property test where physics allows `(v)` [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) §1.5 | What a port PR must contain. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| AGENTS_KOKKOS | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md), "Operating coding agents on a phased WW3 → modern C++ / Kokkos port", written to be dropped in as `AGENTS.md` `(v)` | The rules (§1), ranked port list (§2), interop contract and residency ladder (§3) lessons 11–13 cite by section. | [course/13-bulk-porting-with-agents.md](../course/13-bulk-porting-with-agents.md) |
| KOKKOS_H100_PLAN | [docs/KOKKOS_H100_PLAN_202609.md](KOKKOS_H100_PLAN_202609.md), the single-H100 port plan with the full `model/src` repository map `(v)` | Source of the "239 preprocessor guards in `w3srcemd.F90`" count and the Kokkos-vs-SYCL decision. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| BEND_TRYOUT | [docs/BEND_TRYOUT_202609.md](BEND_TRYOUT_202609.md), a one-week plan to port the `W3SNL1` body to Bend 2 as a third arm of the Fortran / Kokkos parity harness `(v)` | A proposal, not a build record: Bend 2 is `F32`-only with no C ABI and single-owner arrays that parallel branches must clone rather than share, so the experiment is a standalone program driven by the committed fixture, gated by the L1 tolerance rather than `b4b`. | [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| AWESOME-WW3 | [docs/AWESOME-WW3_202609.md](AWESOME-WW3_202609.md), the curated link list `(v)` | Where every third-party tool named in the course is catalogued. | [README.md](../README.md) |
| `switch_lab_shrd` / `switch_lab_mpi` / `switch_lab_st6` | the lab's serial / `DIST MPI` / `ST6`+`FLX4` switch files `(v)` [switches/README.md](../switches/README.md) | Reasonable defaults assembled from documentation, not copies of upstream files; five of their keys are inert (see [WW3 switches](#ww3-switches)). | [switches/README.md](../switches/README.md), [course/01-build.md](../course/01-build.md) |
| `kokkos/` | the C++/Kokkos half of the lab: one CMake tree, one backend per preset `(v)` [kokkos/README.md](../kokkos/README.md) | `src/ww_kokkos` (kernels), `src/fortran_iface` (shim), `intro/`, `tests/`, `tools/`. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `ww_kokkos` | the portable kernel library target (kernels + C ABI), built with `-ffp-contract=off` `(v)` [kokkos/CMakeLists.txt](../kokkos/CMakeLists.txt) | What every test, tool and intro program links. | [kokkos/README.md](../kokkos/README.md) |
| `ww_kokkos_f` | the Fortran interface module target (`w3kokkosmd.F90`), built separately because it is meant to be copied into `WW3/model/src` `(v)` [kokkos/README.md](../kokkos/README.md) | Needs `WW_ENABLE_FORTRAN`. | [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) |
| `ww_intro_01` … `ww_intro_06` | the self-checking intro programs, one Kokkos concept each, registered as CTest cases `intro_NN` `(v)` [kokkos/intro/CMakeLists.txt](../kokkos/intro/CMakeLists.txt) | Views, `parallel_for`, reduce/scan, layouts and mirrors, team scratch, `bind(C)` interop. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `snl1_config.hpp` / `snl1_tables.{hpp,cpp}` / `snl1_dia.{hpp,cpp}` | `ww::snl1::Config` (the `W3GDATMD` inputs), `Tables` + `make_tables()` (= `INSNL1`, host, once per grid), `snl1()` (= `W3SNL1`, device, per call) `(v)` [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) | The DIA port, LGPL-3.0-or-later like its source. | [kokkos/src/ww_kokkos/](../kokkos/src/ww_kokkos/) |
| `"srce.snl1.dia"` | the kernel launch label `(v)` [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) | Maps the profile back to the WW3 routine it replaces (`AGENTS_KOKKOS` §1.3). | [docs/AGENTS_KOKKOS_202609.md](AGENTS_KOKKOS_202609.md) |
| `real.hpp` / `ww::Real` | `float`, WW3's default `REAL` `(v)` [kokkos/src/ww_kokkos/real.hpp](../kokkos/src/ww_kokkos/real.hpp) | The state precision; every literal is `static_cast<Real>`. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `spectrum_fixtures.hpp` / `ww::jonswap` | the JONSWAP × cos² synthetic spectrum as a `KOKKOS_INLINE_FUNCTION` `(v)` [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) | The standard L1 fixture sea state on the C++ side. | [kokkos/src/ww_kokkos/spectrum_fixtures.hpp](../kokkos/src/ww_kokkos/spectrum_fixtures.hpp) |
| `fixture_io.{hpp,cpp}` / `ww::fixture::load()` | the one reader of the binary fixture layout; subtracts one from every 1-based index `(v)` [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) | Shared by tests and `ww_bench_snl1`. | [kokkos/src/ww_kokkos/fixture_io.hpp](../kokkos/src/ww_kokkos/fixture_io.hpp) |
| `ww_kokkos_c.hpp` / `snl1_shim.cpp` / `w3kokkosmd.F90` | the C ABI declarations / their implementation (ownership, lifetime, errors) / the `ISO_C_BINDING` mirror `MODULE W3KOKKOSMD` `(v)` [kokkos/README.md](../kokkos/README.md) | The `bind(C)` boundary; nothing in it can throw. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| W3KOKKOSMD / W3KOKKOS_SETUP / KOKKOS_SNL1 | the Fortran module, its set-up routine, and the `LOGICAL` the inner loop reads `(v)` [kokkos/src/fortran_iface/w3kokkosmd.F90](../kokkos/src/fortran_iface/w3kokkosmd.F90) | Written to be dropped into `WW3/model/src` unchanged. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| `ww_kokkos_init` / `ww_kokkos_finalize` / `ww_snl1_init` / `ww_snl1` / `ww_snl1_enabled` / `ww_snl1_last_error` | the six C entry points: start the runtime (idempotent) / release buffers / build tables once per grid / run `W3SNL1` for `npts` points / report the env switch / status of the last call `(v)` [kokkos/src/fortran_iface/ww_kokkos_c.hpp](../kokkos/src/fortran_iface/ww_kokkos_c.hpp) | `ww_snl1` is `void` because a Fortran `CALL` cannot read a return value. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `WW_KOKKOS_OK` … `WW_KOKKOS_ERR_RUNTIME` | the `WwKokkosStatus` codes 0–4: ok / not initialised / bad shape / kernel threw / Kokkos failed `(v)` [kokkos/src/fortran_iface/ww_kokkos_c.hpp](../kokkos/src/fortran_iface/ww_kokkos_c.hpp) | What `ww_snl1_last_error()` returns. | [kokkos/README.md](../kokkos/README.md) |
| `WW_KOKKOS_SNL1` | environment variable, `=1` makes `ww_snl1_enabled()` return 1 so the patched `W3SRCE` calls the port `(v)` [kokkos/README.md](../kokkos/README.md) | The runtime switch; `L2_replay.sh` flips it between the two runs. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `WW_KOKKOS_DEVICE_ID` | environment variable selecting the GPU (default 0), read instead of `MPI_Comm_rank()` because phase 1 links no MPI `(v)` [kokkos/README.md](../kokkos/README.md) | Set per rank by the launcher in an MPI job. | [kokkos/src/fortran_iface/ww_kokkos_c.hpp](../kokkos/src/fortran_iface/ww_kokkos_c.hpp) |
| `WW_DETERMINISTIC` | CMake option: "force serial reductions for bit-reproducible validation builds" `(v)` [kokkos/CMakeLists.txt](../kokkos/CMakeLists.txt) | On in `serial-debug`; changes nothing for the DIA, which has no reduction. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `WW_ENABLE_FORTRAN` / `WW_WW3_BUILD_DIR` | build the Fortran module, reference and drivers (default ON) / a configured WW3 build (`libww3.a` + `mod/`) that enables the `ww3_lib` cross-check `(v)` [kokkos/README.md](../kokkos/README.md) | The second is what `just l1-crosscheck` sets. | [kokkos/README.md](../kokkos/README.md) |
| `WW_FIXTURE_DIR` / `WW_TEST_ENVIRONMENT` | compile definition pointing tests at `tests/fixtures/` / the CTest environment `OMP_PROC_BIND=false;OMP_NUM_THREADS=2` `(v)` [kokkos/CMakeLists.txt](../kokkos/CMakeLists.txt) | Test plumbing. | [kokkos/tests/CMakeLists.txt](../kokkos/tests/CMakeLists.txt) |
| `serial-debug` / `openmp-release` / `cuda-release` | the three presets: Debug + sanitizers + `WW_DETERMINISTIC` / Release `-O3 -march=x86-64-v3` / Release with `nvcc_wrapper`, arch 89 `(v)` [kokkos/CMakePresets.json](../kokkos/CMakePresets.json) | The name says build type, not backend. | [course/11-kokkos-and-modern-cpp.md](../course/11-kokkos-and-modern-cpp.md) |
| `L1_test_intro` / `L1_test_snl1_tables` / `L1_test_snl1_dia` / `L1_test_snl1_shim` / `L1_test_nccmp_tol` | the GoogleTest suites: lesson invariants / `make_tables` vs `INSNL1` / `snl1` vs `W3SNL1` / the same through the C ABI plus error paths / the comparator on files it writes `(v)` [kokkos/README.md](../kokkos/README.md), [kokkos/tests/CMakeLists.txt](../kokkos/tests/CMakeLists.txt) | All report 0.0 max relative error on all three presets. | [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md), [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `shim_roundtrip` / `shim_driver.F90` | CTest case: a Fortran program calling `W3KOKKOSMD` → shim → kernel and comparing with `W3SNL1_REF` in the same process `(v)` [kokkos/tests/fixtures/CMakeLists.txt](../kokkos/tests/fixtures/CMakeLists.txt) | The fourth parity test. | [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) |
| `snl1_ref.F90` / `SNL1_REF` / `W3SNL1_REF` / `INSNL1_REF` / `SETUP_REF` | the standalone verbatim copy of `W3SNL1`/`INSNL1` (module `SNL1_REF`), with `USE` lines replaced by module variables `(v)` [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md), [kokkos/tests/fixtures/snl1_ref.F90](../kokkos/tests/fixtures/snl1_ref.F90) | The reference the fixture is captured from; LGPL like its source. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `snl1_sea_state.F90` / `SNL1_SEA_STATE` | the deterministic JONSWAP × cos² sea state (10 m/s, 100 km, γ = 3.3) at 1000, 50 and 10 m shared by the generator and the shim driver `(v)` [kokkos/tests/fixtures/snl1_sea_state.F90](../kokkos/tests/fixtures/snl1_sea_state.F90) | Keeps the two programs from drifting apart. | [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) |
| `gen_snl1_fixture` / `snl1_nk25_nth24.bin` / `snl1-fixtures` | the generator, the committed 107 892-byte little-endian fixture (`NK=25`, `NTH=24`, 3 points) and the CMake target that regenerates it `(v)` [kokkos/tests/fixtures/README.md](../kokkos/tests/fixtures/README.md) | `just snl1-fixtures`; a byte-level diff needs a reason in the commit. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `gen_snl1_ww3lib` / `just l1-crosscheck` | the untested cross-check that runs the real `W3SNL1` from a WW3 `libww3.a` and `cmp`s the fixtures `(v)` [kokkos/README.md](../kokkos/README.md) | Never run: the host's WW3 build used `NL0`. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `ww_bench_snl1` / `bench_snl1.cpp` | 1 000 sea points, 20 timed calls after 3 warm-ups, through the shim and kernel-only; not a CTest case `(v)` [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) | Source of the 25.88 / 5.95 / 0.75 ms timing row. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `PORT_STATUS.md` | the port ledger: one row per routine with WW3 file:lines, phase, shim, L1 parity, L2 replay, Serial/OpenMP/CUDA ms `(v)` [kokkos/PORT_STATUS.md](../kokkos/PORT_STATUS.md) | "A row is only allowed to claim a number that a command in this repository reproduces." | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `PATCH.md` | the hunk-by-hunk recipe for wiring the shim into `w3srcemd.F90`, `w3initmd.F90`, `switches.json` and `model/src/CMakeLists.txt` on a fork branch, not applied here `(v)` [kokkos/src/fortran_iface/PATCH.md](../kokkos/src/fortran_iface/PATCH.md) | Four hunks, real line numbers, three phase-1 limits. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `L2_replay.sh` / `just l2` | runs a prepared regtest twice with `WW_KOKKOS_SNL1=0` and `=1`, `nccmp-tol`s the `ww3_ounf` output and appends the table to `PORT_STATUS.md` `(v)` [kokkos/tests/L2_replay.sh](../kokkos/tests/L2_replay.sh) | Until the fork branch exists both runs are the Fortran path. | [course/12-porting-a-kernel-w3snl1.md](../course/12-porting-a-kernel-w3snl1.md) |
| `nccmp-tol` / `nccmp_tol_lib` / `tolerances.txt` | the per-field netCDF comparator (exit 0/1/2), its static library (`ww::nccmp`), and the versioned tolerance file (`name abs rel`; `hs fp dir dp t0m1`) `(v)` [kokkos/tools/nccmp-tol/README.md](../kokkos/tools/nccmp-tol/README.md) | The *comparador por campo*; `just nccmp REF TEST [TOL]`. | [course/06-output.md](../course/06-output.md), [kokkos/tools/nccmp-tol/tolerances.txt](../kokkos/tools/nccmp-tol/tolerances.txt) |
| `ww_bench_case` | C++ generator of a complete WW3 benchmark case (`--size small|medium|large` or `--nx --ny --nk --nth --hours`, `-o DIR`) with CFL-derived timesteps `(v)` [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) | Replaces `bench/make_bench_case.py`; lives in [kokkos/tools/bench_case/](../kokkos/tools/bench_case/). | [course/08-python.md](../course/08-python.md) |
| `ww_fetch_analyse` | C++ analyser of example 01: `Hs` versus fetch against Kahma & Calkoen (1992) and the PM limit `(v)` [course/06-output.md](../course/06-output.md) | Lives in [kokkos/tools/fetch_analyse/](../kokkos/tools/fetch_analyse/); run by the example's `run.sh`. | [course/06-output.md](../course/06-output.md) |
| `gprof_table.sh` / `perf_table.sh` / `just profile` | phase tables of one regtest run from `gprof` (rebuild with `-pg` into `build-pg`) or `perf record` `(v)` [kokkos/tools/profile/README.md](../kokkos/tools/profile/README.md) | Phase map: `w3srce*` source terms, `w3pro*`/`w3uqck*` propagation, `w3gath`/`w3scat`/`mpi_*` communication, `w3io*` I/O. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| `bench/` | `bench_ww3_cpu.sh` (real WW3 MPI scaling), `kernel_bench.f90` (WW3-shaped kernel, resident vs copied), `hetero_split.f90` (CPU+GPU split sweep), `run_all.sh` `(v)` [bench/README.md](../bench/README.md) | The i9-vs-4090 measurements and the methodology notes. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| `gpu/` | the `nvfortran` sandbox: `00_hello_acc`, `01_dispersion`, `02_do_concurrent`, `03_precision`, `build_netcdf_nvfortran.sh` `(v)` [gpu/README.md](../gpu/README.md) | The directive contrast of lesson 10. | [course/10-modern-fortran-refactoring.md](../course/10-modern-fortran-refactoring.md) |
| `examples/01-fetch-limited-growth` / `02-regional-real-forcing` | Cartesian box with constant wind and an analytic answer / 0.1° southern-Brazil shelf with GEBCO bathymetry and real winds `(v)` [examples/README.md](../examples/README.md) | Do lesson 02 with example 01 open. | [course/02-anatomy-of-a-run.md](../course/02-anatomy-of-a-run.md), [course/03-grids.md](../course/03-grids.md) |
| `exercises/` | one sheet per lesson with solutions `(v)` [course/README.md](../course/README.md) | Lessons 09–13 name `ex09_bench.md` … `ex13_port.md` and `solutions/ex09_matrix.sh` … `ex13_compare.sh` ⚠ (not all present on every branch). | [exercises/README.md](../exercises/README.md) |
| `scripts/` | `01_get_ww3.sh`, `02_build_ww3.sh`, `03_run_regtest.sh`, `04_get_swan.sh`, `book_prep.py`, `build_pdf.sh`, `translate_md.py`, submodule helpers `(v)` [justfile](../justfile) | Where every `just` recipe's logic lives; the two `.py` files are the only Python. | [README.md](../README.md), [course/08-python.md](../course/08-python.md) |
| `just rt` / `just build` / `just regtest` | build with a regtest's own switch and run it / full rebuild with a switch file / rerun a regtest step by step into `work_lab/` `(v)` [justfile](../justfile) | `just rt ww3_tp1.1 PR3_UQ` is the smallest reference run. | [course/09-benchmark-profile-compile-run.md](../course/09-benchmark-profile-compile-run.md) |
| `work_lab` / `work_a` / `work_b` / `work_pg` / `work_perf` | run directories under `WW3/regtests/<test>/`: the lab's prepared run, the two L2 replays, the gprof and perf runs `(v)` [kokkos/tests/L2_replay.sh](../kokkos/tests/L2_replay.sh), [kokkos/tools/profile/README.md](../kokkos/tools/profile/README.md) | Never the upstream `work/`. | [README.md](../README.md) |
| `WW3/` submodule / `just src-*` | the `h0ffmann/WW3` fork of NOAA-EMC/WW3 pinned at one revision, with `src-init`, `src-up`, `src-sync`, `src-pr` recipes `(v)` [README.md](../README.md), [justfile](../justfile) | Where the `PATCH.md` fork branch would live; nothing in this repo modifies it. | [course/01-build.md](../course/01-build.md) |
| `nix-config/` submodule / `just submodule-*` | the sparse checkout of `h0ffmann/nix-config` (only `labs/pratico`) and its pin-management recipes `(v)` [README.md](../README.md) | `just submodule-init` after a fresh clone. | [course/01-build.md](../course/01-build.md) |
| `pubs/` | the course book and the UFRJ/DEL proposal (`proposal/en/` source, `proposal/pt/` hand-revised reference, `mapas-mentais.pt.md`, `refs.bib`, `template.tex`) `(v)` [README.md](../README.md) | PDFs land in `pdf/` on `main`. | [README.md](../README.md) |
| course book | `course/*.md` assembled by `scripts/book_prep.py` into one PDF `(v)` [justfile](../justfile) | `just book [abnt|ieee]`. | [README.md](../README.md) |

## Remissive index

Every term above, alphabetically, with the section it lives in.

- `02-regional-real-forcing` — [This repository's own names](#this-repositorys-own-names)
- 7.14 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ABNT — [HPC and software](#hpc-and-software)
- ADA89 — [HPC and software](#hpc-and-software)
- ADA89 / HOPPER90 — [HPC and software](#hpc-and-software)
- AGENTS_KOKKOS — [This repository's own names](#this-repositorys-own-names)
- Amdahl — [HPC and software](#hpc-and-software)
- ASan — [HPC and software](#hpc-and-software)
- ASan / UBSan — [HPC and software](#hpc-and-software)
- ASCII — [WW3 switches](#ww3-switches)
- automatic array — [HPC and software](#hpc-and-software)
- AWESOME-WW3 — [This repository's own names](#this-repositorys-own-names)
- B4B — [WW3 switches](#ww3-switches)
- b4b — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `bench/` — [This repository's own names](#this-repositorys-own-names)
- `bench_snl1.cpp` — [This repository's own names](#this-repositorys-own-names)
- BEND_TRYOUT — [This repository's own names](#this-repositorys-own-names)
- BETAMAX — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- BIN2NC — [WW3 switches](#ww3-switches)
- `bind(C)` — [HPC and software](#hpc-and-software)
- `bind(C)` / ISO_C_BINDING / `extern "C"` — [HPC and software](#hpc-and-software)
- bmi-wavewatch3 — [HPC and software](#hpc-and-software)
- BS0 — [WW3 switches](#ww3-switches)
- BS0 / BS1 — [WW3 switches](#ww3-switches)
- BS1 — [WW3 switches](#ww3-switches)
- BT0 — [WW3 switches](#ww3-switches)
- BT0 / BT1 / BT4 / BT8 / BT9 — [WW3 switches](#ww3-switches)
- BT1 — [WW3 switches](#ww3-switches)
- BT4 — [WW3 switches](#ww3-switches)
- BT8 — [WW3 switches](#ww3-switches)
- BT9 — [WW3 switches](#ww3-switches)
- BYDRZ — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- card deck — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- cc89 — [HPC and software](#hpc-and-software)
- cc89 / sm_89 / compute capability 8.9 — [HPC and software](#hpc-and-software)
- cdo — [HPC and software](#hpc-and-software)
- CDS — [HPC and software](#hpc-and-software)
- CFL — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- CG — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- CG1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- CHA — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- chave de execução — [This repository's own names](#this-repositorys-own-names)
- chave de execução / runtime switch — [This repository's own names](#this-repositorys-own-names)
- CI — [HPC and software](#hpc-and-software)
- CMake presets — [HPC and software](#hpc-and-software)
- CNPq — [Models, projects and institutions](#models-projects-and-institutions)
- CNPq / MCTI / Finep — [Models, projects and institutions](#models-projects-and-institutions)
- coding agent — [HPC and software](#hpc-and-software)
- comparador por campo — [This repository's own names](#this-repositorys-own-names)
- comparador por campo / per-field comparator — [This repository's own names](#this-repositorys-own-names)
- compute capability 8.9 — [HPC and software](#hpc-and-software)
- CONSTANTS — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- CONSTANTS / constants.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- constants.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `CONTIGUOUS` — [HPC and software](#hpc-and-software)
- `CONTIGUOUS` / `INTENT` / `PURE` / explicit interface — [HPC and software](#hpc-and-software)
- Copilot — [HPC and software](#hpc-and-software)
- COU — [WW3 switches](#ww3-switches)
- course book — [This repository's own names](#this-repositorys-own-names)
- `CPU_TIME` — [HPC and software](#hpc-and-software)
- `create_mirror_view` — [HPC and software](#hpc-and-software)
- critérios de paridade — [This repository's own names](#this-repositorys-own-names)
- critérios de paridade / parity gates — [This repository's own names](#this-repositorys-own-names)
- CRT0 — [WW3 switches](#ww3-switches)
- CRT0 / CRT1 / CRT2, CRX0 / CRX1 / CRX2 — [WW3 switches](#ww3-switches)
- CRT1 — [WW3 switches](#ww3-switches)
- CRT2 — [WW3 switches](#ww3-switches)
- CRX0 — [WW3 switches](#ww3-switches)
- CRX1 — [WW3 switches](#ww3-switches)
- CRX2 — [WW3 switches](#ww3-switches)
- CTest — [HPC and software](#hpc-and-software)
- CUDA — [HPC and software](#hpc-and-software)
- CUDA / CUDA Fortran — [HPC and software](#hpc-and-software)
- CUDA Fortran — [HPC and software](#hpc-and-software)
- `cuda-release` — [This repository's own names](#this-repositorys-own-names)
- current.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- CURRLINE — [WW3 programs and files](#ww3-programs-and-files)
- CURV — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- D (VS — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- data-residency ladder — [This repository's own names](#this-repositorys-own-names)
- DB0 — [WW3 switches](#ww3-switches)
- DB0 / DB1 — [WW3 switches](#ww3-switches)
- DB1 — [WW3 switches](#ww3-switches)
- `deep_copy` — [HPC and software](#hpc-and-software)
- definition of done (six items) — [This repository's own names](#this-repositorys-own-names)
- DEL — [Models, projects and institutions](#models-projects-and-institutions)
- DEPTH — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- develop — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- develop / 7.14 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- devShell — [HPC and software](#hpc-and-software)
- DIA — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- dir — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- dir, dp (DIR, DP) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- DIST — [WW3 switches](#ww3-switches)
- DISTAB — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- DMIN — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `do concurrent` — [HPC and software](#hpc-and-software)
- do not improve" — [This repository's own names](#this-repositorys-own-names)
- DOE — [Models, projects and institutions](#models-projects-and-institutions)
- DOE / E3SM / ORNL / LANL — [Models, projects and institutions](#models-projects-and-institutions)
- dp (DIR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- DP) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- DPT — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- DSS0 — [WW3 switches](#ww3-switches)
- DTH — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- DTKTH — [WW3 programs and files](#ww3-programs-and-files)
- DTMAX — [WW3 programs and files](#ww3-programs-and-files)
- DTMAX / DTXY / DTKTH / DTMIN — [WW3 programs and files](#ww3-programs-and-files)
- DTMIN — [WW3 programs and files](#ww3-programs-and-files)
- DTXY — [WW3 programs and files](#ww3-programs-and-files)
- E-cores — [HPC and software](#hpc-and-software)
- E3SM — [Models, projects and institutions](#models-projects-and-institutions)
- ecCodes — [HPC and software](#hpc-and-software)
- ecCodes / grib_to_netcdf / grib_copy / grib_ls — [HPC and software](#hpc-and-software)
- ECMWF — [Models, projects and institutions](#models-projects-and-institutions)
- ECWAM — [Models, projects and institutions](#models-projects-and-institutions)
- EF — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- efth — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- EMC — [Models, projects and institutions](#models-projects-and-institutions)
- ERA5 — [HPC and software](#hpc-and-software)
- ERA5 / CDS — [HPC and software](#hpc-and-software)
- escada de residência de dados — [This repository's own names](#this-repositorys-own-names)
- escada de residência de dados / data-residency ladder — [This repository's own names](#this-repositorys-own-names)
- ESMF — [Models, projects and institutions](#models-projects-and-institutions)
- etapas 1–4 — [This repository's own names](#this-repositorys-own-names)
- etapas 1–4 / steps 1–4 — [This repository's own names](#this-repositorys-own-names)
- `examples/01-fetch-limited-growth` — [This repository's own names](#this-repositorys-own-names)
- `examples/01-fetch-limited-growth` / `02-regional-real-forcing` — [This repository's own names](#this-repositorys-own-names)
- execução de referência congelada — [This repository's own names](#this-repositorys-own-names)
- execução de referência congelada / frozen reference run — [This repository's own names](#this-repositorys-own-names)
- execution space — [HPC and software](#hpc-and-software)
- execution space / memory space — [HPC and software](#hpc-and-software)
- `exercises/` — [This repository's own names](#this-repositorys-own-names)
- explicit interface — [HPC and software](#hpc-and-software)
- EXTCDE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `extern "C"` — [HPC and software](#hpc-and-software)
- F, E — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- F90 — [WW3 switches](#ww3-switches)
- FBI (NL4) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- fence — [HPC and software](#hpc-and-software)
- fence / `Kokkos::fence()` — [HPC and software](#hpc-and-software)
- FESOM2 — [Models, projects and institutions](#models-projects-and-institutions)
- fetch — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `-ffp-contract` — [HPC and software](#hpc-and-software)
- FIELD%LIST — [WW3 programs and files](#ww3-programs-and-files)
- FIELD%PARTITION — [WW3 programs and files](#ww3-programs-and-files)
- FIELD%TIMESPLIT — [WW3 programs and files](#ww3-programs-and-files)
- FIELD%TIMESPLIT / FIELD%PARTITION / FIELD%LIST — [WW3 programs and files](#ww3-programs-and-files)
- FIELD%TYPE — [WW3 programs and files](#ww3-programs-and-files)
- `_FillValue` — [HPC and software](#hpc-and-software)
- `finalize` — [HPC and software](#hpc-and-software)
- Finep — [Models, projects and institutions](#models-projects-and-institutions)
- `fixture_io.{hpp,cpp}` — [This repository's own names](#this-repositorys-own-names)
- `fixture_io.{hpp,cpp}` / `ww::fixture::load()` — [This repository's own names](#this-repositorys-own-names)
- FLAGTR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- flake — [HPC and software](#hpc-and-software)
- FLD0 — [WW3 switches](#ww3-switches)
- FLD0 / FLD1 / FLD2 — [WW3 switches](#ww3-switches)
- FLD1 — [WW3 switches](#ww3-switches)
- FLD2 — [WW3 switches](#ww3-switches)
- float32 — [HPC and software](#hpc-and-software)
- FLX0 — [WW3 switches](#ww3-switches)
- FLX0–FLX5 — [WW3 switches](#ww3-switches)
- FLX1 — [WW3 switches](#ww3-switches)
- FLX2 — [WW3 switches](#ww3-switches)
- FLX3 — [WW3 switches](#ww3-switches)
- FLX4 — [WW3 switches](#ww3-switches)
- FLX5 — [WW3 switches](#ww3-switches)
- FMA — [HPC and software](#hpc-and-software)
- FMA / `-ffp-contract` / `--fmad` — [HPC and software](#hpc-and-software)
- `--fmad` — [HPC and software](#hpc-and-software)
- fp (FP) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- FP32 — [HPC and software](#hpc-and-software)
- FP32 / FP64 / float32 — [HPC and software](#hpc-and-software)
- FP64 — [HPC and software](#hpc-and-software)
- FREQ1 (FR1) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- frozen reference run — [This repository's own names](#this-repositorys-own-names)
- GEBCO — [HPC and software](#hpc-and-software)
- GEFS — [HPC and software](#hpc-and-software)
- `gen_snl1_fixture` — [This repository's own names](#this-repositorys-own-names)
- `gen_snl1_fixture` / `snl1_nk25_nth24.bin` / `snl1-fixtures` — [This repository's own names](#this-repositorys-own-names)
- `gen_snl1_ww3lib` — [This repository's own names](#this-repositorys-own-names)
- `gen_snl1_ww3lib` / `just l1-crosscheck` — [This repository's own names](#this-repositorys-own-names)
- genes_gmd — [HPC and software](#hpc-and-software)
- GFS — [HPC and software](#hpc-and-software)
- GFS / GEFS — [HPC and software](#hpc-and-software)
- GKE (NL5) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- GMD (journal) — [Models, projects and institutions](#models-projects-and-institutions)
- GMD (NL3) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- GMSH — [HPC and software](#hpc-and-software)
- GoogleTest — [HPC and software](#hpc-and-software)
- GoogleTest / GTest — [HPC and software](#hpc-and-software)
- gprof — [HPC and software](#hpc-and-software)
- `gprof_table.sh` — [This repository's own names](#this-repositorys-own-names)
- `gprof_table.sh` / `perf_table.sh` / `just profile` — [This repository's own names](#this-repositorys-own-names)
- `gpu/` — [This repository's own names](#this-repositorys-own-names)
- GQM — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- GRAV — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- GRIB — [HPC and software](#hpc-and-software)
- GRIB / GRIB2 — [HPC and software](#hpc-and-software)
- GRIB2 — [HPC and software](#hpc-and-software)
- grib_copy — [HPC and software](#hpc-and-software)
- grib_ls — [HPC and software](#hpc-and-software)
- grib_to_netcdf — [HPC and software](#hpc-and-software)
- gridgen — [HPC and software](#hpc-and-software)
- gridgen / genes_gmd / OceanMesh2D / GMSH / SMS — [HPC and software](#hpc-and-software)
- GSE — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- GTest — [HPC and software](#hpc-and-software)
- H100 — [HPC and software](#hpc-and-software)
- H100 / RTX 4090 — [HPC and software](#hpc-and-software)
- HDF5 — [HPC and software](#hpc-and-software)
- HOMOG_COUNT — [WW3 programs and files](#ww3-programs-and-files)
- HOMOG_COUNT / HOMOG_INPUT — [WW3 programs and files](#ww3-programs-and-files)
- HOMOG_INPUT — [WW3 programs and files](#ww3-programs-and-files)
- HOPPER90 — [HPC and software](#hpc-and-software)
- Hs (HS) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- IC0 — [WW3 switches](#ww3-switches)
- IC0–IC5 — [WW3 switches](#ww3-switches)
- IC1 — [WW3 switches](#ww3-switches)
- IC2 — [WW3 switches](#ww3-switches)
- IC3 — [WW3 switches](#ww3-switches)
- IC4 — [WW3 switches](#ww3-switches)
- IC5 — [WW3 switches](#ww3-switches)
- ice.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- ICELINE — [WW3 programs and files](#ww3-programs-and-files)
- IDFM — [WW3 programs and files](#ww3-programs-and-files)
- IDLA — [WW3 programs and files](#ww3-programs-and-files)
- IDLA / IDFM — [WW3 programs and files](#ww3-programs-and-files)
- IEEE — [HPC and software](#hpc-and-software)
- IFREMER — [Models, projects and institutions](#models-projects-and-institutions)
- IFREMER / SHOM — [Models, projects and institutions](#models-projects-and-institutions)
- IG1 — [WW3 switches](#ww3-switches)
- IMOD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- INBND_COUNT — [WW3 programs and files](#ww3-programs-and-files)
- INBND_COUNT / INBND_POINT — [WW3 programs and files](#ww3-programs-and-files)
- INBND_POINT — [WW3 programs and files](#ww3-programs-and-files)
- .inp — [WW3 programs and files](#ww3-programs-and-files)
- INSNL1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `INSNL1_REF` — [This repository's own names](#this-repositorys-own-names)
- `INTENT` — [HPC and software](#hpc-and-software)
- IS0 — [WW3 switches](#ww3-switches)
- IS0 / IS1 / IS2 — [WW3 switches](#ww3-switches)
- IS1 — [WW3 switches](#ww3-switches)
- IS2 — [WW3 switches](#ww3-switches)
- ISEA/JSEA — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ISO_C_BINDING — [HPC and software](#hpc-and-software)
- JONSWAP — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Jules — [HPC and software](#hpc-and-software)
- just — [HPC and software](#hpc-and-software)
- just / justfile — [HPC and software](#hpc-and-software)
- `just build` — [This repository's own names](#this-repositorys-own-names)
- `just l1-crosscheck` — [This repository's own names](#this-repositorys-own-names)
- `just l2` — [This repository's own names](#this-repositorys-own-names)
- `just profile` — [This repository's own names](#this-repositorys-own-names)
- `just regtest` — [This repository's own names](#this-repositorys-own-names)
- `just rt` — [This repository's own names](#this-repositorys-own-names)
- `just rt` / `just build` / `just regtest` — [This repository's own names](#this-repositorys-own-names)
- `just src-*` — [This repository's own names](#this-repositorys-own-names)
- `just submodule-*` — [This repository's own names](#this-repositorys-own-names)
- justfile — [HPC and software](#hpc-and-software)
- k (WN) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Kahma & Calkoen (K&C92) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- KDMEAN — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Kokkos — [HPC and software](#hpc-and-software)
- KOKKOS (proposed) — [WW3 switches](#ww3-switches)
- `kokkos/` — [This repository's own names](#this-repositorys-own-names)
- `Kokkos::fence()` — [HPC and software](#hpc-and-software)
- `Kokkos::initialize` — [HPC and software](#hpc-and-software)
- `Kokkos_ENABLE_DEBUG_BOUNDS_CHECK` — [HPC and software](#hpc-and-software)
- KOKKOS_H100_PLAN — [This repository's own names](#this-repositorys-own-names)
- KOKKOS_INLINE_FUNCTION — [HPC and software](#hpc-and-software)
- KOKKOS_LAMBDA — [HPC and software](#hpc-and-software)
- KOKKOS_LAMBDA / KOKKOS_INLINE_FUNCTION — [HPC and software](#hpc-and-software)
- KOKKOS_SNL1 — [This repository's own names](#this-repositorys-own-names)
- L1 — [HPC and software](#hpc-and-software)
- L1 / L2 / L3 / L4 — [HPC and software](#hpc-and-software)
- `L1_test_intro` — [This repository's own names](#this-repositorys-own-names)
- `L1_test_intro` / `L1_test_snl1_tables` / `L1_test_snl1_dia` / `L1_test_snl1_shim` / `L1_test_nccmp_tol` — [This repository's own names](#this-repositorys-own-names)
- `L1_test_nccmp_tol` — [This repository's own names](#this-repositorys-own-names)
- `L1_test_snl1_dia` — [This repository's own names](#this-repositorys-own-names)
- `L1_test_snl1_shim` — [This repository's own names](#this-repositorys-own-names)
- `L1_test_snl1_tables` — [This repository's own names](#this-repositorys-own-names)
- L2 — [HPC and software](#hpc-and-software)
- `L2_replay.sh` — [This repository's own names](#this-repositorys-own-names)
- `L2_replay.sh` / `just l2` — [This repository's own names](#this-repositorys-own-names)
- L3 — [HPC and software](#hpc-and-software)
- L4 — [HPC and software](#hpc-and-software)
- LabECO — [Models, projects and institutions](#models-projects-and-institutions)
- LANL — [Models, projects and institutions](#models-projects-and-institutions)
- LayoutLeft — [HPC and software](#hpc-and-software)
- LayoutLeft / LayoutRight — [HPC and software](#hpc-and-software)
- LayoutRight — [HPC and software](#hpc-and-software)
- level.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- llm — [HPC and software](#hpc-and-software)
- LLM — [HPC and software](#hpc-and-software)
- LLM / coding agent / Jules / Copilot — [HPC and software](#hpc-and-software)
- LN0 — [WW3 switches](#ww3-switches)
- LN0 / LN1 / SEED — [WW3 switches](#ww3-switches)
- LN1 — [WW3 switches](#ww3-switches)
- log.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- LRB4 — [WW3 switches](#ww3-switches)
- LTA — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Managed Memory — [HPC and software](#hpc-and-software)
- mapsta.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- mask.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- mask.ww3 / mapsta.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- matrix.base — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- matrix.comp — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- MCTI — [Models, projects and institutions](#models-projects-and-institutions)
- MDRangePolicy — [HPC and software](#hpc-and-software)
- MEMCHECK — [WW3 switches](#ww3-switches)
- memory space — [HPC and software](#hpc-and-software)
- Mermaid — [HPC and software](#hpc-and-software)
- METIS — [WW3 switches](#ww3-switches)
- METIS / SCOTCH — [WW3 switches](#ww3-switches)
- MGG — [WW3 switches](#ww3-switches)
- MGP — [WW3 switches](#ww3-switches)
- MGP / MGW (MGWIND) / MGG — [WW3 switches](#ww3-switches)
- MGW (MGWIND) — [WW3 switches](#ww3-switches)
- mirror — [HPC and software](#hpc-and-software)
- mirror / `deep_copy` / `create_mirror_view` — [HPC and software](#hpc-and-software)
- MIZ — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- MLIM — [WW3 switches](#ww3-switches)
- mod_def.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- MPI — [WW3 switches](#ww3-switches)
- MPI — [HPC and software](#hpc-and-software)
- MPIBDI — [WW3 switches](#ww3-switches)
- MPRF — [WW3 switches](#ww3-switches)
- MPRF / MEMCHECK / SETUP / BIN2NC / ASCII — [WW3 switches](#ww3-switches)
- mww3_test_01…09 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- N — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- namelists.nml — [WW3 programs and files](#ww3-programs-and-files)
- nc-config — [HPC and software](#hpc-and-software)
- nc-config / nf-config — [HPC and software](#hpc-and-software)
- NC4 — [WW3 switches](#ww3-switches)
- ncap2 — [HPC and software](#hpc-and-software)
- nccmp — [HPC and software](#hpc-and-software)
- `nccmp-tol` — [This repository's own names](#this-repositorys-own-names)
- `nccmp-tol` / `nccmp_tol_lib` / `tolerances.txt` — [This repository's own names](#this-repositorys-own-names)
- `nccmp_tol_lib` — [This repository's own names](#this-repositorys-own-names)
- ncdiff — [HPC and software](#hpc-and-software)
- ncdump — [HPC and software](#hpc-and-software)
- NCEP — [Models, projects and institutions](#models-projects-and-institutions)
- NCEP2 — [WW3 switches](#ww3-switches)
- ncks — [HPC and software](#hpc-and-software)
- NCO — [WW3 switches](#ww3-switches)
- NCO — [HPC and software](#hpc-and-software)
- NCO / ncks / ncap2 / ncdiff — [HPC and software](#hpc-and-software)
- NCO (NCEP) — [Models, projects and institutions](#models-projects-and-institutions)
- nest.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- NetCDF — [HPC and software](#hpc-and-software)
- NetCDF / netCDF-4 / HDF5 — [HPC and software](#hpc-and-software)
- netCDF-4 — [HPC and software](#hpc-and-software)
- nf-config — [HPC and software](#hpc-and-software)
- NFGRIDS — [WW3 programs and files](#ww3-programs-and-files)
- NFGRIDS / WINDLINE / ICELINE / CURRLINE / UNIPOINTS / WW3GRIDLINE — [WW3 programs and files](#ww3-programs-and-files)
- Ninja — [HPC and software](#hpc-and-software)
- Nix — [HPC and software](#hpc-and-software)
- Nix / flake / devShell / `nix develop` — [HPC and software](#hpc-and-software)
- `nix develop` — [HPC and software](#hpc-and-software)
- `nix-config/` submodule — [This repository's own names](#this-repositorys-own-names)
- `nix-config/` submodule / `just submodule-*` — [This repository's own names](#this-repositorys-own-names)
- NK — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- NK, NTH, NSPEC — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- NL0 — [WW3 switches](#ww3-switches)
- NL0–NL5 — [WW3 switches](#ww3-switches)
- NL1 — [WW3 switches](#ww3-switches)
- NL2 — [WW3 switches](#ww3-switches)
- NL3 — [WW3 switches](#ww3-switches)
- NL4 — [WW3 switches](#ww3-switches)
- NL5 — [WW3 switches](#ww3-switches)
- NLS — [WW3 switches](#ww3-switches)
- .nml — [WW3 programs and files](#ww3-programs-and-files)
- .nml / .inp — [WW3 programs and files](#ww3-programs-and-files)
- NNT — [WW3 switches](#ww3-switches)
- NOAA — [Models, projects and institutions](#models-projects-and-institutions)
- NOGRB — [WW3 switches](#ww3-switches)
- NOMADS — [HPC and software](#hpc-and-software)
- NONE — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- NOPA — [WW3 switches](#ww3-switches)
- NOSWLL) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- npl_b4b — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- NSPEC — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- NTH — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- nth_b4b — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- NUOPC — [Models, projects and institutions](#models-projects-and-institutions)
- NUOPC / ESMF — [Models, projects and institutions](#models-projects-and-institutions)
- nvcc — [HPC and software](#hpc-and-software)
- nvcc / nvcc_wrapper — [HPC and software](#hpc-and-software)
- nvcc_wrapper — [HPC and software](#hpc-and-software)
- nvfortran — [HPC and software](#hpc-and-software)
- NVLink — [HPC and software](#hpc-and-software)
- NVMe — [HPC and software](#hpc-and-software)
- NWS — [Models, projects and institutions](#models-projects-and-institutions)
- O0 — [WW3 switches](#ww3-switches)
- O0–O16 — [WW3 switches](#ww3-switches)
- O1 — [WW3 switches](#ww3-switches)
- O10 — [WW3 switches](#ww3-switches)
- O11 — [WW3 switches](#ww3-switches)
- O12 — [WW3 switches](#ww3-switches)
- O13 — [WW3 switches](#ww3-switches)
- O14 — [WW3 switches](#ww3-switches)
- O15 — [WW3 switches](#ww3-switches)
- O16 — [WW3 switches](#ww3-switches)
- O2 — [WW3 switches](#ww3-switches)
- O3 — [WW3 switches](#ww3-switches)
- O4 — [WW3 switches](#ww3-switches)
- O5 — [WW3 switches](#ww3-switches)
- O6 — [WW3 switches](#ww3-switches)
- O7 — [WW3 switches](#ww3-switches)
- O8 — [WW3 switches](#ww3-switches)
- O9 — [WW3 switches](#ww3-switches)
- OASACM — [WW3 switches](#ww3-switches)
- OASICM — [WW3 switches](#ww3-switches)
- OASIS — [Models, projects and institutions](#models-projects-and-institutions)
- OASIS — [WW3 switches](#ww3-switches)
- OASIS / OASACM / OASOCM / OASICM — [WW3 switches](#ww3-switches)
- OASOCM — [WW3 switches](#ww3-switches)
- OceanMesh2D — [HPC and software](#hpc-and-software)
- Ollama — [HPC and software](#hpc-and-software)
- Ollama / llm / `TRANSLATE_*` — [HPC and software](#hpc-and-software)
- Omega — [Models, projects and institutions](#models-projects-and-institutions)
- OMP0 — [WW3 switches](#ww3-switches)
- OMPG — [WW3 switches](#ww3-switches)
- OMPH — [WW3 switches](#ww3-switches)
- ON 525 — [Models, projects and institutions](#models-projects-and-institutions)
- ON 525 / ON 528 — [Models, projects and institutions](#models-projects-and-institutions)
- ON 528 — [Models, projects and institutions](#models-projects-and-institutions)
- OpenACC — [HPC and software](#hpc-and-software)
- OpenMP — [HPC and software](#hpc-and-software)
- `openmp-release` — [This repository's own names](#this-repositorys-own-names)
- ORNL — [Models, projects and institutions](#models-projects-and-institutions)
- out_grd.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- out_grd.ww3 / out_pnt.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- out_pnt.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- P-cores — [HPC and software](#hpc-and-software)
- P-cores / E-cores — [HPC and software](#hpc-and-software)
- pandoc — [HPC and software](#hpc-and-software)
- pandoc / xelatex / ABNT / IEEE — [HPC and software](#hpc-and-software)
- parallel_for — [HPC and software](#hpc-and-software)
- parallel_for / parallel_reduce / parallel_scan — [HPC and software](#hpc-and-software)
- parallel_reduce — [HPC and software](#hpc-and-software)
- parallel_scan — [HPC and software](#hpc-and-software)
- param.scratch — [WW3 programs and files](#ww3-programs-and-files)
- parity gates — [This repository's own names](#this-repositorys-own-names)
- partition (PHS — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- partition (PHS, PTP, PDIR, NOSWLL) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `PATCH.md` — [This repository's own names](#this-repositorys-own-names)
- PCIe — [HPC and software](#hpc-and-software)
- PCIe / NVLink — [HPC and software](#hpc-and-software)
- PDIR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- PDLIB — [WW3 switches](#ww3-switches)
- per-field comparator — [This repository's own names](#this-repositorys-own-names)
- perf — [HPC and software](#hpc-and-software)
- `perf_table.sh` — [This repository's own names](#this-repositorys-own-names)
- `-pg` — [HPC and software](#hpc-and-software)
- `-pg` / gprof — [HPC and software](#hpc-and-software)
- phase 1 — [This repository's own names](#this-repositorys-own-names)
- phase 1 / phase 2 (port phases) — [This repository's own names](#this-repositorys-own-names)
- phase 2 (port phases) — [This repository's own names](#this-repositorys-own-names)
- phase-averaged — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- PHS/PTP/PDIR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- PI — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- PI, TPI, TPIINV — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- PM — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `PORT_STATUS.md` — [This repository's own names](#this-repositorys-own-names)
- `pow11()` — [HPC and software](#hpc-and-software)
- `powi()` — [HPC and software](#hpc-and-software)
- `powi()` / `pow11()` — [HPC and software](#hpc-and-software)
- PR0 — [WW3 switches](#ww3-switches)
- PR0 / PR1 / PR2 / PR3 — [WW3 switches](#ww3-switches)
- PR1 — [WW3 switches](#ww3-switches)
- PR2 — [WW3 switches](#ww3-switches)
- PR3 — [WW3 switches](#ww3-switches)
- pratico — [HPC and software](#hpc-and-software)
- PTP — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- publisher — [HPC and software](#hpc-and-software)
- `pubs/` — [This repository's own names](#this-repositorys-own-names)
- `PURE` — [HPC and software](#hpc-and-software)
- `push_finalize_hook` — [HPC and software](#hpc-and-software)
- pyww3 — [HPC and software](#hpc-and-software)
- pyww3 / WW3-tools / wavespectra / ww3tool / bmi-wavewatch3 / rompy — [HPC and software](#hpc-and-software)
- quadruplet — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- RAII — [HPC and software](#hpc-and-software)
- RangePolicy — [HPC and software](#hpc-and-software)
- RangePolicy / MDRangePolicy / TeamPolicy — [HPC and software](#hpc-and-software)
- `real.hpp` — [This repository's own names](#this-repositorys-own-names)
- `real.hpp` / `ww::Real` — [This repository's own names](#this-repositorys-own-names)
- RECT — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- RECT / CURV / UNST — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- REF0 — [WW3 switches](#ww3-switches)
- REF0 / REF1 — [WW3 switches](#ww3-switches)
- REF1 — [WW3 switches](#ww3-switches)
- REFRX — [WW3 switches](#ww3-switches)
- regtests/ — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- regtests/unittests — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ReNOMO — [Models, projects and institutions](#models-projects-and-institutions)
- restart.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- rompy — [HPC and software](#hpc-and-software)
- rstrt_b4b — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- rstrt_b4b / nth_b4b / npl_b4b — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- RTD — [WW3 switches](#ww3-switches)
- RTX 4090 — [HPC and software](#hpc-and-software)
- runtime switch — [This repository's own names](#this-repositorys-own-names)
- RWND — [WW3 switches](#ww3-switches)
- S, D (VS, VD) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sbot/Sbt — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- ScopeGuard — [HPC and software](#hpc-and-software)
- ScopeGuard / `Kokkos::initialize` / `finalize` / `push_finalize_hook` — [HPC and software](#hpc-and-software)
- SCOTCH — [WW3 switches](#ww3-switches)
- scratch — [HPC and software](#hpc-and-software)
- scratch / `team_scratch(0)` / `TeamThreadRange` / `team_barrier` — [HPC and software](#hpc-and-software)
- SCRIP — [WW3 switches](#ww3-switches)
- SCRIP / SCRIPNC / SCRIPMPI — [WW3 switches](#ww3-switches)
- SCRIPMPI — [WW3 switches](#ww3-switches)
- SCRIPNC — [WW3 switches](#ww3-switches)
- `scripts/` — [This repository's own names](#this-repositorys-own-names)
- Sdb — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sds — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SEC1 — [WW3 switches](#ww3-switches)
- SEC1 / TDYN / DSS0 / XW0 / XW1 / TIDE / REFRX — [WW3 switches](#ww3-switches)
- SEED — [WW3 switches](#ww3-switches)
- `serial-debug` — [This repository's own names](#this-repositorys-own-names)
- `serial-debug` / `openmp-release` / `cuda-release` — [This repository's own names](#this-repositorys-own-names)
- SETUP — [WW3 switches](#ww3-switches)
- `SETUP_REF` — [This repository's own names](#this-repositorys-own-names)
- SF — [WW3 programs and files](#ww3-programs-and-files)
- `shim_driver.F90` — [This repository's own names](#this-repositorys-own-names)
- `shim_roundtrip` — [This repository's own names](#this-repositorys-own-names)
- `shim_roundtrip` / `shim_driver.F90` — [This repository's own names](#this-repositorys-own-names)
- SHOM — [Models, projects and institutions](#models-projects-and-institutions)
- SHOWEX — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SHRD — [WW3 switches](#ww3-switches)
- shuffle — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- shuffle / card deck — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sice — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- σ (SIG) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sin — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sin, Snl, Sds, Sbot/Sbt, Sdb, Sice, Str, Sln — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Sln — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- sm_89 — [HPC and software](#hpc-and-software)
- SMC — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SMC — [WW3 switches](#ww3-switches)
- SMPL — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SMPL / TRPL / NONE — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SMS — [HPC and software](#hpc-and-software)
- Snl — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `snl1-fixtures` — [This repository's own names](#this-repositorys-own-names)
- `snl1_config.hpp` — [This repository's own names](#this-repositorys-own-names)
- `snl1_config.hpp` / `snl1_tables.{hpp,cpp}` / `snl1_dia.{hpp,cpp}` — [This repository's own names](#this-repositorys-own-names)
- `snl1_dia.{hpp,cpp}` — [This repository's own names](#this-repositorys-own-names)
- `snl1_nk25_nth24.bin` — [This repository's own names](#this-repositorys-own-names)
- `SNL1_REF` — [This repository's own names](#this-repositorys-own-names)
- `snl1_ref.F90` — [This repository's own names](#this-repositorys-own-names)
- `snl1_ref.F90` / `SNL1_REF` / `W3SNL1_REF` / `INSNL1_REF` / `SETUP_REF` — [This repository's own names](#this-repositorys-own-names)
- `SNL1_SEA_STATE` — [This repository's own names](#this-repositorys-own-names)
- `snl1_sea_state.F90` — [This repository's own names](#this-repositorys-own-names)
- `snl1_sea_state.F90` / `SNL1_SEA_STATE` — [This repository's own names](#this-repositorys-own-names)
- `snl1_shim.cpp` — [This repository's own names](#this-repositorys-own-names)
- `snl1_tables.{hpp,cpp}` — [This repository's own names](#this-repositorys-own-names)
- SPDX — [HPC and software](#hpc-and-software)
- SPEC — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `spectrum_fixtures.hpp` — [This repository's own names](#this-repositorys-own-names)
- `spectrum_fixtures.hpp` / `ww::jonswap` — [This repository's own names](#this-repositorys-own-names)
- SPR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- SPR, EF, WND, DPT, USS, TUS, SXY, UST, CHA, PHS/PTP/PDIR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `"srce.snl1.dia"` — [This repository's own names](#this-repositorys-own-names)
- ST0 — [WW3 switches](#ww3-switches)
- ST0–ST6 — [WW3 switches](#ww3-switches)
- ST1 — [WW3 switches](#ww3-switches)
- ST2 — [WW3 switches](#ww3-switches)
- ST3 — [WW3 switches](#ww3-switches)
- ST4 — [WW3 switches](#ww3-switches)
- ST5 — [WW3 switches](#ww3-switches)
- ST6 — [WW3 switches](#ww3-switches)
- STAB0 — [WW3 switches](#ww3-switches)
- STAB0 / STAB2 / STAB3 — [WW3 switches](#ww3-switches)
- STAB2 — [WW3 switches](#ww3-switches)
- STAB3 — [WW3 switches](#ww3-switches)
- steps 1–4 — [This repository's own names](#this-repositorys-own-names)
- Str — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- STRACE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- SWAN — [Models, projects and institutions](#models-projects-and-institutions)
- SWASH — [Models, projects and institutions](#models-projects-and-institutions)
- switch_default — [WW3 switches](#ww3-switches)
- switch_default, switch_Ifremer2, switch_NCEP_st4 — [WW3 switches](#ww3-switches)
- switch_Ifremer2 — [WW3 switches](#ww3-switches)
- `switch_lab_mpi` — [This repository's own names](#this-repositorys-own-names)
- `switch_lab_shrd` — [This repository's own names](#this-repositorys-own-names)
- `switch_lab_shrd` / `switch_lab_mpi` / `switch_lab_st6` — [This repository's own names](#this-repositorys-own-names)
- `switch_lab_st6` — [This repository's own names](#this-repositorys-own-names)
- switch_MPI — [WW3 switches](#ww3-switches)
- switch_NCEP_st4 — [WW3 switches](#ww3-switches)
- switch_PR3_UQ — [WW3 switches](#ww3-switches)
- switch_PR3_UQ, switch_MPI — [WW3 switches](#ww3-switches)
- SXY — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `SYSTEM_CLOCK` — [HPC and software](#hpc-and-software)
- `SYSTEM_CLOCK` / `CPU_TIME` — [HPC and software](#hpc-and-software)
- T, T0, T1, S — [WW3 switches](#ww3-switches)
- T0 — [WW3 switches](#ww3-switches)
- T01 — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- T02 — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- T0M1 — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- T1 — [WW3 switches](#ww3-switches)
- TDYN — [WW3 switches](#ww3-switches)
- `team_barrier` — [HPC and software](#hpc-and-software)
- `team_scratch(0)` — [HPC and software](#hpc-and-software)
- TeamPolicy — [HPC and software](#hpc-and-software)
- `TeamThreadRange` — [HPC and software](#hpc-and-software)
- test.comp — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- θ (TH) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- the ladder (*escada*) — [This repository's own names](#this-repositorys-own-names)
- TIDE — [WW3 switches](#ww3-switches)
- Tm — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- Tm, T01, T02, T0M1 — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `tolerances.txt` — [This repository's own names](#this-repositorys-own-names)
- Tp — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TPI — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TPIINV — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TR0 — [WW3 switches](#ww3-switches)
- TR0 / TR1 — [WW3 switches](#ww3-switches)
- TR1 — [WW3 switches](#ww3-switches)
- "translate — [This repository's own names](#this-repositorys-own-names)
- "translate, do not improve" — [This repository's own names](#this-repositorys-own-names)
- `TRANSLATE_*` — [HPC and software](#hpc-and-software)
- TRKNC — [WW3 switches](#ww3-switches)
- TRPL — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TSA — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TSA / FBI (NL4) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- TUS — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- UBSan — [HPC and software](#hpc-and-software)
- UFRJ — [Models, projects and institutions](#models-projects-and-institutions)
- UFS — [Models, projects and institutions](#models-projects-and-institutions)
- UFSC — [Models, projects and institutions](#models-projects-and-institutions)
- UGOBCFILE — [WW3 programs and files](#ww3-programs-and-files)
- ULP — [HPC and software](#hpc-and-software)
- Unified — [HPC and software](#hpc-and-software)
- Unified / Managed Memory — [HPC and software](#hpc-and-software)
- UNIPOINTS — [WW3 programs and files](#ww3-programs-and-files)
- UNO — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- UNO — [WW3 switches](#ww3-switches)
- UNST — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- UOST — [WW3 switches](#ww3-switches)
- UQ — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- UQ — [WW3 switches](#ww3-switches)
- UQ / UNO — [WW3 switches](#ww3-switches)
- USS — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- UST — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- `(v)` — [This repository's own names](#this-repositorys-own-names)
- `(v)` / `⚠` — [This repository's own names](#this-repositorys-own-names)
- VA — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- VA, VS/VD, SPEC, VSNL/VDNL, CG1, WNMEAN, DEPTH, ISEA/JSEA, IMOD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- validação em escada — [This repository's own names](#this-repositorys-own-names)
- validação em escada / validation ladder (FESOM2 recipe) — [This repository's own names](#this-repositorys-own-names)
- validation ladder (FESOM2 recipe) — [This repository's own names](#this-repositorys-own-names)
- VD) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- View — [HPC and software](#hpc-and-software)
- VS/VD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- VSNL/VDNL — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3ADATMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3CSPCMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3DIMA — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3DIMS — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3DISPMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3DISPMD / DISTAB / WAVNU1 / WAVNU3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3DMNL — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3FLDSMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3FLX2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3FLX2MD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3FLX2MD / W3FLX2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3GATH — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3GATH / W3SCAT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3GDATMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3GSRUMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IDATMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3INIT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3INITMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3INITMD / W3INIT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOBC — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOBCMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGO — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGOMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGOMD / W3OUTG / W3IOGO — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGR — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGRMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOGRMD / W3IOGR — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOPO — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOPOMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOPOMD / W3IOPO, W3IORSMD / W3IORS, W3IOBCMD / W3IOBC, W3IOTRMD / W3IOTR — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IORS — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IORSMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOTR — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3IOTRMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3KOKKOS_SETUP — [This repository's own names](#this-repositorys-own-names)
- W3KOKKOSMD — [This repository's own names](#this-repositorys-own-names)
- W3KOKKOSMD / W3KOKKOS_SETUP / KOKKOS_SNL1 — [This repository's own names](#this-repositorys-own-names)
- `w3kokkosmd.F90` — [This repository's own names](#this-repositorys-own-names)
- W3KTP2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3KTP2 / W3KTP3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3KTP3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3MPII — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3NAUX — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3NAUX / W3DIMA / W3SETA / W3DMNL — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3NMOD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3NMOD / W3DIMS / W3SETG — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3ODATMD (W3NOUT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3ODATMD (W3NOUT / W3SETO) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3OUTG — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PARALL — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PARALL / W3TRIAMD / W3GSRUMD / W3CSPCMD / W3FLDSMD / W3TIMEMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PART — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PARTMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PARTMD / W3PART — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PRO2MD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PRO2MD / W3PRO3MD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PRO3MD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PROFSMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3PROFSMD / w3profsmd_pdlib.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- w3profsmd_pdlib.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3QCK1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3QCK2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3QCK3 (W3UQCK*) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3REF1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SBT1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SCAT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SDB1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SDS4 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SDS6 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SERVMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SERVMD / EXTCDE / STRACE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SETA — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SETG — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SETO) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SETW) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SIC1…5 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SIN4 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SLN1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SLN1, W3SBT1, W3SDB1, W3STR1, W3SIC1…5, W3REF1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- `W3SNL1_REF` — [This repository's own names](#this-repositorys-own-names)
- W3SNL1MD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL1MD / w3snl1md.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- w3snl1md.F90 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL2 / W3SNL3 / W3SNL4 / W3SNL5 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL4 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNL5 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SNLGQM — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRC4MD: W3SPR4 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRC4MD: W3SPR4 / W3SIN4 / W3SDS4 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRC6MD: W3SIN6 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRC6MD: W3SIN6 / W3SDS6 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRCE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRCEMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3SRCEMD / W3SRCE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3STR1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3TIMEMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3TRIAMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UCUR — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UICE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UINI — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3ULEV — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UPDTMD (W3UPDT*): W3UWND — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UPDTMD (W3UPDT*): W3UWND / W3UCUR / W3ULEV / W3UICE / W3UINI / W3UTRN — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UQCKMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UQCKMD / W3QCK1 / W3QCK2 / W3QCK3 (W3UQCK*) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3UTRN — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3WAVE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3WAVEMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3WAVEMD / W3WAVE — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3WDATMD (W3NDAT — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3WDATMD (W3NDAT / W3SETW) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3XYP2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3XYP2 / W3XYP3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- W3XYP3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- wall-clock — [HPC and software](#hpc-and-software)
- WAM — [Models, projects and institutions](#models-projects-and-institutions)
- WAM4+ — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- WAM6-GPU — [Models, projects and institutions](#models-projects-and-institutions)
- wavespectra — [HPC and software](#hpc-and-software)
- WAVNU1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- WAVNU3 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- WCOR — [WW3 switches](#ww3-switches)
- wind.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- wind.ww3, current.ww3, ice.ww3, level.ww3 — [WW3 programs and files](#ww3-programs-and-files)
- WINDLINE — [WW3 programs and files](#ww3-programs-and-files)
- wm*md.F90 (WMESMFMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- wm*md.F90 (WMESMFMD, WMSCRPMD, …) — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- WMSCRPMD — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- WND — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- WNMEAN — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- WNT0 — [WW3 switches](#ww3-switches)
- WNT0 / WNT1 / WNT2 — [WW3 switches](#ww3-switches)
- WNT1 — [WW3 switches](#ww3-switches)
- WNT2 — [WW3 switches](#ww3-switches)
- WNX0 — [WW3 switches](#ww3-switches)
- WNX0 / WNX1 / WNX2 — [WW3 switches](#ww3-switches)
- WNX1 — [WW3 switches](#ww3-switches)
- WNX2 — [WW3 switches](#ww3-switches)
- `work_a` — [This repository's own names](#this-repositorys-own-names)
- `work_b` — [This repository's own names](#this-repositorys-own-names)
- `work_lab` — [This repository's own names](#this-repositorys-own-names)
- `work_lab` / `work_a` / `work_b` / `work_pg` / `work_perf` — [This repository's own names](#this-repositorys-own-names)
- `work_perf` — [This repository's own names](#this-repositorys-own-names)
- `work_pg` — [This repository's own names](#this-repositorys-own-names)
- WRST — [WW3 switches](#ww3-switches)
- WRT — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- WRT / XNL (NL2) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- WW3 — [Models, projects and institutions](#models-projects-and-institutions)
- ww3-gpu (ex ww-lab) — [This repository's own names](#this-repositorys-own-names)
- WW3-tools — [HPC and software](#hpc-and-software)
- `WW3/` submodule — [This repository's own names](#this-repositorys-own-names)
- `WW3/` submodule / `just src-*` — [This repository's own names](#this-repositorys-own-names)
- ww3_bounc — [WW3 programs and files](#ww3-programs-and-files)
- ww3_bounc / ww3_bound — [WW3 programs and files](#ww3-programs-and-files)
- ww3_bound — [WW3 programs and files](#ww3-programs-and-files)
- WW3_DATA — [WW3 programs and files](#ww3-programs-and-files)
- WW3_DATA / ww3_from_ftp.sh — [WW3 programs and files](#ww3-programs-and-files)
- ww3_from_ftp.sh — [WW3 programs and files](#ww3-programs-and-files)
- ww3_gint — [WW3 programs and files](#ww3-programs-and-files)
- ww3_grib — [WW3 programs and files](#ww3-programs-and-files)
- ww3_grid — [WW3 programs and files](#ww3-programs-and-files)
- ww3_gspl — [WW3 programs and files](#ww3-programs-and-files)
- ww3_gspl / ww3_grib / ww3_prtide — [WW3 programs and files](#ww3-programs-and-files)
- ww3_multi — [WW3 programs and files](#ww3-programs-and-files)
- ww3_ounf — [WW3 programs and files](#ww3-programs-and-files)
- ww3_ounp — [WW3 programs and files](#ww3-programs-and-files)
- ww3_outf — [WW3 programs and files](#ww3-programs-and-files)
- ww3_outf / ww3_outp — [WW3 programs and files](#ww3-programs-and-files)
- ww3_outp — [WW3 programs and files](#ww3-programs-and-files)
- ww3_prep — [WW3 programs and files](#ww3-programs-and-files)
- ww3_prnc — [WW3 programs and files](#ww3-programs-and-files)
- ww3_prtide — [WW3 programs and files](#ww3-programs-and-files)
- ww3_shel — [WW3 programs and files](#ww3-programs-and-files)
- ww3_strt — [WW3 programs and files](#ww3-programs-and-files)
- ww3_systrk — [WW3 programs and files](#ww3-programs-and-files)
- ww3_tc1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp1.1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp2.17 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp2.2 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp2.5 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp2.7 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_tp2.7 / ww3_tp2.17 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_trck — [WW3 programs and files](#ww3-programs-and-files)
- ww3_trnc — [WW3 programs and files](#ww3-programs-and-files)
- ww3_trnc / ww3_trck — [WW3 programs and files](#ww3-programs-and-files)
- ww3_ts1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_ufs1.1 — [WW3 modules, routines and regtests](#ww3-modules-routines-and-regtests)
- ww3_uprstr — [WW3 programs and files](#ww3-programs-and-files)
- WW3GRIDLINE — [WW3 programs and files](#ww3-programs-and-files)
- ww3tool — [HPC and software](#hpc-and-software)
- WW4 — [Models, projects and institutions](#models-projects-and-institutions)
- `ww::fixture::load()` — [This repository's own names](#this-repositorys-own-names)
- `ww::jonswap` — [This repository's own names](#this-repositorys-own-names)
- `ww::Real` — [This repository's own names](#this-repositorys-own-names)
- `ww_bench_case` — [This repository's own names](#this-repositorys-own-names)
- `ww_bench_snl1` — [This repository's own names](#this-repositorys-own-names)
- `ww_bench_snl1` / `bench_snl1.cpp` — [This repository's own names](#this-repositorys-own-names)
- `WW_DETERMINISTIC` — [This repository's own names](#this-repositorys-own-names)
- `WW_ENABLE_FORTRAN` — [This repository's own names](#this-repositorys-own-names)
- `WW_ENABLE_FORTRAN` / `WW_WW3_BUILD_DIR` — [This repository's own names](#this-repositorys-own-names)
- `ww_fetch_analyse` — [This repository's own names](#this-repositorys-own-names)
- `WW_FIXTURE_DIR` — [This repository's own names](#this-repositorys-own-names)
- `WW_FIXTURE_DIR` / `WW_TEST_ENVIRONMENT` — [This repository's own names](#this-repositorys-own-names)
- `ww_intro_01` — [This repository's own names](#this-repositorys-own-names)
- `ww_intro_01` … `ww_intro_06` — [This repository's own names](#this-repositorys-own-names)
- `ww_intro_06` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_c.hpp` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_c.hpp` / `snl1_shim.cpp` / `w3kokkosmd.F90` — [This repository's own names](#this-repositorys-own-names)
- `WW_KOKKOS_DEVICE_ID` — [This repository's own names](#this-repositorys-own-names)
- `WW_KOKKOS_ERR_RUNTIME` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_f` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_finalize` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_init` — [This repository's own names](#this-repositorys-own-names)
- `ww_kokkos_init` / `ww_kokkos_finalize` / `ww_snl1_init` / `ww_snl1` / `ww_snl1_enabled` / `ww_snl1_last_error` — [This repository's own names](#this-repositorys-own-names)
- `WW_KOKKOS_OK` — [This repository's own names](#this-repositorys-own-names)
- `WW_KOKKOS_OK` … `WW_KOKKOS_ERR_RUNTIME` — [This repository's own names](#this-repositorys-own-names)
- `WW_KOKKOS_SNL1` — [This repository's own names](#this-repositorys-own-names)
- `ww_snl1` — [This repository's own names](#this-repositorys-own-names)
- `ww_snl1_enabled` — [This repository's own names](#this-repositorys-own-names)
- `ww_snl1_init` — [This repository's own names](#this-repositorys-own-names)
- `ww_snl1_last_error` — [This repository's own names](#this-repositorys-own-names)
- `WW_TEST_ENVIRONMENT` — [This repository's own names](#this-repositorys-own-names)
- `WW_WW3_BUILD_DIR` — [This repository's own names](#this-repositorys-own-names)
- xelatex — [HPC and software](#hpc-and-software)
- XFR — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- XNL (NL2) — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- XW0 — [WW3 switches](#ww3-switches)
- XW1 — [WW3 switches](#ww3-switches)
- XX0 — [WW3 switches](#ww3-switches)
- ZLIM — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- ZLIM, DMIN — [Wave physics and the spectrum](#wave-physics-and-the-spectrum)
- &DEPTH_NML — [WW3 programs and files](#ww3-programs-and-files)
- &DOMAIN_NML — [WW3 programs and files](#ww3-programs-and-files)
- &DOMAIN_NML, &INPUT_NML, &OUTPUT_TYPE_NML, &OUTPUT_DATE_NML — [WW3 programs and files](#ww3-programs-and-files)
- &GRID_NML — [WW3 programs and files](#ww3-programs-and-files)
- &INPUT_NML — [WW3 programs and files](#ww3-programs-and-files)
- &MASK_NML — [WW3 programs and files](#ww3-programs-and-files)
- &OBST_NML — [WW3 programs and files](#ww3-programs-and-files)
- &OUTPUT_DATE_NML — [WW3 programs and files](#ww3-programs-and-files)
- &OUTPUT_TYPE_NML — [WW3 programs and files](#ww3-programs-and-files)
- &RUN_NML — [WW3 programs and files](#ww3-programs-and-files)
- &SPECTRUM_NML — [WW3 programs and files](#ww3-programs-and-files)
- &SPECTRUM_NML, &RUN_NML, &TIMESTEPS_NML, &GRID_NML, &DEPTH_NML, &MASK_NML, &OBST_NML — [WW3 programs and files](#ww3-programs-and-files)
- &TIMESTEPS_NML — [WW3 programs and files](#ww3-programs-and-files)
