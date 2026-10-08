#!/usr/bin/env bash
# release — cut a citable release from main in one command:
#   1. refuse unless on main, clean, and level with origin/main
#   2. refuse a version that isn't vMAJOR.MINOR.PATCH or whose tag already exists
#   3. tag and push; .github/workflows/release.yml publishes the GitHub release and
#      Zenodo's webhook archives it and mints the version DOI
#   just release 0.2.0 --dry-run   prints what would happen.
set -euo pipefail
dry_run=0 version=""
for a in "$@"; do
  case "$a" in
    --dry-run) dry_run=1 ;;
    *) version="$a" ;;
  esac
done
[ -n "$version" ] || { echo "release: usage: just release <version> [--dry-run], e.g. just release 0.2.0" >&2; exit 1; }
tag="v${version#v}"
[[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "release: '$version' is not MAJOR.MINOR.PATCH" >&2; exit 1; }

[ "$(git branch --show-current)" = main ] || { echo "release: check out main first" >&2; exit 1; }
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "release: working tree is dirty — commit or stash first" >&2
  exit 1
fi
git fetch -q --tags origin main
[ "$(git rev-parse HEAD)" = "$(git rev-parse origin/main)" ] \
  || { echo "release: main is not level with origin/main — pull or push first" >&2; exit 1; }
if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
  echo "release: tag $tag already exists" >&2
  exit 1
fi

prev="$(git tag -l 'v*' --sort=-v:refname | head -1)"
echo "release: $tag at $(git log -1 --format='%h %s') (previous: ${prev:-none})"
if [ "$dry_run" -eq 1 ]; then
  echo "+ git tag -a $tag -m 'ww3-gpu $tag'"
  echo "+ git push origin $tag"
  exit 0
fi
git tag -a "$tag" -m "ww3-gpu $tag"
git push origin "$tag"
echo "release: pushed $tag. The Release workflow publishes it; Zenodo mints the DOI a few minutes later."
