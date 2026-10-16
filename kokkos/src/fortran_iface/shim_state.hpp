// kokkos/src/fortran_iface/shim_state.hpp
// The per-grid state one shim reads from another: the Kokkos W3SRCE
// (srce_shim.cpp) calls the DIA and ST4 device code with the tables that
// ww_snl1_init() and ww_st4_commit() built for the same grid. Internal to the
// library, not part of the C ABI.
#pragma once

#include "ww_kokkos/snl1_config.hpp"
#include "ww_kokkos/snl1_tables.hpp"
#include "ww_kokkos/st4_arrays.hpp"
#include "ww_kokkos/st4_params.hpp"

namespace ww::snl1 {
/// Grid `igrid`'s configuration, tables and SIG(1:NK) on the device, as
/// ww_snl1_init() left them. False when that grid has none.
bool grid_state(int igrid, Config& c, Tables& t, RealView1D& sig);
}  // namespace ww::snl1

namespace ww::st4 {
/// Grid `igrid`'s Params and Fields with the module state merged in, as
/// ww_sds4() and the other ST4 entry points use them. False before
/// ww_st4_commit() for that grid.
bool grid_state(int igrid, Params& p, Fields& f);
}  // namespace ww::st4
