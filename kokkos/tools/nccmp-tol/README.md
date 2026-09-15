# `nccmp-tol` — per-field NetCDF comparator

Compares two NetCDF files variable by variable and judges the ones a tolerance
file names. It exists so an L2 replay (`tests/L2_replay.sh`) can say "the Kokkos
kernel reproduces the Fortran to within the noise floor of each output field"
with one exit code, instead of eyeballing `ncdiff` output.

```
nccmp-tol REF TEST [TOLERANCES]
```

| exit | meaning |
|---|---|
| 0 | every judged variable passes |
| 1 | at least one judged variable fails |
| 2 | I/O or usage error: unreadable file, malformed tolerance row, shape mismatch |

Without `TOLERANCES` the CLI reads the committed `tolerances.txt` from the source
tree (the path is baked in at configure time, like the tests' fixture directory).

## What it compares

- Every **numeric** variable present in **both** files, in the reference file's
  order. Text variables and variables present in only one file are skipped, not
  reported.
- Each variable is read as `double` whatever its stored type (`nc_get_var_double`),
  so float32 fields from `ww3_ounf`, doubles and integer masks share one code path.
- A cell is excluded when it is NaN on either side, or equal to that file's fill
  value on either side. The fill comes from `nc_inq_var_fill`, which also yields
  the type's *default* fill when a variable has no `_FillValue` attribute --
  netCDF's own meaning of "never written". A cell the *reference* lacks (land,
  mask) is simply skipped; a cell the reference has and the *test* lacks is
  counted as `dropped` and reported, because a kernel that writes NaN where
  the Fortran wrote a number is exactly what this tool exists to catch.
- A shape mismatch is an error (exit 2), not a failed comparison.

## The tolerance file

```
# variable  abs      rel     (a value passes if |d| <= abs OR |d|/max(|ref|,eps) <= rel)
hs          1e-4     1e-4
fp          1e-4     1e-4
dir         1e-2     1e-4
dp          1e-2     1e-4
t0m1        1e-4     1e-4
```

One row per judged variable; `#` starts a comment, blank lines are skipped. A
cell passes if it is within `abs` **or** within `rel` (relative to `|ref|`, with
`eps = DBL_EPSILON` guarding a zero reference). The verdict rule is:

> a judged variable **passes** iff it has at least one compared cell (`n > 0`)
> and every compared cell passes.

Nothing to compare is not agreement: a test field that is all NaN or all fill
where the reference is valid is `FAIL (n=0)`. Dropped cells (see above) do not
fail a variable on their own -- the verdict stays about the cells that could be
compared -- but the count is in the table and the CLI notes it on stderr.
`abs` is the field's noise floor -- directions get `1e-2` degrees because
`dir`/`dp` are quantised by the directional bin width.

A variable that is in both files but not in the tolerance file is reported as
`unlisted`: its statistics are printed so the reader can see what else moved,
but it never affects the verdict. A listed variable that is missing from either
file is noted on stderr and does not fail the run.

## Output

```
variable         n  dropped      max|d|         rms     max rel  verdict
--------------------------------------------------------------------------
hs               3        0   0.000e+00   0.000e+00   0.000e+00  PASS
fp               4        0   1.000e-01   5.000e-02   2.500e-01  FAIL
dir              0        4   0.000e+00   0.000e+00   0.000e+00  FAIL (n=0)
foo              4        0   9.000e+00   4.500e+00   9.000e+00  unlisted
nccmp-tol: FAIL (3 judged, 1 unlisted)
```

`n` is the number of cells compared after exclusions, `dropped` the cells the
reference has and the test does not; `max|d|` and `rms` are absolute, `max rel`
is `max |d| / max(|ref|, eps)` over the compared cells.

## Library

`nccmp_tol_lib` (static) exposes the same machinery in-process, which is how
`tests/L1_test_nccmp_tol.cpp` drives it on files it writes with netcdf-c:

```cpp
namespace ww::nccmp {
struct Tolerance { std::string name; double abs; double rel; };
std::vector<Tolerance> parse_tolerances(std::istream&);
struct Stats { std::string name; std::size_t n, dropped; double max_abs, rms, max_rel; bool judged, pass; };
std::vector<Stats> compare_files(const std::string& ref, const std::string& test,
                                 const std::vector<Tolerance>&);
bool all_pass(const std::vector<Stats>&);      // ignores unjudged variables
std::string format_table(const std::vector<Stats>&);
}
```

`compare_files` throws `std::runtime_error` on any NetCDF error; the open
datasets are RAII handles, so nothing leaks on that path.

## From the repository root

```bash
just nccmp ref.nc test.nc                     # default tolerances
just nccmp ref.nc test.nc my_tolerances.txt
```

SPDX-License-Identifier: MIT
