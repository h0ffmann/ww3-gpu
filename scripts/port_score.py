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

Three levels, each a set of rows of kokkos/ports.tsv:
  translated  every row: a Kokkos translation of the routine exists in kokkos/src
  ported      `wired` is yes: WW3 at the WW3/ pin calls the Kokkos code behind its run-time flag
  regtested   `regtested` is yes: WW3's regtests ran with it on Kokkos and agreed with the Fortran
and for each level two shares: of the profile's self time (what a run moves off the Fortran) and of
every Fortran line in WW3's model/src. Beside them: the routines (over every SUBROUTINE and
FUNCTION in model/src) and the files (over every .F90 file) of each level
and, beside them, the size of the port itself: the C++ files and lines under kokkos/src.
--write also writes site/data/port_score.tsv, which the website reads (site/content), and
kokkos/PORT_RANKING.md: the routines still in Fortran by self time, up to RANK_TOP, and for each
ported routine its Kokkos files and its regtest record (the port has no tests of its own, ADR-0010),
and kokkos/PORT_TABLE.tsv: one row per routine that is translated, claimed (kokkos/port_claims.tsv)
or timed by any committed profile (docs/data/ww3_*_gprof.tsv), the next port first.
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
CLAIMS = ROOT / "kokkos" / "port_claims.tsv"
TABLE_TSV = ROOT / "kokkos" / "PORT_TABLE.tsv"
KOKKOS_SRC = ROOT / "kokkos" / "src"
START, END = "<!-- port-score:start -->", "<!-- port-score:end -->"
STEPS = ["ported", "L2", "gpu"]
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


LEVELS = ("translated", "ported", "regtested")


def is_yes(p, key):
    return p.get(key, "").startswith("yes:")


def at_level(ports, level):
    """The rows of one level: every row, the wired ones, the regtested ones."""
    if level == "ported":
        return [p for p in ports if is_yes(p, "wired")]
    if level == "regtested":
        return [p for p in ports if is_yes(p, "regtested")]
    return list(ports)


def summary(meta_p, prof, meta_w, ports, kokkos=(0, 0)):
    """The global numbers, one dict, for the PORT_STATUS block and the website."""
    total = sum(float(r["self_s"]) for r in prof) or 1.0
    w_lines, w_rout, w_files = int(meta_w["ww3_lines"]), int(meta_w["ww3_routines"]), int(meta_w.get("ww3_files", 0) or 0)
    out = {
        "ww3_routines": w_rout, "ww3_files": w_files, "ww3_lines": w_lines,
        "kokkos_files": kokkos[0], "kokkos_lines": kokkos[1], "ww3_rev": meta_w["ww3_rev"],
        "case": meta_p.get("case", "?"),
    }
    for lv in LEVELS:
        rows = at_level(ports, lv)
        names = {p["routine"] for p in rows}
        out[lv] = {
            "time_pct": 100 * sum(float(r["self_s"]) for r in prof if r["routine"] in names) / total,
            "routines": len(rows), "lines": sum(int(p["lines"]) for p in rows),
            "files": len({p["file"].split(":")[0] for p in rows if p.get("file")}),
        }
    return out


def pct(a, b):
    return f"{100 * a / b:.2f} %" if b else "?"


def grouped(n):
    return f"{n:,}".replace(",", " ")


WHAT = {
    "translated": "a Kokkos translation exists in kokkos/src",
    "ported": "WW3 at the WW3/ pin calls the Kokkos code, behind its run-time flag",
    "regtested": "WW3's regtests ran with it on Kokkos and agreed with the Fortran",
}


def site_rows(s):
    """The rows of site/data/port_score.tsv: name, share (the big number), count, what it counts.
    For each level, its share of the run time, then of WW3's lines of code."""
    rows = []
    for lv in LEVELS:
        v = s[lv]
        rows.append((f"of the run time, {lv}", f"{v['time_pct']:.1f} %",
                     f"{v['routines']} of {grouped(s['ww3_routines'])} routines",
                     f"{WHAT[lv]}; the profile's self time, {s['case']}"))
    for lv in LEVELS:
        v = s[lv]
        rows.append((f"of the lines of code, {lv}", pct(v['lines'], s['ww3_lines']),
                     f"{grouped(v['lines'])} of {grouped(s['ww3_lines'])}",
                     f"Fortran lines of the {lv} routines, of every line in WW3's model/src, in "
                     f"{v['files']} of {s['ww3_files']} files"))
    rows.append(("lines of Kokkos C++", grouped(s['kokkos_lines']), f"in {s['kokkos_files']} files",
                 "under kokkos/src: the port itself"))
    return rows


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
        f"| level | run time | lines | routines | files | what it counts (WW3@{meta_w['ww3_rev']}) |",
        "|---|---|---|---|---|---|",
    ]
    for lv in LEVELS:
        v = s[lv]
        lines.append(f"| {lv} | **{v['time_pct']:.1f} %** | {v['lines']} of {s['ww3_lines']} ({pct(v['lines'], s['ww3_lines'])}) | "
                     f"{v['routines']} of {s['ww3_routines']} ({pct(v['routines'], s['ww3_routines'])}) | "
                     f"{v['files']} of {s['ww3_files']} | {WHAT[lv]} |")
    lines += [
        "",
        f"The port itself: {s['kokkos_lines']} lines in {s['kokkos_files']} files, the C++ under kokkos/src (every .cpp and .hpp line).",
        "",
        "On a GPU: " + (", ".join(f"`{p['routine']}`" for p in ports if p["status"] == "gpu") or
                        "none yet. The CUDA build links; the RTX 4090 run, `just smoke-ww3-gpu gpu`, is the owner's, supervised."),
    ]
    return "\n".join(lines)


# Modules whose file is not named after them (`grep -il "^ *module <name>" WW3/model/src/*.F90`).
MODULE_FILE = {"pdlib_w3profsmd": "w3profsmd_pdlib"}


def ww3_file(routine):
    """w3src4md:w3sds4 -> model/src/w3src4md.F90; a name without a module has no file here."""
    mod = routine.split(":")[0] if ":" in routine else ""
    return f"model/src/{MODULE_FILE.get(mod, mod)}.F90" if mod else "—"


def ranking(meta_p, prof, meta_w, ports):
    by_name = {p["routine"]: p for p in ports}
    total = sum(float(r["self_s"]) for r in prof) or 1.0
    todo = [r for r in prof if r["routine"] not in by_name][:RANK_TOP]
    timed = sum(1 for r in todo if float(r["self_s"]) > 0)
    out = [
        "# PORT_RANKING",
        "",
        "The WW3 routines still in Fortran, the most expensive first, and how much of each ported",
        "routine's Kokkos code was regtested. Written by `scripts/port_score.py --write` from",
        "`docs/data/ww3_full_gprof.tsv` and `kokkos/ports.tsv`; do not",
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
        "## Regtested, per port",
        "",
        "The port has no tests of its own ([ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)):",
        "a ported routine is tested only by WW3's regtests, run inside WW3 with the routine on Kokkos and",
        "compared pair to pair with the same WW3 without it (`scripts/ww3-gpu.sh --ww3-gpu --cpu`, `just ww3-gpu-matrix`).",
        "The record is the `regtested` column of `kokkos/ports.tsv`.",
        "",
        "| routine | status | Kokkos files | regtested |",
        "|---|---|---|---|",
    ]
    for p in ports:
        files = [f.strip() for f in p.get("kokkos", "").split(",") if f.strip()]
        out.append(f"| `{p['routine']}` | {p['status']} | {', '.join(f'`{Path(f).name}`' for f in files) or '—'} | "
                   f"{p.get('regtested', '').replace('|', '/') or '?'} |")
    out.append("")
    return "\n".join(out)


TABLE_COLS = ["priority", "routine", "ww3_file", "self_pct", "profile", "state", "claim", "translated",
              "wired", "checkpoint", "regtested", "test_comp", "nccmp_tol", "googletest", "report"]


def profiles(data_dir=None):
    """Every committed gprof profile: {case: [rows]}, from docs/data/ww3_<case>_gprof.tsv."""
    data_dir = data_dir or PROFILE.parent
    return {p.name[len("ww3_"):-len("_gprof.tsv")]: meta_and_rows(p)[1]
            for p in sorted(data_dir.glob("ww3_*_gprof.tsv"))}


def tsv_cell(text):
    return " ".join(text.replace("\t", " ").split())


def port_table(profs, ports, claims, reports_dir=None):
    """kokkos/PORT_TABLE.tsv: the untranslated routines by their largest self share in any profile
    (claimed ones without a profile row after them), then every translated routine. One line per
    routine; the regtest columns are read from the `regtested` record of kokkos/ports.tsv."""
    reports_dir = reports_dir or REPORTS
    by_port = {p["routine"]: p for p in ports}
    by_claim = {c["routine"]: c for c in claims}
    best = {}
    for case, rows in profs.items():
        for r in rows:
            pct = float(r["self_pct"])
            if r["routine"] not in best or pct > best[r["routine"]][0]:
                best[r["routine"]] = (pct, case)
    names = set(best) | set(by_port) | set(by_claim)
    todo = sorted((n for n in names if n not in by_port), key=lambda n: (-best.get(n, (-1.0, ""))[0], n))
    done = sorted(by_port, key=lambda n: (-best.get(n, (-1.0, ""))[0], n))
    out = [
        "# Every WW3 routine the port has translated, claimed or seen in a profile, the next port first.",
        "# Written by scripts/port_score.py --write from kokkos/ports.tsv, kokkos/port_claims.tsv and",
        "# docs/data/ww3_*_gprof.tsv; do not edit. priority: the order to port in (ADR-0002, wall time),",
        "# for routines not yet translated. self_pct: the largest self share in any committed profile,",
        "# with that profile's case; '—' when no profile times it. checkpoint: the ADR-0011 regtest",
        "# checkpoint of a translated routine, passed when regtested is yes. googletest: the port has no",
        "# tests of its own (ADR-0010). test_comp and nccmp_tol quote the regtest record.",
        "\t".join(TABLE_COLS),
    ]
    for i, n in enumerate(todo + done, 1):
        p, c = by_port.get(n), by_claim.get(n, {})
        pct, case = best.get(n, (None, "—"))
        reg = p.get("regtested", "") if p else ""
        rec = re.sub(r"^(yes|no): ", "", reg)
        if p:
            state = "regtested" if is_yes(p, "regtested") else "wired" if is_yes(p, "wired") else "translated"
        else:
            state = "claimed" if c else "todo"
        test_comp = "; ".join(re.findall(r"byte-identical on all \d+(?: \(test\.comp\))?", rec)) if p else ""
        nccmp = "; ".join(re.findall(r"nccmp-tol \d+ PASS[^;(]*?(?=, after|, with|\(|;|$)", rec)) if p else ""
        row = {
            "priority": str(i) if not p else "—",
            "routine": n,
            "ww3_file": (p or c).get("file", "") if (p or c) else ww3_file(n),
            "self_pct": f"{pct:.2f}" if pct is not None else "—",
            "profile": case,
            "state": state,
            "claim": c.get("claim", "—") if not p else "—",
            "translated": "yes: " + p.get("kokkos", "") if p else "no",
            "wired": p.get("wired", "") if p else "no",
            "checkpoint": ("passed" if is_yes(p, "regtested") else "pending") if p else "—",
            "regtested": ("yes" if is_yes(p, "regtested") else "no") if p else "no",
            "test_comp": test_comp or ("—" if not p or not is_yes(p, "regtested") else "not recorded"),
            "nccmp_tol": nccmp.strip(" ,") or "—",
            "googletest": "none: ADR-0010",
            "report": f"kokkos/port_report/{report_name(n)}" if p and (reports_dir / report_name(n)).exists() else "—",
        }
        out.append("\t".join(tsv_cell(row[k]) for k in TABLE_COLS))
    return "\n".join(out) + "\n"


def table_text():
    claims = meta_and_rows(CLAIMS)[1] if CLAIMS.exists() else []
    return port_table(profiles(), meta_and_rows(PORTS)[1], claims)


def ranking_text():
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, ports = meta_and_rows(PORTS)
    return ranking(meta_p, prof, meta_w, ports)


def block():
    meta_p, prof = meta_and_rows(PROFILE)
    meta_w, ports = meta_and_rows(PORTS)
    return render(meta_p, prof, meta_w, ports)


def report_name(routine):
    """w3snl1md:w3snl1 -> W3SNL1_KOKKOS.md, the report every ported routine has."""
    return routine.split(":")[-1].upper() + "_KOKKOS.md"


def unrecorded(ports):
    """Rows that do not say, yes or no, whether the Kokkos code was regtested inside WW3, or whether
    WW3 at the pin calls it."""
    return [f"{p['routine']} ({k})" for p in ports for k in ("regtested", "wired")
            if not re.match(r"^(yes|no): \S", p.get(k, ""))]


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
        print(f"port_score: kokkos/ports.tsv must say 'yes: …' or 'no: …' in {', '.join(bad)}", file=sys.stderr)
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
    table = table_text()
    old_table = TABLE_TSV.read_text(encoding="utf-8") if TABLE_TSV.exists() else ""
    if new == text and tsv == old_tsv and rank == old_rank and table == old_table:
        print("port_score: PORT_STATUS.md, PORT_RANKING.md, PORT_TABLE.tsv and site/data/port_score.tsv are current")
        return 0
    if check:
        print("port_score: the port score is stale (PORT_STATUS.md, PORT_RANKING.md, PORT_TABLE.tsv or site/data/port_score.tsv): run scripts/port_score.py --write", file=sys.stderr)
        return 1
    STATUS_MD.write_text(new, encoding="utf-8")
    SITE_TSV.write_text(tsv, encoding="utf-8")
    RANKING_MD.write_text(rank, encoding="utf-8")
    TABLE_TSV.write_text(table, encoding="utf-8")
    print("port_score: wrote PORT_STATUS.md, PORT_RANKING.md, PORT_TABLE.tsv and site/data/port_score.tsv")
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
    ports = [{"routine": "w3snl1md:w3snl1", "lines": "359", "status": "L2", "regtested": "yes: L2", "wired": "yes: pin"},
             {"routine": "w3snl1md:insnl1", "lines": "304", "status": "ported", "regtested": "no: x", "wired": "yes: pin"},
             {"routine": "w3src4md:w3sds4", "lines": "600", "status": "ported", "regtested": "no: x", "wired": "no: branch"}]
    out = render({"case": "t"}, rows, {"ww3_rev": "abc", "ww3_lines": "10000", "ww3_routines": "100"}, ports)
    ok("| translated | **81.5 %** |" in out, "translated counts every row's self time (2.01 + 0.42 of 2.98 s)")
    ok("| ported | **14.1 %** |" in out, "ported counts the wired rows only")
    ok("| regtested | **14.1 %** | 359 of 10000 (3.59 %) |" in out, "regtested counts the regtested rows only")
    ok(status_rank("L1") == -1, "L1 is no longer a step (ADR-0010)")
    ok("| 2 | `w3snl1md:w3snl1` | 0.42 | 14.1 | 81.5 | L2 |" in out, "a ported routine shows its status in the ranking")
    ok("| 4 | `w3srce` | 0.25 | 8.4 | 100.0 | Fortran |" in out, "an unported routine is Fortran")
    ok("3 of 100 (3.00 %)" in out and "1263 of 10000 (12.63 %)" in out, "translated counts every row's routines and lines")
    ports_f = [dict(p, file="model/src/w3snl1md.F90:1-2") for p in ports[:2]] + [dict(ports[2], file="model/src/w3src4md.F90:1-2")]
    s = summary({"case": "t"}, rows, {"ww3_rev": "abc", "ww3_lines": "10000", "ww3_routines": "100", "ww3_files": "4"}, ports_f, (3, 120))
    ok(s["translated"]["files"] == 2 and s["ported"]["files"] == 1, "two routines of one file are one file")
    t = site_tsv(s)
    ok("of the run time, ported\t14.1 %\t2 of 100 routines\t" in t, "the site's ported row: its share and routines")
    ok("of the lines of code, translated\t12.63 %\t1 263 of 10 000\t" in t and "in 2 of 4 files" in t,
       "the site's lines row for a level, with its files")
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
    ok(unrecorded([{"routine": "a", "regtested": "yes: L2", "wired": "yes: pin"},
                   {"routine": "b", "regtested": "no: not built", "wired": "no: branch"}]) == [],
       "yes and no, each with a reason, are records")
    ok(unrecorded([{"routine": "a", "regtested": "", "wired": "yes: pin"}, {"routine": "b", "regtested": "maybe", "wired": ""}])
       == ["a (regtested)", "b (regtested)", "b (wired)"], "an empty or unclear record is refused")
    ok(report_name("w3snl1md:w3snl1") == "W3SNL1_KOKKOS.md", "a report is named after the routine, upper case")
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "W3SNL1_KOKKOS.md").write_text("x", encoding="utf-8")
        ok(missing_reports(ports[:2], Path(tmp)) == ["INSNL1_KOKKOS.md"], "a ported routine without a report is named")
    ok(ww3_file("w3src4md:w3sds4") == "model/src/w3src4md.F90" and ww3_file("init") == "—"
       and ww3_file("pdlib_w3profsmd:x") == "model/src/w3profsmd_pdlib.F90",
       "a module's routine maps to its file; a bare name has none")
    rk = ranking({"case": "t"}, rows, {"ww3_rev": "abc"},
                 [dict(ports[0], kokkos="kokkos/src/a.cpp, kokkos/src/s.cpp", regtested="yes: L2, ww3_ts1")])
    ok("| 1 | `w3src4md:w3sds4` | `model/src/w3src4md.F90` | 2.01 | 67.45 | 81.5 |" in rk,
       "the ranking skips the ported routine and its cumulative share starts from the port's")
    ok("this table has 3 rows, not 100" in rk, "the ranking says how many rows the profile allows")
    ok("| `w3snl1md:w3snl1` | L2 | `a.cpp`, `s.cpp` | yes: L2, ww3_ts1 |" in rk, "a port's row names its files and its regtest record")
    tports = [dict(ports[0], file="model/src/w3snl1md.F90:1-2", kokkos="kokkos/src/a.cpp",
                   regtested="yes: matrix of 100 entries, byte-identical on all 100 (test.comp), nccmp-tol 71 PASS, 29 n/a, 0 FAIL, after AF11"),
              dict(ports[2], file="model/src/w3src4md.F90:1-2", kokkos="kokkos/src/b.hpp")]
    claims = [{"routine": "w3srce", "file": "model/src/w3srcemd.F90:1-9", "claim": "goal/1-x"},
              {"routine": "w3xxmd:w3new", "file": "model/src/w3xxmd.F90:1-2", "claim": "Kimi Code"}]
    with tempfile.TemporaryDirectory() as tmp:
        tb = port_table({"t": rows}, tports, claims, Path(tmp)).splitlines()
    body = [ln.split("\t") for ln in tb if not ln.startswith("#")]
    ok(body[0] == TABLE_COLS, "the table's header is its columns")
    ok([r[1] for r in body[1:]] == ["w3src4md:w3sin4", "w3srce", "w3xxmd:w3new", "w3src4md:w3sds4", "w3snl1md:w3snl1"],
       "untranslated by self share, a claim without a profile row after them, then the translated")
    ok(body[1][0] == "1" and body[4][0] == "—", "only untranslated routines get a priority")
    ok(body[2][5] == "claimed" and body[2][6] == "goal/1-x" and body[1][5] == "todo", "a claim shows in state and claim")
    ok(body[3][3] == "—" and body[3][4] == "—", "a routine no profile times has no share")
    ok(body[5][5] == "regtested" and body[5][9] == "passed" and body[5][11] == "byte-identical on all 100 (test.comp)"
       and body[5][12] == "nccmp-tol 71 PASS, 29 n/a, 0 FAIL", "a regtested row quotes its test.comp and nccmp-tol record")
    ok(body[4][5] == "translated" and body[4][9] == "pending" and body[4][13] == "none: ADR-0010",
       "a translated, unregtested row has its checkpoint pending")
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
