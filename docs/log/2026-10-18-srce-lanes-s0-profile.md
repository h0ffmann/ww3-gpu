# One thread of the resident `W3SRCE` batch: 91 % in the port's own code, 9 % in libm

2026-10-18, for M. Hoffmann (goal `c3e7eecf-3917`), unit S0 of
[the points-in-lanes plan](../plans/2026-10-18-005-srce-points-in-lanes-plan.md), PR #197.

## Question

Before sea points go into vector lanes: how much of one thread's time in the batched `W3SRCE`
is scalar per-point code that lanes could vectorise, and how much is glibc's `libm` and
`libmvec`, which lanes would leave scalar? The committed profile
([2026-10-18-resident-profile](2026-10-18-resident-profile.md)) charged libm frames to their
callers and was taken before #193's DIA halo, so it cannot answer this.

## What was done

`ww3_shel` of the `--repeat 3` timing build (repository `5cd0140`, `WW3@50a10730`,
`switch_lab_ompg` plus `KOKKOS`, Kokkos 5.2.2 OpenMP, g++ 16.2.0, no `-g`) on the `full` case
(`kokkos/tests/fixtures/bench_small`, as `scripts/ww3-gpu.sh` stages it in
`build/runs/ww3-gpu-full-openmp/kokkos`), with `OMP_NUM_THREADS=1 OMP_PROC_BIND=false
WW_KOKKOS_ALL=0 WW_KOKKOS_PRESET=resident`, under gdb, stopped with `SIGUSR2` every 0.1 s of run
time; each sample is charged to the innermost frame of thread 1 when a frame of the sample names
`ww_srce_batch`. Intel Core i9-14900, 2026-10-18 20:16 to 20:19 UTC, load 9.0 to 10.1 from another
session's regtest jobs ⚠ (a share of one thread's samples, not a time).

## Result

- (v) The run's `out_grd.ww3` is byte-identical to the timing run's Kokkos output (`cmp`).
- (v) 1 602 samples, 1 439 inside `ww_srce_batch`:

| Innermost frame | Samples | Share of the batch |
|---|---|---|
| `ww::st4::w3sds4_point` | 357 | 24.8 % |
| `ww::snl1::snl1_team_halo` | 305 | 21.2 % |
| `ww::snl1::snl1_team` | 256 | 17.8 % |
| `ww::st4::w3sin4_point` | 186 | 12.9 % |
| `ww::srce::w3srce_team` | 142 | 9.9 % |
| `_ZGVbN4vv_powf_sse4` (`libmvec.so.1`) | 70 | 4.9 % |
| `ww::st4::w3spr4_point` | 50 | 3.5 % |
| `__powf_fma` (`libm.so.6`) | 28 | 1.9 % |
| `atan2f` (`libm.so.6`) | 17 | 1.2 % |
| `__memset_avx2_unaligned_erms` | 10 | 0.7 % |
| `__expf_fma` (`libm.so.6`) | 7 | 0.5 % |
| `tanhf`, `__logf_fma`, `log10f`, `powf@plt` | 5 | 0.3 % |

- (v) `libm` and `libmvec` together: 127 samples, 8.9 % of the batch.
- (v) The scalar per-point routines (`W3SDS4`, `W3SIN4`, `w3srce_team` with `W3SLN1`, `W3SDB1`,
  `W3SBT1` inlined into it, `W3SPR4`): 735 samples, 51.1 %.
- (v) The DIA (`snl1_team` and `snl1_team_halo`): 561 samples, 39.0 %. How its 17.8 % outside the
  halo routine splits between sections 1 and 2 and code the compiler inlined from the halo
  routine needs a `-g` build ⚠.

## What it changes

Lanes can reach about half of one thread's batch, and libm does not bound them: the plan goes on
to its units L1 (the lane layer) and L2 (`W3SIN4`). The DIA is the largest single routine but
already a vector loop over directions per point; it is the plan's last unit.

## How to repeat

`profile.sh`, run from any directory with the timing build in place (`bash scripts/ww3-gpu.sh
--ww3-gpu full --cpu --kokkos resident` first):

```bash
#!/usr/bin/env bash
# S0 of the points-in-lanes plan: where does one thread of the resident W3SRCE batch spend its
# time, libm or the rest? gdb stack sampling (perf is locked here), the method of
# docs/log/2026-10-18-resident-profile.md, on the full case, OMP_NUM_THREADS=1.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo=/home/hoffmann/code/ww3-gpu-2
exe="$repo/build/ww3-gpu-openmp/ww3/bin/ww3_shel"
case_dir="$repo/build/runs/ww3-gpu-full-openmp/kokkos"
period="${1:-0.1}"
run="$here/run"
rm -rf "$run"; mkdir -p "$run"
for f in ST4TABUHF2.bin depth.inp mask.inp mod_def.ww3 namelists.nml ww3_shel.nml; do cp "$case_dir/$f" "$run/"; done
cat > "$here/pmp.gdb" <<'EOF'
set pagination off
set confirm off
set print thread-events off
set print inferior-events off
set print frame-arguments none
set width 0
handle SIGUSR2 stop print nopass
handle SIGPIPE nostop noprint pass
run > ww3_shel.out 2>&1
while 1
  echo ==SAMPLE==\n
  thread apply all bt 60
  continue
end
EOF
cd "$run"
echo "start $(date -u +%FT%TZ) $(uptime)"
env OMP_NUM_THREADS=1 OMP_PROC_BIND=false WW_KOKKOS_ALL=0 WW_KOKKOS_PRESET=resident \
  gdb -q -batch -nx -iex "set auto-load safe-path /nix/store" -x "$here/pmp.gdb" --args "$exe" \
  > "$here/samples.txt" 2>&1 &
gdbpid=$!
child=""
for _ in $(seq 100); do
  child=$(pgrep -P "$gdbpid" -x ww3_shel || true); [ -n "$child" ] && break; sleep 0.1
done
[ -n "$child" ] || { echo "no ww3_shel child" >&2; exit 1; }
while kill -0 "$child" 2>/dev/null; do
  st=$(awk '{print $3}' "/proc/$child/stat" 2>/dev/null || echo X)
  if [ "$st" != "t" ] && [ "$st" != "X" ]; then sleep "$period"; kill -USR2 "$child" 2>/dev/null || true
  else sleep 0.01; fi
done
wait "$gdbpid" || true
echo "end $(date -u +%FT%TZ) $(uptime)"
tail -3 "$run/ww3_shel.out"
cmp "$run/out_grd.ww3" "$case_dir/out_grd.ww3" && echo "out_grd.ww3 identical to the timing run's Kokkos output"
python3 -I "$here/reduce.py" "$here/samples.txt"
```

`reduce.py`, which `profile.sh` calls on the samples:

```python
"""Reduce gdb samples: share of the W3SRCE batch in libm/libmvec, and its innermost frames."""
import collections
import re
import sys

text = open(sys.argv[1], errors="replace").read()
samples = text.split("==SAMPLE==")[1:]
frame = re.compile(r"^#(\d+)\s+(?:0x[0-9a-f]+ in )?(\S+)(.*)$")
total = 0
in_srce = 0
lib = collections.Counter()
inner = collections.Counter()
for s in samples:
    # thread 1 only (OMP_NUM_THREADS=1): the frames of the first "Thread" block
    blocks = re.split(r"\nThread \d+ ", s)
    main = None
    for b in blocks:
        if re.match(r"1 \(", b) or "Thread 1 " in b[:40]:
            main = b
            break
    if main is None and len(blocks) > 1:
        main = blocks[-1]
    if main is None:
        continue
    frames = [frame.match(l.strip()) for l in main.splitlines() if l.strip().startswith("#")]
    frames = [f for f in frames if f]
    if not frames:
        continue
    total += 1
    if not any("ww_srce_batch" in (f.group(2) + f.group(3)) or "srce_batch" in f.group(3) for f in frames):
        continue
    in_srce += 1
    name, rest = frames[0].group(2), frames[0].group(3)
    obj = "libmvec" if "libmvec" in rest else "libm" if "libm.so" in rest else "other"
    lib[obj] += 1
    inner[(obj, name)] += 1

print(f"samples {total}, in ww_srce_batch {in_srce}")
for k, v in lib.most_common():
    print(f"  {k:8s} {v:6d}  {100.0 * v / max(in_srce, 1):5.1f} % of the batch")
print("innermost frames in the batch:")
for (obj, name), v in inner.most_common(15):
    print(f"  {v:6d}  {100.0 * v / max(in_srce, 1):5.1f} %  {obj:8s} {name}")
```
