// kokkos/src/ww_kokkos/st4_tables_build.hpp
// INSIN4 and the TABU_* tables of ST4, translated as host code: they run once per
// grid at initialisation, so they are not kernels. Selected at run time by
// WW_KOKKOS_ST4_TABLES=1; otherwise the shim uses the tables the Fortran built
// and registered, which have the same bits by construction.
//
// WW3 heritage: WW3/model/src/w3src4md.F90 at WW3@bb08623a: INSIN4 :1014-1288,
// TABU_STRESS :1290-1425, TABU_TAUHF :1427-1596, TABU_TAUHF2 :1598-1831.
//
// Each function writes what its Fortran routine writes: the tables into `a`
// (allocated beforehand by allocate_tables(), st4_arrays.hpp, with the
// Fortran's shapes and conditions) and the W3SRC4MD module scalars (DELU,
// DELTAUW, DELUST, DELALP, DELTAIL) into `p`. `a` holds the registered grid
// arrays (SIG, DSIP, TH, ECOS, ...) on the host, which INSIN4 reads.
#pragma once

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

/// TABU_STRESS: TAUT, DELU, DELTAUW (w3src4md.F90:1290-1425).
void tabu_stress(Params& p, HostArrays& a);

/// TABU_TAUHF(SIGMAX): TAUHFT, DELUST, DELALP (w3src4md.F90:1427-1596).
void tabu_tauhf(Params& p, Real sigmax, HostArrays& a);

/// TABU_TAUHF2(SIGMAX): TAUHFT2, DELUST, DELALP, DELTAIL (w3src4md.F90:1598-1831).
/// The cache file ST4TABUHF2.bin (unit 993) is neither read nor written: the
/// table is always computed.
void tabu_tauhf2(Params& p, Real sigmax, HostArrays& a);

/// INSIN4(FLTABS): SATINDICES, SATWEIGHTS, IKTAB, DCKI, QBI, CUMULW and, when
/// FLTABS, the TABU_* tables (w3src4md.F90:1014-1288).
void insin4(Params& p, bool fltabs, HostArrays& a);

}  // namespace ww::st4
