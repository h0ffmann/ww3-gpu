# WW3's Fortran with glibc's fma builds and with its sse2 builds: byte-identical on two cases

2026-10-17, an agent for the owner. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
[`GPU_LIBM_BITS_202610.md`](../GPU_LIBM_BITS_202610.md).

## Question

glibc gives `expf`, `logf`, `powf`, `sincosf`, `exp` and `tanh` an fma build on a CPU with FMA
and AVX2 and an sse2 build otherwise, and the two differ on a few inputs. Does WW3's output
depend on which one the CPU gets?

## What was done

- Pure NOAA WW3 (`--ww3`, `WW3-noaa` at `03adda91`, `switches/switch_lab_ompg`), the binary
  `build/ww3/ww3/bin/ww3_shel` of 2026-10-15, on the owner's i9-14900 in the pinned `ww3` shell
  (glibc 2.44-25), `OMP_NUM_THREADS=4`, `OMP_PROC_BIND=false`, under `nice -n 19`.
- Each case's inputs copied from `build/runs/ww3-<case>/` into two directories, and `ww3_shel`
  run in one with `GLIBC_TUNABLES` unset (the fma build) and in the other with
  `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA` (the sse2 build); `scripts/libm_bits.sh probe`
  under the same variable answers "sse2" for `expf` and `sincosf` (v).
- `cmp` of `out_grd.ww3` and `out_pnt.ww3` between the two runs, and against the run of
  2026-10-15.

## Result

- (v) `ww3_ts1` (smoke): `out_grd.ww3` and `out_pnt.ww3` identical between the builds and
  identical to the 2026-10-15 run.
- (v) `bench_small` (full, 120 x 80, 12 h): `out_grd.ww3` identical between the builds and
  identical to the 2026-10-15 run.
- No wall time is quoted: the runs shared the machine with other work.

## What it changes

Nothing in the port. On these two cases the host's glibc build does not enter WW3's output; in
general it can (`GPU_LIBM_BITS_202610.md` counts the inputs), so the H100 node runs
`scripts/libm_bits.sh fingerprint` before its results are compared with this machine's.
