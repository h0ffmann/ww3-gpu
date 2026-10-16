# UOST_SOURCETERM_SETGRID_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The `SETGRID` method of the unresolved-obstacles source term object (switch `UOST`, Mentaschi
et al.): points the object at a spatial and a spectral grid and caches `COS(TH)`, `SIN(TH)` of
the spectral directions. Its only caller is `UOST_SETGRID` through the type binding
(`w3uostmd.F90:277`), itself called by `W3INIT` (`w3initmd.F90:545`) and on every entry of
`W3WAVE` (`w3wavemd.F90:700`).

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SOURCETERM_SETGRID`, `model/src/w3uostmd.F90:606-667` at NOAA-EMC/WW3 `03adda91`, 62 lines, and the type `UOST_SOURCETERM` (`:75-93`); compiled by nine regtest switches (`ww3_tp2.21` `switch_NO_PDLIB`, `switch_PDLIB`, `switch_PDLIB_METIS`; `ww3_ts4` `input_rg_multi`, `input_rg_shel`, `input_ug`, each with and without MPI; `git grep -lw UOST -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Its only `#ifdef` block is `W3_S` (trace) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_sourceterm_setgrid.hpp`](../src/ww_kokkos/uost_uost_sourceterm_setgrid.hpp): the struct `ww::uost::UostSourceterm<Grid, Sgrd>` (the type's data: `costh`, `sinth` as `std::vector`, empty while unallocated; `gammaup = 10`, `gammadown = 20`; `grd`, `sgd` as pointers) and `uost_sourceterm_setgrid(self, grd, sgd, nth, th)`, an `inline` host function (it allocates). `nth`, `th` are `SGD%NTH`, `SGD%TH`, passed explicitly so the header does not depend on a C++ `SGRD` layout |
| Fit with `UOST_SETGRID` | [`uost_uost_setgrid.hpp`](../src/ww_kokkos/uost_uost_setgrid.hpp) takes `std::optional<UostSourceterm<Grid, Sgrd>>` and a callable `[](auto& st, auto& g, auto& s) { uost_sourceterm_setgrid(st, g, s, s.nth, s.th); }`; a throwaway unit instantiating both compiles warning-free |
| Type-bound procedures | free functions taking the object; `COMPUTE`, `COMPUTE_LD`, `COMPUTE_SE` are not translated here |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled with the build's flags and
checked byte-identical in disassembly to the CMake object, 2026-10-16. The C++ unit was compiled
with the openmp-release flags (g++ 16.2, `-O3 -march=x86-64-v3 -ffp-contract=off`), no warning.

- (v) The loop `:663-666` is vectorised (`-fopt-info-vec`: "loop vectorized using 16 byte
  vectors", "versioned for vectorization because of possible aliasing"). When `NTH-1 > 4`
  (unsigned) and the alias check passes, lanes of four from `ITH = 1` call `_ZGVbN4v_cosf`, then
  `_ZGVbN4v_sinf` on the same four values; the last `NTH mod 4` call scalar `cosf`, `sinf`. For
  `NTH <= 5` every element is scalar. The C++ does the same split with explicit libmvec calls;
  its object calls `_ZGVbN4v_cosf`, `_ZGVbN4v_sinf` under `> 4` and scalar `cosf`, `sinf` in the
  tail, with no `sincosf` merge and no other vectorisation of the tail.
- (v) The alias check compares the fresh `COSTH`, `SINTH` with `SGD%TH`; they cannot overlap, so
  the vector path is taken for `6 <= NTH < HUGE(0)` (the object also tests `NTH != 2147483647`).
  The C++ omits the alias check and keeps the `NTH != 2147483647` test, which also keeps its
  `i + 4` from overflowing. `th` is read as contiguous: `SGD%TH` is a pointer component
  (`w3gdatmd.F90:800`), allocated contiguous ⚠ (the object handles any stride).
- (v) No other floating-point operation, no reduction, no MIN/MAX. The defaults `10`, `20` are
  exact in `float`.
- ⚠ The Fortran deallocates and re-allocates `COSTH`, `SINTH` (`:655-662`); the C++ reassigns
  the vectors. The difference is not observable: every element is overwritten.
- ⚠ libmvec is host only (`KOKKOS_IF_ON_HOST` is not needed: the function is not a kernel). The
  variant glibc picks at run time depends on the CPU, for the Fortran and the C++ alike.
- ⚠ CUDA not compiled: host-only code.
- (v) Fresh review 2026-10-16: no difference in bits for any reachable `NTH`. The Fortran
  object was rebuilt from `build_mixa`'s flags and found byte-identical to the CMake object; its
  disassembly was read against the C++ unit's object (`_ZGVbN4v_cosf`, `_ZGVbN4v_sinf` lanes of
  four from `ITH = 1` under `NTH-1 > 4`, scalar `cosf`, `sinf` tail, no `sincosf`, no other
  vectorisation, warning-free). Fixed: the C++ took the vector path for `NTH = 2147483647`,
  where the object stays scalar and the C++ `i + 4` overflowed; the test was added and the
  object read again.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
