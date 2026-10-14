#!/usr/bin/env python3
"""port_score: how much of WW3 runs on the Kokkos backend, and which routines are next.

    scripts/port_score.py                     print the ranking and the port score
    scripts/port_score.py --write             write them into kokkos/PORT_STATUS.md (between markers)
    scripts/port_score.py --check             exit 1 if PORT_STATUS.md's block is stale, or a ported
                                              routine has no kokkos/port_report/<ROUTINE>_KOKKOS.md (CI)
    scripts/port_score.py --from-gprof GPROF OUT.tsv
                                              reduce a `gprof -b` text to the profile TSV this reads
    scripts/port_score.py --measure           count WW3's Fortran lines and routines at ports.tsv's
                                              ww3_rev (needs the WW3/ submodule) and rewrite them
    scripts/port_score.py --self-test

Inputs: kokkos/ports.tsv (the ported routines and how far each was tested) and the committed
profile, docs/data/ww3_full_gprof.tsv (self time per routine, from `just profile-case full`).

Three scores, each with what it counts:
  time      the share of the profile's self time spent in routines that pass the WW3 replay (L2
            or gpu): what a run gains when those routines leave the Fortran
  routines  ported routines over every SUBROUTINE and FUNCTION in WW3's model/src
  lines     the ported routines' lines over every Fortran line in model/src
The profile is one case on one machine: the time score is that case's, not every model's.
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORTS = ROOT / "kokkos" / "ports.tsv"
PROFILE = ROOT / "docs" / "data" / "ww3_full_gprof.tsv"
STATUS_MD = ROOT / "kokkos" / "PORT_STATUS.md"
REPORTS = ROOT / "kokkos" / "port_report"
START, END = "<!-- port-score:start -->", "<!-- port-score:end -->"
STEPS = ["ported", "L1", "L2", "gpu"]
TOP = 15


def meta_and_rows(path):
    """A TSV with `# key<TAB>value` metadata lines, other `#` comments, then a header and rows."""
    meta, rows, header = {}, [], None
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        if line.startswith("#"):
            m = re.match(r"#\s*([a-z0-9_]+)\t(.*)$", line)
            if m:
                meta[m.group(1)] = m.group(2).strip()
            continue
        cells = line.split("\t")
        if header is None:
            header = cells
        else:
            rows.append(dict(zip(header, cells)))
    return meta, rows


def gprof_name(sym):
    """__w3snl1md_MOD_w3snl1 -> w3snl1md:w3snl1; w3srce_ -> w3srce."""
    s = sym.lower().lstrip("_")
    s = s.replace("_mod_", ":")
    return s.rstrip("_")


def from_gprof(text):
    """The flat profile of `gprof -b` as [(routine, self seconds, self %)], in its order."""
    out, in_flat = [], False
    for line in text.splitlines():
        if line.strip().startswith("%") and "cumulative" in line:
            in_flat = True
            continue
        if in_flat:
            if not line.strip():
                if out:
                    break
                continue
            parts = line.split()
            if len(parts) < 4 or "seconds" in line:
                continue
            try:
                pct, self_s = float(parts[0]), float(parts[2])
            except ValueError:
                continue
            out.append((gprof_name(parts[-1]), self_s, pct))
    return out


def status_rank(status):
    return STEPS.index(status) if status in STEPS else -1


def render(meta_p, prof, meta_w, ports):
    by_name = {p["routine"]: p for p in ports}
    total = sum(float(r["self_s"]) for r in prof) or 1.0
    lines = [
        f"Profile: {meta_p.get('case', '?')}, {meta_p.get('build', '?')}, "
        f"WW3@{meta_p.get('ww3_rev', '?')}, {meta_p.get('date', '?')} "
        f"(`{meta_p.get('command', 'just profile')}`). Self time; a routine's callees are not in its row.",
        "",
        "| # | routine | self s | self % | cumulative % | port status |",
        "|---|---|---|---|---|---|",
    ]
    cum = 0.0
    for i, r in enumerate(prof[:TOP], 1):
        s = float(r["self_s"])
        cum += s
        st = by_name.get(r["routine"], {}).get("status", "Fortran")
        lines.append(f"| {i} | `{r['routine']}` | {s:.2f} | {100 * s / total:.1f} | {100 * cum / total:.1f} | {st} |")
    passed = [p for p in ports if status_rank(p["status"]) >= STEPS.index("L2")]
    t_ported = sum(float(r["self_s"]) for r in prof if r["routine"] in {p["routine"] for p in passed})
    p_lines = sum(int(p["lines"]) for p in ports)
    w_lines, w_rout = int(meta_w["ww3_lines"]), int(meta_w["ww3_routines"])
    lines += [
        "",
        "| port score | value | what it counts |",
        "|---|---|---|",
        f"| time | **{100 * t_ported / total:.1f} %** | the profile's self time in routines that pass the WW3 replay (L2 or gpu) |",
        f"| routines | {len(ports)} of {w_rout} ({100 * len(ports) / w_rout:.2f} %) | ported routines over every SUBROUTINE and FUNCTION in model/src at WW3@{meta_w['ww3_rev']} |",
        f"| lines | {p_lines} of {w_lines} ({100 * p_lines / w_lines:.2f} %) | the ported routines' Fortran lines over model/src's |",
        "",
        "On a GPU: " + (", ".join(f"`{p['routine']}`" for p in ports if p["status"] == "gpu") or
                        "none yet. The CUDA build and the RTX 4090 run of `just smoke-hoff` are the next step."),
    ]
    return "\n".join(lines)


def block():
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, ports = meta_and_rows(PORTS)
    return render(meta_p, prof, meta_w, ports)


def report_name(routine):
    """w3snl1md:w3snl1 -> W3SNL1_KOKKOS.md, the report every ported routine has."""
    return routine.split(":")[-1].upper() + "_KOKKOS.md"


def missing_reports(ports, reports_dir):
    return [report_name(p["routine"]) for p in ports if not (reports_dir / report_name(p["routine"])).exists()]


def write_or_check(check):
    _, ports = meta_and_rows(PORTS)
    missing = missing_reports(ports, REPORTS)
    if missing:
        print(f"port_score: no port report for {', '.join(missing)} in kokkos/port_report/", file=sys.stderr)
        return 1
    text = STATUS_MD.read_text(encoding="utf-8")
    if text.count(START) != 1 or text.count(END) != 1:
        print(f"port_score: {STATUS_MD.name} needs one {START} and one {END}", file=sys.stderr)
        return 1
    head, rest = text.split(START)
    _, tail = rest.split(END)
    new = f"{head}{START}\n{block()}\n{END}{tail}"
    if new == text:
        print("port_score: PORT_STATUS.md is current")
        return 0
    if check:
        print("port_score: PORT_STATUS.md's port score is stale: run scripts/port_score.py --write", file=sys.stderr)
        return 1
    STATUS_MD.write_text(new, encoding="utf-8")
    print("port_score: wrote PORT_STATUS.md")
    return 0


def measure():
    meta_w, _ = meta_and_rows(PORTS)
    rev = meta_w["ww3_rev"]
    ww3 = ROOT / "WW3"
    files = subprocess.run(["git", "-C", str(ww3), "ls-tree", "-r", "--name-only", rev, "model/src"],
                           check=True, capture_output=True, text=True).stdout.split()
    routine = re.compile(r"^\s*(?:(?:recursive|pure|elemental|module)\s+)*(?:subroutine|(?:[a-z ()*0-9=,]*\s)?function)\s+\w+", re.I)
    n_lines = n_rout = 0
    for f in files:
        if not f.endswith(".F90"):
            continue
        src = subprocess.run(["git", "-C", str(ww3), "show", f"{rev}:{f}"], check=True,
                             capture_output=True, text=True, errors="replace").stdout
        n_lines += src.count("\n")
        n_rout += sum(1 for ln in src.splitlines() if routine.match(ln) and not re.match(r"^\s*end\s", ln, re.I))
    text = PORTS.read_text(encoding="utf-8")
    text = re.sub(r"^# ww3_lines\t.*$", f"# ww3_lines\t{n_lines}", text, flags=re.M)
    text = re.sub(r"^# ww3_routines\t.*$", f"# ww3_routines\t{n_rout}", text, flags=re.M)
    PORTS.write_text(text, encoding="utf-8")
    print(f"port_score: WW3@{rev}: {n_lines} Fortran lines, {n_rout} routines in model/src")
    return 0


def self_test():
    fails = 0

    def ok(cond, label):
        nonlocal fails
        print(f"  {'ok  ' if cond else 'FAIL'} {label}")
        fails += 0 if cond else 1

    gp = """Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total
 time   seconds   seconds    calls  ms/call  ms/call  name
 67.45      2.01     2.01    14401     0.14     0.14  __w3src4md_MOD_w3sds4
 14.09      2.43     0.42    14401     0.03     0.03  __w3snl1md_MOD_w3snl1
 10.07      2.73     0.30                             __w3src4md_MOD_w3sin4
  8.39      2.98     0.25                             w3srce_

 %         the percentage of the total running time of the
"""
    prof = from_gprof(gp)
    ok([p[0] for p in prof] == ["w3src4md:w3sds4", "w3snl1md:w3snl1", "w3src4md:w3sin4", "w3srce"],
       "gprof's flat profile is read in order, names as module:routine")
    ok(abs(prof[1][1] - 0.42) < 1e-9, "self seconds are the third column")
    rows = [{"routine": r, "self_s": str(s), "self_pct": str(p)} for r, s, p in prof]
    ports = [{"routine": "w3snl1md:w3snl1", "lines": "359", "status": "L2"},
             {"routine": "w3snl1md:insnl1", "lines": "304", "status": "L1"}]
    out = render({"case": "t"}, rows, {"ww3_rev": "abc", "ww3_lines": "1000", "ww3_routines": "100"}, ports)
    ok("**14.1 %**" in out, "the time score counts only L2 and beyond (0.42 of 2.98 s)")
    ok("| 2 | `w3snl1md:w3snl1` | 0.42 | 14.1 | 81.5 | L2 |" in out, "a ported routine shows its status in the ranking")
    ok("| 1 | `w3src4md:w3sds4` | 2.01 | 67.4 | 67.4 | Fortran |" in out, "an unported routine is Fortran")
    ok("2 of 100 (2.00 %)" in out and "663 of 1000 (66.30 %)" in out, "routines and lines count every ported row")
    ok("On a GPU: none yet" in out, "no gpu row, said so")
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "x.tsv"
        p.write_text("# about\n# ww3_rev\tabc\nroutine\tlines\nx\t3\n", encoding="utf-8")
        meta, rws = meta_and_rows(p)
        ok(meta == {"ww3_rev": "abc"} and rws == [{"routine": "x", "lines": "3"}], "metadata lines and rows are read apart")
    ok(report_name("w3snl1md:w3snl1") == "W3SNL1_KOKKOS.md", "a report is named after the routine, upper case")
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "W3SNL1_KOKKOS.md").write_text("x", encoding="utf-8")
        ok(missing_reports(ports, Path(tmp)) == ["INSNL1_KOKKOS.md"], "a ported routine without a report is named")
    print("port_score self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--measure", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--from-gprof", nargs=2, metavar=("GPROF", "OUT"))
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if a.measure:
        return measure()
    if a.from_gprof:
        rows = from_gprof(Path(a.from_gprof[0]).read_text(encoding="utf-8", errors="replace"))
        out = Path(a.from_gprof[1])
        head = out.read_text(encoding="utf-8").split("routine\t")[0] if out.exists() else ""
        out.write_text(head + "routine\tself_s\tself_pct\n" +
                       "".join(f"{r}\t{s:.2f}\t{p:.2f}\n" for r, s, p in rows), encoding="utf-8")
        print(f"port_score: {len(rows)} routines into {out}")
        return 0
    if a.write or a.check:
        return write_or_check(a.check)
    print(block())
    return 0


if __name__ == "__main__":
    sys.exit(main())
