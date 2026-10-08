# WFIP-0001: WW3 samples driven by WeatherNext 3 wind on an RTX 4090 box

| | |
|---|---|
| **Status** | Draft |
| **Author** | M. Hoffmann (asked 2026-10-08 in the project thread; "WF-001") |
| **Created** | 2026-10-08 |
| **Deliverable** | D1, D6 |
| **Related** | [#45](https://github.com/h0ffmann/ww3-gpu/issues/45) §3 (verification table) and the plan's §5–§6 ([`W3SDS4_TRITON_PLANO_202610.pt.md`](../W3SDS4_TRITON_PLANO_202610.pt.md)); [`examples/02-regional-real-forcing`](../../examples/02-regional-real-forcing/README.md), whose grid and namelists this reuses |
| **Effort** | M: no kernel and no new program; one data path (Zarr → NetCDF → `ww3_prnc`), one new namelist, one verification script in shell, and the box set-up |
| **Gain** | `science` (the first measured answer to "does the WeatherNext 3 wind change Hs?"); `proposal` (D1: the first configuration frozen in the repo's format, with the box named; D6: a repeatable example); `lab/dev-loop` (the RTX 4090 becomes the CUDA test machine for `just kokkos-cuda-test`) |
| **Effort vs Gain** | do next: it needs nothing from the LabECO cases, which are the schedule's main risk (`07-methodology.md`, "Riscos"), and it exercises the whole chain from forcing to output on hardware the author owns |
| **Depends on** | access to WeatherNext 3 data (form, 5–7 working days `(v)` plan §5); GEBCO and GFS downloads of example 02; a working CUDA driver on the box; nothing from other WFIPs |
| **Blocked by** | none |
| **Risk** | the access form is refused or the Zarr v3 store cannot be read without Python, which the lab rule forbids in `examples/`; then the conversion runs outside the repo and only the NetCDF enters, as a documented step |
| **Cost so far** | — |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | [`WFIP-0001.tasks.md`](WFIP-0001.tasks.md) |
| **Tests** | none: no Python or C++ changes; the checks are the commands in §7 |
| **Spec-kit** | [`specs/001-weathernext3-wind/spec.md`](../../specs/001-weathernext3-wind/spec.md) |
| **Issues** | not filed: Draft |

## 1. Summary

Run the southern-Brazil shelf case of example 02 twice on the author's RTX 4090 workstation, once
with the GFS 10 m wind it already uses and once with WeatherNext 3's 10 m wind for the same
hindcast period, and publish the difference in `Hs` at the example's output points with a buoy or
altimeter check. The same box runs `just kokkos-cuda-test`, so the configuration frozen here
(toolchain, switches, namelists, hardware) is the first entry in the format D1 asks for.

## 2. Motivation

The plan's §5 found that WeatherNext 3 is the only open source of a 64-member 0.1° wind with a
CC BY 4.0 licence for any instant older than one hour `(v)` plan §5, 2026-10-08, and that a 5 %
CRPS gain in wind does not imply a gain in `Hs` `(v)` arXiv:2609.03582 §4.2. Nobody has run WW3
on it in this repository: the README's "Triton and ML weather forcing" row says *Planned*.
Example 02 already has the grid (52°W–44°W, 32°S–24°S at 0.1°, 81 × 81), the bathymetry step and
a wind namelist `(v)` [`examples/02-regional-real-forcing/README.md`](../../examples/02-regional-real-forcing/README.md),
but its own README says the GFS download "has not been possible" in the environment it was
written in, so the example has never run end to end either ⚠.

## 3. What changes for the reader

Before: `examples/02-regional-real-forcing/README.md` has the GFS path and a `⚠` that the file was
never verified.

After: the example README gains a second wind path and a results table of this shape, with the
commands that reproduce it:

```
Point        | Hs bias GFS | Hs bias WN3 | RMSE GFS | RMSE WN3 | n
Florianópolis|  … m        |  … m        |  … m     |  … m     | …
Rio Grande   |  … m        |  … m        |  … m     |  … m     | …
```

and `kokkos/PORT_STATUS.md` gains a CUDA timing line naming the box (Ada, 24 GB), with the
proposal's caveat that an RTX 4090 number does not stand for the H100 (`07-methodology.md`,
"Riscos", fourth risk).

## 4. Sources and dependencies reviewed

### WeatherNext 3 data
Zarr v3 on Google Cloud Storage (the only path with full members), BigQuery and Earth Engine
(surface statistics only); 0.1° surface, hourly initialisation, 00/06/12/18 UTC to 360 h;
CC BY 4.0 for data older than 1 h, real time under experimental terms; access by form, 5–7
working days `(v)` plan §5, each cell citing `developers.google.com/weathernext`, read 2026-10-08.
Egress cost from GCS ⚠ not quoted. The model weights are not published, so the wind is fetched,
not generated; the RTX 4090 plays no part in producing it ⚠ (nothing on the WeatherNext pages
read for the plan offers weights).

### GFS 0.25° wind
NOMADS `filter_gfs_0p25.pl`, no account, about ten days retained `(v)` example 02 README. The
baseline, kept as is.

### Bathymetry
GEBCO 2024 subset `(v)` example 02 README; nearest-neighbour sampling by `make_bathy.F90`.

### Zarr → NetCDF without Python
`examples/` may not hold Python (`AGENTS.md`, "What this repository is"). Candidates: GDAL's
multidimensional translate (`gdalmdimtranslate`), which reads Zarr v2 and, from a version to be
confirmed, Zarr v3 ⚠; `cdo`, which does not read Zarr ⚠; a one-off `xarray` notebook kept outside
the repository, with only the NetCDF and its `ncdump -h` entering. Pick: GDAL if it reads the
store, else the notebook, stated as such in the example README.

### Verification data
Brazilian buoys (PNBOIA) and altimeter tracks for the box ⚠ availability for the chosen period
not checked; AIFS Single Wave as the ML wave reference `(v)` README row.

### The box
RTX 4090 (Ada, 24 GB) on Ubuntu, toolchain from the pinned Nix shell (`just ww3`, `just dev`);
CUDA through the host driver. A sandboxed NixOS is the alternative (§11).

## 5. Design

Files touched: `examples/02-regional-real-forcing/` gains `ww3_prnc_wind_wn3.nml` (a copy of the
GFS namelist with the WeatherNext variable and coordinate names, `⚠` until executed, as every
namelist here), `get_wn3.sh` (fetch one cycle for the box and convert, or print the out-of-repo
step), `verify.sh` (bias and RMSE at `points.list` against the buoy file, with `cdo`/`nco`), and a
results section in its README. `kokkos/PORT_STATUS.md` gains the CUDA timing line. Nothing in
`WW3/` changes.

Flow, on the box: `just ww3` → `./get_gfs.sh` and `./get_wn3.sh` → `ww3_grid` once → `ww3_prnc`
per wind → `ww3_shel` per wind → `ww3_ounf`/`ww3_ounp` → `./verify.sh`. WW3 runs on the CPU
(OpenMP switch); the GPU is used by `just kokkos-cuda-test` and the `bench_snl1` timing only.
Everything is deterministic; no LLM in the loop.

## 6. Parity and physics impact

None: no kernel changes. The two runs differ only in the wind file, so the `Hs` difference is the
forcing's. The CUDA timing enters `PORT_STATUS.md` only with a passed L1 on the same build
(`AGENTS.md`, "No timing without parity").

## 7. Verification plan and definition of done

- [ ] Example 02 ran end to end with GFS wind on the box, producing `out_grd.ww3` and the point
      output: `cd examples/02-regional-real-forcing && ./get_gfs.sh <cycle> && ./run.sh`
- [ ] WeatherNext 3 access granted and one hindcast cycle (older than 1 h, CC BY 4.0) for the box
      saved as NetCDF with `u10`/`v10` on 0.1°: `./get_wn3.sh <cycle>` and `ncdump -h wn3_winds.nc`
- [ ] `ww3_prnc` read it and `ww3_shel` ran with it; the `Hs` difference map and the point series
      exist: `./run.sh --wind wn3`
- [ ] Bias and RMSE of both runs against at least one buoy or altimeter track in the box, as the
      table of §3: `./verify.sh`
- [ ] `just kokkos-cuda-test` passed on the box and `kokkos/PORT_STATUS.md` has its timing line
      naming the machine, with the H100 caveat
- [ ] The example README holds the table, the commands and the frozen configuration (D1 format);
      this WFIP's Status is Implemented and the README row for #45 no longer says *Planned*

## 8. Risks, limitations, and honest caveats

One cycle on one 8° box is a sample, not a verification campaign: it says whether the chain
works and roughly how far the two winds disagree, not which is better. The GFS baseline is
0.25° against WeatherNext's 0.1°, so part of any difference is resolution. A result of "no
difference in Hs" is a result (plan §5). The RTX 4090 timing does not transfer to the H100.

## 9. Alternatives considered

- **Do nothing until the LabECO cases arrive.** Loses the one experiment that needs no external
  case and leaves the example unverified.
- **Use the IFS open data instead of GFS as baseline.** Closer to the ReNOMO's forcing (plan §5),
  but 0.25° and GRIB2 as well; a later WFIP can add it as a third wind without redoing the chain.
- **Run on the H100 directly.** Access is not scheduled (`07-methodology.md`, fourth risk); the
  4090 is available now.

## 11. Open questions

- Ubuntu with the pinned Nix shell, or a sandboxed NixOS? **Default:** Ubuntu and `just ww3`,
  which exist today; a NixOS VM or `nixos-container` is tried only if the shell fails on the box,
  because GPU pass-through into a VM is the hard part ⚠ and nothing in this WFIP needs isolation.
  The author decides.
- Can GDAL read the Zarr v3 store without Python? **Default:** try `gdalmdimtranslate`; if it
  cannot, the conversion is a documented out-of-repo step and only the NetCDF enters.
- Which buoy or altimeter? **Default:** the PNBOIA buoy nearest Florianópolis if it has data for
  the cycle, else the altimeter tracks the LabECO group already uses; Pedro decides.

## Appendix
### Checked live
Everything external in this WFIP was checked for the plan on 2026-10-08 and is cited through it
(`W3SDS4_TRITON_PLANO_202610.pt.md` §5, §6); example 02's facts are from its README in this
repository.

### Not checked
WeatherNext 3 weights availability; GDAL Zarr v3 support; PNBOIA data for the period; GCS egress
cost; the WeatherNext variable names in the Zarr store.
