# Examples

Two hand-written cases, plus a catalogue of the upstream regression tests. They are the
real treasure and are far better than anything I could write from scratch.

| Dir | What | Needs external data? |
|---|---|---|
| `01-fetch-limited-growth/` | Cartesian box, flat bottom, constant wind. Compares against the classic empirical growth laws. | No |
| `02-regional-real-forcing/` | 0.1° spherical grid off southern Brazil, GEBCO bathymetry, GFS winds from NOMADS, partitioned + spectral output. | Yes (GEBCO; GFS via `get_gfs.sh`) |

Start with 01. It runs in a minute and has an analytic answer, so if it's wrong you know
your build is wrong rather than your configuration.

---

## The upstream regression tests

`$WW3/regtests/` contains roughly 80 complete, maintained, working configurations. Every one
is a real example with real input files, and they are kept working by CI. **This is the best
example collection that exists**: better than any tutorial, including this one.

Stage one into a scratch directory you can edit freely:

```bash
bash scripts/stage_upstream_example.sh $WW3 ww3_tp2.2
```

Or run one step by step, watching each program:

```bash
bash scripts/03_run_regtest.sh $WW3 ww3_tp2.2
```

### Naming convention

| Prefix | Family |
|---|---|
| `ww3_tp1.*` | **T**est **p**ropagation, **1**D: pure propagation in one dimension. No physics, just numerics. |
| `ww3_tp2.*` | Test propagation, 2D: including curvilinear, unstructured, SMC, rotated grids, and nesting. |
| `ww3_ts*` | Test source terms: growth, dissipation, nonlinear transfer in isolation. |
| `ww3_tc*` | Test cases with moving/idealised storms (e.g. a translating hurricane). |
| `ww3_tic*` | Ice physics (`IC1`–`IC5`, `IS1`/`IS2`). |
| `ww3_ta*` | Assorted: assimilation, tracking, utilities. |
| `mww3_test_*` | Multi-grid (`ww3_multi`): two-way nested mosaics of grids. |
| `ww3_ufs*` | UFS-coupled configurations. |

### Ones worth your time, in order

1. **`ww3_tp1.1`**: 1D propagation of a spectrum in space. No source terms. If this is
   wrong, nothing else can be right. Smallest possible sanity check.
2. **`ww3_tp2.2`**: the standard "did my build work" test, and what most install tutorials
   use. Runs `ww3_grid → ww3_strt → ww3_shel → ww3_ounf`.
3. **`ww3_tc1`**: an idealised storm. Its `ww3_shel.nml` is a beautifully minimal example
   of homogeneous forcing: one wind entry, one output field, nothing else. Worth reading
   even if you never run it.
4. **`ww3_ts1`**: source-term behaviour in isolation. This is where you go to understand
   what `ST4` versus `ST6` actually does.
5. **`ww3_tp2.7`**: unstructured grids with an external open-boundary list file
   (`UGOBCFILE`). The reference for triangular meshes.
6. **`ww3_tp2.17`**: the canonical example for **specifying open boundaries** in
   `ww3_grid.nml` and feeding them with `ww3_bounc`/`ww3_bound`. When people on the
   Discussions board ask how nesting works, this is the test they get pointed at.
7. **`mww3_test_01` … `_08`**: multi-grid. Once you want a coarse global grid feeding a
   fine coastal one in a single `ww3_multi` run, live in here.
8. **`ww3_tic1.*`**: sea ice. Increasingly the interesting frontier in wave modelling.

### How to read a regtest

```
regtests/ww3_tp2.2/
├── info              # what the test is for, and what to expect
├── input/            # every .nml or .inp the test needs   <- READ THIS
└── (reference output, fetched by ww3_from_ftp.sh)
```

Read `info` first, then diff the test's `input/` against the annotated templates in
`$WW3/model/nml/`. The delta between "every option documented" and "what this test actually
sets" is the fastest way to learn which knobs matter.
