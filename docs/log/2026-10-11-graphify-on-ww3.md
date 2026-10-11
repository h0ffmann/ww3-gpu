# Graphify on WW3 at the pin: raw and preprocessed, code only

2026-10-11, an agent for Hoffmann, [#68](https://github.com/h0ffmann/ww3-gpu/issues/68) and
[ADR-0007](../ADRs/ADR-0007-graphify-ww3-locator.md).

## Question

Before writing ADR-0007: does graphify's keyless code pass give a usable graph of WW3's Fortran,
and can it answer #68's questions (a) "every caller of `W3SDS4`" and (b) "the path from `W3WAVE` to
`W3SNL1`"?

## What was done

Machine: a cloud container, Intel Xeon at 2.80 GHz, 4 cores, x86_64. graphify 0.9.85 from PyPI
(`pip install graphifyy`), gfortran from the container. WW3 is the `h0ffmann/WW3` fork at the
submodule pin `03adda91a1421898ac1ac4433f909e8519e13800`, cloned outside the repository. All
paths below are relative to a scratch directory `$S`.

```bash
git clone --filter=blob:none --no-checkout https://github.com/h0ffmann/WW3.git ww3
git -C ww3 checkout 03adda91a1421898ac1ac4433f909e8519e13800
G="env -i PATH=/usr/local/bin:/usr/bin:/bin HOME=$S/home GRAPHIFY_NO_AUTO_REFRESH=1 OLLAMA_BASE_URL=http://127.0.0.1:9"

# 1. raw tree
cd ww3/model/src && $G GRAPHIFY_OUT=$S/out graphify extract . --code-only --no-label

# 2. preprocessed for the lab switch
D=""; for t in $(cat switches/switch_lab_shrd); do D="$D -DW3_$t"; done   # run from ww3-gpu
for f in *.F90; do gfortran -E -cpp -P $D -I. "$f" -o "$S/pp/${f%.F90}.f90"; done
cd $S/pp && $G GRAPHIFY_OUT=$S/out2 graphify extract . --code-only --no-label

# questions, on each graph
$G graphify query "who calls W3SDS4" --graph $S/out/graph.json --budget 400
$G graphify path W3SRCE W3SDS4 --graph $S/out/graph.json
$G graphify path W3WAVE W3SNL1 --graph $S/out2/graph.json
grep -in "call *w3sds4" ww3/model/src/*.F90
```

Edge counts were read from `graph.json` with a few lines of Python: `calls` edges whose two nodes
have different `source_file`, and the `imports` targets of the nodes in `w3srcemd`.

## Result

- `(v)` Raw tree: 165 code files parsed (4 build files skipped, `w3macros.h` unparsed,
  `mod_xnl4v5.f90` read as cp1252), 2,427 nodes, 4,389 edges, all `EXTRACTED`, `graph.json`
  2,047,120 bytes, 4.9 s wall ⚠ one run.
- `(v)` Preprocessed tree: 0 of 136 files failed `gfortran -E`; 2,223 nodes, 4,057 edges, 2.9 s
  wall for graphify ⚠ one run.
- `(v)` `calls` edges across files: 0 of 752 (raw), 0 of 656 (preprocessed). Calls are resolved
  inside one file only.
- `(v)` `W3SRCE`'s module imports: raw `constants`, `w3dispmd`, `w3gdatmd`, `w3idatmd`;
  preprocessed adds `w3sbt1md`, `w3sdb1md`, `w3sln1md`, `w3snl1md`, `w3src4md`, the modules the lab
  switch selects. `w3srcemd.F90` has 239 `#ifdef` lines, and every `USE` behind one is lost on the
  raw tree.
- `(v)` Question (a): the graph returns only the definition of `w3sds4()` (`w3src4md.F90` L1831 raw,
  `w3src4md.f90` L1936 preprocessed). `grep` returns 7 call sites in 4 files, among them
  `w3srcemd.F90:1303`, `ww3_outp.F90:2283`, `ww3_ounp.F90:2188` and `gx_outp.F90:1206`.
- `(v)` Question (b) and `W3SRCE` → `W3SDS4`: "No directed path found" on both graphs.
- `(v)` No model call: every graphify command ran with an empty environment and the Ollama URL
  pointed at a closed port, and finished.

## What it changes

ADR-0007 adopts the graph only for module dependencies and symbol lookup on the preprocessed tree,
and leaves callers and call paths to `grep`. Question (c) of #68 and the token and time comparison
are step 1 of that record, not done here.
