#!/usr/bin/env bash
# runner.sh -- the ww3-gpu self-hosted GitHub Actions runner on the owner's machine.
#
#   scripts/runner.sh register [--dry-run]   download, verify, register for ONE job, start it
#   scripts/runner.sh remove   [--dry-run]   stop the service, unregister, delete the directory
#   scripts/runner.sh status                 the local service and what GitHub reports
#   scripts/runner.sh check-workflows        refuse a self-hosted workflow with any trigger but
#                                            workflow_dispatch
#   scripts/runner.sh --self-test
#
# The only workflow that uses this runner is .github/workflows/regtest-selected.yml (dispatch
# only). The repository is public, and a self-hosted runner executes a job's code on this machine
# as the user that registered it. check-workflows keeps THIS repository's workflows dispatch-only,
# but it cannot stop a workflow that a fork's pull request adds and that asks for the same labels:
# GitHub runs a pull request's own workflow files. So the runner is ephemeral (--ephemeral): it is
# registered just before a dispatch, takes one job and unregisters itself, and between runs no
# runner of this repository exists. Also set the repository's Actions setting "Approval for
# running fork pull request workflows" to "Require approval for all external contributors".
#
# The registration and removal tokens come from `gh api` at run time (valid for about one hour)
# and are never written to a file. The runner release is pinned (RUNNER_VERSION) and its SHA-256
# is checked before extraction. The pattern follows marola-dev/marola-devkit's
# scripts/setup-runners.sh, with a systemd user unit in place of svc.sh (which needs sudo).
#
# Environment: WW3GPU_RUNNER_REPO (h0ffmann/ww3-gpu), WW3GPU_RUNNER_DIR
# (~/.ww3-gpu-runner/<name>), WW3GPU_RUNNER_NAME (ww3-gpu-<short hostname>),
# WW3GPU_RUNNER_LABELS (ww3-gpu).
set -euo pipefail

RUNNER_VERSION=2.338.0
RUNNER_SHA256=af4b794c1bc41d73d40535e3fe092a39f9679cd8d965954c2aca25a05ca41d32  # linux-x64, from the release notes
REPO="${WW3GPU_RUNNER_REPO:-h0ffmann/ww3-gpu}"
NAME="${WW3GPU_RUNNER_NAME:-ww3-gpu-$(hostname -s 2>/dev/null || echo host)}"
LABELS="${WW3GPU_RUNNER_LABELS:-ww3-gpu}"
DIR="${WW3GPU_RUNNER_DIR:-$HOME/.ww3-gpu-runner/$NAME}"
UNIT="ww3-gpu-runner-$NAME.service"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

tarball() { printf 'actions-runner-linux-x64-%s.tar.gz' "$RUNNER_VERSION"; }
tarball_url() { printf 'https://github.com/actions/runner/releases/download/v%s/%s' "$RUNNER_VERSION" "$(tarball)"; }

checksum_ok() {  # file sha256 -> 0 when the file has that SHA-256
  [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ]
}

config_argv() {  # token -> config.sh's arguments, one per line
  printf '%s\n' --unattended --replace --ephemeral --url "https://github.com/$REPO" --token "$1" \
    --name "$NAME" --labels "$LABELS" --work _work
}

unit_text() {  # dir -> the systemd user unit
  cat <<EOF
[Unit]
Description=GitHub Actions runner $NAME for $REPO (scripts/runner.sh)
After=network-online.target

[Service]
WorkingDirectory=$1
ExecStart=$1/run.sh
Restart=on-failure
KillSignal=SIGTERM
TimeoutStopSec=60

[Install]
WantedBy=default.target
EOF
}

# A workflow file that names self-hosted must have workflow_dispatch as its only trigger. Reads
# the `on:` block by indentation: a top-level `on:` and its two-space keys.
workflow_triggers() {  # file -> its trigger names, one per line
  awk '
    /^on:[[:space:]]*\[/ { s=$0; sub(/^on:[[:space:]]*\[/, "", s); sub(/\].*/, "", s); gsub(/[ ,]+/, "\n", s); print s; next }
    /^on:[[:space:]]*[a-z_]+[[:space:]]*$/ { s=$0; sub(/^on:[[:space:]]*/, "", s); print s; next }
    /^on:[[:space:]]*$/ { inon=1; next }
    inon && /^[^ #]/ { inon=0 }
    inon && /^  [a-z_]+:/ { s=$0; sub(/^  /, "", s); sub(/:.*/, "", s); print s }
  ' "$1" | sed '/^$/d'
}

check_workflows() {  # dir -> 0 when every self-hosted workflow is dispatch-only
  local f bad=0 trig
  for f in "$1"/*.yml "$1"/*.yaml; do
    [ -f "$f" ] || continue
    grep -qE '^[^#]*self-hosted' "$f" || continue
    trig="$(workflow_triggers "$f" | sort -u | tr '\n' ' ')"
    if [ "$trig" != "workflow_dispatch " ]; then
      echo "runner: $f runs on self-hosted and has the triggers '$trig'; only workflow_dispatch is allowed" >&2
      bad=1
    fi
    if grep -qE '^[^#]*runs-on:.*\$\{\{' "$f"; then
      echo "runner: $f has an expression in runs-on; a self-hosted workflow names its labels literally" >&2
      bad=1
    fi
  done
  return "$bad"
}

preflight() {
  local tool missing=()
  for tool in curl sha256sum tar gh git nix just systemctl; do
    command -v "$tool" >/dev/null || missing+=("$tool")
  done
  [ "${#missing[@]}" -eq 0 ] || { echo "runner: not on PATH: ${missing[*]}" >&2; return 3; }
  [ -z "${IN_NIX_SHELL:-}" ] || { echo "runner: leave the nix shell first; the runner keeps the PATH it is configured with, and the workflow enters the pinned shell itself" >&2; return 3; }
  gh auth status >/dev/null 2>&1 || { echo "runner: gh is not logged in (gh auth login)" >&2; return 3; }
}

run() {  # print under --dry-run, run otherwise
  if [ "$dry" = 1 ]; then printf '+ %s\n' "$*"; else "$@"; fi
}

register() {
  preflight
  if [ -f "$DIR/.runner" ]; then
    if systemctl --user is-active --quiet "$UNIT"; then
      echo "runner: $NAME is registered and waiting for its one job; dispatch the workflow"
      status; return 0
    fi
    # An ephemeral runner that ran its job is gone from GitHub; its local files are stale.
    run rm -f "$DIR/.runner" "$DIR/.credentials" "$DIR/.credentials_rsaparams"
  fi
  echo "repo:   $REPO"; echo "name:   $NAME"; echo "labels: $LABELS (and self-hosted, Linux, X64)"
  echo "dir:    $DIR"; echo "runner: v$RUNNER_VERSION"
  run mkdir -p "$DIR"
  local tb
  tb="$DIR/$(tarball)"
  if [ ! -x "$DIR/config.sh" ] || [ "$(cat "$DIR/.ww3gpu-version" 2>/dev/null)" != "$RUNNER_VERSION" ]; then
    run curl -fsSL -o "$tb" "$(tarball_url)"
    if [ "$dry" = 1 ]; then
      echo "+ check SHA-256 $RUNNER_SHA256"
    else
      checksum_ok "$tb" "$RUNNER_SHA256" || { echo "runner: $(tarball) does not have the pinned SHA-256; removed" >&2; rm -f "$tb"; return 1; }
    fi
    run tar -C "$DIR" -xzf "$tb"
    run rm -f "$tb"
    [ "$dry" = 1 ] || printf '%s\n' "$RUNNER_VERSION" >"$DIR/.ww3gpu-version"
  fi
  local token argv
  if [ "$dry" = 1 ]; then
    echo "+ gh api -X POST repos/$REPO/actions/runners/registration-token"
    mapfile -t argv < <(config_argv '<token>')
  else
    token="$(gh api -X POST "repos/$REPO/actions/runners/registration-token" --jq .token)"
    mapfile -t argv < <(config_argv "$token")
  fi
  if [ "$dry" = 1 ]; then echo "+ (cd $DIR && ./config.sh ${argv[*]})"; else (cd "$DIR" && ./config.sh "${argv[@]}"); fi
  unset token argv
  local ud="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
  if [ "$dry" = 1 ]; then
    echo "+ write $ud/$UNIT:"; unit_text "$DIR" | sed 's/^/    /'
  else
    mkdir -p "$ud"; unit_text "$DIR" >"$ud/$UNIT"
  fi
  run systemctl --user daemon-reload
  run systemctl --user enable --now "$UNIT"
  echo "runner: to keep it running after logout, run once: loginctl enable-linger $USER"
  [ "$dry" = 1 ] || status
}

remove() {
  preflight
  local ud="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
  run systemctl --user disable --now "$UNIT" || true
  run rm -f "$ud/$UNIT"
  run systemctl --user daemon-reload
  if [ -x "$DIR/config.sh" ] || [ "$dry" = 1 ]; then
    if [ "$dry" = 1 ]; then
      echo "+ gh api -X POST repos/$REPO/actions/runners/remove-token"
      echo "+ (cd $DIR && ./config.sh remove --token <token>)"
    else
      local token
      token="$(gh api -X POST "repos/$REPO/actions/runners/remove-token" --jq .token)"
      (cd "$DIR" && ./config.sh remove --token "$token")
      unset token
    fi
  fi
  run rm -rf "$DIR"
  echo "runner: $NAME removed"
}

status() {
  echo "local:  $DIR $([ -f "$DIR/.runner" ] && echo configured || echo 'not configured')"
  echo "unit:   $UNIT $(systemctl --user is-active "$UNIT" 2>/dev/null || true)"
  gh api "repos/$REPO/actions/runners" \
    --jq '.runners[] | "github: \(.name) \(.status) busy=\(.busy) labels=\([.labels[].name] | join(","))"' 2>/dev/null \
    || echo "github: could not read the runner list (gh auth status; needs admin on $REPO)"
}

self_test() {
  local f=0 t
  ok() { if [ "$2" = "$3" ]; then echo "  ok   $1"; else echo "  FAIL $1: got '$2', want '$3'" >&2; f=$((f + 1)); fi; }
  ok "the tarball is the pinned version" "$(tarball)" "actions-runner-linux-x64-$RUNNER_VERSION.tar.gz"
  ok "from the actions/runner release" "$(tarball_url)" \
    "https://github.com/actions/runner/releases/download/v$RUNNER_VERSION/actions-runner-linux-x64-$RUNNER_VERSION.tar.gz"
  ok "the pinned SHA-256 is 64 hex digits" "$(grep -cE '^[0-9a-f]{64}$' <<<"$RUNNER_SHA256")" 1
  ok "config.sh runs unattended" "$(config_argv TOK | grep -c -- '^--unattended$')" 1
  ok "--replace, so a second register does not fail on the name" "$(config_argv TOK | grep -c -- '^--replace$')" 1
  ok "--ephemeral: one job, then the runner unregisters" "$(config_argv TOK | grep -c -- '^--ephemeral$')" 1
  ok "the labels reach config.sh" "$(config_argv TOK | grep -A1 -- '^--labels$' | tail -1)" "$LABELS"
  ok "the repository reaches --url" "$(config_argv TOK | grep -A1 -- '^--url$' | tail -1)" "https://github.com/$REPO"
  ok "the token is passed once" "$(config_argv TOK | grep -c '^TOK$')" 1
  ok "the unit runs run.sh from the runner directory" "$(unit_text /x | grep -c '^ExecStart=/x/run.sh$')" 1
  ok "the unit holds no token" "$(unit_text /x | grep -ci token || true)" 0
  t=$(mktemp -d)
  printf 'abc' >"$t/a"
  ok "a file with the expected SHA-256 passes" \
    "$(checksum_ok "$t/a" ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad && echo y || echo n)" y
  ok "a file with another one does not" "$(checksum_ok "$t/a" "$RUNNER_SHA256" && echo y || echo n)" n
  mkdir -p "$t/wf"
  printf 'on:\n  workflow_dispatch:\n    inputs:\n      jobs:\njobs:\n  a:\n    runs-on: [self-hosted, ww3-gpu]\n' >"$t/wf/ok.yml"
  ok "a dispatch-only self-hosted workflow passes" "$(check_workflows "$t/wf" 2>/dev/null && echo y || echo n)" y
  printf 'on:\n  pull_request:\n  workflow_dispatch:\njobs:\n  a:\n    runs-on: [self-hosted]\n' >"$t/wf/pr.yml"
  ok "a self-hosted workflow on pull_request fails" "$(check_workflows "$t/wf" 2>/dev/null && echo y || echo n)" n
  rm "$t/wf/pr.yml"
  printf 'on: [push, workflow_dispatch]\njobs:\n  a:\n    runs-on: self-hosted\n' >"$t/wf/push.yml"
  ok "the flow form of on: is read too" "$(check_workflows "$t/wf" 2>/dev/null && echo y || echo n)" n
  rm "$t/wf/push.yml"
  # shellcheck disable=SC2016 # the expression is the text under test
  printf 'on: workflow_dispatch\njobs:\n  a:\n    runs-on: ${{ vars.R || %s }}\n' "'self-hosted'" >"$t/wf/expr.yml"
  ok "an expression in runs-on fails" "$(check_workflows "$t/wf" 2>/dev/null && echo y || echo n)" n
  rm "$t/wf/expr.yml"
  printf 'on:\n  pull_request:\njobs:\n  a:\n    runs-on: ubuntu-latest\n' >"$t/wf/hosted.yml"
  ok "a hosted workflow on pull_request is not this check's business" "$(check_workflows "$t/wf" 2>/dev/null && echo y || echo n)" y
  rm -rf "$t"
  ok "this repository's workflows pass" "$(check_workflows "$root/.github/workflows" && echo y || echo n)" y
  echo "runner self-test: $([ "$f" -eq 0 ] && echo ok || echo "$f FAILED")"
  [ "$f" -eq 0 ]
}

cmd="${1:-}"; shift || true
dry=0
for a in "$@"; do
  case "$a" in
    --dry-run) dry=1 ;;
    *) echo "runner: unknown argument '$a'" >&2; usage ;;
  esac
done
case "$cmd" in
  register) register ;;
  remove) remove ;;
  status) status ;;
  check-workflows) check_workflows "$root/.github/workflows" && echo "runner: every self-hosted workflow is dispatch-only" ;;
  --self-test) self_test ;;
  -h|--help|"") usage ;;
  *) echo "runner: unknown command '$cmd'" >&2; usage ;;
esac
