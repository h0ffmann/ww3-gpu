#!/usr/bin/env bash
# agent_env — give a fresh Debian/Ubuntu container (a cloud agent session, a devcontainer, a CI
# runner) what the gates in AGENTS.md need: just, shellcheck and gfortran from apt, the Python
# test dependency (PyYAML), and the nix-config submodule sparse-checked to labs/pratico over https
# (.gitmodules uses ssh URLs a container has no key for). The Kokkos gates and `just build` need
# the Nix shell on top; this script does not install Nix.
#
# Idempotent: every step checks before acting, so it is safe on every session start.
#
#   scripts/agent_env.sh             # set up
#   scripts/agent_env.sh --dry-run   # print what it would do
set -euo pipefail

dry=0
case "${1:-}" in
    --dry-run) dry=1 ;;
    "") ;;
    *) echo "agent_env: unknown option '$1'" >&2; exit 2 ;;
esac

root="$(cd "$(dirname "$0")/.." && pwd)"
run() { if [ "$dry" = 1 ]; then echo "would run: $*"; else "$@"; fi; }
sudo_if() { if [ "$(id -u)" = 0 ]; then run "$@"; else run sudo "$@"; fi; }

missing=()
for pkg in just shellcheck gfortran; do
    command -v "$pkg" >/dev/null 2>&1 || missing+=("$pkg")
done
if [ "${#missing[@]}" -gt 0 ]; then
    sudo_if apt-get update -qq
    sudo_if env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${missing[@]}"
else
    echo "agent_env: just, shellcheck, gfortran present"
fi

if ! python3 -c 'import yaml' 2>/dev/null; then
    sudo_if env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3-yaml
fi

if [ ! -d "$root/nix-config/labs/pratico" ]; then
    # The fallback fetches the pinned SHA directly when it lives off the tracked branch (as ci.yml).
    https=(-c url.https://github.com/.insteadOf=git@github.com:)
    run git -C "$root" "${https[@]}" submodule update --init nix-config || {
        sha="$(git -C "$root" ls-tree HEAD nix-config | awk '{print $3}')"
        run git -C "$root/nix-config" "${https[@]}" fetch origin "$sha"
        run git -C "$root/nix-config" checkout -q "$sha"
    }
    run git -C "$root/nix-config" sparse-checkout set labs/pratico
else
    echo "agent_env: nix-config/labs/pratico present"
fi
