# W3GFCL_R8_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Search of the grid-search-utility (GSU) object for the cell that encloses a target point
`(XT,YT)`: the bucket that contains the point, then a cross-product test of each of its cells.
With `FNCL`, when no cell encloses the point, the cell whose vertex centroid is nearest, searched
over rings of neighbouring buckets.

## What was ported

| | |
|---|---|
| WW3 routine | `W3GFCL_R8`, `model/src/w3gsrumd.F90:1369-1669` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, v), as `switches/switch_lab_shrd` compiles it |
| Callees, translated in the same header | `W3CKCL_R8` (`:3615-3840`), `W3DIST_R8` (`:5829-5877`, with `DIST_WITH_SINE`, defined at `:5827`), `W3NNSC` (`:6819-6879`) |
| Kokkos code | [`../src/ww_kokkos/gsru_w3gfcl_r8.hpp`](../src/ww_kokkos/gsru_w3gfcl_r8.hpp), `ww::gsru::w3gfcl_r8`, host-only C++ |
| GSU object | `GsuClass` of [`gsru_gsu_create.hpp`](../src/ww_kokkos/gsru_gsu_create.hpp), not redefined; `ww::gsru::w3nnsc_hook` fills that header's `W3NNSC` hook |
| Not ported | `W3_S` (`STRACE`) and `DIST_CHECK_NAN` (`:5870-5875`; `:5828` defines `DIST_CHECK_NAN____disabled`) |
| Messages | the `DEBUG` `WRITE`s to unit `*` are text in `GfclResult::out`, the error `WRITE`s to unit 0 in `::err`; `EXTCDE` (W3SERVMD's, `w3servmd.F90:1064-1211`) is returned as its exit code |

## Arithmetic

The reference is the `-O3` object of `w3gsrumd.F90` (WW3's CMake Release flags under
`switch_lab_shrd`, gfortran 16.2.0), recompiled in a scratch directory with
`-fdump-tree-optimized-lineno`; its disassembly equals the CMake object's (v, 2026-11-12). Read
as GIMPLE and assembly:

- (v) `MOD(X,D360)` is `__builtin_fmod`, an inline x87 `fprem` loop (`:1464`, `:1527`, `:1607`);
  `fmod` is exact, so `std::fmod` gives the same value.
- (v) `XT.LT.XMIN-LEPS` is emitted as `XMIN-LEPS > XT` (`:1475-1476`), the same predicate for
  every input: a NaN target is not rejected and is searched.
- (v) `INT((XT-XMIN)/DXB)` is a 32-bit `cvttsd2si` (`INT_MIN` for a NaN or out-of-range
  quotient), `+1` wraps in 32 bits, then `MAX(.,1)` and `MIN(NBX,.)` (`:1484-1485`).
- (v) `SUM(XS)/FOUR` is `(((XS(1)+0.0)+XS(2))+XS(3))+XS(4)` times 0.25 (`:1616`): the `+0.0`
  turns a `-0.0` first vertex into `+0.0`.
- (v) `NINT(HALF*MAX(NBX,NBY))` is one `lround` call on `(double)MAX*0.5`, low 32 bits (`:1563`).
- (v) `W3DIST_R8`: `ARGD = sqrt(((cos(YT*D2R)*cos(YS*D2R))*SLAM)*SLAM + SPHI*SPHI)`,
  `DIST = asin(ARGD)*(2*R2D)`; `sin`, `cos`, `asin` are libm calls, `SQRT` is `sqrtsd`.
- (v) `W3CKCL_R8`: `MINVAL`/`MAXVAL` are gfortran's NaN-aware inline reductions;
  `V1X = XXS(J)-XXS(J)` (`:3743`) is computed, not folded (NaN for a non-finite vertex).
- (v) No FMA (`vfmadd`: 0 in the Fortran object and in the C++ unit); no floating-point loop is
  vectorised (the vectorised loops of `:1661`, `:3681` are `COUNT`s).
- (v) The C++ unit with the `openmp-release` flags and with `-O0 -g -fsanitize=address,undefined`
  compiles with no warning.
- (v) A throwaway differential program (not kept, ADR-0010) built GSU objects with the Fortran
  (`W3GSUC_TGT_R4/_R8`) and called `__w3gsrumd_MOD_w3gfcl_r8` and `w3gfcl_r8` on 4 grid kinds,
  `R4` and `R8`, `IJG` true and false, 4000 target points each (NaN, `-0.0`, shifts of ±360,
  vertices, `FNCL`, `EPS` variants), plus 160 `DEBUG` cases: cell indices, vertices, `POLE`, the
  overwritten `NNB`, stdout and stderr text and exit status identical at `-O3`, and at `-O0`
  under ASan/UBSan with leak detection off (the Fortran's child exits 1 under LeakSanitizer,
  see `:1563` below). The fresh review reran it with the same result (v, 2026-11-12). ⚠ No
  command in the repository reproduces these counts.
- ⚠ When both operands of a commutative add or multiply are NaN with different payloads, the
  NaN can differ: the review read one such site, `DX**2 + DY**2` (`:5868`), which the Fortran
  object adds as `DX**2` first; the copy of `w3dist_r8` inlined in `w3gfcl_r8` does the same,
  the out-of-line copy adds `DY**2` first (v, objdump of both), and swapping the C++ terms swaps
  both copies. GCC picks the order, not the source. Not otherwise compared.
- ⚠ On a GPU `sin`, `cos`, `asin` would be CUDA's; the routine is host-only and no GPU path
  exists.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller
hunk in the fork.

## WW3 defects found

Not fixed (WW3 is read-only). Where the Fortran's behaviour is undefined, the C++ stops with a
code that is not a WW3 code.

- `:1563`: `GSU` is `INTENT(IN)`, but `NNB = W3NNSC(...)` is an intrinsic assignment through the
  pointer `NNB => GSU%PTR%NNB` (read in the GIMPLE, v): every `FNCL` call that reaches it
  overwrites the object's `NNB` with a new one and leaks the old arrays and the new object. The
  leak is reported by LeakSanitizer on the Fortran side of the differential program (v); its size
  per call ⚠ not measured. The C++ overwrites `gsu->nnb` the same way, without the leak.
- ⚠ `:1484`: with `LCLO`, `IB` is not clamped to `NBX`, so a target that `XT+D360` rounds to 360
  gives `IB = NBX+1` and reads `B(JB,NBX+1)`. The C++ returns `kExitBucketBounds` (-131).
- ⚠ `:1642-1649`: with `FNCL`, when no cell gives `DD < HUGE`, `IS1..YS1` and `LVL1..K1` are
  read undefined. The C++ returns `kExitNoClosestCell` (-132).
- ⚠ `POLE` is `INTENT(OUT)` and left undefined on the returns of `:1479` and `:1553`; the C++
  leaves `*pole` unchanged.
