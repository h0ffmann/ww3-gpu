#!/usr/bin/env python3
"""ww4_status — what changed in NOAA-EMC/WW4 since the last recorded snapshot.

    scripts/ww4_status.py            # fetch WW4, print the changes as markdown
    scripts/ww4_status.py --write    # ... and record the new state in docs/ww4-status.json
    scripts/ww4_status.py --json     # print the current state as JSON

Git only, no GitHub API and no token: everything comes from a clone of the public repository
(develop, tags, VERSION, refs/pull/*/head) kept in $WW4_CACHE (default ~/.cache/ww3-gpu/WW4).
What git cannot see (issue state, PR open/merged state, review comments, the wiki, Office Notes)
is left to the /ww4-status skill, which reads those pages by hand.

A source term or solver counts as "implemented" when its .cpp has more than STUB_LOC code lines
(non-blank, non-comment); the PR #72 placeholders are 7 (source terms) and 29 (solvers, with a
"Mock propagation" loop). That threshold is a heuristic: the report prints the line counts so a
reader can judge. Standard library only.
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import datetime as dt

ROOT = pathlib.Path(__file__).resolve().parent.parent
STATE = ROOT / "docs" / "ww4-status.json"
URL = "https://github.com/NOAA-EMC/WW4"
CACHE = pathlib.Path(os.environ.get("WW4_CACHE", pathlib.Path.home() / ".cache" / "ww3-gpu" / "WW4"))
STUB_LOC = 60
PHYSICS = re.compile(r"^src/ww4_core/(source_terms/ww4_\w+|solver_\w+)/[^/]+\.cpp$")


def git(*args, check=True) -> str:
    r = subprocess.run(["git", "-C", str(CACHE), *args], capture_output=True, text=True)
    if check and r.returncode:
        sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout


def sync() -> None:
    if not (CACHE / ".git").exists():
        CACHE.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", "-q", "--filter=blob:none", URL, str(CACHE)], check=True)
    git("fetch", "-q", "--prune", "--tags", "origin",
        "+refs/heads/develop:refs/remotes/origin/develop", "+refs/pull/*/head:refs/pr/*")


def code_loc(ref: str, path: str) -> int:
    n, block = 0, False
    for line in git("show", f"{ref}:{path}").splitlines():
        s = line.strip()
        if block:
            block = "*/" not in s
            continue
        if s.startswith("/*"):
            block = "*/" not in s
            continue
        if s and not s.startswith("//"):
            n += 1
    return n


def physics(ref: str) -> dict:
    files = [f for f in git("ls-tree", "-r", "--name-only", ref, "src").splitlines() if PHYSICS.match(f)]
    return {pathlib.PurePosixPath(f).parent.name: code_loc(ref, f) for f in files}


def spectral(ref: str) -> dict:
    yaml = git("show", f"{ref}:templates/ww4_run_config.yaml", check=False)
    block = yaml.split("spectral_space:", 1)[1] if "spectral_space:" in yaml else ""
    out = {}
    for line in block.splitlines()[1:]:
        if line and not line.startswith(" "):
            break
        m = re.match(r"\s+(\w+):\s*([-\d.]+)", line)
        if m:
            out[m.group(1)] = float(m.group(2))
    return out


def kokkos(ref: str) -> list:
    return sorted(git("grep", "-il", "kokkos", ref, "--", ".", check=False).replace(f"{ref}:", "").split())


def state() -> dict:
    dev = "origin/develop"
    prs = {}
    for line in git("for-each-ref", "--format=%(refname:short) %(objectname:short)", "refs/pr").splitlines():
        name, sha = line.split()
        prs[name.split("/")[-1]] = sha
    log = git("log", "-1", "--format=%h|%cs|%s", dev).strip().split("|", 2)
    return {
        "checked": dt.date.today().isoformat(),
        "develop": {"sha": log[0], "date": log[1], "subject": log[2],
                    "commits": int(git("rev-list", "--count", dev))},
        "version": git("show", f"{dev}:VERSION", check=False).strip(),
        "tags": sorted(git("tag").split()),
        "spectral_space": spectral(dev),
        "kokkos_files": kokkos(dev),
        "physics_loc": physics(dev),
        "tests": sorted({m for m in re.findall(r"\b(L\d)_test", git("ls-tree", "-r", "--name-only", dev, "tests"))}),
        "pr_heads": dict(sorted(prs.items(), key=lambda kv: int(kv[0]))),
    }


def implemented(loc: dict) -> list:
    return sorted(k for k, v in loc.items() if v > STUB_LOC)


def report(old: dict, new: dict) -> str:
    out = [f"# WW4 since {old.get('checked', '(no snapshot)')}  (checked {new['checked']})", ""]
    d0, d1 = old.get("develop", {}), new["develop"]
    if d0.get("sha") != d1["sha"]:
        out += [f"## develop: {d0.get('commits', '?')} -> {d1['commits']} commits, now `{d1['sha']}` ({d1['date']})", "```"]
        rng = f"{d0['sha']}..origin/develop" if d0.get("sha") else "-15 origin/develop"
        out += [git("log", "--format=%h %cs %an | %s", *rng.split()).rstrip(), "```", ""]
    for key, label in [("version", "VERSION"), ("tags", "tags / releases"), ("tests", "test levels present"),
                       ("spectral_space", "default spectral space (templates/ww4_run_config.yaml)"),
                       ("kokkos_files", "files on develop mentioning Kokkos")]:
        if old.get(key) != new[key]:
            out += [f"## {label} changed", f"- was: `{old.get(key)}`", f"- now: `{new[key]}`", ""]
    if old.get("physics_loc") != new["physics_loc"]:
        out += ["## source terms and solvers on develop (code lines; > %d = implemented)" % STUB_LOC]
        for k in sorted(set(old.get("physics_loc", {})) | set(new["physics_loc"])):
            a, b = old.get("physics_loc", {}).get(k), new["physics_loc"].get(k)
            if a != b:
                out.append(f"- `{k}`: {a} -> {b}" + ("  **IMPLEMENTED**" if (b or 0) > STUB_LOC >= (a or 0) else ""))
        out.append("")
    p0, p1 = old.get("pr_heads", {}), new["pr_heads"]
    changed = [n for n in p1 if p0.get(n) != p1[n]]
    if changed:
        out += ["## pull requests with new or moved heads (open, merged or closed: check each page)"]
        for n in changed:
            ref = f"refs/pr/{n}"
            subj = git("log", "-1", "--format=%cs %an | %s", ref).strip()
            loc = physics(ref)
            impl = implemented(loc)
            kk = kokkos(ref)
            extra = []
            if impl:
                extra.append("implements: " + ", ".join(f"{k} ({loc[k]} lines)" for k in impl))
            if kk:
                extra.append(f"mentions Kokkos in {len(kk)} files")
            out.append(f"- #{n} `{p1[n]}` {subj}  {URL}/pull/{n}" + ("".join(f"\n  - {e}" for e in extra)))
        out.append("")
    if len(out) == 2:
        out.append("No change in anything git can see. Still check the pages listed in the /ww4-status skill.")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--write", action="store_true", help=f"record the new state in {STATE.relative_to(ROOT)}")
    ap.add_argument("--json", action="store_true", help="print the current state as JSON and exit")
    a = ap.parse_args()
    sync()
    new = state()
    if a.json:
        print(json.dumps(new, indent=2))
        return 0
    old = json.loads(STATE.read_text()) if STATE.exists() else {}
    print(report(old, new))
    if a.write:
        STATE.write_text(json.dumps(new, indent=2) + "\n")
        print(f"\nrecorded {STATE.relative_to(ROOT)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
