#!/usr/bin/env python3
"""ww4_parts: can WW4 run this case yet? Every part the case needs, and whether WW4 has it.

    scripts/ww4_parts.py smoke|full [--ww4 DIR] [--switch FILE]
    scripts/ww4_parts.py --self-test

`scripts/hoff.sh --ww4` runs this first. Exit 0: every part is ported. Exit 5: a part is not
ported yet, and the table says which. The parts come from the WW3 switch the case runs with
(switches/switch_lab_shrd, as in --ww3 and --hoff):
  - each source-term switch that is on (LN1, ST4, NL1, BT1, DB1, ...; a 0 means off) is WW4's
    src/ww4_core/source_terms/ww4_<SWITCH>/ww4_<SWITCH>.cpp;
  - the propagation scheme (UQ, SMC) is src/ww4_core/solver_<uq|smc>/solver_<uq|smc>.cpp, needed
    by `full` only: `smoke` is one point (regtest ww3_ts1), where nothing propagates;
  - the time loop is src/ww4_core/w4core_wave.cpp.
A file counts as ported when it has more than STUB_LOC lines of code, the threshold of
scripts/ww4_status.py: the placeholders of NOAA-EMC/WW4 PR #72 have 7 (source terms) and 29
(solvers). It is a heuristic; the table prints the counts so a reader can judge.
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ww4_status import STUB_LOC  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
NOT_PORTED = 5
SOURCE_TERMS = ("LN", "ST", "NL", "BT", "DB", "TR", "BS", "IC", "IS", "REF")
WW3_NAMES = {"LN1": "W3SLN1", "ST2": "W3SIN2, W3SDS2", "ST4": "W3SIN4, W3SDS4", "ST6": "W3SIN6, W3SDS6",
             "NL1": "W3SNL1", "NL2": "W3SNL2", "NL3": "W3SNL3", "BT1": "W3SBT1", "BT4": "W3SBT4",
             "DB1": "W3SDB1", "UQ": "W3XYP3, W3KTP3 (W3UQCKMD)", "SMC": "W3PSMC"}


def code_loc(path):
    """Non-blank lines that are not comments, as scripts/ww4_status.py counts them."""
    n, block = 0, False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
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


def parts(case, switch):
    """[(part, WW3 routines, WW4 file)] that the case needs, from the switch's tokens."""
    tokens = switch.split()
    out = [("time loop", "W3WAVE", "src/ww4_core/w4core_wave.cpp")]
    for t in tokens:
        m = re.fullmatch(r"([A-Z]+)(\d+)", t)
        if m and m.group(1) in SOURCE_TERMS and m.group(2) != "0":
            out.append((f"source term {t}", WW3_NAMES.get(t, "?"), f"src/ww4_core/source_terms/ww4_{t}/ww4_{t}.cpp"))
    if case == "full":
        for scheme in ("UQ", "SMC"):
            if scheme in tokens:
                low = scheme.lower()
                out.append((f"propagation {scheme}", WW3_NAMES[scheme], f"src/ww4_core/solver_{low}/solver_{low}.cpp"))
    return out


def assess(case, ww4, switch):
    rows = []
    for name, ww3, rel in parts(case, switch):
        f = ww4 / rel
        loc = code_loc(f) if f.exists() else 0
        state = "ported" if loc > STUB_LOC else ("stub" if f.exists() else "missing")
        rows.append((name, ww3, rel, loc, state))
    return rows


def render(case, rows, rev):
    out = [f"WW4 parts for --ww4 {case} (WW4@{rev}; ported = more than {STUB_LOC} lines of code):", "",
           f"  {'part':<20} {'WW3':<28} {'loc':>5}  {'state':<8} WW4 file"]
    for name, ww3, rel, loc, state in rows:
        out.append(f"  {name:<20} {ww3:<28} {loc:>5}  {state:<8} {rel}")
    return "\n".join(out)


def self_test():
    fails = 0

    def ok(cond, label):
        nonlocal fails
        print(f"  {'ok  ' if cond else 'FAIL'} {label}")
        fails += 0 if cond else 1

    sw = "F90 SHRD PR3 UQ FLX0 LN1 ST4 STAB0 NL1 BT1 DB1 TR0 BS0 IC0 IS0 REF0 O0"
    names = [p[0] for p in parts("smoke", sw)]
    ok(names == ["time loop", "source term LN1", "source term ST4", "source term NL1", "source term BT1",
                 "source term DB1"], "smoke needs the time loop and the switches that are on")
    ok("propagation UQ" in [p[0] for p in parts("full", sw)], "full needs the propagation scheme too")
    ok("source term TR0" not in names and "source term STAB0" not in names, "a 0 switch and STAB are not parts")
    with tempfile.TemporaryDirectory() as tmp:
        w = Path(tmp)
        (w / "src/ww4_core/source_terms/ww4_ST4").mkdir(parents=True)
        (w / "src/ww4_core/source_terms/ww4_ST4/ww4_ST4.cpp").write_text("/* c */\n" + "x;\n" * 7)
        (w / "src/ww4_core/w4core_wave.cpp").write_text("// c\n" + "x;\n" * 80)
        rows = {r[0]: r for r in assess("smoke", w, "ST4 NL1")}
        ok(rows["time loop"][4] == "ported" and rows["time loop"][3] == 80, "comments do not count; 80 lines is ported")
        ok(rows["source term ST4"][4] == "stub", "a 7-line placeholder is a stub, not ported")
        ok(rows["source term NL1"][4] == "missing", "no file is missing")
    print("ww4_parts self-test: " + ("ok" if not fails else f"{fails} failure(s)"))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("case", nargs="?", choices=["smoke", "full"])
    ap.add_argument("--ww4", default=str(ROOT / "WW4"))
    ap.add_argument("--switch", default=str(ROOT / "switches" / "switch_lab_shrd"))
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.case:
        ap.error("smoke or full")
    ww4 = Path(a.ww4)
    if not (ww4 / "src" / "ww4_core").is_dir():
        print(f"ww4_parts: {ww4} has no src/ww4_core: git submodule update --init WW4", file=sys.stderr)
        return 3
    head = ww4 / ".git"
    rev = "?"
    if head.exists():
        import subprocess
        rev = subprocess.run(["git", "-C", str(ww4), "rev-parse", "--short", "HEAD"],
                             capture_output=True, text=True).stdout.strip() or "?"
    rows = assess(a.case, ww4, Path(a.switch).read_text(encoding="utf-8"))
    print(render(a.case, rows, rev))
    missing = [r[0] for r in rows if r[4] != "ported"]
    if missing:
        print(f"\nww4_parts: not ported to WW4 yet: {', '.join(missing)}. --ww4 {a.case} cannot run (exit {NOT_PORTED}).",
              file=sys.stderr)
        return NOT_PORTED
    print(f"\nww4_parts: every part --ww4 {a.case} needs is ported")
    return 0


if __name__ == "__main__":
    sys.exit(main())
