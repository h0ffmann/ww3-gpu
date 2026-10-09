# ww3-lab -- task runner. Everything real lives in scripts/. Run `just` for the list.
#
# Source trees: <ww3> defaults to $WW3, else ~/src/WW3 (upstream NOAA-EMC clone from
# `just get`); pass `WW3` to use the fork submodule instead. SWAN defaults to ~/src/swan.

set shell := ["bash", "-euo", "pipefail", "-c"]
set positional-arguments

ww3_src  := env_var_or_default("WW3",   env_var("HOME") + "/src/WW3")
swan_src := env_var_or_default("SWAN",  env_var("HOME") + "/src/swan")
pratico  := justfile_directory() + "/nix-config/labs/pratico"

default:
    @just --list --unsorted

# ---------------------------------------------------------------------
# Host toolchain (Debian/Ubuntu) -- alternative to the Nix shell below
# ---------------------------------------------------------------------

# Install build dependencies with apt (gfortran, OpenMPI, NetCDF, CMake, ...).
prereqs:
    bash scripts/00_prereqs.sh

# ---------------------------------------------------------------------
# Nix toolchain from nix-config/labs/pratico (gfortran, OpenMPI, NetCDF, ...)
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

# Fortran + MPI + NetCDF smoke test in the Nix sandbox (what pratico's CI runs).
smoke:
    just -f {{pratico}}/justfile smoke

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

# Measured results as data (bench/results/*.json): `just results table` regenerates the tables that quote them; `check` is the CI gate.
results *args:
    python3 scripts/results.py "$@"

# Regenerate codemeta.json from CITATION.cff and .zenodo.json; `just codemeta --check` is the CI gate.
codemeta *args:
    python3 scripts/codemeta.py "$@"

# Install the gate tools (just, shellcheck, gfortran, PyYAML) and nix-config in a fresh container; `--dry-run` prints.
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
# Submodules: nix-config (sparse, labs/pratico), WW3 and WW4 (forks of NOAA-EMC/WW3 and NOAA-EMC/WW4)
# ---------------------------------------------------------------------

setup_script := "scripts/ww-lab-tool-setup.sh"
src_script   := "scripts/ww3-submodule.sh"

# Materialise nix-config (sparse: only labs/pratico). Safe after a fresh clone.
submodule-init:
    bash {{setup_script}} .

# Move the nix-config pin to the latest origin/<branch> and stage it.
submodule-update branch="main":
    bash {{setup_script}} . --bump --branch {{branch}}

# Same as submodule-update, but also commit .gitmodules + the new pin.
submodule-commit branch="main":
    bash {{setup_script}} . --bump --branch {{branch}} --commit

# Show the nix-config pinned commit vs. the branch tip.
submodule-status:
    @git submodule status -- nix-config
    @git -C nix-config fetch --quiet --depth 1 origin "$(git config -f .gitmodules submodule.nix-config.branch)"
    @echo "tip: $(git -C nix-config rev-parse --short FETCH_HEAD)  pinned: $(git -C nix-config rev-parse --short HEAD)"
    @echo "checked out: $(git -C nix-config sparse-checkout list | tr '\n' ' ')"

alias up := submodule-update
alias st := submodule-status

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

# Every submodule to its latest: nix-config, WW3 and WW4 (forks synced and pushed). Stages, no commit.
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
# Kokkos (kokkos/): C++ kernels, intro programs and GoogleTest suites
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

# Regenerate the committed W3SNL1 parity fixture from the Fortran reference.
snl1-fixtures: (kokkos-configure "serial-debug")
    nix develop "{{pratico}}#ww3" --command cmake --build "{{kokkos_dir}}/build/serial-debug" --target snl1-fixtures
    git -C "{{justfile_directory()}}" diff --stat -- kokkos/tests/fixtures

# UNTESTED (see kokkos/README.md): cross-check snl1_ref.F90 against the real W3SNL1 in <ww3>'s build.
l1-crosscheck ww3=ww3_src:
    nix develop "{{pratico}}#ww3" --command cmake -S "{{kokkos_dir}}" -B "{{kokkos_dir}}/build/crosscheck" \
        -DCMAKE_BUILD_TYPE=Release -DWW_WW3_BUILD_DIR="{{ww3}}/build"
    nix develop "{{pratico}}#ww3" --command cmake --build "{{kokkos_dir}}/build/crosscheck" \
        --target gen_snl1_fixture gen_snl1_ww3lib
    nix develop "{{pratico}}#ww3" --command "{{kokkos_dir}}/build/crosscheck/tests/fixtures/gen_snl1_fixture" /tmp/snl1_ref.bin
    nix develop "{{pratico}}#ww3" --command "{{kokkos_dir}}/build/crosscheck/tests/fixtures/gen_snl1_ww3lib" /tmp/snl1_ww3lib.bin
    cmp /tmp/snl1_ref.bin /tmp/snl1_ww3lib.bin && echo "snl1_ref.F90 is byte-identical to WW3's own W3SNL1"

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
