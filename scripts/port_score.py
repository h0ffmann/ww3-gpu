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
  files     WW3 files with a ported routine over every .F90 file in model/src
  lines     the ported routines' lines over every Fortran line in model/src
and, beside them, the size of the port itself: the C++ files and lines under kokkos/src.
--write also writes site/data/port_score.tsv, which the website reads (site/content).
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
SITE_TSV = ROOT / "site" / "data" / "port_score.tsv"
KOKKOS_SRC = ROOT / "kokkos" / "src"
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


def kokkos_size(src=None):
    """(files, lines) of the port's C++ under kokkos/src: every .cpp and .hpp, every line."""
    src = src or KOKKOS_SRC
    files = sorted(p for p in src.rglob("*") if p.suffix in (".cpp", ".hpp"))
    return len(files), sum(p.read_text(encoding="utf-8").count("\n") for p in files)


def summary(meta_p, prof, meta_w, ports, kokkos=(0, 0)):
    """The global numbers, one dict, for the PORT_STATUS block and the website."""
    total = sum(float(r["self_s"]) for r in prof) or 1.0
    passed = {p["routine"] for p in ports if status_rank(p["status"]) >= STEPS.index("L2")}
    t_ported = sum(float(r["self_s"]) for r in prof if r["routine"] in passed)
    files = sorted({p["file"].split(":")[0] for p in ports if p.get("file")})
    lines = sum(int(p["lines"]) for p in ports)
    w_lines, w_rout, w_files = int(meta_w["ww3_lines"]), int(meta_w["ww3_routines"]), int(meta_w.get("ww3_files", 0) or 0)
    return {
        "time_pct": 100 * t_ported / total, "routines": len(ports), "ww3_routines": w_rout,
        "files": len(files), "ww3_files": w_files, "lines": lines, "ww3_lines": w_lines,
        "kokkos_files": kokkos[0], "kokkos_lines": kokkos[1], "ww3_rev": meta_w["ww3_rev"],
        "case": meta_p.get("case", "?"),
    }


def pct(a, b):
    return f"{100 * a / b:.2f} %" if b else "?"


def grouped(n):
    return f"{n:,}".replace(",", " ")


def site_rows(s):
    """The rows of site/data/port_score.tsv: name, share (the big number), count, what it counts."""
    return [
        ("of the run time", f"{s['time_pct']:.1f} %", "", f"the profile's self time in routines that pass the WW3 replay; profile: {s['case']}"),
        ("of the lines of code", pct(s['lines'], s['ww3_lines']), f"{grouped(s['lines'])} of {grouped(s['ww3_lines'])}", "the ported routines' Fortran lines, of every line in WW3's model/src"),
        ("of the files", pct(s['files'], s['ww3_files']), f"{s['files']} of {s['ww3_files']}", "WW3 .F90 files with a ported routine"),
        ("of the routines", pct(s['routines'], s['ww3_routines']), f"{s['routines']} of {grouped(s['ww3_routines'])}", "ported routines, of every SUBROUTINE and FUNCTION in model/src"),
        ("lines of Kokkos C++", grouped(s['kokkos_lines']), f"in {s['kokkos_files']} files", "under kokkos/src: the port itself"),
    ]


def site_tsv(s):
    head = ("# The global numbers of the Kokkos port, written by scripts/port_score.py --write; do not edit.\n"
            f"# WW3@{s['ww3_rev']}. The website reads this file (site/content).\n"
            "# name\tshare\tcount\twhat\n")
    return head + "".join("\t".join(r) + "\n" for r in site_rows(s))


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
    s = summary(meta_p, prof, meta_w, ports, kokkos_size() if KOKKOS_SRC.exists() else (0, 0))
    lines += [
        "",
        "| port score | value | what it counts |",
        "|---|---|---|",
        f"| time | **{s['time_pct']:.1f} %** | the profile's self time in routines that pass the WW3 replay (L2 or gpu) |",
        f"| routines | {s['routines']} of {s['ww3_routines']} ({pct(s['routines'], s['ww3_routines'])}) | ported routines over every SUBROUTINE and FUNCTION in model/src at WW3@{meta_w['ww3_rev']} |",
        f"| files | {s['files']} of {s['ww3_files']} ({pct(s['files'], s['ww3_files'])}) | WW3 .F90 files with a ported routine, over every .F90 file in model/src |",
        f"| lines | {s['lines']} of {s['ww3_lines']} ({pct(s['lines'], s['ww3_lines'])}) | the ported routines' Fortran lines over model/src's |",
        f"| the port | {s['kokkos_lines']} lines in {s['kokkos_files']} files | the C++ under kokkos/src (every .cpp and .hpp line) |",
        "",
        "On a GPU: " + (", ".join(f"`{p['routine']}`" for p in ports if p["status"] == "gpu") or
                        "none yet. The CUDA build links; the RTX 4090 run, `just smoke-hoff gpu`, is the owner's, supervised."),
    ]
    return "\n".join(lines)


def block():
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, ports = meta_and_rows(PORTS)
    return render(meta_p, prof, meta_w, ports)


def report_name(routine):
    """w3snl1md:w3snl1 -> W3SNL1_KOKKOS.md, the report every ported routine has."""
    return routine.split(":")[-1].upper() + "_KOKKOS.md"


def unrecorded(ports):
    """Rows that do not say, yes or no, whether the Kokkos code was regtested and GoogleTested."""
    bad = []
    for p in ports:
        for col in ("regtested", "googletest"):
            if not re.match(r"^(yes|no): \S", p.get(col, "")):
                bad.append(f"{p['routine']} ({col})")
    return bad


def missing_reports(ports, reports_dir):
    return [report_name(p["routine"]) for p in ports if not (reports_dir / report_name(p["routine"])).exists()]


def write_or_check(check):
    _, ports = meta_and_rows(PORTS)
    missing = missing_reports(ports, REPORTS)
    if missing:
        print(f"port_score: no port report for {', '.join(missing)} in kokkos/port_report/", file=sys.stderr)
        return 1
    bad = unrecorded(ports)
    if bad:
        print(f"port_score: kokkos/ports.tsv must say 'yes: …' or 'no: …' for {', '.join(bad)}", file=sys.stderr)
        return 1
    text = STATUS_MD.read_text(encoding="utf-8")
    if text.count(START) != 1 or text.count(END) != 1:
        print(f"port_score: {STATUS_MD.name} needs one {START} and one {END}", file=sys.stderr)
        return 1
    head, rest = text.split(START)
    _, tail = rest.split(END)
    new = f"{head}{START}\n{block()}\n{END}{tail}"
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, _ = meta_and_rows(PORTS)
    tsv = site_tsv(summary(meta_p, prof, meta_w, ports, kokkos_size()))
    old_tsv = SITE_TSV.read_text(encoding="utf-8") if SITE_TSV.exists() else ""
    if new == text and tsv == old_tsv:
        print("port_score: PORT_STATUS.md and site/data/port_score.tsv are current")
        return 0
    if check:
        print("port_score: the port score is stale (PORT_STATUS.md or site/data/port_score.tsv): run scripts/port_score.py --write", file=sys.stderr)
        return 1
    STATUS_MD.write_text(new, encoding="utf-8")
    SITE_TSV.write_text(tsv, encoding="utf-8")
    print("port_score: wrote PORT_STATUS.md and site/data/port_score.tsv")
    return 0


def measure():
    meta_w, _ = meta_and_rows(PORTS)
    rev = meta_w["ww3_rev"]
    ww3 = ROOT / "WW3"
    files = subprocess.run(["git", "-C", str(ww3), "ls-tree", "-r", "--name-only", rev, "model/src"],
                           check=True, capture_output=True, text=True).stdout.split()
    routine = re.compile(r"^\s*(?:(?:recursive|pure|elemental|module)\s+)*(?:subroutine|(?:[a-z ()*0-9=,]*\s)?function)\s+\w+", re.I)
    n_lines = n_rout = n_files = 0
    for f in files:
        if not f.endswith(".F90"):
            continue
        n_files += 1
        src = subprocess.run(["git", "-C", str(ww3), "show", f"{rev}:{f}"], check=True,
                             capture_output=True, text=True, errors="replace").stdout
        n_lines += src.count("\n")
        n_rout += sum(1 for ln in src.splitlines() if routine.match(ln) and not re.match(r"^\s*end\s", ln, re.I))
    text = PORTS.read_text(encoding="utf-8")
    text = re.sub(r"^# ww3_lines\t.*$", f"# ww3_lines\t{n_lines}", text, flags=re.M)
    text = re.sub(r"^# ww3_routines\t.*$", f"# ww3_routines\t{n_rout}", text, flags=re.M)
    if re.search(r"^# ww3_files\t", text, flags=re.M):
        text = re.sub(r"^# ww3_files\t.*$", f"# ww3_files\t{n_files}", text, flags=re.M)
    else:
        text = re.sub(r"^(# ww3_routines\t.*)$", rf"\1\n# ww3_files\t{n_files}", text, flags=re.M)
    PORTS.write_text(text, encoding="utf-8")
    print(f"port_score: WW3@{rev}: {n_lines} Fortran lines, {n_rout} routines, {n_files} .F90 files in model/src")
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
    ports_f = [dict(p, file="model/src/w3snl1md.F90:1-2") for p in ports]
    s = summary({"case": "t"}, rows, {"ww3_rev": "abc", "ww3_lines": "1000", "ww3_routines": "100", "ww3_files": "4"}, ports_f, (3, 120))
    ok(s["files"] == 1 and s["ww3_files"] == 4, "two routines of one file are one file")
    t = site_tsv(s)
    ok("of the files\t25.00 %\t1 of 4\tWW3 .F90 files with a ported routine" in t, "the site's file row says 25 %, 1 of 4")
    ok("lines of Kokkos C++\t120\tin 3 files\t" in t, "the port's own size is a row")
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "a.cpp").write_text("x\ny\n", encoding="utf-8")
        (Path(tmp) / "b.hpp").write_text("z\n", encoding="utf-8")
        (Path(tmp) / "c.txt").write_text("no\n", encoding="utf-8")
        ok(kokkos_size(Path(tmp)) == (2, 3), "the port's size counts .cpp and .hpp lines only")
    ok("On a GPU: none yet" in out, "no gpu row, said so")
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "x.tsv"
        p.write_text("# about\n# ww3_rev\tabc\nroutine\tlines\nx\t3\n", encoding="utf-8")
        meta, rws = meta_and_rows(p)
        ok(meta == {"ww3_rev": "abc"} and rws == [{"routine": "x", "lines": "3"}], "metadata lines and rows are read apart")
    ok(unrecorded([{"routine": "a", "regtested": "yes: L2", "googletest": "no: none yet"}]) == [],
       "yes and no, each with a reason, are records")
    ok(unrecorded([{"routine": "a", "regtested": "", "googletest": "maybe"}]) == ["a (regtested)", "a (googletest)"],
       "an empty or unclear record is refused")
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
