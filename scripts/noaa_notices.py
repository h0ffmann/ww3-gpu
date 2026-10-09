#!/usr/bin/env python3
"""noaa_notices — NOAA announcements since the last recorded snapshot that touch this repository.

    scripts/noaa_notices.py            # fetch the listings, print what is new and relevant
    scripts/noaa_notices.py --write    # ... and record what was seen in docs/noaa-notices.json
    scripts/noaa_notices.py --all      # also list the new notices the keyword filter dropped

Sources (issue #85): the NWS notification page (PNS and SCN PDFs), NCEP's model-change table
(which links the SCN/TIN behind each implementation) and NOAA-EMC/WW3's tags and `production/*`
branches through `git ls-remote`. A notice is its PDF file name: an update gets a new suffix
(`_aaa`, `_aab`) and so reads as new, which is wanted. The listings are scraped for PDF links only, so a page
redesign that keeps the links keeps working; a page that yields no link at all is an error,
never "no change". Standard library only.
"""
import argparse
import datetime as dt
import html
import json
import pathlib
import re
import subprocess
import sys
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
STATE = ROOT / "docs" / "noaa-notices.json"
PAGES = ["https://www.weather.gov/notification/", "https://www.nco.ncep.noaa.gov/pmb/changes/"]
WW3 = "https://github.com/NOAA-EMC/WW3"
LINK = re.compile(r'<a\b[^>]*href="([^"]*/notification/[^"]*?((?:pns|scn|tin)[^"/]*)\.pdf)"[^>]*>(.*?)</a>',
                  re.I | re.S)
# What this repository consumes: GFS winds from NOMADS (example 02), GFS-Wave as the reference
# global run (lesson 04), WW3 itself, and the NCEP systems that run WW3 inside them.
RELEVANT = re.compile(r"gfs|gefs|gdas|wave|wavewatch|ww3|nwps|glwu|rtofs|hafs|nomads|grib|"
                      r"marine|sea.?state|parallel|ciphers?", re.I)
YEAR = re.compile(r"(?:pns|scn|tin)(\d\d)", re.I)


def fetch(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "ww3-gpu noaa_notices.py"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.read().decode("utf-8", "replace")
    except OSError as e:
        sys.exit(f"{url}: {e} (a cloud egress proxy may refuse NOAA hosts; see the /noaa-notices skill)")


def notices(page: str, base: str) -> dict:
    out = {}
    for href, name, text in LINK.findall(page):
        text = " ".join(html.unescape(re.sub(r"<[^>]+>", " ", text)).split())
        out[name] = {"title": text, "url": urllib.parse.urljoin(base, href)}
    return out


def ww3_refs() -> dict:
    """Tags, and the `production/*` branches NCEP cuts WW3 for each operational system (GFS.v17, ...)."""
    r = subprocess.run(["git", "ls-remote", "--tags", "--heads", WW3], capture_output=True, text=True, timeout=120)
    if r.returncode:
        sys.exit(f"git ls-remote {WW3}: {r.stderr.strip()}")
    refs = {"tags": set(), "production": {}}
    for sha, ref in (ln.split("\t") for ln in r.stdout.splitlines()):
        if ref.startswith("refs/tags/"):
            refs["tags"].add(ref[len("refs/tags/"):].removesuffix("^{}"))
        elif ref.startswith(("refs/heads/production/", "refs/heads/prod/")):
            refs["production"][ref[len("refs/heads/"):]] = sha[:7]
    return {"tags": sorted(refs["tags"]), "production": dict(sorted(refs["production"].items()))}


def recent(name: str, today: dt.date) -> bool:
    """The change table goes back to 2006; a notice older than last year is history, not news."""
    m = YEAR.match(name)
    return not m or 2000 + int(m.group(1)) >= today.year - 1


def relevant(name: str, n: dict) -> bool:
    return bool(RELEVANT.search(name.replace("_", " ") + " " + n["title"]))


def report(old: dict, found: dict, ww3: dict, show_all: bool, today: dt.date) -> str:
    seen = set(old.get("notices", []))
    new = {k: v for k, v in sorted(found.items()) if k not in seen and recent(k, today)}
    keep = {k: v for k, v in new.items() if relevant(k, v)}
    out = [f"# NOAA notices since {old.get('checked') or '(no snapshot)'}  (checked {today})", ""]
    if keep:
        out.append(f"## {len(keep)} relevant new notice(s)")
        out += [f"- `{k}` {v['title'] or '(no link text)'}  {v['url']}" for k, v in keep.items()]
        out.append("")
    dropped = len(new) - len(keep)
    if dropped:
        out.append(f"{dropped} other new notice(s) did not match the keyword filter" +
                   (":" if show_all else " (`--all` lists them)."))
        if show_all:
            out += [f"- `{k}` {v['title']}  {v['url']}" for k, v in new.items() if k not in keep]
        out.append("")
    o = old.get("ww3", {})
    tags = sorted(set(ww3["tags"]) - set(o.get("tags", [])))
    moved = {b: h for b, h in ww3["production"].items() if o.get("production", {}).get(b) != h}
    if tags:
        out += [f"## NOAA-EMC/WW3: new tags {', '.join(f'`{t}`' for t in tags)}", ""]
    if moved:
        out.append("## NOAA-EMC/WW3: production branches new or moved")
        out += [f"- `{b}` {o.get('production', {}).get(b, '(new)')} -> `{h}`  {WW3}/commits/{b}"
                for b, h in moved.items()]
        out.append("")
    if not keep and not tags and not moved:
        out.append("Nothing relevant since the snapshot.")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--write", action="store_true", help=f"record what was seen in {STATE.relative_to(ROOT)}")
    ap.add_argument("--all", action="store_true", help="also list new notices the keyword filter dropped")
    a = ap.parse_args()
    found = {}
    for url in PAGES:
        got = notices(fetch(url), url)
        if not got:
            sys.exit(f"{url}: no notice PDF links found; the page layout changed, fix LINK in {__file__}")
        found.update(got)
    ww3 = ww3_refs()
    old = json.loads(STATE.read_text()) if STATE.exists() else {}
    today = dt.date.today()
    print(report(old, found, ww3, a.all, today))
    if a.write:
        state = {"checked": today.isoformat(),
                 "notices": sorted(set(old.get("notices", [])) | set(found)), "ww3": ww3}
        STATE.write_text(json.dumps(state, indent=2) + "\n")
        print(f"\nrecorded {STATE.relative_to(ROOT)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
