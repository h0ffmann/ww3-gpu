# Switch files

A **switch file** is WW3's compile-time feature selector: a single whitespace-separated
list of CPP keys. The build system preprocesses the `.F90` sources against these keys, so
**changing the switch file means a full rebuild** (`rm -rf build`). This is the number one
source of "I changed the physics and nothing happened".

```bash
cmake .. -DSWITCH=/abs/path/to/switch_lab_shrd
# or, for a switch file that lives in $WW3/model/bin:
cmake .. -DSWITCH=Ifremer2
```

## ⚠ Before you trust these

The three files here are *reasonable lab defaults assembled from documentation and
tutorials*, not files copied out of an upstream tree. **Diff them against the real ones in
your clone before relying on them:**

```bash
ls $WW3/model/bin/switch_*
diff <(tr ' ' '\n' < $WW3/model/bin/switch_default | sort) \
     <(tr ' ' '\n' < switches/switch_lab_shrd      | sort)
```

If a key is misspelled or a mandatory group is missing, `ww3_grid` will usually fail loudly
at configure/preprocess time rather than silently, which is the good case. If in doubt,
start from `$WW3/model/bin/switch_default` or `switch_Ifremer2` and edit one group at a time.

## The files

| File | For |
|---|---|
| `switch_lab_shrd` | Serial. Start here. Easiest to debug, fine for the course examples. |
| `switch_lab_mpi`  | `DIST MPI`: distributed memory. Use once examples work; run with `mpirun -np N ./ww3_shel`. |
| `switch_lab_st6`  | Same as MPI but swaps the source-term package to `ST6` (observation-based) so you can A/B it against `ST4`. |

## Reading a switch line

Keys come in mutually-exclusive *groups*. You pick exactly one from most groups.

| Key(s) | Group | Meaning |
|---|---|---|
| `F90` | language | Use Fortran 90 style / system calls |
| `NOGRB` | GRIB | No GRIB output. Alternatives write GRIB via NCEP libs, and it isn't worth it. |
| `NOPA` | coupling | No coupling to an external driver |
| `LRB4` | I/O | 4-byte record length for binary files. Affects `mod_def.ww3`/`restart.ww3` portability. |
| `NC4` | output | Legacy "netCDF-4 output" key. **Inert in 7.14** `(v)`: not in `model/src/cmake/switches.json` or `model/bin/all_switches`, and no `W3_NC4` guard in `model/src`, so no build honours it. `ww3_ounf`/`ww3_ounp` are built whenever CMake finds netCDF; netCDF-3 vs -4 is `FILE%NETCDF` in `ww3_ounf.nml`. Kept here because it is harmless and appears in older switch files. |
| `SHRD` / `DIST MPI` | parallel | Shared-memory (serial) vs distributed (MPI). `OMPG`/`OMPH` add OpenMP. |
| `PR3 UQ` | propagation | Third-order ULTIMATE QUICKEST scheme with the Garden Sprinkler correction. The standard choice. `PR1` is first-order upwind, `PR2` second-order. |
| `FLX0` / `FLX4` | wind flux | Air-sea flux computation. `FLX0` means *no separate flux routine*: `ST4` computes its own stress in `W3SPR4`, and upstream pairs `ST4` with `FLX0` everywhere (`switch_NCEP_st4`, `switch_Ifremer2`, every `ST4` regtest switch). The lab files used to say `FLX2` here; under `ST4` that made `W3FLX2` (Tolman & Chalikov 1996) overwrite `USTAR`/`CD` every step (`w3srcemd.F90`, the `W3_FLX2` block right after `W3SPR4`), so it was replaced. `FLX4` is the usual partner for `ST6`. |
| `LN1` | linear input | Cavaleri & Malanotte-Rizzoli linear wave growth (seeds the spectrum from calm) |
| `ST4` / `ST6` / `ST2` | **source terms** | The big one. `ST4` = Ardhuin et al. 2010 (saturation-based dissipation + swell dissipation). `ST6` = Rogers/Babanin/Zieger observation-based. `ST0` = no wind input/dissipation (pure propagation tests). |
| `STAB0` | stability | No stability correction on the wind input |
| `NL1` | nonlinear | Discrete Interaction Approximation (DIA). `NL2` = exact (WRT, ~1000× slower), `NL3` = GMD. `NL1` is both the default and the single largest physical approximation in the model. |
| `BT1` | bottom friction | JONSWAP empirical. `BT4` = SHOWEX movable-bed. |
| `DB1` | depth breaking | Battjes-Janssen |
| `MLIM` | limiter | Miche-style shallow-water Hs limiter |
| `TR0 BS0` | triads, bottom scattering | Off |
| `IC0 IS0` | sea ice | No ice dissipation / no ice scattering. `IC1`–`IC5` and `IS1`/`IS2` turn on ice physics. |
| `REF0` | reflection | No shoreline reflection. `REF1` needs a slope map. |
| `WNT1 WNX1` | wind interpolation | Linear in time / in space |
| `RWND` | wind | Wind is relative to current |
| `CRT1 CRX1` | current interpolation | Linear in time / space |
| `O0`–`O14` | output | Verbosity and extra log output. `O7` gives point-output diagnostics; `O11`/`O14` add multi-grid and mask logging. Harmless to include. |

## Exercise

Build twice (once with `switch_lab_shrd`, ST4, and once with `switch_lab_st6`, ST6) into
two separate build directories, run `examples/01-fetch-limited-growth` against both, and plot
Hs(fetch) for each. That difference *is* the current state of the art disagreeing with itself,
and it's much bigger than most people assume.
