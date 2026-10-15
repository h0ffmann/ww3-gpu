// kokkos/src/ww_kokkos/st4_calc_ustar.hpp
// CALC_USTAR, ported to Kokkos (phase 1, translation). NOT YET TRANSLATED: this stub
// fixes the interface the shim (fortran_iface/st4_shim.cpp) calls; its body is
// written by the translation of CALC_USTAR.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1849-2011 at WW3@bb08623a.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

/// CALC_USTAR(WINDSPEED, TAUW, USTAR, Z0, CHARN): friction velocity, roughness
/// and Charnock coefficient from the 10 m wind and the wave-supported stress.
/// Called by w3spr4_point (w3src4md.F90:361).
KOKKOS_INLINE_FUNCTION void calc_ustar(const Params& p, const Fields& f, Real windspeed,
                                       Real tauw, Real& ustar, Real& z0, Real& charn) {
  (void)p; (void)f; (void)windspeed; (void)tauw; (void)ustar; (void)z0; (void)charn;
}

}  // namespace ww::st4
