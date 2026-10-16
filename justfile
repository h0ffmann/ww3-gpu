# ww3-lab -- task runner. Everything real lives in scripts/. Run `just` for the list.
#
# Source trees: <ww3> defaults to $WW3, else ~/src/WW3 (upstream NOAA-EMC clone from
# `just get`); pass `WW3` to use the fork submodule instead. SWAN defaults to ~/src/swan.

set shell := ["bash", "-euo", "pipefail", "-c"]
set positional-arguments

# The old names, as the dated records in docs/log and docs/data cite them.
alias smoke-hoff := smoke-ww3-gpu
alias full-hoff := full-ww3-gpu
alias hoff-matrix := ww3-gpu-matrix

ww3_src  := env_var_or_default("WW3",   env_var("HOME") + "/src/WW3")
swan_src := env_var_or_default("SWAN",  env_var("HOME") + "/src/swan")
pratico  := justfile_directory() + "/picasso/labs/pratico"
ww3_shell := 'nix develop "' + pratico + '#ww3" --command'
ww3gpu_build := justfile_directory() + "/build/ww3-gpu-matrix-"

default:
    @just --list --unsorted

# ---------------------------------------------------------------------
# Host toolchain (Debian/Ubuntu) -- alternative to the Nix shell below
# ---------------------------------------------------------------------

# Install build dependencies with apt (gfortran, OpenMPI, NetCDF, CMake, ...).
prereqs:
    bash scripts/00_prereqs.sh

# ---------------------------------------------------------------------
# Nix toolchain from picasso/labs/pratico (gfortran, OpenMPI, NetCDF, ...)
# ---------------------------------------------------------------------

# Enter the toolchain-only shell (`nix develop .#ww3` in pratico).
ww3:
    nix develop "{{pratico}}#ww3"

# Interactive shell: toolchain + zsh-ai (Ctrl+O -> local Ollama model) + ai-jail. Works from any cwd.
dev:
    nix develop "{{pratico}}"

# One-shot question to the local model through llm, e.g. `just ask "list files over 100 MB"`.
ask *question:
    IN_PRATICO=1 nix develop "{{pratico}}" --command bash -c 'llm -m "$LLM_MODEL" "$@"' _ "$@"

# Pull the Ollama model the shell expects (qwen2.5-coder:7b; LLM_MODEL overrides).
pull:
    just -f {{pratico}}/justfile pull

# Run one command inside the toolchain shell, e.g. `just ww3-run gfortran --version`.
ww3-run *cmd:
    nix develop "{{pratico}}#ww3" --command "$@"

# Print exact toolchain versions (delegates to pratico's justfile).
toolchain:
    just -f {{pratico}}/justfile toolchain

# Fortran + MPI + NetCDF smoke test of the toolchain in the Nix sandbox (what pratico's CI runs).
toolchain-smoke:
    just -f {{pratico}}/justfile smoke

# ---------------------------------------------------------------------
# WW3 two ways (scripts/ww3-gpu.sh): --ww3, pure NOAA WW3 from WW3-noaa/; --ww3-gpu, WW3 built
# with the KOKKOS switch from the h0ffmann/WW3 branch ww3-gpu/kokkos-st4 and linked to kokkos/.
# --ww3-gpu runs on the CPU (Kokkos OpenMP) unless asked: `just smoke-ww3-gpu gpu` runs CUDA, and only
# with a human supervising (AGENTS.md hard rule; it asks at the terminal).
# ---------------------------------------------------------------------

# Both smoke tests: WW3 as written, then WW3 with the Kokkos port (ww3_ts1, seconds on a CPU).
smoke: smoke-ww3 smoke-ww3-gpu

# WW3 as written: regtest ww3_ts1 (one point, ST4 + NL1, 10 model days) must finish.
smoke-ww3:
    bash scripts/ww3-gpu.sh --ww3 smoke

# WW3 with the Kokkos DIA: ww3_ts1 with the Fortran and the Kokkos W3SNL1, compared by nccmp-tol.
smoke-ww3-gpu device="cpu":
    bash scripts/ww3-gpu.sh --ww3-gpu smoke --{{device}}

# The expensive case, as written: bench_small (120 x 80, 9 520 sea points, 12 hours).
full-ww3:
    bash scripts/ww3-gpu.sh --ww3 full

# The expensive case with the Kokkos DIA, compared with the Fortran by nccmp-tol.
full-ww3-gpu device="cpu":
    bash scripts/ww3-gpu.sh --ww3-gpu full --{{device}}

# The Kokkos DIA through WW3's own regtest harness (WFIP-0003), as CTest (regtests/CMakeLists.txt):
# one test per NL1 entry of matrix.base, the Fortran and the Kokkos DIA of one executable, compared
# by test.comp and nccmp-tol. CPU only. <selection> is serial (99 entries), omp (50 shared-memory
# OpenMP entries), mpi (161, mpirun with $WW3GPU_MPI_RANKS ranks, default 4) or all (310); each entry
# gets (nproc --all)/<jobs> threads ($WW3GPU_OMP_THREADS overrides), shared among its ranks. The
# rest goes to ctest: `just ww3-gpu-matrix 8 serial -R ww3_ts1`, `-L entry`, `-N`. jobs=N and
# selection=S are accepted too. WW3GPU_OMP_NESTED=ON in the environment exports
# OMP_MAX_ACTIVE_LEVELS=2 to every run (no longer needed: inside W3WAVE's parallel region the shim
# runs the kernel on Kokkos::Serial). WW3GPU_ROUTINES (SNL1 by default; SDS4, SIN4, SPR4 select the
# ST4 entries; several with commas) names the routines the Kokkos run turns on.
# Record: build/ww3-gpu-matrix-<selection>[-<routines>]/summary.tsv.
ww3-gpu-matrix jobs="8" selection="serial" *ctest_args:
    #!/usr/bin/env bash
    set -euo pipefail
    jobs=8 sel=serial rest=() i=0
    for a in "$@"; do
      i=$((i + 1))
      case "$a" in
        jobs=*) jobs="${a#jobs=}" ;;
        selection=*) sel="${a#selection=}" ;;
        *) if [ "$i" = 1 ] && [[ "$a" =~ ^[0-9]+$ ]]; then jobs="$a"
           elif [ "$i" -le 2 ] && [[ "$a" =~ ^(serial|omp|mpi|all)$ ]]; then sel="$a"
           else rest+=("$a"); fi ;;
      esac
    done
    [[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo "ww3-gpu-matrix: jobs is a positive integer, not '$jobs'" >&2; exit 64; }
    t=$(( $(nproc --all) / jobs )); t=${WW3GPU_OMP_THREADS:-$(( t > 0 ? t : 1 ))}
    data=$(nix build --no-link --print-out-paths "{{pratico}}#ww3-regtest-data")
    routines="${WW3GPU_ROUTINES:-SNL1}"; routines="${routines//[ ,]/;}"
    b="{{ww3gpu_build}}$sel"; [ "$routines" = SNL1 ] || b="$b-${routines//;/-}"
    {{ww3_shell}} cmake -S regtests -B "$b" -DWW3GPU_SELECTION="$sel" -DWW3GPU_OMP_THREADS="$t" \
      -DWW3GPU_MPI_RANKS="${WW3GPU_MPI_RANKS:-4}" -DWW3GPU_DATA="$data" -DWW3GPU_ROUTINES="$routines" \
      -DWW3GPU_OMP_NESTED="${WW3GPU_OMP_NESTED:-OFF}" >/dev/null
    WW3GPU_COMMAND="${WW3GPU_ROUTINES:+WW3GPU_ROUTINES=$WW3GPU_ROUTINES }${WW3GPU_OMP_NESTED:+WW3GPU_OMP_NESTED=$WW3GPU_OMP_NESTED }just ww3-gpu-matrix $*" {{ww3_shell}} ctest --test-dir "$b" -j "$jobs" --output-on-failure "${rest[@]}"

# The entries of a selection, as CTest test names (NNN:<test>:<switch>), without running them.
ww3-gpu-matrix-list selection="serial":
    #!/usr/bin/env bash
    set -euo pipefail
    routines="${WW3GPU_ROUTINES:-SNL1}"; routines="${routines//[ ,]/;}"
    b="{{ww3gpu_build}}{{selection}}"; [ "$routines" = SNL1 ] || b="$b-${routines//;/-}"
    {{ww3_shell}} cmake -S regtests -B "$b" -DWW3GPU_SELECTION="{{selection}}" -DWW3GPU_ROUTINES="$routines" >/dev/null
    {{ww3_shell}} ctest --test-dir "$b" -N -L entry -FA ".*"

# WW3's regtest data kept on NOAA's ftp (ww3_tp2.15's wind.nc, ...), SHA-256 pinned: pratico's
# ww3-regtest-data package (picasso/labs/pratico/flake.nix). Prints its store path.
ww3-gpu-matrix-fetch:
    nix build --no-link --print-out-paths "{{pratico}}#ww3-regtest-data"

# The DIA on the CPU, Serial and OpenMP: WW3's Fortran, the Kokkos port and ww3-dia-gpu's kernel,
# alone and in ww3_shel (scripts/bench_dia.sh). BENCH_REGTEST="passed: <command>" writes the record.
# Benchmark the DIA three ways on the CPU (`--kernel-only` for minutes, not an hour)
bench-dia *args:
    bash scripts/bench_dia.sh {{args}}

# ---------------------------------------------------------------------
# The same runs in Docker (docker/Dockerfile): Ubuntu 26.04, no Nix, no picasso. The build stage
# compiles Kokkos 5.2.2, the port, nccmp-tol, WW3 with and without KOKKOS, and WW4; the image keeps
# only the compiled programs, their runtime libraries and the case inputs. CPU only. Every recipe
# that runs scripts/ww3-gpu.sh has its pair here, `d` in front: smoke-ww3-gpu -> dsmoke-ww3-gpu. Runs land
# in build/docker-runs/; DOCKER_IMAGE picks another image than ww3-gpu:local.
# ---------------------------------------------------------------------

# Build the Ubuntu image ww3-gpu:<tag> from this checkout (CPU only, compiled code only)
dbuild tag="local":
    bash scripts/docker.sh build {{tag}}

# Run scripts/ww3-gpu.sh in the image, any of its options: `just drun --ww3-gpu full`
drun *args:
    bash scripts/docker.sh run {{args}}

# Scan the image for HIGH/CRITICAL vulnerabilities with a fix, as CI does (docker-scan.yml)
dscan tag="local":
    docker run --rm -v /var/run/docker.sock:/var/run/docker.sock aquasec/trivy:0.76.0 image --scanners vuln --severity HIGH,CRITICAL --ignore-unfixed --exit-code 1 ww3-gpu:{{tag}}

# Build the GPU image ww3-gpu-cuda:<tag>: --ww3-gpu on Kokkos' CUDA backend (docker/Dockerfile.cuda)
dbuild-cuda tag="local":
    bash scripts/docker.sh build-cuda {{tag}}

# Run scripts/ww3-gpu.sh in the GPU image with --gpus all (needs the NVIDIA Container Toolkit)
drun-cuda *args:
    bash scripts/docker.sh run-cuda {{args}}

# `just smoke-ww3-gpu gpu` in the GPU image; asks the supervising human for "yes" (AGENTS.md)
dsmoke-ww3-gpu-cuda:
    bash scripts/docker.sh run-cuda --ww3-gpu smoke --gpu

# `just full-ww3-gpu gpu` in the GPU image; asks the supervising human for "yes" (AGENTS.md)
dfull-ww3-gpu-cuda:
    bash scripts/docker.sh run-cuda --ww3-gpu full --gpu

# `just smoke` in the image: WW3 as written, then WW3 with the Kokkos DIA.
dsmoke: dsmoke-ww3 dsmoke-ww3-gpu

# `just smoke-ww3` in the image.
dsmoke-ww3:
    bash scripts/docker.sh run --ww3 smoke

# `just smoke-ww3-gpu` in the image (CPU only: the image has no CUDA build).
dsmoke-ww3-gpu:
    bash scripts/docker.sh run --ww3-gpu smoke

# `just full-ww3` in the image.
dfull-ww3:
    bash scripts/docker.sh run --ww3 full

# `just full-ww3-gpu` in the image (CPU only).
dfull-ww3-gpu:
    bash scripts/docker.sh run --ww3-gpu full

# `just smoke-ww4` in the image; exit 5 while a part the case needs is not ported.
dsmoke-ww4:
    bash scripts/docker.sh run --ww4 smoke

# `just full-ww4` in the image.
dfull-ww4:
    bash scripts/docker.sh run --ww4 full

# The matrix's parsers and verdict on fixtures, and the entry counts at the WW3 pin.
ww3-gpu-matrix-selftest:
    {{ww3_shell}} cmake -P regtests/cmake/selftest.cmake

# The same cases on WAVEWATCH IV, built from the WW4/ submodule (h0ffmann/WW4). Exit 5, before
# any build, when a part the case needs is not ported to WW4 yet (scripts/ww4_parts.py).
smoke-ww4:
    bash scripts/ww3-gpu.sh --ww4 smoke

full-ww4:
    bash scripts/ww3-gpu.sh --ww4 full

# Which parts of a case WW4 has, and which it does not (exit 5 if one is missing).
ww4-parts case="smoke":
    python3 scripts/ww4_parts.py {{case}}

# Profile WW3 as written on a case (smoke: ww3_ts1; full: bench_small) with gprof -> docs/data/ww3_<case>_gprof.tsv.
profile-case case="full":
    bash scripts/profile_case.sh {{case}}

# How much of WW3 runs on the Kokkos backend: the ranking of the routines by measured time, each
# with its port status, and the port score (kokkos/ports.tsv, docs/data/ww3_profile_full.tsv).
port-score *args:
    python3 scripts/port_score.py "$@"

# ---------------------------------------------------------------------
# WW3: get, build, regtest, examples, bench
# ---------------------------------------------------------------------

# Clone upstream NOAA-EMC/WW3 develop into <ww3>. No FTP bundle unless WW3_DATA=1.
get ww3=ww3_src:
    WW3_DATA="${WW3_DATA:-0}" bash scripts/01_get_ww3.sh "{{ww3}}"

# Full rebuild of <ww3> with <switch> (a path, or a name resolved in <ww3>/model/bin).
build switch="switches/switch_lab_shrd" ww3=ww3_src:
    nix develop "{{pratico}}#ww3" --command bash scripts/02_build_ww3.sh "{{ww3}}" "{{switch}}"

# Run one upstream regtest step by step (grid, strt, shel, ounf, ounp) in <ww3>/regtests/<test>/work_lab.
regtest test="ww3_tp1.1" ww3=ww3_src:
    nix develop "{{pratico}}#ww3" --command bash scripts/03_run_regtest.sh "{{ww3}}" "{{test}}"

# The simple regtest: build with the test's own switch_<sw>, then run it.
rt test="ww3_tp1.1" sw="PR3_UQ" ww3=ww3_src: (build (ww3 + "/regtests/" + test + "/input/switch_" + sw) ww3) (regtest test ww3)

# Run the first course example (fetch-limited growth, ~1 min) against <ww3>'s build; builds ww_fetch_analyse first.
example01 ww3=ww3_src: (kokkos-build "openmp-release")
    cd examples/01-fetch-limited-growth && WW3="{{ww3}}" nix develop "{{pratico}}#ww3" --command bash run.sh

# i9 vs 4090 benchmarks (kernel + real WW3 MPI scaling) against <ww3>'s build; builds ww_bench_case first.
bench ww3=ww3_src: (kokkos-build "openmp-release")
    bash bench/run_all.sh "{{ww3}}/build"

# Generate a WW3 benchmark case, e.g. `just bench-case --size small -o bench/case_small` (see bench/README.md).
bench-case *args: (kokkos-build "openmp-release")
    "{{kokkos_dir}}/build/openmp-release/tools/bench_case/ww_bench_case" "$@"

# Build the GPU sandbox (needs nvfortran; `just gpu CC_ARCH=cc90` for an H100).
gpu *args:
    make -C gpu "$@"

# Clone and build SWAN (TU Delft GitLab) into <swan>.
swan swan=swan_src:
    bash scripts/04_get_swan.sh "{{swan}}"

# Delete run artefacts (bench, gpu binaries, example outputs, exercise builds); keep configs and kokkos/build.
clean-runs:
    make -C bench clean
    rm -rf gpu/00_hello_acc gpu/01_dispersion gpu/02_do_concurrent gpu/03_precision
    rm -rf exercises/solutions/build exercises/solutions/out
    rm -f examples/01-fetch-limited-growth/make_inputs examples/02-regional-real-forcing/make_bathy
    rm -rf examples/02-regional-real-forcing/gfs.*
    find examples -name '*.nc' ! -name gebco.nc ! -name gfs_winds.nc -delete
    find examples -name '*.ww3' -delete
    find examples -name '*.out' -delete
    find examples -name '*.inp' -delete

# ---------------------------------------------------------------------
# Pull requests
# ---------------------------------------------------------------------

# Push the branch and create (or refresh) its PR with a body generated from the commits.
pr *args:
    scripts/pr.sh "$@"

# Write or refresh a PR description from the branch's commits; `just uprd --dry-run`, `just uprd 12`.
uprd *args:
    scripts/uprd.sh "$@"

# Wave Forecaster Improvement Proposals (docs/WFIPs): `just wfip new <slug> --title "…" --deliverable D5`, `index`, `check`, `status --since <tag>`.
wfip *args:
    python3 scripts/wfip.py "$@"

# One line per ADR (docs/ADRs): number, status, date, decision; `just adrs --status accepted`.
adrs *args:
    python3 scripts/adrs.py "$@"

# Measured results as data (bench/results/*.json): `just results table` regenerates the tables that quote them; `check` is the CI gate.
results *args:
    python3 scripts/results.py "$@"

# Every entry of pubs/proposal/refs.bib has a row and a verdict in pubs/proposal/REFERENCES.md; --strict fails on unverified.
refs *args:
    python3 scripts/refs_check.py "$@"

# The slip box (NOTES.md): front matter, ids, links, the index.
notes *args:
    python3 scripts/notes_check.py "$@"

# The ai-memory pages tagged `publish`, in memory/ (MEMORY.md): export (owner), import (reader), check (CI).
memory *args:
    python3 scripts/memory.py "$@"

# Regenerate codemeta.json from CITATION.cff and .zenodo.json; `just codemeta --check` is the CI gate.
codemeta *args:
    python3 scripts/codemeta.py "$@"

# Install the gate tools (just, shellcheck, gfortran, PyYAML) and picasso in a fresh container; `--dry-run` prints.
agent-env *args:
    bash scripts/agent_env.sh "$@"

# GitHub's Spec Kit (github.com/github/spec-kit). `just specify init` sets a clone up for the /speckit-* skills; anything else is passed to `specify`.
specify *args:
    #!/usr/bin/env bash
    set -euo pipefail
    args=({{ args }})
    if [ "${args[0]:-}" = init ] && [ "${#args[@]}" -eq 1 ]; then
        args+=(--here --force --non-interactive --integration claude --script sh)
    fi
    uvx --from specify-cli specify "${args[@]:-}"

# Tag main as v<version> (default: the next patch) and push it; CI publishes the GitHub release and Zenodo mints its DOI. `just release --dry-run`, `just release 0.2.0`.
release *args:
    scripts/release.sh "$@"

# List release tags, newest first.
releases:
    git fetch -q --tags origin && git tag -l 'v*' --sort=-v:refname --format='%(refname:short)  %(creatordate:short)  %(subject)'

# ---------------------------------------------------------------------
# Submodules: picasso (sparse, labs/pratico), WW3 and WW4 (forks of NOAA-EMC/WW3 and NOAA-EMC/WW4)
# ---------------------------------------------------------------------

setup_script := "scripts/ww-lab-tool-setup.sh"
src_script   := "scripts/ww3-submodule.sh"

# Materialise picasso (sparse: only labs/pratico). Safe after a fresh clone.
submodule-init:
    bash {{setup_script}} .

# Move the picasso pin to the latest origin/<branch> and stage it.
submodule-update branch="main":
    bash {{setup_script}} . --bump --branch {{branch}}

# Same as submodule-update, but also commit .gitmodules + the new pin.
submodule-commit branch="main":
    bash {{setup_script}} . --bump --branch {{branch}} --commit

# Show the picasso pinned commit vs. the branch tip.
submodule-status:
    @git submodule status -- picasso
    @git -C picasso fetch --quiet --depth 1 origin "$(git config -f .gitmodules submodule.picasso.branch)"
    @echo "tip: $(git -C picasso rev-parse --short FETCH_HEAD)  pinned: $(git -C picasso rev-parse --short HEAD)"
    @echo "checked out: $(git -C picasso sparse-checkout list | tr '\n' ' ')"

alias up := submodule-update
alias st := submodule-status

# Boot picasso's workstation VM (its root `just vm`) at the pinned commit.
vm: _picasso-full
    just -f picasso/justfile -d picasso vm

# Delete that VM's disk; the next `just vm` starts fresh.
vm-reset: _picasso-full
    just -f picasso/justfile -d picasso vm-reset

# The VM lives at picasso's root, which the labs/pratico sparse checkout leaves off disk,
# and nix refuses a shallow git checkout ("'revCount' is not available"), which .gitmodules makes it.
# Local state only; `just submodule-init` or `just up` makes it sparse again.
_picasso-full:
    @[ -f picasso/flake.nix ] || { echo "vm: checking out all of picasso (was sparse: labs/pratico)"; git -C picasso sparse-checkout disable; }
    @[ "$(git -C picasso rev-parse --is-shallow-repository)" = false ] || { echo "vm: fetching picasso's full history (was shallow)"; git -C picasso fetch --unshallow; }

# Add the WW3 fork as ./WW3, or initialise it after a fresh clone.
src-init:
    bash {{src_script}} .

# Move the WW3 pin to the fork's latest <branch> and stage it.
src-up branch="develop":
    bash {{src_script}} . --bump --branch {{branch}}

# Fast-forward the fork from upstream, push it, and bump the pin (the usual refresh).
src-sync branch="develop":
    bash {{src_script}} . --sync --push --bump --branch {{branch}}

# Pin ./WW3 to an unmerged upstream pull request, e.g. `just src-pr 1234`.
src-pr n:
    bash {{src_script}} . --pr {{n}}

# WW3 pinned commit vs. fork and upstream tips.
src-st:
    @git submodule status -- WW3
    @git -C WW3 fetch --quiet origin develop && git -C WW3 fetch --quiet upstream develop
    @echo "pinned: $(git -C WW3 rev-parse --short HEAD)  fork/develop: $(git -C WW3 rev-parse --short origin/develop)  upstream/develop: $(git -C WW3 rev-parse --short upstream/develop)"
    @echo "fork is $(git -C WW3 rev-list --count origin/develop..upstream/develop) commits behind upstream"

# Fast-forward the h0ffmann/WW4 fork from NOAA-EMC/WW4, push it, and bump ./WW4.
ww4-sync branch="develop":
    bash {{src_script}} . --path WW4 --branch {{branch}} --fork git@github.com:h0ffmann/WW4.git --upstream https://github.com/NOAA-EMC/WW4.git --sync --push --bump

# Every submodule to its latest: picasso, WW3 and WW4 (forks synced and pushed). Stages, no commit.
sub-sync: submodule-update src-sync ww4-sync

# ---------------------------------------------------------------------
# Publications: markdown -> PDF (flake.nix at the repo root; sources in pubs/)
# ---------------------------------------------------------------------

# Enter the publications shell (pandoc, xelatex, python+openai).
pubs-shell:
    nix develop "{{justfile_directory()}}"

# Course book PDF from course/*.md -> build/ww3-lab-course.pdf.
book style="abnt":
    nix develop "{{justfile_directory()}}" --command scripts/build_pdf.sh book {{style}}

# Course book as a Word document -> build/ww3-lab-course.docx (pandoc's docx writer, no TeX).
book-docx:
    nix develop "{{justfile_directory()}}" --command scripts/build_docx.sh book

# The book as a static website, one page per chapter -> build/site; CI deploys it to GitHub Pages.
book-html:
    scripts/build_html.sh

# The Gleam front page (site/, DESIGN.md) reads its facts from the repository; CI deploys build/site.
# The whole website -> build/site: the chapters at /book/, then the Gleam front page at /.
site:
    scripts/build_html.sh
    nix develop "{{justfile_directory()}}#site" --command scripts/build_site.sh --check

# Serve build/site on http://localhost:8000 (after `just site`).
site-serve:
    python3 -m http.server 8000 --directory build/site

# The book as a Leanpub manuscript (Markua) -> build/leanpub/manuscript; CI pushes it to the `leanpub` branch.
leanpub *args:
    python3 scripts/leanpub_manuscript.py {{args}}

# Proposal as a Word document for review -> build/proposal_<lang>.docx (no DEL cover page).
proposal-docx lang="pt":
    nix develop "{{justfile_directory()}}" --command scripts/build_docx.sh proposal {{lang}}

# Proposal PDF -> build/proposal_<lang>.pdf; lang pt|en, style abnt (default) or ieee.
proposal lang="pt" style="abnt":
    nix develop "{{justfile_directory()}}" --command scripts/build_pdf.sh proposal {{lang}} {{style}}

# Pinned mermaid-cli, Geist and DejaVu Sans (fallback glyphs) from flake.lock's nixpkgs; --force re-renders all (scripts/figures.py).
# Render every mermaid fence to pubs/figures/mermaid/<id>.pdf|png and rewrite pubs/figures/README.md.
figures *args:
    FIGURES_FONT_DIR="$(nix build --inputs-from "{{justfile_directory()}}" nixpkgs#geist-font --no-link --print-out-paths)/share/fonts:$(nix build --inputs-from "{{justfile_directory()}}" nixpkgs#dejavu_fonts --no-link --print-out-paths)/share/fonts" \
      nix shell --inputs-from "{{justfile_directory()}}" nixpkgs#mermaid-cli nixpkgs#python3 \
      --command python3 scripts/figures.py render {{args}}

# Every mermaid fence has its header, its reading card and a current render (what CI checks).
figures-check:
    python3 scripts/figures.py check

# AI-writing tells in the English Markdown: the gate CI runs; --report lists every judgement rule (issue #36).
vale *args:
    scripts/vale.sh {{args}}

# Parecer do revisor-proposta sobre pubs/proposal (norma ABNT/DEL, registro científico, jargão).
# Sem argumentos revisa pt/ e en/; passe caminhos para revisar só parte.
proposal-review *files:
    claude -p "Use o subagente revisor-proposta para revisar {{files}} e escreva o parecer." --permission-mode plan

# Registrar o parecer do revisor para o texto atual (libera o portão de CI e o Stop hook).
proposal-review-record parecer:
    python3 scripts/proposal_review_gate.py --record {{parecer}}

# Checagens determinísticas: paridade PT/EN de citações e números, marcadores, vírgula decimal, siglas.
proposal-lint *args:
    python3 scripts/proposal_lint.py {{args}}

# O texto atual está coberto por um parecer? (o mesmo que a CI verifica)
proposal-review-check:
    python3 scripts/proposal_review_gate.py --check

# What changed in NOAA-EMC/WW4 since docs/ww4-status.json (--write records the new state). See the /ww4-status skill.
ww4-status *args:
    python3 scripts/ww4_status.py {{args}}

# NOAA notices (NWS PNS/SCN, NCEP model changes, NOAA-EMC/WW3 production branches) since docs/noaa-notices.json (--write records them). See the /noaa-notices skill.
noaa-notices *args:
    python3 scripts/noaa_notices.py {{args}}

# Translate pubs/proposal/en -> pt (changed files only; --force, --dry-run).
translate *args:
    nix develop "{{justfile_directory()}}" --command python3 scripts/translate_md.py "$@"

# Everything: book + proposal pt + proposal en (same as `nix build .`).
pubs: book (proposal "pt") (proposal "en")

# ---------------------------------------------------------------------
# Kokkos (kokkos/): C++ kernels; GoogleTest suites of the intro programs and tools (the port has none, ADR-0010)
# ---------------------------------------------------------------------

kokkos_dir := justfile_directory() + "/kokkos"

# Configure kokkos/ with a preset: serial-debug (default), openmp-release, cuda-release (needs `just cuda` shell).
kokkos-configure preset="serial-debug":
    nix develop "{{pratico}}#ww3" --command cmake -S "{{kokkos_dir}}" --preset {{preset}}

# Build a preset.
kokkos-build preset="serial-debug": (kokkos-configure preset)
    nix develop "{{pratico}}#ww3" --command cmake --build "{{kokkos_dir}}/build/{{preset}}"

# Build and run ctest for a preset.
kokkos-test preset="serial-debug": (kokkos-build preset)
    nix develop "{{pratico}}#ww3" --command ctest --test-dir "{{kokkos_dir}}/build/{{preset}}" --output-on-failure

# Configure, build and test the cuda-release preset in the CUDA shell (RTX 4090 / ADA89).
kokkos-cuda-test:
    nix develop "{{pratico}}#cuda" --command cmake -S "{{kokkos_dir}}" --preset cuda-release
    nix develop "{{pratico}}#cuda" --command cmake --build "{{kokkos_dir}}/build/cuda-release"
    nix develop "{{pratico}}#cuda" --command ctest --test-dir "{{kokkos_dir}}/build/cuda-release" --output-on-failure

# Remove kokkos/build.
kokkos-clean:
    rm -rf "{{kokkos_dir}}/build"

# ---------------------------------------------------------------------
# Validation (kokkos/tools, kokkos/tests): field comparison, L2 replays, profiling
# ---------------------------------------------------------------------

nccmp_bin := kokkos_dir + "/build/openmp-release/tools/nccmp-tol/nccmp-tol"

# Compare <test> against <ref> field by field; exit 0 iff every variable in <tol> is within tolerance.
nccmp ref test tol="kokkos/tools/nccmp-tol/tolerances.txt":
    [ -x "{{nccmp_bin}}" ] || just kokkos-build openmp-release
    nix develop "{{pratico}}#ww3" --command "{{nccmp_bin}}" "{{ref}}" "{{test}}" "{{tol}}"

# L2 replay of <test> (after `just rt <test>`): ww3_shel with WW_KOKKOS_SNL1=0 vs 1, nccmp-tol on the ww3_ounf output, row in kokkos/PORT_STATUS.md.
l2 test="ww3_ts1" ww3=ww3_src:
    [ -x "{{nccmp_bin}}" ] || just kokkos-build openmp-release
    nix develop "{{pratico}}#ww3" --command bash kokkos/tests/L2_replay.sh "{{ww3}}" "{{test}}"

# Phase tables for <test>: gprof (rebuilds WW3 with -pg into <ww3>/build-pg), then perf if the host has it.
profile test="ww3_tp1.1" ww3=ww3_src:
    nix develop "{{pratico}}#ww3" --command bash kokkos/tools/profile/gprof_table.sh "{{ww3}}" "{{test}}"
    nix develop "{{pratico}}#ww3" --command bash kokkos/tools/profile/perf_table.sh "{{ww3}}" "{{test}}" || [ $? -eq 3 ]
