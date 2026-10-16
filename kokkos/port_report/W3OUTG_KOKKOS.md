# W3OUTG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim, the
`W3WAVE` wiring and the two hooks below.

- (v) Fresh review 2026-10-16: every statement `switch_lab_shrd` compiles in `:1201-2380`, read
  against a fresh `-O3` GIMPLE, `-fverbose-asm` assembly and `objdump` of `w3iogomd.F90` from a
  CMake Release build of the pin (disassembly identical to the CMake object): index bases, loop and
  reduction order per point, literals, power trees, the operand order of all 18 `maxss`/`minss`,
  the libmvec split of `:1733-1745`, the space-time block and section 6 in full, the dead locals,
  `FLOLOC` handling, the `SKEWNESS` call and the hook order. No defect in the arithmetic. Fixed in
  this report: the `LOGICAL` item (the object uses short-circuit selects, not bitwise
  operations), the wording of the `sqrtf` check and of what was run; two ⚠ closed with evidence
  (`LOGICAL`, the `SKEWNESS` object). The C++ compiles clean on OpenMP, Serial and CUDA (below).

`W3OUTG` turns the action spectrum `A(NTH,NK,0:NSEAL)` of every local sea point into the gridded
output fields: Hs, the mean periods `T0M1`, `T01`, `T02` and wavelength, the mean and peak
directions and frequency, the directional spread, the near-bottom orbital amplitude and velocity,
the radiation stresses, the Stokes drift and transport, the mean square slopes, the second-order
pressure spectrum that drives microseisms (`PRMS`, `TPMS`, `P2SMS`), the space-time extremes
(`STMAXE`, `HMAXE`, ...), and the partition fields copied from `W3PART`'s table. `W3WAVE` calls it
before every gridded output.

## What was ported

| | |
|---|---|
| WW3 routine | `W3OUTG`, `model/src/w3iogomd.F90:1201-2380` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), as `switches/switch_lab_shrd` compiles it. The text of `W3OUTG`, `SKEWNESS` and `SECONDHH` is identical at NOAA-EMC/WW3 `03adda91` (v, `diff` of the subroutines, 2026-10-16) |
| Kokkos code | [`../src/ww_kokkos/iogo_w3outg.hpp`](../src/ww_kokkos/iogo_w3outg.hpp): `ww::iogo::w3outg<ExecSpace>`, five `parallel_for` over the points (the whole-array fills of section 1, sections 2-4 per point, the partition fill and copy of section 6, `SKEWNESS`) |
| Callees | `SKEWNESS` (`:2355-2357`) through `skewness_point` of [`iogo_skewness.hpp`](../src/ww_kokkos/iogo_skewness.hpp), with `SECONDHH`'s `FAC0..FAC3` as inputs as that port requires. `CALC_U3STOKES` (`:4112-4341`, called at `:2347-2353`) and `CALC_WBT` (`:4367-4607`, `:2362`) are not translated: they are host hooks of `W3OutgHooks`, called after a fence in the Fortran's order, and a hook whose `FLOLOC` flag is set and that is missing aborts. `CALC_WBTv2` of `W3SNL5` (`snl5_calc_wbtv2.hpp`) is not `CALC_WBT`: it has a fixed `BETA = 1.2` where `CALC_WBT` reads `BTBETA`, no `MAX(DW,DMIN)`, no `MAPSTA` test and another `SIGP` threshold |
| Not ported, by switch | `W3_IG1` `HSIG` (`:1556-1558`), `W3_O9` negative Hs (`:1995-2003`), `W3_O8` filter (`:2058-2068`), `W3_T` prints (`:2219-2237`, `:2368-2375`), `W3_S` STRACE, the `W3_OMPG` directives. `switch_lab_shrd` sets none of them |
| Not computed | Locals that reach no output: `NKH` (`:1512-1513`), `FXPMC` (`:1388`), `ETMSSL`, `ETMSSCL` (`:1577-1589`), `ETUSCX`, `ETUSCY` (`:1617-1618`), `UBS` (`:1590`, `:1975`), `SCREST` (`:1809-1811`). The object keeps the arrays as stores that nothing reads (`NKH` with its `logf`) and folds the scalar `FXPMC` away; none of them has an effect a Release build can observe (no `errno` in Fortran, no trapping: `-ffpe-trap` is in WW3's Debug flags only) |
| Structural changes | The Fortran runs each section as a loop over `JSEA`; here one thread runs sections 2-4 for one point, which reads only that point's data. The local `EBD(NK,NSEAL)` is a caller work view; `SKEWNESS`'s `F2`, `SIGHF`, `DFIMHF`, `FAK` are work views with one column per point. `FLOLOC` is computed on the host and passed as booleans |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3iogomd.F90` compiled with the flags of WW3's CMake
Release build under `switch_lab_shrd` (gfortran 16.2.0 in the pinned `ww3` shell), with a
disassembly identical to the CMake object of the same commit, read as GIMPLE
(`-fdump-tree-optimized`) and assembly. The C++ is a throwaway unit with the `openmp-release`
flags, read the same way. 2026-10-16.

- (v) Products keep the Fortran's precedence: `X*Y**2` is `X*(Y*Y)` (`ET02`, `ETXX`, `ETYY`,
  `ETXY`, `UBR`, `UBA`, `UBD`, `MSSXM`, `MSSYM`, `MSSXYM`, `:1565-1576`, `:1645-1647`,
  `:1760-1771`), `FACTOR2 = (((DTH*2)/TPI**2)*SIG)*(TPI*SIG/CG)**2*ABST` (`:1597-1600`),
  `MSCX = (MA*C**2 + ((2*MB)*S)*C) + MA*S**2` (`:1631-1632`), the same for `MSSX`, `MSSY`
  (`:1949-1952`), `FT1 = ((0.3333*SIG**2)*DTH)*SIG` (`:1390`).
- (v) Integer powers as gfortran's power tree: `SIG**5 = (x2*x)*x2`, `SIG(NK)**4 = x2*x2`,
  `XFR**3 = xfr*x2`.
- (v) Folded `PARAMETER` products, `static_assert`ed against the object's bits: `0.4*TPI`,
  `TPI**2`, `GRAV**2`, `RADE*0.5`, `0.01*RADE`, `DWAT*GRAV`, `SQRT(TPI)`, `SQRT(6.)`,
  `PI/SQRT(6.)`, `DERA`. `SQRT(TPI)` and `SQRT(6.)` are written as hex literals: each value is
  in the object's constant pool (`* 2.5066282749176025390625e+0` in the GIMPLE of `:1836`), and a
  throwaway program checked once that `sqrtf` returns both.
- (v) Every `MAX` is `maxss` with an operand order the object chooses, so a NaN or a signed-zero
  tie gives the source operand: `FACTOR` (`:1530`), `KD` (`:1639`), `ABXX` (`:1655`), both in `THS`
  (`:2008-2009`) and `DENOM` (`:2144`) have the expression as destination; `ABYY` (`:1656`), `ABR`
  (`:2024`) and `UBR` (`:2031`) have `0.` as destination. `MAX(1E-20,AB)` (`:1678`, `:1683`)
  differs between the two copies of the `JSEA` loop the object keeps, chosen by `FLOLOC(3,5)`:
  `AB` is the destination when it is set, `1E-20` when it is not; the translation picks the same. `STED` (`:1823-1827`) became `maxss` with `STEDU` destination.
  `MINVAL` (`:1904`) starts at the first non-NaN element, then `a < min ? a : min`.
- (v) libm: `atan2f`, `sinhf`, `coshf`, `logf`, `powf` (`ET**(-1.5)`), `cosf` in the `PHI` loop,
  and `sincosf` for each `SIN`/`COS` pair of one argument (`MSCD`, `MSSD`, `THMP`). glibc 2.44 of
  the pinned shell returns the bits of `sinf` and `cosf` from `sincosf` for all 2^32 inputs
  (exhaustive run, 2026-10-16, repeated independently by the fresh review), so the C++ may call
  either. `MOD` is inlined as `fprem`; `fmodf` is
  exact, as `fprem` is.
- (v) libmvec. The object vectorises the `JSEA` loop of `:1733-1745` by fours with
  `_ZGVbN4v_sinf`/`_ZGVbN4v_cosf` of `THMP` when `NSEAL >= 4`, for `JSEA <= 4*(NSEAL/4)`, and
  calls `sincosf` for the other points; it is the only libmvec use in `W3OUTG`. The C++ makes the
  same choice per point (`thmp_sincos`) and broadcasts `THMP` to the four lanes. The exhaustive run
  showed that a lane's result depends on its own input only (each of 2^32 inputs in every lane
  position with other neighbours; the broadcast form on one input in 64). The fresh review's own
  run compared, for every 2^32 input, the four lanes of the broadcast form and the input in lane
  `i mod 4` among neighbours `1e30`, NaN or `0.5`: no difference.
- (v) Sums keep the Fortran's order per point. g++ vectorises the `ITH` sums of the space-time
  block as in-order fold-left reductions (lane by lane into the accumulator); gfortran's loops
  over `JSEA` do not mix points. No FMA in the C++ object (`objdump`, no `vfmadd`/`vfmsub`).
- (v) Index bases: `A(:,:,JSEA)` is column `jsea` of `A(NTH,NK,0:NSEAL)`, `CG`, `WN`
  `(0:NK+1,0:NSEA)` and `DW(0:NSEA)` are read at `ISEA = JSEA` (`INIT_GET_ISEA` under `W3_SHRD`,
  `w3parall.F90:1484`), `SIG(0:NK+1)` at `IK`, `EF`..`STH2M` at `IK - E3DF(2,k)`, `P2SMS` at
  `IK - P2MSF(2)`, `PHS`.. at `0:NOSWLL` (`w3adatmd.F90:1084-1119`, `:1211`).
- (v) Section 1 fills over the `NSEALM` extent of the fields, as the Fortran's whole-array
  assignments do; `CGE` is not reset (`:2038` writes it at sea points only).
- ⚠ NaN payloads. When both operands of an addition are NaN, the result carries the first
  operand's payload; the two objects need not order those operands alike. Bits can differ only
  for two NaNs of different payloads.
- (v) `LOGICAL` flags. The object builds `FLOLOC` with short-circuit selects (`FLOUTG /= 0`, then
  `FLOGRD /= 0`, else `FLOUTG2 /= 0` and `FLOGR2`) and every read of it, and of `FLOUTG`,
  `FLOUTG2`, tests `/= 0`; the C++ tests the same, so the truth values agree for any nonzero
  `LOGICAL`. Turning `FLPART`, `FLOUTG`, `FLOUTG2` into `bool` the same way is the shim's job.
- ⚠ glibc. The `sincosf` identity and libmvec lane independence were shown for glibc 2.44 only.
- ⚠ GPU. On a device the libmvec lanes fall back to `Kokkos::sin`/`cos`, and CUDA's
  transcendental functions are not shown to return glibc's bits (`AGENTS.md`, "Bit for bit"). The
  CUDA build compiles (below, not run). Its SASS has 1179 `FFMA` with `--fmad=false` against 1434
  with `--fmad=true`; the remaining ones are taken to be inside CUDA's math functions, not checked.
- (v) `skewness_point` was compared with the `03adda91` object (`SKEWNESS_KOKKOS.md`). The
  `SKEWNESS` and `SECONDHH` code of the `8e433f99` object is the same: disassembly with relocations,
  addresses stripped, identical, and the 20 and 13 constant-pool entries they reference equal.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest, and the port's code was never run. Test code was run, not the port: the
translator linked and ran a throwaway program that checked two `sqrtf` results against the folded
constants, and an exhaustive check program of glibc's `sincosf` and libmvec lanes; the fresh review
ran its own exhaustive libmvec/`sincosf` program (above). Compiled, 2026-10-16 (v):

- a throwaway translation unit that includes the header and explicitly instantiates
  `ww::iogo::w3outg` on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, with
  `g++ -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -ffp-contract=off` against the pinned `kokkos-openmp-5.2.2`: no warning, no `vfmadd`/`vfmsub`
  in the object, no libmvec call other than the two of `thmp_sincos`;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- the same unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89 --fmad=false
  --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against `kokkos-cuda-5.2.2`
  (CUDA 12.9): no warning, compile only.

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
