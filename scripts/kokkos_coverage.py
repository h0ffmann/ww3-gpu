#!/usr/bin/env python3
"""kokkos_coverage: how much of each port's Kokkos code the GoogleTest suite runs.

    scripts/kokkos_coverage.py              build kokkos/ with --coverage in kokkos/build/coverage,
                                            run CTest, gcov the files in ports.tsv's `kokkos`
                                            column, write docs/data/kokkos_coverage.tsv
    scripts/kokkos_coverage.py --dry-run    print the commands, change nothing
    scripts/kokkos_coverage.py --self-test

Line and function coverage per file, from gcov's JSON (gcc 15), Debug at -O0, the lab shell's
default backends (OpenMP and Serial; no CUDA). It measures what CTest runs, the L1 level; the L2
replay inside WW3 is not instrumented. A line that ran is not a line that was checked: a test can
execute a line whose result it never compares. `just kokkos-coverage` wraps this.
"""

import argparse
import datetime
import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORTS = ROOT / "kokkos" / "ports.tsv"
OUT = ROOT / "docs" / "data" / "kokkos_coverage.tsv"
BUILD = ROOT / "kokkos" / "build" / "coverage"
SHELL = ["nix", "develop", f"{ROOT}/nix-config/labs/pratico#ww3", "--command"]
CONFIGURE = ["cmake", "-S", str(ROOT / "kokkos"), "-B", str(BUILD), "-DCMAKE_BUILD_TYPE=Debug",
             "-DCMAKE_CXX_FLAGS=--coverage -O0", "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
             "-DCMAKE_Fortran_FLAGS=--coverage", "-DWW_DETERMINISTIC=ON"]


def port_files(path=PORTS):
    """{routine: [kokkos/src/... paths]} from ports.tsv's `kokkos` column (comma-separated)."""
    header, out = None, {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        cells = line.split("\t")
        if header is None:
            header = cells
            continue
        row = dict(zip(header, cells))
        out[row["routine"]] = [f.strip() for f in row.get("kokkos", "").split(",") if f.strip()]
    return out


def ranges(nums):
    """[89, 90, 91, 120] -> '89-91, 120'."""
    out, nums = [], sorted(set(nums))
    i = 0
    while i < len(nums):
        j = i
        while j + 1 < len(nums) and nums[j + 1] == nums[j] + 1:
            j += 1
        out.append(str(nums[i]) if i == j else f"{nums[i]}-{nums[j]}")
        i = j + 1
    return ", ".join(out)


def from_gcov_json(text, source):
    """(lines, lines run, functions, functions run, the line numbers not run) of `source` only."""
    lines, funcs = {}, {}
    for doc in text.splitlines():
        if not doc.startswith("{"):
            continue
        for f in json.loads(doc).get("files", []):
            if not f["file"].endswith(source):
                continue
            for ln in f["lines"]:
                lines[ln["line_number"]] = max(lines.get(ln["line_number"], 0), ln["count"])
            for fn in f["functions"]:
                funcs[fn["name"]] = max(funcs.get(fn["name"], 0), fn["execution_count"])
    missed = [n for n, c in lines.items() if c == 0]
    return len(lines), len(lines) - len(missed), len(funcs), sum(1 for c in funcs.values() if c), missed


def gcno_for(src):
    """kokkos/src/ww_kokkos/snl1_dia.cpp -> the .gcno CMake writes for it in the ww_kokkos target."""
    rel = Path(src).relative_to("kokkos")
    return BUILD / "CMakeFiles" / "ww_kokkos.dir" / f"{rel}.gcno"


def run(cmd, dry, **kw):
    print("+ " + " ".join(shlex.quote(c) for c in cmd), file=sys.stderr)
    if dry:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return subprocess.run(cmd, check=True, text=True, **kw)


def measure(dry):
    files = sorted({f for fs in port_files().values() for f in fs})
    if not files:
        print("kokkos_coverage: no file in ports.tsv's `kokkos` column", file=sys.stderr)
        return 1
    run(SHELL + CONFIGURE, dry, stdout=subprocess.DEVNULL)
    run(SHELL + ["cmake", "--build", str(BUILD), "-j"], dry, stdout=subprocess.DEVNULL)
    for gcda in ([] if dry else BUILD.rglob("*.gcda")):
        gcda.unlink()
    ctest = run(SHELL + ["ctest", "--test-dir", str(BUILD)], dry, capture_output=True)
    summary = next((ln.strip() for ln in ctest.stdout.splitlines() if "tests passed" in ln), "dry run")
    rows = []
    for src in files:
        g = run(SHELL + ["gcov", "--json-format", "--stdout", "-o", str(gcno_for(src)), str(ROOT / src)],
                dry, capture_output=True, cwd=BUILD if not dry else None)
        n, hit, nf, nfhit, missed = from_gcov_json(g.stdout, src)
        rows.append((src, n, hit, nf, nfhit, ranges(missed)))
    if dry:
        return 0
    sha = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"], check=True,
                         capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain", "--", "kokkos/src", "kokkos/tests"],
                           check=True, capture_output=True, text=True).stdout.strip()
    head = ("# Line and function coverage of each port's Kokkos files under CTest, written by\n"
            "# scripts/kokkos_coverage.py (`just kokkos-coverage`); do not edit.\n"
            f"# date\t{datetime.date.today().isoformat()}\n"
            f"# commit\t{sha}{' (kokkos/ dirty)' if dirty else ''}\n"
            "# build\tDebug -O0 --coverage, OpenMP and Serial backends, WW_DETERMINISTIC=ON; no CUDA\n"
            f"# ctest\t{summary}\n"
            "# command\tjust kokkos-coverage\n")
    body = "file\tlines\tlines_run\tfunctions\tfunctions_run\tlines_not_run\n" + "".join(
        f"{s}\t{n}\t{h}\t{nf}\t{nfh}\t{m}\n" for s, n, h, nf, nfh, m in rows)
    OUT.write_text(head + body, encoding="utf-8")
    print(f"kokkos_coverage: {len(rows)} files into {OUT.relative_to(ROOT)} ({summary})")
    return 0


def self_test():
    fails = 0

    def ok(cond, label):
        nonlocal fails
        print(f"  {'ok  ' if cond else 'FAIL'} {label}")
        fails += 0 if cond else 1

    ok(ranges([120, 89, 90, 91, 90]) == "89-91, 120", "line numbers fold into ranges")
    ok(ranges([]) == "", "no missed line, empty")
    doc = json.dumps({"files": [
        {"file": "/nix/store/x/Kokkos_Core.hpp", "lines": [{"line_number": 1, "count": 0}], "functions": []},
        {"file": "/r/kokkos/src/ww_kokkos/a.cpp",
         "lines": [{"line_number": 10, "count": 3}, {"line_number": 11, "count": 0}, {"line_number": 12, "count": 0}],
         "functions": [{"name": "f", "execution_count": 1}, {"name": "g", "execution_count": 0}]}]})
    ok(from_gcov_json("gcov: note\n" + doc, "kokkos/src/ww_kokkos/a.cpp") == (3, 1, 2, 1, [11, 12]),
       "only the port's file counts; headers from the store do not")
    ok(from_gcov_json(doc + "\n" + doc.replace('"count": 0}, {"line_number": 12', '"count": 5}, {"line_number": 12'),
                      "kokkos/src/ww_kokkos/a.cpp")[1] == 2, "a line run in any instantiation counts as run")
    ok(gcno_for("kokkos/src/ww_kokkos/snl1_dia.cpp").name == "snl1_dia.cpp.gcno", "the notes file keeps the .cpp")
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "ports.tsv"
        p.write_text("# c\nroutine\tstatus\tkokkos\na\tL2\tx.cpp, y.cpp\nb\tL1\t\n", encoding="utf-8")
        ok(port_files(p) == {"a": ["x.cpp", "y.cpp"], "b": []}, "the kokkos column is read, empty allowed")
    print("kokkos_coverage self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    return self_test() if a.self_test else measure(a.dry_run)


if __name__ == "__main__":
    sys.exit(main())
