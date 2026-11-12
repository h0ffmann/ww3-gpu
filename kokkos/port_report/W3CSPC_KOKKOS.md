# W3CSPC_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Conversion of `NSP` spectra from one discrete (frequency, direction) grid to another: each input
bin's energy is redistributed over the output bins it overlaps, the result is turned back into
densities, and above the last input frequency a tail `FTL*SP2(:,J-1,:)` is added. The
redistribution tables of each pair of grids are kept in a chain and reused on the next call.

## What was ported

| | |
|---|---|
| WW3 routine | `W3CSPC`, `model/src/w3cspcmd.F90:143-603` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3cspcmd.F90` is empty, v), as `switches/switch_lab_shrd` compiles it |
| Module state | `TYPE CASE` and the chain `NCASES`/`IDATA` (`w3cspcmd.F90:104-114`), as `ww::cspc::CspcState`, owned by the caller |
| Kokkos code | [`../src/ww_kokkos/cspc_w3cspc.hpp`](../src/ww_kokkos/cspc_w3cspc.hpp), `ww::cspc::cspc_w3cspc`, host-only C++ (no View, no kernel) |
| Not ported | `W3_S` (`STRACE`) and the test output `W3_T`, `W3_T1`, `W3_T2`: not compiled under the lab switch |
| Messages | the `WRITE`s to `NDSE` (`:271`, `:276`, `:281`) are text in `CspcResult::ndse`; `EXTCDE` (`:272`, `:277`) is returned as its exit code (`EXTCDE` ends in `CALL EXIT`, `w3servmd.F90:1207`) |
| Caller | `W3IOBC` (`w3iobcmd.F90:738`); `cspc_w3cspc` has the argument list of `IobcHooks::w3cspc` in [`iobc_w3iobc.hpp`](../src/ww_kokkos/iobc_w3iobc.hpp) plus the state, so a capturing lambda fills that hook |

## Arithmetic

The reference is the `-O3` object of `w3cspcmd.F90` (WW3's CMake Release flags under
`switch_lab_shrd`, gfortran 16.2.0), recompiled in a scratch directory with
`-fdump-tree-optimized-lineno -fopt-info-vec-optimized`; its disassembly equals the CMake
object's (v, 2026-11-12). Read as GIMPLE and assembly:

- (v) `TPI` is the `REAL` literal 6.283185482025146484375 (`constants.F90:73`);
  `DTH = TPI/REAL(NTH)` in `REAL`; `INT(DTH1/DTH2)` is a 32-bit `cvttss2si` (`:387`).
- (v) `RLOW = (LOW-TH2)/DTH2 + 1`, `NINT(RLOW)` is a call of `lroundf` of which the low 32 bits
  are kept (`:401`): a NaN gives 0.
- (v) `MIN(BHGH,HGH)` is `minss` with `BHGH` first, `MAX(BLOW,LOW)` is `maxss` with `BLOW` first
  (`:404`, `:472`): a NaN or a tie gives `HGH`, `LOW`.
- (v) `XDF = 0.5*(XF - 1./XF)` (`:430`, `:432`) and the tail `FTL*SP2` (`:558`) are vectorised
  elementwise in the object, same bits as the scalar C++.
- (v) `FRAC = (((R2*FRQ1(I2))*XDF1)*R1)*DTH1` (`:531`), `FACT = 1./((FRQ2(J2)*XDF2)*DTH2)`
  (`:546`), in that order. No FMA in either object (`vfmadd`: 0).
- (v) The C++ unit with the `openmp-release` flags (`-O3 -march=x86-64-v3 -std=c++20 -fopenmp
  -ffp-contract=off -Wall -Wextra -Wpedantic -Wshadow -Wconversion`) and with `-O0 -g
  -fsanitize=address,undefined` compiles with no warning; 0 `vfmadd`.
- (v) A throwaway differential program linked the reference object (not kept, ADR-0010; nothing
  of it is in the repository) and called `__w3cspcmd_MOD_w3cspc` and `cspc_w3cspc` on the same
  inputs: 128 pairs of grids, with the chain reused across calls, and the 6 message and exit
  paths. Spectra, `NDSE` text and exit codes were identical at `-O3` and under ASan/UBSan. The
  fresh review reran it and added (v, 2026-11-12): 40 calls whose `SP1` holds NaNs of random
  payload and sign (and a NaN `FTL`), 40 825 NaN outputs, every one the Fortran's NaN at `-O3`;
  and `TH2 = TH1 + 8` with output frequencies below the input's, where the Fortran returns zero
  spectra (below). ⚠ No command in the repository reproduces these counts.
- ⚠ Not checked: a `W3IOBC` run through the hook; the GPU (the routine is host-only).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller
hunk in the fork.

## WW3 defects found

Read in the source and the object, not fixed (WW3 is read-only). Where the Fortran's behaviour
is undefined, the C++ stops with a code that is not a WW3 code.

- ⚠ `:440-445`: the loop that grows `NRMAX` never ends when `AUX1` cannot become negative, which
  happens for `XF1` or `XF2` NaN or `+Inf` (a NaN `XF` passes the check of `:269`) or for a large
  `XDF1/XDF2`; `NRMAX` overflows. Seen in the object's loop, not run to the end. The C++ returns
  `kExitNrmax` (-141).
- ⚠ `:474-476`: `IDFR(0,I)` can exceed `NRMAX`, a write outside `IDFR`/`RDFR`. The C++ returns
  `kExitTableBounds` (-142).
- ⚠ `:407`: `1+MOD(J-1+NTH2,NTH2)` is outside `1..NTH2` when `J-1+NTH2` is negative (`NINT(RLOW)`
  below `1-NTH2`, that is `TH1` more than about one turn below `TH2`). Storing it is harmless; it
  goes outside `SP2` only where `:532` reads it, which needs an overlap in frequency. The C++
  returns `kExitTableBounds` there. The translator's C++ returned it when the table was built,
  so a case without overlap, which the Fortran converts to zero spectra, failed; the fresh review
  moved the check to `:532` (v, the differential case above).
- ⚠ `:557`: with `NFR2T = 1` the tail reads `SP2(:,0,:)`, outside the array. Same code.
