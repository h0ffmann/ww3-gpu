#!/usr/bin/env bash
# ww3_base — the NOAA-EMC/WW3 commit the fork WW3/ branched from, without a submodule of its own.
#
#   scripts/ww3_base.sh rev              print the commit (kokkos/src/fortran_iface/WW3_BASE)
#   scripts/ww3_base.sh export [<dir>]   write that commit's tree to <dir> (WW3-noaa/, which git
#                                        ignores) and print the directory
#   scripts/ww3_base.sh --self-test
#
# --ww3-noaa of scripts/ww3-gpu.sh, the pair of --ww3-gpu, builds pure NOAA WW3 at this commit, and
# the images of docker/ compile it. The commit is in the fork's own history (h0ffmann/WW3 is
# NOAA-EMC/WW3 plus the KOKKOS patch), so the tree is read from WW3/ with `git archive`. Until
# 2026-11-11 WW3-noaa/ was a submodule at that commit (the owner: one MetOcean submodule, no NOAA
# one); the directory keeps its name, so a citation `WW3-noaa/model/src/<file>.F90:<line>` still
# opens once this command has run. A clone of the fork that is one commit deep does not hold the
# base: it is fetched by its SHA, over https, once.
#
# <dir> keeps the commit it was written from in <dir>/.ww3-base and is written again only when
# WW3_BASE changes. A directory this script did not write is never replaced, except that the
# checkout an older clone still has in WW3-noaa/ is used as it is when it is at WW3_BASE.
#
# Exit codes: 0 ok; 3 WW3/ is not checked out, or the commit cannot be fetched; 64 usage.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
base_file="$root/kokkos/src/fortran_iface/WW3_BASE"

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

base_rev() { # <file>
  local r
  r="$(tr -d '[:space:]' < "$1")"
  [[ "$r" =~ ^[0-9a-f]{40}$ ]] || { echo "ww3_base: $1 does not hold a 40-character commit" >&2; exit 3; }
  printf '%s\n' "$r"
}

export_tree() { # <fork> <rev> <dir>
  local fork="$1" rev="$2" dir="$3"
  if [ -f "$dir/model/src/CMakeLists.txt" ]; then
    [ "$(cat "$dir/.ww3-base" 2>/dev/null)" != "$rev" ] || return 0
    # The submodule checkout of a clone made before 2026-11-11 (its .git is a file or a directory).
    if [ -e "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" = "$rev" ]; then return 0; fi
  fi
  git -C "$fork" rev-parse -q --verify HEAD >/dev/null 2>&1 \
    || { echo "ww3_base: ${fork#"$root/"}/ is not checked out: git submodule update --init WW3" >&2; exit 3; }
  if ! git -C "$fork" cat-file -e "$rev^{commit}" 2>/dev/null; then
    # .gitmodules has an ssh URL, which a runner cannot use; the fork is public.
    git -C "$fork" -c url.https://github.com/.insteadOf=git@github.com: fetch -q --depth 1 origin "$rev" \
      || { echo "ww3_base: commit $rev is not in ${fork#"$root/"}/ and could not be fetched from its origin" >&2; exit 3; }
  fi
  if [ -e "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
    [ -f "$dir/.ww3-base" ] && [ ! -e "$dir/.git" ] && [[ "$dir" == "$root/"?* ]] \
      || { echo "ww3_base: $dir exists and this script did not write it (no .ww3-base): remove it yourself" >&2; exit 64; }
    rm -rf -- "$dir"
  fi
  mkdir -p "$dir"
  git -C "$fork" archive "$rev" | tar -x -C "$dir"
  printf '%s\n' "$rev" > "$dir/.ww3-base"
}

if [ "${1:-}" = --self-test ]; then
  t="$(mktemp -d)"; trap 'rm -rf "$t"' EXIT
  git init -q "$t/fork"
  mkdir -p "$t/fork/model/src"
  echo base > "$t/fork/model/src/CMakeLists.txt"
  git -C "$t/fork" add -A
  git -C "$t/fork" -c user.name=t -c user.email=t@example.invalid commit -q -m base
  b="$(git -C "$t/fork" rev-parse HEAD)"
  echo patched > "$t/fork/model/src/CMakeLists.txt"
  git -C "$t/fork" -c user.name=t -c user.email=t@example.invalid commit -q -am patch
  echo "$b" > "$t/base"
  [ "$(base_rev "$t/base")" = "$b" ] || { echo "ww3_base: rev lost the commit" >&2; exit 1; }
  echo "not a commit" > "$t/bad"
  (base_rev "$t/bad") >/dev/null 2>&1 && { echo "ww3_base: accepted a bad WW3_BASE" >&2; exit 1; }
  root="$t" export_tree "$t/fork" "$b" "$t/build/out"
  [ "$(cat "$t/build/out/model/src/CMakeLists.txt")" = base ] \
    || { echo "ww3_base: the export is not the base commit's tree" >&2; exit 1; }
  [ "$(cat "$t/build/out/.ww3-base")" = "$b" ] || { echo "ww3_base: no stamp" >&2; exit 1; }
  # A second export of the same commit leaves the directory alone.
  touch "$t/build/out/kept"
  root="$t" export_tree "$t/fork" "$b" "$t/build/out"
  [ -f "$t/build/out/kept" ] || { echo "ww3_base: an unchanged export was rewritten" >&2; exit 1; }
  # A directory this script did not write is never replaced.
  mkdir -p "$t/elsewhere"; echo mine > "$t/elsewhere/file"
  (root="$t" export_tree "$t/fork" "$b" "$t/elsewhere") >/dev/null 2>&1 \
    && { echo "ww3_base: replaced a directory it did not write" >&2; exit 1; }
  [ -f "$t/elsewhere/file" ] || { echo "ww3_base: removed a directory it did not write" >&2; exit 1; }
  # A new base replaces the tree this script wrote.
  p="$(git -C "$t/fork" rev-parse HEAD)"
  root="$t" export_tree "$t/fork" "$p" "$t/build/out"
  [ "$(cat "$t/build/out/model/src/CMakeLists.txt")" = patched ] && [ ! -e "$t/build/out/kept" ] \
    || { echo "ww3_base: a changed base did not replace the tree" >&2; exit 1; }
  # A checkout already at the base (an older clone's submodule directory) is used as it is.
  git clone -q "$t/fork" "$t/clone"
  root="$t" export_tree "$t/fork" "$p" "$t/clone"
  [ -d "$t/clone/.git" ] && [ ! -e "$t/clone/.ww3-base" ] \
    || { echo "ww3_base: rewrote a checkout that was at the base" >&2; exit 1; }
  # The committed base is a commit id.
  base_rev "$base_file" >/dev/null
  echo "ww3_base self-test: ok"
  exit 0
fi

case "${1:-}" in
  rev) base_rev "$base_file" ;;
  export)
    dir="${2:-$root/WW3-noaa}"
    # Normalised, so a `..` in <dir> cannot pass the "under this checkout" test below.
    dir="$(realpath -m -- "$dir")"
    export_tree "$root/WW3" "$(base_rev "$base_file")" "$dir"
    printf '%s\n' "$dir" ;;
  *) usage ;;
esac
