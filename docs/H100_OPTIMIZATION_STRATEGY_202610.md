# Optimising the port for the H100: what goes where, whether all-CUDA pays, and what NOAA runs

2026-10-16. Asked by the owner: once the H100 regtest has run, how the program can be optimised
for the H100, which routines could run on OpenMP and which on CUDA, and whether 100 % CUDA would be
better anyway given how NOAA runs WW3 operationally, with honest concerns. Issue
[#42](https://github.com/h0ffmann/ww3-gpu/issues/42). It builds on
[`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md) (what Kokkos allows, the PCIe arithmetic of a
split by physics, the bit-for-bit constraint on a split) and does not repeat it; the plan it
serves is [`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md), and the regtest cadence is
[ADR-0012](ADRs/ADR-0012-translate-and-wire-now-regtest-matrix-on-the-h100.md) (Proposed).

Nothing here is a measurement on an H100. No `--ww3-gpu --gpu` run of the port inside WW3 exists
(`kokkos/PORT_STATUS.md`, "On a GPU: none yet"). Every speed statement below is either a recorded
CPU measurement, with its command, or reasoning marked ⚠.

## Short answer

- Put the per-point physics on the GPU once the spectrum lives there: `W3SRCE` and everything it
  calls. Keep I/O and set-up on the host whatever the backend. Propagation decides whether the
  spectrum can stay on the device, and it is not wired yet.
- 100 % CUDA is not a goal in itself. A whole time step on the GPU with the spectrum resident is
  the configuration that every successful GPU port in the plan's survey reached
  (`KOKKOS_H100_PLAN_202609.md` §5, "Cross-cutting conclusions", item 2); "100 %" of
  WW3, including its Fortran I/O and set-up, is neither reachable nor useful.
- NOAA runs WW3 in production with MPI on CPU-only nodes (section [NOAA](#how-noaa-runs-ww3)). A
  single-H100 WW3 serves this lab's use. What keeps the port useful to an operational centre is
  the CPU backend held bit for bit with the Fortran, and an MPI path, which the port does not
  have yet.

## Where the port stands

From `kokkos/PORT_STATUS.md` (`python3 scripts/port_score.py --write`), against the committed
profile `docs/data/ww3_full_gprof.tsv` (`just profile-case full`: gprof self time, `bench_small`,
120 x 80, 9 520 sea points, 12 h, `switch_lab_shrd`, one thread, `WW3@6d1bcdfe`, 2026-10-14) (v,
read 2026-10-16). Two branches are being integrated and their scores differ; both are given.

| Level | Wiring branch (`WW3/` at `357f575c`) | Translation branch (`e6d0c6e`) |
|---|---|---|
| translated (Kokkos code exists) | 99.3 % of run time, 294 of 1 023 routines | 99.3 %, 305 routines |
| ported (WW3 calls it behind its flag) | 87.6 %, 19 routines | 87.4 %, 10 routines |
| regtested (WW3's regtests agreed) | 48.6 %, 10 routines | 48.4 %, 2 routines |

The merged score is ⚠ until `python3 scripts/port_score.py --write` runs on the integrated tree.

What the shares mean, by class of routine, on that profile:

| Class | Routines (share of self time) | Wired | Regtested |
|---|---|---|---|
| Per-point source terms | `W3SNL1` 48.4 %, `W3SDS4` 23.2 %, `W3SIN4` 12.7 %, `W3SRCE` 5.6 %, `W3SPR4` 3.1 %, `W3SDB1`, `W3SBT1`, `W3SLN1` 0.7 % | yes | `W3SNL1` by the matrix; ST4 and `W3SRCE` by smoke and full only |
| Propagation | `W3XYP3` 3.0 %, `W3QCK3` 1.1 %, `W3QCK1` 0.9 %, `W3KTP3` 0.3 % | no | no |
| Transposes | `W3SCAT` 0.16 %, `W3GATH` 0.11 % (`SHRD` branch only) | yes | smoke and full |
| Set-up, I/O, the rest | `init` 0.7 %, everything else under 0.02 % each | partly (`DISTAB`, `W3UWND`, maps) | partly |

Two caveats on the profile. ⚠ The listed self times add to 131.4 s of the 146 s run
(`awk -F'\t' '!/^#/ && $1!="routine" {s+=$2} END {print s}' docs/data/ww3_full_gprof.tsv`); the
other 14.6 s, 10 %, are not attributed to any routine, and gprof does not see time spent in
libgfortran's I/O, libm or the kernel. And `bench_small` is a small regional grid with one output
stream; an operational global run writes more output per step of compute ⚠. The I/O share on
the H100 is therefore unknown, and it is the serial fraction that bounds everything else.

## Which routines on the CPU, which on the GPU

The test for each class is the one [`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md) sets: a
kernel earns the GPU only if its gain exceeds the cost of moving its data across PCIe, and two
copies of the spectrum per step cost as much as sixty to a hundred passes over it in HBM there
(illustration, ⚠).

### Per-point source terms: the GPU, once the spectrum is resident

`W3SRCE` integrates the source terms of one sea point at a time over its own spectrum, `VA(NSPEC,
ISEA)` contiguous per point (`model/src/w3wdatmd.F90:491`, `ALLOCATE (WDATAS(IMOD)%VA(NSPEC,0:NSEALM))`,
at `WW3@8e433f99`, v 2026-10-16). Points do not talk to each other inside `W3SRCE`, so the work
is a loop over 10^4 to 10^6 independent points, each with NSPEC = 600 to 1 152 values and a
dynamic sub-step loop: one team per point, the layout the plan already gives
(`KOKKOS_H100_PLAN_202609.md`:241). This is where a GPU is strongest, and where the one CUDA
measurement in the repository is: the DIA kernel alone 0.047 ms per 1 000 points on the RTX 4090
against 24.96 ms on one i9 thread and 5.02 ms on 32 (kernel only; end to end through the shim
0.75 ms against 25.88 ms and 5.95 ms) (v, `bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`,
`kokkos/tests/bench_snl1.cpp`; parity then was the L1 of its day, so under ADR-0010 this record
is history, not a timing for a table).

The class holds 93 % of the profile (`W3SNL1`, ST4, `W3SRCE` and the small source terms). It
goes on the GPU as one kernel, `W3SRCE` with its callees inside, not as separate kernels per term:
`WW_KOKKOS_SRCE` already replaces the whole of `W3SRCE` (`PATCH.md` §7). Per-term kernels would
each read and write the spectrum.

The OpenMP path of this class stays as important as the CUDA one: it is what an MPI run on CPU
nodes uses, and it is the only path the bit-for-bit rule has shown (see concern 1).

### Propagation: the decision that keeps the spectrum on the device

Spatial propagation (`W3XYP3` with `W3QCK1`/`W3QCK3` for `PR3` `UQ`) works on one spectral
component at a time over the whole grid. WW3 transposes for it: `W3GATH` copies row
`VA(ISPEC, :)` into a 2-D field, the propagation advects it, `W3SCAT` writes it back (`PATCH.md`
§8: one row per call, `O(NSEA)` values). Spectral propagation (`W3KTP3`) works per point again.

On a GPU this matters more than its 4 % share says:

- If propagation stays on the host, the spectrum crosses PCIe twice per step, and every gain of
  the source terms is spent on the copies ([`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md),
  "What a split by physics costs").
- If it moves to the GPU, the transposes become strided reads of `VA` on the device. With `VA`
  as `[NSEA][NSPEC]` (spectrum contiguous, the source terms' layout), reading one component for
  every point is a stride of NSPEC floats; with `[NSPEC][NSEA]` it is contiguous for propagation
  and strided for the source terms. Propagation is memory-bound (a few operations per value), so
  the layout sets its speed. Kokkos lets the layout be a template argument (`LayoutLeft`,
  `LayoutRight`), but every shim and kernel reads the layout it was written for.
- The choice is measured, not argued: time the device-resident step with each layout, or with
  `W3GATH`/`W3SCAT` kept as an explicit on-device transpose between two layouts, and keep the
  faster. Changing a loop order in propagation changes no arithmetic only if every reduction keeps
  its order; a reordering is a physics change with its own PR (`AGENTS.md`, "Porting").

The MPI branches of `W3GATH`/`W3SCAT` and `PDLIB`, which exchange data between ranks, are not
ported (`PATCH.md` §8); NOAA's operational switches use them (`WW3-noaa/model/bin/switch_NCEP_st4`
has `DIST MPI`) (v, read 2026-10-16).

### Host I/O and set-up: the host, whatever the backend

`W3IOGR`, `W3IOGO`, `W3IORS`, `W3IOBC`, `W3IOPO`, `W3IOPP`, `W3FLDO`/`W3FLDG`, `W3INIT`,
`W3SETG` and their kind stay on the CPU. Their Kokkos translations (goal 42) are host code: they
reproduce Fortran unformatted records, libgfortran's `IOSTAT` values and pointer association
(`kokkos/ports.tsv`, their `evidence` column), and the six are not wired, one reason each
(`PATCH.md` §8). A GPU cannot write a file; what moves to the device is only the computation of
output fields from the spectrum (the `W3OUTG` integrals), which WAM6-GPU found it had to do on
the device to avoid copying the spectrum back for output (`KOKKOS_H100_PLAN_202609.md`:205).
`W3OUTG` is translated and not wired.

### What the H100 node's host cores can do meanwhile

The H100 sits in a server with its own CPUs; OpenMP runs there, CUDA on the GPU
([`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md), "Can the H100 do both"). Which CPU, how
many cores, and whether the H100 is the PCIe or the SXM part are not recorded ⚠. With a
device-resident step, the host cores are free for:

1. writing output while the GPU computes the next step (output of step n overlaps compute of
   step n+1, if a device-to-host copy of the output fields is cheaper than the step);
2. running a second sub-domain on the CPU, a split by sea points, which exchanges only a boundary
   (the cheaper split of `CPU_GPU_SPLIT_202610.md`);
3. running the regtest matrix on the CPU, which is ADR-0012's use of the node.

## Would 100 % CUDA be better?

For this lab's runs, a whole time step on the GPU with the spectrum resident is the target, and
in that sense yes. "100 % CUDA" of WW3 is not, for six reasons, each with what the repository
measured or did not.

1. **Phase 1 copies are slower than the Fortran, already on the CPU.** Phase 1 copies the
   spectrum in and out at every call. On `bench_small`, one thread, the run with the Kokkos DIA
   took 217.9 s against 152.7 s for the Fortran (v, `docs/PORT_ASSESSMENT_202610.md` §6,
   `just full-ww3-gpu`; machine not stated in the port report ⚠). On a GPU each copy crosses
   PCIe instead of staying in cache. No GPU number can be claimed until phase 2.
2. **PCIe is the ceiling of any partial offload.** The OpenACC port of WW3 v6.07's source terms
   for E3SM gained about 2x on a P100 and 4x on a V100 with one MPI rank per GPU, and about 1.4x
   for a whole Summit node (6 GPUs against 42 CPU cores), limited by host-device transfers
   (v, Ikuyajolu et al., GMD 16, 1445-1458, 2023,
   [doi:10.5194/gmd-16-1445-2023](https://doi.org/10.5194/gmd-16-1445-2023), read 2026-10-16;
   `KOKKOS_H100_PLAN_202609.md`:204 says 1.3x, which this reading corrects).
3. **I/O and set-up are serial host work.** If a fraction s of the run stays on the host, the
   best speed-up is 1/s. With s = 10 % (the unattributed share of the profile, an upper guess ⚠)
   that is 10x, however fast the GPU. The operational share of output is not measured here ⚠.
4. **Bit for bit on CUDA needs libdevice to return glibc's bits.** CUDA's `expf`, `logf`, `powf`,
   `tanhf`, `sincosf`, `atan2f` come from libdevice, whose algorithms differ from glibc's; the
   ST4 and `W3SNL3` ports are bit for bit on the CPU only because they reproduce gfortran's calls
   into glibc's libmvec (`PATCH.md` §7, the parity column; `CPU_GPU_SPLIT_202610.md`). Whether
   any of the transcendental kernels can be bit for bit on the GPU is unknown ⚠; for most it is
   unlikely without a device copy of glibc's algorithms.
5. **`--fmad=false` costs throughput.** Contraction is off because it changes the bits
   (`AGENTS.md`, "Porting"). An FMA does a multiply and an add in one instruction; without it the
   GPU issues two. How much that costs on these kernels is not measured ⚠; the 2026-09-15 DIA
   kernel record was already built with it.
6. **Reductions keep the Fortran's order per point.** A tree reduction over NSPEC inside a team
   is what makes a GPU fast; the rule forbids reordering a sum without the owner. One team per
   point with a serial reduction inside it keeps the order and leaves the parallelism over points,
   which is plenty for 10^4 points and up.

So 100 % CUDA is better only for a single-node, single-GPU, whole-step-resident configuration,
and only if the owner accepts a tolerance (an L2 verdict) for the transcendental kernels on the
GPU, or someone shows libdevice bits equal glibc's for every input those kernels see. Neither
condition holds today.

## How NOAA runs WW3

Every fact below was read on 2026-10-16. NOAA's production configuration files are public in
NCO's production code tree, <https://www.nco.ncep.noaa.gov/pmb/codes/nwprod/>, read as
downloaded files; a (v) on them means the file was read, not that the run was observed.

### The operational systems

| System | What runs | Source |
|---|---|---|
| GFS-Wave, part of GFS v16 since 2021-03-22 | WW3 forced one way by the atmosphere through NEMS; three computational grids in a `ww3_multi` mosaic: global 10 arcmin (2160 x 406, 15S to 52.5N), Southern Ocean 15 arcmin (1440 x 277), Arctic 9 km curvilinear (1006 x 1006); the 0.25° global and 0.16° regional fields are interpolated products. Forecasts to 384 h, four cycles a day | (v) SCN 21-20, [scn21-20gfs_v16.0_aac.pdf](https://www.weather.gov/media/notification/pdf2/scn21-20gfs_v16.0_aac.pdf); (v) [GFS-Wave model description](https://polar.ncep.noaa.gov/waves/Model_Description.pdf); (v) NCO `gfs.v16.3.33`, `parm/config/config.wave` |
| GFS-Wave spectral grid and physics | 50 frequencies x 36 directions = 1 800 components, from 0.035 Hz with factor 1.07; ST4 and the DIA (`&SIN4`, `&SDS4`, `&SNL1`) | (v) NCO `gfs.v16.3.33`, `fix/fix_wave_gfs/ww3_grid.inp.gnh_10m` |
| GFS-Wave resources | 448 MPI tasks for the wave component (`npe_wav_gfs`) beside 1 728 FV3 tasks, nodes of 128 cores; the forecast job's wall-clock *limit* is 2:30. No measured wall time of the wave part is published that we found ⚠ | (v) NCO `gfs.v16.3.33`, `parm/config/config.fv3.nco.static`, `config.resources.nco.static` |
| GEFS-Wave, GEFS v12 since 2020-09-23 | 31 members (30 perturbed and the control), 0.25°, 16 days, wind every hour; three grids in `ww3_multi` (global 15 arcmin 1440 x 501, two polar caps); 33 x 36 = 1 188 components, factor 1.1 | (v) SCN 20-75, [scn20-75gefs_v12_changes.pdf](https://www.weather.gov/media/notification/pdf2/scn20-75gefs_v12_changes.pdf); (v) NCO `gefs.v12.3.22`, `parm/gefs_wave.parm`, `fix/fix_wave/ww3_grid.inp.glo_15m`; wave task counts not found ⚠ |
| GLWU, Great Lakes, since 2017 | WW3 on an unstructured mesh, 2.5 km offshore to 250 m at the coast, hourly cycles; the current mesh has 253 010 nodes | (v) SCN 17-75, [scn17-75glwu.pdf](https://www.weather.gov/media/notification/pdfs/scn17-75glwu.pdf); (v) NCO `glwu.v2.0.7`, `fix/mesh.glwu` |
| GFS v17, proposed | Fully coupled (FV3 C1152, MOM6 and CICE6 at ¼°), "Continued use of the WAVEWATCH III model for the wave component while introducing an unstructured grid", the waves receiving ice and currents and feeding back to the atmosphere and ocean; proposed for October 2026, with a Service Change Notice 30 days before implementation | (v) PNS 26-29, 2026-04-15, [pns26-29_Science_for_GFSv17.pdf](https://www.weather.gov/media/notification/pdf_2026/pns26-29_Science_for_GFSv17.pdf). No SCN for GFS v17 is on the [notification index](https://www.weather.gov/notification/) on 2026-10-16 (v), so the October date has not been met (an inference) |

The switch files NOAA keeps in WW3 under its own names say the same: `switch_NCEP_st4` is
`DIST MPI PR3 UQ ST4 NL1 ...`, `switch_NCEP_glwu` is `DIST MPI ... ST4 NL3`, and
`switch_NWS_rwps` is `DIST MPI ... PDLIB SCOTCH ... ST4 NL1 ... IC4 REF1` (v,
`WW3-noaa/model/bin/`, `03adda91`). Which switch file each production job compiles is set in
NOAA's build scripts, not read here ⚠.

### The hardware

- **WCOSS2** is two HPE Cray EX systems, Cactus and Dogwood, each 327 680 cores of AMD EPYC 7742
  (64 cores, 2.25 GHz) on Slingshot-10, with no accelerator listed (v,
  [TOP500, Cactus](https://top500.org/system/179944), checked twice); 12.1 PF each in 2022, 14.5
  PF after the 2023 upgrade (v, NOAA releases of
  [2022-06-28](https://www.noaa.gov/news-release/us-supercomputers-for-weather-and-climate-forecasts-get-major-bump)
  and [2023-08-10](https://www.noaa.gov/news-release/noaa-completes-upgrade-to-weather-and-climate-supercomputer-system),
  neither mentions GPUs).
- **The successor is the cloud.** On 2026-07-27 NOAA named Google Cloud the primary provider of
  high-performance computing for WCOSS; GFS and GEFS move through early 2027, everything by
  December 2027 (v, [NOAA release](https://www.noaa.gov/news-release/noaas-use-of-cloud-infrastructure-grows-to-include-weather-prediction-models),
  checked twice). Google's announcement names H4D virtual machines with fifth-generation AMD
  EPYC processors as the computing backbone (v, read by a research agent,
  [Google Cloud blog](https://cloud.google.com/blog/topics/public-sector/modernizing-the-skies-noaa-google-cloud-collaborate-advance-weather-forecasting)).
  Neither mentions GPUs; NOAA's release speaks only of "the latest chips".
- **NOAA's GPUs are research machines.** Ursa (RDHPCS) has 58 production GPU nodes with two
  NVIDIA H100 NVL (94 GB) and 192 AMD Genoa cores each, plus exploratory GH200 and MI300X nodes
  (v, [Ursa user guide](https://docs.rdhpcs.noaa.gov/systems/ursa_user_guide.html), checked
  twice). Rhea (NESCC, about 30 PF) "features graphics processing units" for AI and machine
  learning (v, [NOAA release, 2024-09-03](https://www.noaa.gov/news-release/biden-harris-administration-invests-100-million-in-new-high-performance-computer-system));
  its GPU model and service date are not stated.

### GPU plans for WW3 and WW4

- No NOAA or EMC paper, plan or notice for running WW3 on GPUs was found ⚠ (a negative search
  result, not a proof). The published GPU work on WW3 is the E3SM OpenACC port of the source
  terms (Ikuyajolu et al. 2023, above). The fully GPU global wave model in print is WAM, not
  WW3: WAM6-GPU, a 7-day global 1/10° run in 7.6 min on 8 A100 (Yuan et al., GMD 17,
  6123-6136, 2024, [doi:10.5194/gmd-17-6123-2024](https://doi.org/10.5194/gmd-17-6123-2024),
  figures from the abstract ⚠).
- NOAA's 2015 Software Engineering for Novel Architectures (SENA) project named WRF, MPAS or
  FV3, GFDL's climate models and NMMB as its priorities for GPU and many-core work; waves are
  not listed (v, [Hart, NOAA OCIO, 2015](https://www.hpcuserforum.com/presentations/colorado-sept2015/NOAA-Hart.pdf),
  read by a research agent). No current NOAA GPU-readiness document was found ⚠.
- **WAVEWATCH IV** is NOAA's C++ rewrite. Its Phase I report says the rise of GPUs requires
  focusing on memory use and access, and Phase IV of its plan, "Initial Model Capability", is a
  single domain with CPU *and* GPU efficiency as its focus (v, NCEP Office Note 525,
  [doi:10.25923/h7j3-1h25](https://doi.org/10.25923/h7j3-1h25), and Office Note 528,
  [doi:10.25923/0wyp-9f39](https://doi.org/10.25923/0wyp-9f39), as read for
  [lesson 14](../course/14-ww4-and-the-future.md)). Its code has no Kokkos, no GPU path and no
  physics yet, and the CPU-GPU architecture question was closed without a recorded decision
  (lesson 14, snapshot of 2026-10-07 in `docs/ww4-status.json`: `kokkos_files` empty; the
  repository page, read 2026-10-16, mentions no GPU, Kokkos or OpenACC). WW4's default spectral
  grid, 50 x 36 from 0.035 Hz with factor 1.07 (`docs/ww4-status.json`, `spectral_space`), is
  GFS-Wave's.

### What follows from it

As far as these sources go: NOAA's wave production is WW3 under MPI on CPU-only nodes today,
and the replacement announced for 2027 is a CPU cloud; no public plan puts operational WW3 on
GPUs; the GPU interest at NOAA is for AI and for WW4, which is not yet a model. Two consequences
for this port:

- **A 100 %-CUDA, single-H100 WW3 serves this lab's use**, several runs a day of one
  configuration on one node at UFSC, more than it serves NOAA's production path. It is a
  legitimate target, and it is where the per-point physics pays.
- **What keeps the port useful to an operational centre is the CPU backend held bit for bit with
  the Fortran, plus the MPI path.** A centre on CPU nodes would take a C++/Kokkos WW3 only if it
  gives the Fortran's answer under `DIST MPI` on its switches; that is the OpenMP and Serial
  backends, not CUDA. NOAA's production configurations also differ from the lab's in three ways
  the port does not cover yet: `ww3_multi` with several grids, `DIST MPI` (the MPI branches of
  `W3GATH`/`W3SCAT` are not ported), and, with GFS v17, an unstructured grid, whose implicit
  `PDLIB` solver dominates its profile (`pdlib_jacobi_gauss_seidel_block` 37.5 % and
  `calcarray_jacobi_vec` 20.4 % of self time on `ww3_tp2.17`, 3 070 nodes, one rank, v,
  `docs/data/ww3_pdlib_gprof.tsv`, `just profile-case pdlib`).

None of this says a GPU WW3 would not interest NOAA; the evidence is only that nothing public
plans one.

## Concerns

Each with what would resolve it.

1. **Bit for bit on the GPU may be unattainable for the transcendental kernels.** ST4, `W3SRCE`,
   `W3UWND`, `DISTAB` and most source terms call `expf`, `logf`, `powf`, `tanhf`, `sincosf` or
   `atan2f`, and some reproduce glibc's libmvec on the CPU (`PATCH.md` §7). libdevice is a
   different implementation. *Resolves it:* an exhaustive comparison of libdevice against glibc
   over the 2^32 `float` inputs of each one-argument function (ADR-0001's sweep, run on the GPU
   under the owner's supervision), and for the rest a decision by the owner, before any GPU work,
   on whether the GPU side is held to bits or to a tolerance (an L2 verdict).
2. **Phase-1 copies dominate until the spectrum is resident.** On the CPU the Kokkos DIA run is
   already 43 % slower than the Fortran on `bench_small` (217.9 s against 152.7 s). *Resolves it:*
   phase 2, the whole step on the device; until then no GPU number is quoted.
3. **Propagation and its layout are unported and undecided.** Without them the spectrum cannot
   stay on the device. *Resolves it:* wire `W3XYP3`, `W3KTP3`, `W3QCK*` (translated), then time
   both `VA` layouts in a device-resident step.
4. **MPI with GPUs is out of scope, and NOAA runs MPI.** `ww3_multi` is supported by the shims
   (one context per grid), but no MPI branch of a transpose and no `PDLIB` exchange is ported, and
   phase 1 links no MPI (`WW_KOKKOS_DEVICE_ID` instead of the rank). *Resolves it:* a WFIP for
   `DIST MPI` on the CPU backend first (bit for bit against the Fortran under MPI, which the
   matrix's 344 MPI entries can judge), then one GPU per rank.
5. **The single-H100 target is not NOAA's shape.** GFS-Wave gives the wave component 448 MPI
   tasks on three grids with 1 800 spectral components; the lab's case is one grid on one node.
   At GFS-Wave's (and WW4's default) 50 x 36 grid, the DIA's level-0 scratch per point does not
   fit the RTX 4090's limit and the kernel falls back to level-1 scratch (`PORT_STATUS.md`, "Level-0
   scratch at WW4's default grid"); on the H100 neither the fit nor the cost is measured ⚠. *Resolves it:* say in the proposal and the book which use the port
   serves, and time GFS-Wave's spectral grid on the H100 once a supervised run is possible.
6. **The profile may not represent an operational run.** It is `bench_small` (9 520 points, 12 h,
   one output stream, one thread, gprof); 10 % of its wall time is unattributed, and the order of
   the hot routines changes with the case (`ww3_ts1` puts `W3SDS4` first, `ww3_tp2.17` the
   `PDLIB` solver). *Resolves it:* profile LabECO's operational configuration (its grid, NK = 32,
   NTH = 36, its output) with a wall-clock timer per phase, not only gprof, and record it in
   `docs/data/`.
7. **I/O is Amdahl's serial fraction.** Every output and restart path is Fortran unformatted I/O
   on the host, and the GPU makes the rest faster around it. *Resolves it:* measure the I/O share
   on the operational case; if it exceeds a few percent, overlap output of step n with compute of
   step n+1 on the host cores, which needs the output fields copied off the device each output
   time, not the spectrum.
8. **The fork must follow upstream.** The port lives in `h0ffmann/WW3` branches with hunks in
   `w3srcemd`, `w3wavemd`, `w3initmd` and a new `w3kokkosmd`; upstream `develop` moves, and GFS
   v17 brings WW3 changes of its own. Today the fork's base equals upstream `develop` (`PATCH.md`,
   "The fork today"), so the cost is not yet visible. *Resolves it:* a rebase of the fork at each
   upstream release, with the checkpoint of ADR-0011 item 2 after it, budgeted as recurring work.
9. **Validation on the H100 has one supervisor.** GPU runs need the owner present (`AGENTS.md`,
   "Running on the GPU"); the matrix on the H100 node's CPUs does not, but its machine, its
   toolchain (Nix on the node ⚠) and access are not recorded. *Resolves it:* record the node in
   [`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md) and in a `bench/results/` machine record,
   and schedule the supervised GPU sessions as a fixed slot rather than ad hoc.
10. **`regtested` "yes" has two meanings today.** Rows passed by smoke and full alone say "yes",
    as does `W3SNL1` with its matrix (ADR-0012, Consequences). A table built on that column would
    overstate what the matrix has judged. *Resolves it:* the owner's answer in ADR-0012.
11. **Wired-but-unregtested code grows with every batch.** `WW_KOKKOS_ALL=1` turns on parts no
    regtest has judged; a result produced that way and quoted would break "no timing without
    parity". *Resolves it:* ADR-0012 item 3, and the parts line in `log.ww3` kept in every
    record.

## Recommended order after the H100 regtest

Each step has a gate; a step does not start until the gate before it passes.

1. **The H100 node's CPU matrix** (ADR-0012). Run `WW3GPU_ROUTINES=<every source-term routine>
   just ww3-gpu-matrix <jobs> all` on the node's host cores with `WW3GPU_STRICT=ON`, then the FULL
   mode (Kokkos run with `WW_KOKKOS_ALL=1`) once it exists. *Gate:* every entry byte-identical or
   excused as `ww3_self`; each failure in a `docs/log/` entry and bisected by flag. Only then do
   rows move to L2.
2. **The owner's decision on the GPU parity rule** (concern 1). *Gate:* a written answer: bits,
   or a tolerance with its fields and values, for each class of kernel.
3. **Propagation wired and checkpointed on the CPU** (concern 3). *Gate:* the matrix's `PR3`
   entries byte-identical with the propagation flags on.
4. **Phase 2 on the CPU first:** the shim hands all sea points of a step to one kernel launch,
   and the spectrum stays in Kokkos Views across the step. On the OpenMP backend this removes the
   per-call copies and the `!$OMP CRITICAL`, and it can be validated without a GPU. *Gate:* the
   matrix again, and a CPU timing (median of three) that is no slower than the Fortran.
5. **The operational profile** (concern 6). *Gate:* a committed `docs/data/` table for LabECO's
   configuration, with the I/O share measured; it reorders steps 6 and 7 if I/O is large.
6. **The supervised GPU run of the device-resident step**, `W3SRCE` and propagation on CUDA,
   I/O on the host. *Gate:* the verdict of step 2 met, then the first GPU timing, kernel-only and
   end to end, with the machine named.
7. **Only then, per-kernel placement**: time each kernel on OpenMP and on CUDA, try the split by
   sea points ([`CPU_GPU_SPLIT_202610.md`](CPU_GPU_SPLIT_202610.md), "Next"), and the layout
   question of propagation with numbers. *Gate:* each placement change is its own PR with its
   regtest record.
8. **MPI** (concern 4), as its own WFIP, CPU first. This is the step that makes the port usable
   by a centre like NCEP; it can run in parallel with 6 and 7 because it is CPU work.
