# WMIOEG_KOKKOS

**Status:** ported, the stores of `VA` only, wired and regtested on the CPU (2026-10-18) on the
fork branch h0ffmann/WW3 `ww3-gpu/u1-multi-va` (`f330dc53`), under the resident `VA`; not at the
`WW3/` pin until the integrator moves it ⚠.

`WMIOEG` updates grid `IMOD`'s `VA` from the grids of equal rank: it scales the native columns
(2.a) and adds the averaged, converted spectra of every other grid (2.b, or copies them on an SMC
grid). With the resident `VA` it is the same column path as
[`WMIOHG_KOKKOS.md`](WMIOHG_KOKKOS.md), with a read before, since 2.a and 2.b read the columns
they write.

## What was ported

| | |
|---|---|
| WW3 routine | `WMIOEG`, the stores of 2.a, `model/src/wminiomd.F90:3258-3262` at NOAA-EMC/WW3 `03adda91` (`VA(:,JSEA) = WGHT * VA(:,JSEA)`), and of 2.b (`:3278`, `:3326`, `:3330`) |
| Kokkos code | `ww_va_host_read_cols` (gather) and `ww_va_host_wrote_cols` (scatter) in [`../src/fortran_iface/resident_shim.cpp`](../src/fortran_iface/resident_shim.cpp) |
| Fortran side | `W3KOKKOS_VA_READ_COLS` before 2.a and `W3KOKKOS_VA_WRITE_COLS` after 2.b, with the union of `EQSTGE(IMOD,IMOD)%JSEA` and `EQSTGE(IMOD,J)%JSEA` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 26) |

## Arithmetic

None on the device; the Fortran statements run unchanged on the host columns, which the gather
made current first. Bits move, as in `WMIOHG_KOKKOS.md`.

## Benchmarks

None of its own: the regtest entries of `WMIOHG_KOKKOS.md` count both routines' columns
together, untimed ⚠.

## How it was regtested

The entries of [`WMIOHG_KOKKOS.md`](WMIOHG_KOKKOS.md). Whether those inputs reach `WMIOEG`'s 2.b
(grids of equal rank with overlap) was not checked ⚠: `WMIOEG` runs on every grid and its 2.a
loop runs when `EQSTGE(IMOD,IMOD)%NREC > 0`.
