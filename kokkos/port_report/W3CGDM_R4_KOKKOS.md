# W3CGDM_R4_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

The metric of a curvilinear grid at each output point. The derivatives of the physical
coordinates `(x,y)` with respect to the index coordinates `(p,q)` come from finite differences
of order `NFD`. Where `NFD > 2` gives a left-handed cell, the routine falls back to `NFD = 2`.
From those derivatives it returns the covariant metric `gpp`, `gqq`, `gpq`, the Jacobian
`sqrt(g)`, the scale factors `hp`, `hq`, the contravariant metric, the inverse derivatives
`dp/dx .. dq/dy`, `cos(alpha)` and the local grid rotation.

## What was ported

| | |
|---|---|
| WW3 routine | `W3CGDM_R4`, `model/src/w3gsrumd.F90:4014-4363` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v), as `switches/switch_lab_shrd` compiles it |
| Callees, translated in the same header | `GET_FDW3` (`:9582-9604`), `W3FDWT_R8` (`:6706-6747`) |
| Callees, untranslated hooks | `DXYDP` (`:8410-8687`) and `DXYDQ` (`:8693-8982`), the finite-difference stencils with masks, index closure and the polar stereographic branch: `CgdmHooks::dxydp`, `::dxydq`, called where the Fortran calls them (`:4214`, `:4230`, `:4259`, `:4275`); a null hook that is needed ends with `kExitMissingHook` (-101) |
| Kokkos code | [`../src/ww_kokkos/gsru_w3cgdm_r4.hpp`](../src/ww_kokkos/gsru_w3cgdm_r4.hpp), `ww::gsru::w3cgdm_r4`, host-only C++; constants and helpers shared with [`gsru_gsu_create.hpp`](../src/ww_kokkos/gsru_gsu_create.hpp) and [`gsru_w3gfcl_r8.hpp`](../src/ww_kokkos/gsru_w3gfcl_r8.hpp) |
| Not ported | `W3_S` (`STRACE`, `:4074-4077`) |
| Messages | the error `WRITE`s to unit 0 are text in `CgdmResult::err`; `EXTCDE` (`w3servmd.F90:1064-1211`) is returned as its exit code when `RC` is absent, else `RC` is set |
| Caller | `W3CGDM` generic (`w3gsrumd.F90:454-457`), called by `W3GNTX` for the curvilinear metric of the model grid (`w3gdatmd.F90:3050`, `:3065`) |

## Arithmetic

The reference is the `-O3` object of `w3gsrumd.F90` (WW3's CMake Release flags under
`switch_lab_shrd`, gfortran 16.2.0), recompiled in a scratch directory with
`-fdump-tree-optimized-lineno`; its disassembly equals the CMake object's (v, 2026-11-12). Read
as GIMPLE and assembly:

- (v) `FACY = R*D2R` (`:4141`); `FACX = FACY*cos(REAL(Y,8)*D2R)`, a libm `cos` call (`:4243`,
  `:4288`); `REARTH = 4.D7/PI2` folded.
- (v) `GSQRL = DXDPL*DYDQL - DXDQL*DYDPL` (`:4249`, `:4294`); the sums of products of
  `:4312-4314`, `:4322-4324` as written.
- (v) `MAX(x,SMALL)` is `maxsd` with `x` first (`:4315-4317`): a NaN or a tie gives
  `SMALL = 1D-15`.
- (v) `DPDYL = -(DXDQL/GSQRL)`, `DQDXL = -(DYDPL/GSQRL)`: the negation after the division.
- (v) `SQRT` is `sqrtsd`; `GSQRL**2` is `GSQRL*GSQRL` (`:4328`); `ATAN2` is a libm `atan2` call
  times `R2D` (`:4333`); each output is a `cvtsd2ss` of its `REAL(8)` local.
- (v) `W3FDWT_R8`: the recurrences of `:6735-6742` in the source's order; `:6735` is vectorised
  over `K` elementwise, same bits.
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit). The C++ unit with the
  `openmp-release` flags and with `-O0 -g -fsanitize=address,undefined` compiles with no warning.
- (v) A throwaway differential program (not kept, ADR-0010) called `__w3gsrumd_MOD_w3cgdm_r4`
  and `w3cgdm_r4`, with the Fortran `DXYDP`/`DXYDQ` as the C++ hooks, on 123 cases: `NFD` 2, 4,
  6, a masked block, `SPHERE` false, `RADIUS` with `DX`/`DY`, NaN latitudes, the left-handed
  error text with `RC`, the `NFD` and `ICLO` exits. Of 1 056 510 output values, every one
  matched bit for bit except 41 (all `APQC`) at `-O3` and 234 (`APPC`, `AQQC`, `APQC`) at `-O0`
  under ASan/UBSan, which are NaN in both and differ only in the sign of the NaN. Error text
  and `RC` matched. The fresh review reran it (v, 2026-11-12): the same 41 and 234 on the
  translator's header; after its change to the allocation of `K` and `C` (below), 43 at `-O3`
  (all `APQC`) and 234 at `-O0`, every other value equal. ⚠ No command in the repository
  reproduces these counts.
- (v) `ALLOCATE(K(0:N,0:N,1:N), C(...), STAT=ISTAT)` (`:4172`): in the object an element count
  `N*(N+1)**2` that overflows `INTEGER(8)`, or whose byte size does, sets `ISTAT = 5014`, a
  failed `malloc` 5020 (read in the GIMPLE). The translator's C++ returned 5020 only on
  `std::bad_alloc`, and a huge `NFD` (`2**21`) ended in an uncaught `std::length_error`; the
  review's `cgdm_detail::alloc_fd` reproduces both codes, and the differential program gives
  the same `RC` and text as the Fortran for `NFD = 2**21` (5014), `2**20` and 92680 (5020), at
  `-O3` only (ASan aborts on such a request).
- ⚠ Which NaN comes out of a commutative add or multiply when both operands are NaN is the
  operand order GCC picks from its SSA numbering, which the two front ends number differently.
  Reordering the C++ terms made the count worse, so the source order is kept; the review tried
  two other orders of `APQC` (41 and 109) and a `__builtin_assoc_barrier` (41): none removed it, and an unrelated
  change moved it (41 to 43). The Fortran object itself has two copies of `:4312-4333` whose
  GIMPLE orders the same product differently (`HQFCL*HPFCL` and `HPFCL*HQFCL`, `:4327`). Not
  fixable from the C++ source; it is the one known divergence from the Fortran's bits.
- ⚠ Not checked: the `DXYDP`/`DXYDQ` hooks filled by C++ (they are untranslated); a GPU, where
  `cos` and `atan2` would be CUDA's (the routine is host-only).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller
hunk in the fork.

## WW3 defects found

Read in the source, not fixed (WW3 is read-only), translated as written.

- ⚠ `:4328`: `SINAL = GSQRL**2/(GPPCL*GQQCL)` is `sin²(alpha)`, where `sqrt` of it, `sin(alpha)`,
  would be expected beside `COSAL = GPQCL/(HPFCL*HQFCL)`. Apparent from the source; its effect on
  `COSC`, `SINC`, `ANGL` not reproduced numerically.
- ⚠ `:4330`: `SINTP = DYDPL/HQFCL`, where `HPFCL` would be expected beside
  `COSTP = DXDPL/HPFCL`. Apparent from the source; not reproduced numerically.
- ⚠ `GET_FDW3` (`:9592-9601`) leaves the entries of `K` and `C` with `J > L` or `I > L` undefined;
  the C++ keeps them zero.
- `W3CGDM_R4` allocates `K2`, `C2` and computes `GET_FDW3(2,...)` on every call
  (`:4185-4196`), whether or not a point falls back to `NFD = 2` (read in the source, v); a cost,
  not a wrong value.
