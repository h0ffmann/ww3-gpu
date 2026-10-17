// kokkos/src/ww_kokkos/resident_state.hpp
// The resident spectrum of WFIP-0005: VA(NSPEC,0:NSEALM) of one WW3 grid kept in a device
// View for the whole run, beside the Fortran array it mirrors, and which of the two holds
// the newer bits. The protocol is Kokkos::DualView's (modify/sync), spelt out here because
// the host side is WW3's own allocation, which the Fortran keeps owning (WFIP-0005 §5.2).
//
// The shim (fortran_iface/resident_shim.cpp) keeps one VaState per grid and is the only
// code that moves its bits; a kernel shim asks it for the device View through
// ww::resident::device_va(), which syncs first. Copies are deep_copy of the whole array,
// which moves bits and computes nothing, so on every backend a copy cannot change a value.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::resident {

/// Which copy holds the newer bits: HOST (the Fortran array), DEVICE (d_va), BOTH (equal).
enum class Side { Host, Device, Both };

using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
/// VA(ISPEC,JSEA) at d_va(ISPEC-1, JSEA): the Fortran's layout, column 0 the sink.
using DevVa = Kokkos::View<float**, Kokkos::LayoutLeft, DeviceSpace>;
using HostVa = Kokkos::View<float**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                            Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// d_va of grid `igrid`, holding the newer bits: binds `va` (nspec x (nsealm+1)) on the
/// grid's first call, refuses another address or shape (WW_KOKKOS_ERR_BAD_SHAPE), copies
/// host to device when the state is HOST, and leaves it BOTH. Returns a status of
/// ww_kokkos_c.hpp. Not thread-safe: called from outside OpenMP parallel regions only.
int device_va(int igrid, float* va, int nspec, int nsealm, DevVa& out);

/// After a kernel wrote d_va of `igrid`: the state becomes DEVICE.
void device_written(int igrid);

}  // namespace ww::resident
