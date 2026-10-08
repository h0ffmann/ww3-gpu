# WW3 GPU Lab

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23221351.svg)](https://doi.org/10.5281/zenodo.23221351)
[![CI](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml/badge.svg)](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml)
[![License: MIT + LGPL-3.0 kernels](https://img.shields.io/badge/license-MIT%20%2B%20LGPL--3.0%20kernels-blue)](#licensing)
[![Leia em português](https://img.shields.io/badge/leia%20em-portugu%C3%AAs-green)](README.pt-BR.md)

A book in progress on agentic coding and agentic research, compiled from this repository, written
around one problem: moving the expensive kernels of WAVEWATCH III® (WW3), NOAA's third-generation
spectral wind-wave model, to GPUs without changing the answer.

The repository is the book's source and its evidence. Its 16 lessons are the chapters, built into
a PDF on every merge ([`pdf/ww3-lab-course.pdf`](pdf/ww3-lab-course.pdf)); the code they describe
is next to them: a Nix-pinned Fortran/MPI/NetCDF toolchain that builds WW3 and runs a regression
test in one command, and a C++/Kokkos port of the DIA nonlinear interaction term (`W3SNL1`),
written with coding agents, that reproduces the Fortran output bit for bit on the Serial, OpenMP
and CUDA backends. On an RTX 4090 it runs 1,000 sea points in 0.047 ms against 24.96 ms serial
([`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md)). The plans, decisions and measurements that
the chapters quote carry their evidence (`(v)` checked, `⚠` not) and the command that reproduces
each number. The same repository is the groundwork for an undergraduate project at Escola
Politécnica, UFRJ, co-advised at LabECO, UFSC.

It is written first for scientists: PhD researchers, postdocs and independent researchers in
wave modelling, numerical methods and HPC. Its second reader is a researcher or engineer from
another field who wants to run coding agents on scientific code without losing the answer. If you
use the repository, please [cite it](#how-to-cite).

## The book

*Without Changing the Answer* (working title) is written in the tradition of self-published
technical books built from a public repository, such as Sandy Maguire's *Thinking with Types* and
Gabriel Volpe's *Practical FP in Scala* `(v)` marola's
[MIP-0014 §4](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md),
read 2026-10-08. Its subject is a method, not only a model: how coding agents port Fortran under a
bit-for-bit gate and a validation ladder (agentic coding), and how plans, evaluations and
measurements are written so that an agent can be held to them, every claim with its evidence
(agentic research). The repository is run the way the book says to run one. The rules in
[`AGENTS.md`](AGENTS.md), the design docs in [`docs/WFIPs/`](docs/WFIPs/README.md), the decisions
in [`docs/ADRs/`](docs/ADRs/README.md) and the research log in [`docs/log/`](docs/log/README.md)
are chapters in waiting.

[ADR-0003](docs/ADRs/ADR-0003-repository-is-a-book.md) records the decision and
[WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md) the plan: parts and front matter, a gate that
checks every quoted listing against the code on each build, the PDF attached to every release,
and three new chapters on how this lab is run with agents. The shift is incremental, one pull
request per task, and every gate is green after each. Each tagged release is an edition,
archived with the source on Zenodo. `just book` builds it; [`pubs/README.md`](pubs/README.md)
has the pipeline. A free Leanpub edition is fed by CI from the same chapters
([`docs/LEANPUB_202610.md`](docs/LEANPUB_202610.md): what the workflow does and the steps only
the author can take); [ADR-0004](docs/ADRs/ADR-0004-book-channels-and-first-slice.md) records the
channels and what the first slice measured.

## Study areas

Each line is a separate piece of work. Merged ones live on `main`; the rest are open pull
requests or issues, linked so you can follow them.

| Area | Question it answers | Where | State |
|---|---|---|---|
| The course | How do you build, run and measure WW3, then port a kernel? | [`course/`](course/README.md), [`examples/`](examples/README.md), [`exercises/`](exercises/README.md) | 16 lessons, merged |
| Kokkos port of `W3SNL1` | Can a WW3 kernel run on a GPU with bit-identical results? | [`kokkos/`](kokkos/README.md), [lesson 12](course/12-porting-a-kernel-w3snl1.md) | Bit-identical on 3 backends; WW3 replay pending |
| Benchmarks | What do an i9 and an RTX 4090 actually give WW3? | [`bench/`](bench/README.md), [`gpu/`](gpu/README.md), [lesson 09](course/09-benchmark-profile-compile-run.md) | Tooling merged |
| Port priority | Which routine should be ported next? | [#46](https://github.com/h0ffmann/ww3-gpu/pull/46), [`PORT_STATUS.md`](kokkos/PORT_STATUS.md) | Rule in review: port by measured wall time. A first profile puts `W3SDS4` at 67 % of source-term time ([#45](https://github.com/h0ffmann/ww3-gpu/issues/45)) |
| Porting with agents | How can coding agents port forty routines without a human redoing the checks? | [`AGENTS_KOKKOS`](docs/AGENTS_KOKKOS_202609.md), [lesson 13](course/13-bulk-porting-with-agents.md), [#42](https://github.com/h0ffmann/ww3-gpu/issues/42) | Rules merged; task queue and parity ladder in #42 |
| Agent tooling | Which agent frameworks and research tools fit that workflow? | [#25](https://github.com/h0ffmann/ww3-gpu/pull/25) (NVIDIA NOOA), [#41](https://github.com/h0ffmann/ww3-gpu/pull/41) (Consensus, Antigravity, NotebookLM), [#38](https://github.com/h0ffmann/ww3-gpu/issues/38) | Evaluations in review |
| Bit-for-bit proof | What can be proved, and not only tested, about the Fortran → C++ translation, and should the port adopt a proof language? | [`BITWISE_PROOF`](docs/BITWISE_PROOF_202610.md), [`docs/proof/`](docs/proof/README.md), [ADR-0001](docs/ADRs/ADR-0001-proof-language.md) | Decision proposed: no proof language; exhaustive sweeps, GIMPLE diffs and SMT queries. `W3SNL1` section 1 swept over all 2^32 inputs |
| Triton and ML weather forcing | Does a Triton kernel of the `W3SDS4` cumulative term beat the Kokkos one, and can the Fortran call it? Does Google's WeatherNext 3 wind improve the wave forecast? | [#45](https://github.com/h0ffmann/ww3-gpu/issues/45), [`W3SDS4_TRITON_PLANO`](docs/W3SDS4_TRITON_PLANO_202610.pt.md) (pt-BR) | Planned, with ECMWF AIFS Single Wave as the ML wave reference; the port plan and the CPU/GPU balance on an H100 are written |
| Single-H100 port | What would a full port to one H100 take? | [`KOKKOS_H100_PLAN`](docs/KOKKOS_H100_PLAN_202609.md) | Plan |
| WW4 and SWAN | What replaces WW3, and what covers the coast? | [lesson 14](course/14-ww4-and-the-future.md), [lesson 15](course/15-swan.md) | Merged |
| The book and the proposal | The lessons compiled as a book, and the project proposal | [`pubs/`](pubs/README.md), PDFs and Word files in [`pdf/`](pdf/), [WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md) | Built by CI on every merge; parts, listings gate and new chapters planned |

[`docs/AWESOME-WW3_202609.md`](docs/AWESOME-WW3_202609.md) is a curated, annotated link list, and
[`docs/GLOSSARY.md`](docs/GLOSSARY.md) expands every abbreviation, switch, routine and tool name
used here. Non-trivial changes are designed before they are built as Wave Forecaster Improvement
Proposals, each tied to a deliverable of the project proposal with its definition of done;
[`docs/WFIPs/README.md`](docs/WFIPs/README.md) is the index and the standing progress report.

## Quickstart

You need [Nix](https://nixos.org) and [just](https://github.com/casey/just). The compilers come from
the pinned flake, so there is nothing else to install.

```bash
git clone --recurse-submodules git@github.com:h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init   # sparse-checkout nix-config (once per clone)
just get              # clone upstream NOAA-EMC/WW3 develop into ~/src/WW3
just rt               # build with ww3_tp1.1's own switch and run that regtest (~30 s)
just build            # rebuild with the lab switch (switches/switch_lab_shrd, ST4 physics)
just example01        # first course example: fetch-limited growth (~1 min)
just kokkos-test serial-debug   # build and test the C++/Kokkos kernels
```

Then start at [`course/00-orientation.md`](course/00-orientation.md). `just` lists every task. Each
recipe is a thin wrapper over a script in `scripts/`, which you can run directly with a host
toolchain instead (`just prereqs` installs one on Debian/Ubuntu). The toolchain, the WW3 fork and
the Kokkos presets are described in [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md).

For coastal cases, add SWAN with `just swan`.

## Two things worth knowing before you invest

WAVEWATCH IV™ (WW4) exists, and WW3 is scheduled for sunset. [NOAA-EMC/WW4](https://github.com/NOAA-EMC/WW4)
is a rewrite from scratch: a new repository with no backward compatibility, a C++ core with Rust
alongside, and Fortran kept only for solvers. On 2026-10-07 it had 39 commits, placeholders for solvers and source terms,
no physics and no release.
The first public release is expected in January 2027 according to the proposal's advisor ⚠ (no NOAA
source; ON 525 said summer 2027). The plan, including the commitment to stop supporting WW3 once WW4
matures, is in [NCEP Office Note 525](https://doi.org/10.25923/h7j3-1h25). WW3 is still worth
learning: the physics is the same and the concepts carry over, only the interfaces change. See
[`course/14-ww4-and-the-future.md`](course/14-ww4-and-the-future.md). `just ww4-status` diffs WW4 against the last
recorded snapshot (`docs/ww4-status.json`), and the [`ww4-status`](.claude/skills/ww4-status/SKILL.md) skill
turns that into an issue and a PR.

SWAN is the other half of the toolkit. It is implicit and unconditionally stable, has no CFL limit
and runs in stationary mode, so the standard coastal set-up is WW3 offshore and SWAN nearshore. Its
source now lives on [TU Delft GitLab](https://gitlab.tudelft.nl/citg/wavemodels/swan), which most
tutorials have not caught up with. See [`course/15-swan.md`](course/15-swan.md).

## Can WW3 use my GPU?

NVIDIA ships a Fortran compiler: `nvfortran`, part of the free
[NVIDIA HPC SDK](https://developer.nvidia.com/hpc-sdk). It handles CUDA Fortran, OpenACC, OpenMP
target offload and `do concurrent` offload (`-stdpar=gpu`). An RTX 4090 is Ada, compute capability
8.9, so the flag is `-gpu=cc89`.

WW3 itself has no GPU support upstream. The only published port
([Ikuyajolu et al., GMD 2023](https://gmd.copernicus.org/articles/16/1445/2023/)) put OpenACC on one
module, `W3SRCEMD` (the source-term integration), and got about 1.3× against 42 CPU cores on
Summit's V100s. Data transfer bound the result, and the code was not merged into `NOAA-EMC/WW3`. A
consumer PCIe card without NVLink will not do better with that approach.

So compile WW3 with `nvfortran` for the CPU, which works and is useful. Use `gpu/` to learn GPU
Fortran on kernels that suit a 4090, and `bench/` to measure your own hardware instead of trusting
anyone's table, including the ones here. The Kokkos port takes the other route: kernels that keep
their data on the device and match the Fortran bit for bit. The reasoning and an experiment plan
are in [`course/09-benchmark-profile-compile-run.md`](course/09-benchmark-profile-compile-run.md).

## Repository layout

| Path | Contents |
|---|---|
| `course/` | The book's chapters: 16 lessons, 00 to 15, from the wave spectrum through benchmarking, modern Fortran, Kokkos, the `W3SNL1` port and bulk porting with agents, then WW4 and SWAN |
| `examples/` | Self-contained runnable cases with real `.nml` input files |
| `exercises/` | Exercises for lessons 09 to 13, with solutions, in shell, Fortran and C++ |
| `kokkos/` | The `ww_kokkos` kernel library (`W3SNL1` ported), GoogleTest suites, and the tools `nccmp-tol`, `ww_bench_case` and `ww_fetch_analyse` |
| `gpu/` | nvfortran, OpenACC and CUDA Fortran sandbox for an RTX 4090 |
| `bench/` | i9 against 4090: a WW3-shaped kernel, a concurrent CPU+GPU split sweep, and WW3 MPI scaling |
| `proof/` | The runnable half of the proof plan: `snl1_cons/` checks section 1 of `W3SNL1` against the Fortran on all 2^32 inputs (`bash proof/snl1_cons/run.sh`, gfortran and g++ only); `smt/` holds two SMT queries over IEEE-754 |
| `scripts/` | Get, build and run WW3 and SWAN; stage upstream regression tests; release |
| `switches/` | Annotated switch files (WW3's compile-time feature selection) |
| `env/` | conda environment and Dockerfile |
| `docs/` | Plans, evaluations, decisions (`ADRs/`), design docs (`WFIPs/`), the research log, the link list and the glossary |
| `pubs/` | The book's build (`book/`) and the UFRJ/DEL project proposal; built files land in `pdf/` |
| `nix-config/` | Submodule (sparse, `labs/pratico` only): the pinned toolchain |
| `WW3/` | Submodule: the [h0ffmann/WW3](https://github.com/h0ffmann/WW3) fork of NOAA-EMC/WW3 |
| `WW4/` | Submodule: the [h0ffmann/WW4](https://github.com/h0ffmann/WW4) fork of [NOAA-EMC/WW4](https://github.com/NOAA-EMC/WW4), for reading and comparing against WW3; not fetched by CI |

Lab code is C++, Fortran and shell. Python appears only in the publishing pipeline, which is the
stance the [project proposal](pubs/proposal/pt/) takes: the model's own languages, plus the one the
port is written in. CI (`.github/workflows/ci.yml`) checks shell syntax, compiles the Fortran
sandbox and the example and exercise Fortran, builds and tests `kokkos/` on both CPU presets, and
link-checks the markdown. It does not build WW3, which needs the NOAA FTP data bundle and takes
too long on a free runner.

## Conventions

- `⚠` marks a claim I could not verify. Check it before relying on it.
- `(v)` marks a claim verified against a source fetched while building this repo.
- Input files use the namelist (`.nml`) interface instead of the legacy fixed-format `.inp` files.
  WW3 v7 accepts both, and `.nml` is much easier to read. It is also what the annotated templates
  in `$WW3/model/nml/` and the generators in `examples/` produce.

## How to cite

Cite the concept DOI [10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351) for the
project as a whole. It always resolves to the latest release. To pin the exact code you ran, cite
that release's own DOI and title instead; v0.1.0 is
[10.5281/zenodo.23221352](https://doi.org/10.5281/zenodo.23221352), under the title that release
carried. The title below is the one `CITATION.cff` carries from the next release on. GitHub's
**Cite this repository** button (right sidebar) exports APA and BibTeX from
[`CITATION.cff`](CITATION.cff).

BibTeX:

```bibtex
@software{hoffmann_ww3gpu,
  author    = {Hoffmann, Matheus},
  title     = {{WW3 GPU Lab: agentic coding and agentic research on a bit-for-bit GPU port of WAVEWATCH III}},
  year      = {2026},
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.23221351},
  url       = {https://github.com/h0ffmann/ww3-gpu}
}
```

APA:

> Hoffmann, M. (2026). *WW3 GPU Lab: agentic coding and agentic research on a bit-for-bit GPU
> port of WAVEWATCH III* [Computer software]. Zenodo. https://doi.org/10.5281/zenodo.23221351

ABNT (NBR 6023):

> HOFFMANN, Matheus. **WW3 GPU Lab**: agentic coding and agentic research on a bit-for-bit GPU
> port of WAVEWATCH III. [S. l.]: Zenodo, 2026. DOI 10.5281/zenodo.23221351.
> Disponível em: https://doi.org/10.5281/zenodo.23221351.

A downstream project can also declare the dependency in its own `CITATION.cff`, which is how
[marola](#related-work) will cite this repository:

```yaml
references:
  - type: software
    title: "WW3 GPU Lab: agentic coding and agentic research on a bit-for-bit GPU port of WAVEWATCH III"
    authors:
      - family-names: Hoffmann
        given-names: Matheus
        orcid: "https://orcid.org/0009-0009-1056-7661"
    doi: 10.5281/zenodo.23221351
    repository-code: "https://github.com/h0ffmann/ww3-gpu"
```

Cite WW3 itself separately, as the WAVEWATCH III Development Group's user manual for the version
you ran. A new patch release is cut every Friday when `main`
has changed, or by hand with `just release`, and Software Heritage archives the git history
next to each Zenodo deposit; see
[`.claude/skills/release`](.claude/skills/release/SKILL.md) for the chain from tag to DOI.

## Related work

[marola](https://github.com/marola-dev/marola) ([marola.dev](https://marola.dev/)) is an open,
non-profit platform for sea conditions and bathing-water quality at Brazilian beaches, built on
public data, by the same author and Bruno Valério. Its map ranks beaches around Florianópolis, Rio de Janeiro and Salvador by the hour, using Open-Meteo sea and
weather forecasts and the official bathing-water bulletins. Its wave data already comes from
WAVEWATCH III through NCEP's GFS-Wave. Running a detailed spectral wave model for its own bays is
the planned next step ([MIP-0052](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0052-wave-model-compute.md)),
and this repository is its groundwork: marola's Zenodo record lists the WW3 GPU Lab as related work
`(v)`. Cite marola by its concept DOI, [10.5281/zenodo.23224155](https://doi.org/10.5281/zenodo.23224155), which always resolves to the
latest version (v0.2.0 is [10.5281/zenodo.23224156](https://doi.org/10.5281/zenodo.23224156)).

## Licensing

The repository is MIT, except the kernels translated from WW3, which are derived works of WW3 and
carry `LGPL-3.0-or-later`: `kokkos/src/ww_kokkos/snl1_*`, `kokkos/src/fortran_iface/w3kokkosmd.F90`,
the WW3 patch in `kokkos/src/fortran_iface/PATCH.md` and the fixture reference
`kokkos/tests/fixtures/snl1_ref.F90`. Each file states its licence in an
`SPDX-License-Identifier` line. See [`LICENSE`](LICENSE).

No WW3 source is vendored here. `scripts/01_get_ww3.sh` clones it, and upstream regression-test
inputs are fetched, not redistributed. Third-party tools listed in
`docs/AWESOME-WW3_202609.md` keep their own licences (`pyww3` is GPL-3.0, `wavespectra` is MIT).

## Trademarks

WAVEWATCH III® is a registered trademark and WAVEWATCH IV™ a trademark of NOAA's National Weather
Service. They are used here only to refer to that software. This repository is an independent
learning project and is not affiliated with, sponsored by or endorsed by NOAA. In prose we say WW3
and WW4.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md). The most useful contribution is confirming or correcting
anything marked `⚠`. Coding agents read [`AGENTS.md`](AGENTS.md) first: the repo invariants, where a
change belongs and the gates CI runs. A pattern or skill from another repository comes in through
the [`sharingan`](.claude/skills/sharingan/SKILL.md) skill, which pins it, reads its licence and
maps it to this repository's homes before anything is written.
