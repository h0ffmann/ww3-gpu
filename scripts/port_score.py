#!/usr/bin/env python3
"""port_score: how much of WW3 runs on the Kokkos backend, and which routines are next.

    scripts/port_score.py                     print the ranking and the port score
    scripts/port_score.py --write             write them into kokkos/PORT_STATUS.md (between markers),
                                              and write kokkos/PORT_RANKING.md
    scripts/port_score.py --check             exit 1 if PORT_STATUS.md's block or PORT_RANKING.md is
                                              stale, or a ported routine has no
                                              kokkos/port_report/<ROUTINE>_KOKKOS.md (CI)
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
--write also writes site/data/port_score.tsv, which the website reads (site/content), and
kokkos/PORT_RANKING.md: the routines still in Fortran by self time, up to RANK_TOP, and for each
ported routine the coverage of its Kokkos files from docs/data/kokkos_coverage.tsv
(`just kokkos-coverage`).
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
RANKING_MD = ROOT / "kokkos" / "PORT_RANKING.md"
COVERAGE = ROOT / "docs" / "data" / "kokkos_coverage.tsv"
KOKKOS_SRC = ROOT / "kokkos" / "src"
START, END = "<!-- port-score:start -->", "<!-- port-score:end -->"
STEPS = ["ported", "L1", "L2", "gpu"]
TOP = 15
RANK_TOP = 100


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


def ww3_file(routine):
    """w3src4md:w3sds4 -> model/src/w3src4md.F90; a name without a module has no file here."""
    mod = routine.split(":")[0] if ":" in routine else ""
    return f"model/src/{mod}.F90" if mod else "—"


def coverage_of(files, cov):
    """(lines, run, functions, run, ["file: ranges"]) summed over a port's files."""
    rows = [cov[f] for f in files if f in cov]
    tot = [sum(int(r[k]) for r in rows) for k in ("lines", "lines_run", "functions", "functions_run")]
    gaps = [f"`{Path(r['file']).name}` {r['lines_not_run']}" for r in rows if r.get("lines_not_run")]
    return (*tot, gaps, len(rows) == len(files))


def ranking(meta_p, prof, meta_w, ports, meta_c, cov_rows):
    by_name = {p["routine"]: p for p in ports}
    cov = {r["file"]: r for r in cov_rows}
    total = sum(float(r["self_s"]) for r in prof) or 1.0
    todo = [r for r in prof if r["routine"] not in by_name][:RANK_TOP]
    timed = sum(1 for r in todo if float(r["self_s"]) > 0)
    out = [
        "# PORT_RANKING",
        "",
        "The WW3 routines still in Fortran, the most expensive first, and how much of each ported",
        "routine's Kokkos code the tests run. Written by `scripts/port_score.py --write` from",
        "`docs/data/ww3_full_gprof.tsv`, `kokkos/ports.tsv` and `docs/data/kokkos_coverage.tsv`; do not",
        "edit. Port order is wall time ([ADR-0002](../docs/ADRs/ADR-0002-port-order-wall-time.md)):",
        "the next port is row 1, and `/port-next` starts it.",
        "",
        "## Still in Fortran, by self time",
        "",
        f"Profile: {meta_p.get('case', '?')}, {meta_p.get('build', '?')}, WW3@{meta_p.get('ww3_rev', '?')}, "
        f"{meta_p.get('date', '?')} (`{meta_p.get('command', '?')}`). Self time, so a routine's callees and",
        "what the compiler inlined into it are in the caller's row; the shares are this case's and this",
        "switch's, not every model's.",
        "",
        f"The profile names {len(prof)} routines, {len(prof) - len(todo)} of them ported, so this table has "
        f"{len(todo)} rows, not {RANK_TOP}. Rows {timed + 1} to {len(todo)} took less than one gprof sample "
        "(10 ms): their order is gprof's, not a cost order, and a longer case or `perf` is needed to rank them. "
        "A routine this case never calls (another switch, another grid) is not in the profile and not here.",
        "",
        "| # | routine | WW3 file | self s | self % | cumulative % |",
        "|---|---|---|---|---|---|",
    ]
    cum = sum(float(r["self_s"]) for r in prof if r["routine"] in by_name)
    for i, r in enumerate(todo, 1):
        sec = float(r["self_s"])
        cum += sec
        shown = f"{sec:.2f}" if sec > 0 else "< 0.01"
        out.append(f"| {i} | `{r['routine']}` | `{ww3_file(r['routine'])}` | {shown} | {100 * sec / total:.2f} | {100 * cum / total:.1f} |")
    out += [
        "",
        "The cumulative column starts from the ported routines' share, so it reads as the share of the run",
        "that would be in Kokkos once every row down to it is ported.",
        "",
        "## Tested, per port",
        "",
        f"Coverage: {meta_c.get('build', '?')}; {meta_c.get('ctest', '?')}; commit {meta_c.get('commit', '?')}, "
        f"{meta_c.get('date', '?')} (`{meta_c.get('command', '?')}`).",
        "",
        "| routine | status | Kokkos files | lines run | functions run | lines not run | regtested |",
        "|---|---|---|---|---|---|---|",
    ]
    for p in ports:
        files = [f.strip() for f in p.get("kokkos", "").split(",") if f.strip()]
        n, hit, nf, nfh, gaps, complete = coverage_of(files, cov)
        lines_cell = f"{hit} of {n} ({pct(hit, n)})" if n else "not measured"
        if n and not complete:
            lines_cell += ", some files not measured"
        out.append(f"| `{p['routine']}` | {p['status']} | {', '.join(f'`{Path(f).name}`' for f in files) or '—'} | "
                   f"{lines_cell} | {f'{nfh} of {nf}' if nf else '—'} | {'; '.join(gaps) or 'none'} | "
                   f"{p.get('regtested', '').split(':')[0] or '?'} |")
    out += [
        "",
        "What these numbers are not:",
        "",
        "- A line that ran is not a line that was checked: a test can run a line and never compare its result.",
        "- They are the L1 level only (CTest, Serial and OpenMP). The CUDA build and the L2 replay inside WW3",
        "  are not instrumented, so a path only those take, such as level-1 team scratch, counts only if CTest",
        "  takes it too.",
        "- A shim file shared by two routines is counted in full for each, so the rows do not add up.",
        "",
    ]
    return "\n".join(out)


def ranking_text():
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, ports = meta_and_rows(PORTS)
    meta_c, cov = meta_and_rows(COVERAGE) if COVERAGE.exists() else ({}, [])
    return ranking(meta_p, prof, meta_w, ports, meta_c, cov)


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
    rank = ranking_text()
    old_rank = RANKING_MD.read_text(encoding="utf-8") if RANKING_MD.exists() else ""
    if new == text and tsv == old_tsv and rank == old_rank:
        print("port_score: PORT_STATUS.md, PORT_RANKING.md and site/data/port_score.tsv are current")
        return 0
    if check:
        print("port_score: the port score is stale (PORT_STATUS.md, PORT_RANKING.md or site/data/port_score.tsv): run scripts/port_score.py --write", file=sys.stderr)
        return 1
    STATUS_MD.write_text(new, encoding="utf-8")
    SITE_TSV.write_text(tsv, encoding="utf-8")
    RANKING_MD.write_text(rank, encoding="utf-8")
    print("port_score: wrote PORT_STATUS.md, PORT_RANKING.md and site/data/port_score.tsv")
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
    ok(ww3_file("w3src4md:w3sds4") == "model/src/w3src4md.F90" and ww3_file("init") == "—",
       "a module's routine maps to its file; a bare name has none")
    rk = ranking({"case": "t"}, rows, {"ww3_rev": "abc"},
                 [dict(ports[0], kokkos="kokkos/src/a.cpp, kokkos/src/s.cpp", regtested="yes: L2")],
                 {"build": "b", "ctest": "ok"},
                 [{"file": "kokkos/src/a.cpp", "lines": "10", "lines_run": "9", "functions": "2", "functions_run": "2", "lines_not_run": "7"},
                  {"file": "kokkos/src/s.cpp", "lines": "10", "lines_run": "5", "functions": "1", "functions_run": "1", "lines_not_run": "1-5"}])
    ok("| 1 | `w3src4md:w3sds4` | `model/src/w3src4md.F90` | 2.01 | 67.45 | 81.5 |" in rk,
       "the ranking skips the ported routine and its cumulative share starts from the port's")
    ok("this table has 3 rows, not 100" in rk, "the ranking says how many rows the profile allows")
    ok("| 14 of 20 (70.00 %) | 3 of 3 | `a.cpp` 7; `s.cpp` 1-5 | yes |" in rk, "a port's coverage sums its files")
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
