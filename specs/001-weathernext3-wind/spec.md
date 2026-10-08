# Feature Specification: WW3 samples driven by WeatherNext 3 wind

**Feature Branch**: `001-weathernext3-wind`

**Created**: 2026-10-08

**Status**: Draft

**Input**: User description: "Rodar alguns samples de WW3 com Google WeatherNext 3 na minha
RTX 4090, em Ubuntu ou num possível NixOS (sandboxed)." Design doc: `docs/WFIPs/WFIP-0001-weathernext3-wind-rtx4090.md`.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The example runs on the box (Priority: P1)

The author runs example 02 (southern-Brazil shelf, GFS wind) on the RTX 4090 workstation from a
clean clone, with the pinned toolchain, and gets the gridded and point output.

**Why this priority**: nothing else can be measured until the chain from forcing to output works
on hardware the author owns; it also freezes the first configuration in the D1 format.

**Independent Test**: `cd examples/02-regional-real-forcing && ./get_gfs.sh <cycle> && ./run.sh`
produces `out_grd.ww3` and the point output without manual edits beyond the documented `sed`.

**Acceptance Scenarios**:

1. **Given** a clone, `just ww3` and a GFS cycle within NOMADS' retention, **When** `./run.sh`
   runs, **Then** `ww3_grid`, `ww3_prnc`, `ww3_shel`, `ww3_ounf` and `ww3_ounp` finish and the
   README's download `⚠` is replaced by what was observed.
2. **Given** the same box, **When** `just kokkos-cuda-test` runs, **Then** every L1 test passes
   and the timing line names the machine.

---

### User Story 2 - The same case with WeatherNext 3 wind (Priority: P2)

The author fetches one WeatherNext 3 hindcast cycle for the box, converts it to the NetCDF
`ww3_prnc` reads, and runs the case a second time.

**Why this priority**: the question of #45 §3 and the plan's §5 (does the ML wind change `Hs`?)
has no measurement yet.

**Independent Test**: `./get_wn3.sh <cycle> && ./run.sh --wind wn3` produces a second output set
and a difference map against the GFS run.

**Acceptance Scenarios**:

1. **Given** approved access and a cycle older than 1 h (CC BY 4.0), **When** `./get_wn3.sh`
   runs, **Then** `wn3_winds.nc` has `u10`/`v10` on 0.1° over 52°W–44°W, 32°S–24°S and
   `ncdump -h` is recorded.
2. **Given** that file, **When** `./run.sh --wind wn3` runs, **Then** `ww3_prnc` accepts it with
   `ww3_prnc_wind_wn3.nml` and `ww3_shel` finishes.

---

### User Story 3 - A verified comparison (Priority: P3)

A reader of the example README sees bias and RMSE of `Hs` for both winds at the output points
against a buoy or altimeter, with the commands that reproduce them.

**Why this priority**: without an observation the difference between the two runs has no sign.

**Independent Test**: `./verify.sh` prints the table of WFIP-0001 §3 with `n` per point.

**Acceptance Scenarios**:

1. **Given** both runs and one observation series in the box, **When** `./verify.sh` runs,
   **Then** the table has a row per point and the README carries it with `(v)`.

### Edge Cases

- The WeatherNext store cannot be read without Python: the conversion is an out-of-repo step,
  documented, and only the NetCDF enters (`AGENTS.md`: no Python in lab code).
- Access is refused or late: stories 1 and the CUDA tests still complete; story 2 waits.
- The box's driver and the pinned CUDA disagree: record the versions and fall back to the
  OpenMP build for WW3; the Kokkos CUDA test is reported as not run.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The example MUST run from the pinned Nix shell on Ubuntu with no edit outside the
  documented `sed` retiming.
- **FR-002**: `get_wn3.sh` MUST fetch one cycle for the box and either produce the NetCDF or print
  the out-of-repo conversion step; it MUST NOT be Python.
- **FR-003**: `ww3_prnc_wind_wn3.nml` MUST follow `$WW3/model/nml/ww3_prnc.nml` and stay `⚠`
  until executed.
- **FR-004**: `run.sh --wind <gfs|wn3>` MUST select the wind without duplicating the other steps.
- **FR-005**: `verify.sh` MUST compute bias and RMSE per point with `cdo`/`nco` and print the §3
  table.
- **FR-006**: Every number published MUST carry the command that reproduces it and `(v)`/`⚠`.
- **FR-007**: The CUDA timing MUST enter `kokkos/PORT_STATUS.md` only with a passed L1 on the same
  build, naming the machine and the H100 caveat.

### Key Entities

- **Wind file**: NetCDF with `u10`, `v10`, `longitude`, `latitude`, `time`; source GFS or WN3.
- **Run**: one `ww3_shel` execution; identified by wind source and cycle.
- **Observation**: a buoy or altimeter series inside the box for the cycle's period.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Both runs complete on the box and their configuration is frozen in the README.
- **SC-002**: The §3 table has at least one point with `n ≥ 24` hourly observations.
- **SC-003**: `just wfip check` passes with WFIP-0001 at DoD 6/6 and Status Implemented.

## Assumptions

- The author's box has an RTX 4090, Ubuntu, and can run the pinned Nix shell.
- WeatherNext 3 access is granted for research use within the schedule.
- The GFS baseline stays at 0.25°; the resolution gap is reported, not corrected.
