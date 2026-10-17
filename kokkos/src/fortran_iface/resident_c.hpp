// kokkos/src/fortran_iface/resident_c.hpp
// The C ABI of the resident spectrum (WFIP-0005 units 1 and 2): declarations only.
// resident_shim.cpp implements the ww_va_* calls, srce_shim.cpp ww_srce_batch_va;
// model/src/w3kokkos_residentmd.F90 and w3kokkosmd.F90 on the fork
// (h0ffmann/WW3 ww3-gpu/kokkos-resident) mirror them. The rules of ww_kokkos_c.hpp hold:
// Fortran order, caller-owned arrays, float is WW3's REAL, nothing throws.
//
// One context per grid `igrid` (W3GDATMD's IGRID): VA(NSPEC,0:NSEALM) of that grid,
// bound on the grid's first call to the address and shape it passes, and a device copy
// d_va. The state says which copy holds the newer bits: HOST (the start, and after every
// ww_va_host_write), DEVICE (after a kernel wrote d_va), BOTH. A host statement of W3WAVE
// that reads VA calls ww_va_host_read before it, one that writes VA ww_va_host_write;
// a kernel shim syncs d_va before its launch. With no kernel on d_va the state never
// leaves HOST and the host calls copy nothing (PATCH.md section 11).
//
// Diagnostics, read once, which ignore WW_KOKKOS_ALL as every *_COUNT flag does:
//   WW_KOKKOS_VA_RESIDENT=1  every host call first pushes a HOST array to the device, then
//                            copies d_va back over it, so a host write W3WAVE makes without
//                            its ww_va_host_write is overwritten and shows in the output;
//   WW_KOKKOS_VA_COUNT=1     the traffic line and counters at exit (stderr).
//
// SPDX-License-Identifier: MIT
#pragma once

extern "C" {

/// Before a host read of VA: DEVICE -> one device-to-host copy, state BOTH; else no copy.
/// va is VA(1,0) of grid igrid, nspec = SIZE(VA,1), nsealm = UBOUND(VA,2). Returns a
/// WwKokkosStatus: BAD_SHAPE when va, nspec or nsealm differ from the grid's first call.
int ww_va_host_read(int igrid, float* va, int nspec, int nsealm);

/// Before a host write of VA: ww_va_host_read, then state HOST.
int ww_va_host_write(int igrid, float* va, int nspec, int nsealm);

/// Inside a kernel shim, before its launch reads d_va of a bound grid: HOST -> one
/// host-to-device copy, state BOTH; write = 1 then sets DEVICE (the launch follows).
int ww_va_device(int igrid, int write);

/// The traffic line and counters, to stderr. Printed at exit under WW_KOKKOS_VA_COUNT=1.
void ww_va_report(void);

/// ww_srce_batch() on the resident spectrum: the spectra of the npts active points are the
/// columns jsea[0..npts) (W3WAVE's JSEA, 1..NSEAL) of VA(NSPEC,0:NSEALM), read and written
/// in d_va (ww::resident::device_va syncs it first; state DEVICE after the launch), so VA
/// is not packed and does not cross for the source terms. va, nspec, nsealm as for
/// ww_va_host_read; every other argument as ww_srce_batch's, without spec. Status in
/// ww_srce_last_error().
void ww_srce_batch_va(int igrid, int it, int inflags24, int flogrd57, int flogrd58, float dtg,
                      float* va, int nspec, int nsealm, int npts, const int* jsea,
                      const float* wn1, const float* cg1, const float* pin, float* psc,
                      float* whitecap, float* taubbl, float* tauice, int* shave);

}  // extern "C"
