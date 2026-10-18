# ADR-0018: Where `ww_kokkos` lives if WW3 upstream takes the Kokkos runtime

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-18 |
| **Deciders** | Hoffmann |
| **Written by** | an agent, from gap 12 of [`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md) and unit U6 of the [production single-node plan](../compound/plans/2026-10-18-002-production-single-node-plan.md); no option is chosen |
| **Scope** | Where the C++ library `ww_kokkos` and its C ABI would live for a WW3 upstream that takes the `KOKKOS` switch: an external package WW3 finds, a submodule of WW3, or sources inside `model/src`. Not whether upstream wants it, not the Fortran half (it is in WW3 in every option), not the single-node scope ([ADR-0017](ADR-0017-kokkos-port-is-single-node-mpi-stays-fortran.md)) |
| **Related** | [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [ADR-0016](ADR-0016-speed-first-regtests-postponed-one-person-loop.md); [ADR-0017](ADR-0017-kokkos-port-is-single-node-mpi-stays-fortran.md); [`kokkos/README.md`](../../kokkos/README.md), "Install-only build"; [`kokkos/src/fortran_iface/PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md) |

## Context

The port has two halves. The Fortran half is on the fork (`h0ffmann/WW3`, pinned at
`WW3@80644ba9`); the C++ half is `kokkos/` in this repository. `diff -rq WW3-noaa/model WW3/model`
(v, 2026-10-18) lists what the fork adds to NOAA's WW3 at the commit it branched from:

| Fork change | Size (v, 2026-10-18) |
|---|---|
| Six new modules, `model/src/w3kokkos*md.F90` | 4 082 lines |
| `w3initmd.F90`, `w3srcemd.F90`, `w3wavemd.F90` | 114, 456 and 249 changed lines (`diff \| grep -c '^[<>]'`) |
| `model/src/CMakeLists.txt` | 64 changed lines, the `WW_KOKKOS` block |
| `model/src/cmake/switches.json` | the `kokkos` category |

The C++ that the `KOKKOS` switch links is larger (v, `cat kokkos/src/*/*.{cpp,hpp} | wc -l`,
2026-10-18): 48 571 lines in `kokkos/src/ww_kokkos/` (the kernels) and 9 647 in
`kokkos/src/fortran_iface/` (the C ABI and shims), 347 files.

How WW3 reaches it today: `find_package(ww_kokkos 0.9 REQUIRED)` and `target_link_libraries(ww3_lib
PUBLIC ww_kokkos::ww_kokkos)` (v, `model/src/CMakeLists.txt:43-44` at `WW3@80644ba9`). Since this
change the package can be configured with Kokkos as its one dependency
(`-DWW_KOKKOS_BUILD_TESTS=OFF`), and its config asks for the exact Kokkos version, backends and
architecture it was built with (v, `kokkos/README.md`, "Install-only build", configure traced
2026-10-18). Its own version contract is `SameMajorVersion` at 0.9.0 with entry points only added
(v, `kokkos/CMakeLists.txt`); nothing checks the "only added" ⚠.

Licences, as the files state them (v, `grep SPDX-License-Identifier`, 2026-10-18):

- WW3 is under the GNU Lesser General Public License v3 (v, `WW3/LICENSE.md`, first lines). The
  fork's new modules carry `LGPL-3.0-or-later` (v, `model/src/w3kokkosmd.F90:33`).
- Of the 347 C++ files under `kokkos/src/`, 249 carry `LGPL-3.0-or-later`, all in `ww_kokkos/`:
  the kernels translated from WW3's Fortran. 22 carry `MIT`: the shims, the timers, the fixture
  reader and `real.hpp`. 76 carry no line (71 in `ww_kokkos/`, 5 in `fortran_iface/`); the
  repository is MIT (v, `LICENSE`), but whether a file translated from WW3 without the line is
  MIT or a derived work under the LGPL is not settled here ⚠.
- Gap 12 of `PRODUCTION_READINESS_202610.md` read "the C++ it calls is MIT"; the count above
  shows most of the C++ is LGPL-3.0-or-later, as `README.md`, "licensing", already says of the
  translated kernels.

⚠ No legal reading was made for any statement on what a licence permits in this ADR. Each option
names the licence question it raises; a person qualified to answer it has to.

## Decision

None yet. This ADR records the three options with the evidence above, for the owner and, if it
comes to that, for WW3's maintainers to choose from. What the evidence supports today:

- Option (a) is what exists and was built end to end on 2026-10-18: install-only configure,
  install, WW3 built against the prefix, `smoke` byte-identical pair to pair
  (`PRODUCTION_READINESS_202610.md`, gap 5).
- Options (b) and (c) were not built. Their costs below are read from the files, not measured ⚠.

## Alternatives considered

### (a) An external package WW3 finds (today's layout)

WW3 keeps `find_package(ww_kokkos <major.minor> REQUIRED)` behind `-DWW_KOKKOS=ON`; `ww_kokkos` is
released and installed on its own, like NetCDF or ESMF.

- The upstream diff is the Fortran half only: 4 082 new and 819 changed lines of Fortran, 64 lines
  of CMake, one `switches.json` category.
- The C++ keeps its own repository, licence files, CMake minimum (3.25; WW3 asks for 3.19, v,
  `WW3/CMakeLists.txt:4`) and C++20 requirement, and its own release cadence.
- Two version contracts must move together: WW3's `find_package` minimum and the ABI version of
  `ww_kokkos`. Today a WW3 commit that needs a new entry point raises the minimum by hand.
- An operational centre installs one more library, built against the same Kokkos as its WW3; the
  `EXACT` check makes a mismatch a configure error instead of a link-time surprise.
- WW3's exported targets name `ww_kokkos::ww_kokkos` (`ww3_lib` links it `PUBLIC`), so a consumer
  of an installed WW3 must find the package too ⚠ (no consumer built).
- Licence: the LGPL-derived kernels are distributed from a repository whose top licence is MIT,
  each file with its own line. Whether that is enough is the question marked ⚠ above.

### (b) A submodule of WW3

WW3 adds `ww_kokkos` as a git submodule at a pinned commit and builds it with `add_subdirectory`
or `ExternalProject_Add` when `KOKKOS` is set.

- WW3 has no submodule at the pin (v, no `.gitmodules` in `WW3/`); this would be its first, with
  `git clone --recurse-submodules` in every build recipe that uses the switch.
- The gitlink pins the C++ exactly, so the two-version problem of (a) becomes one commit; the
  `find_package` minimum is no longer needed.
- `add_subdirectory` brings this tree's CMake 3.25 minimum and C++20 into WW3's configure, and the
  install-only option is what keeps GoogleTest and NetCDF out of it. `ExternalProject_Add` keeps
  the trees apart at the cost of a second configure step ⚠ (neither built).
- The C++ stays in its own repository with its own licence files, as in (a).
- Who maintains the submodule's repository, and who may move the gitlink, is a governance
  question WW3's maintainers answer, not this repository.

### (c) Sources inside `model/src`

The C++ is copied into WW3, for instance `model/src/kokkos/`, and built by WW3's CMake.

- The upstream diff grows by about 58 000 lines of C++ in 347 files, about 14 times the 4 082 new lines of the Fortran half.
- One repository, one version, one licence file: the LGPL-derived kernels would sit in an LGPL
  project. The 22 MIT files would sit in it too ⚠ (whether WW3 accepts MIT files, or relicences
  them, is for its maintainers and a legal reading).
- WW3's CMake would need C++ in its languages, the contraction flags (`-ffp-contract=off`,
  `--fmad=false`) on the C++ sources only, and the CUDA `libm` table generator; today those live
  in `kokkos/src/ww_kokkos/CMakeLists.txt`.
- The lab's tools and intro programs stay here; the port's validation is WW3's regtests in every
  option (ADR-0010), so no test suite moves.
- Every change to the C++ becomes a WW3 pull request; this repository would then track WW3 for
  the C++ as it does for the Fortran.

## Consequences

Whatever is chosen:

- The Fortran half is upstream's in every option; its 819 changed lines are what a reviewer reads,
  and with `KOKKOS` off they compile to the original (`PRODUCTION_READINESS_202610.md`, gap 12).
- The licence of the files without an `SPDX` line has to be settled before any option ships ⚠.
- The `EXACT` Kokkos check stays: in (b) and (c) the Kokkos is still an external package, and a
  build against another Kokkos still links code compiled for one Kokkos with another's runtime.
- Gap 12 stays a Blocker for target (b) of `PRODUCTION_READINESS_202610.md` until an option is
  accepted. Gap 5's configure dependencies are closed by the install-only option; its other
  items (a static archive only, C++20, CMake 3.25) remain.

## Revisit when

The owner opens a conversation with WW3's maintainers about taking the runtime, or a legal reading
of the licence question is available, or WW3 adds submodules or C++ for another reason.
