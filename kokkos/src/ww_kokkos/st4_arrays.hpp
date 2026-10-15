// kokkos/src/ww_kokkos/st4_arrays.hpp
// The arrays WAVEWATCH III's ST4 package reads: the spectral grid, the INSIN4
// tables, W3SRC4MD's stress tables and CONSTANTS' FWTABLE, as Views.
//
// WW3 heritage: w3gdatmd.F90 (SGRD :796-806, SRCP :903-905, the ST4 allocations
// :2089-2107), w3src4md.F90 (TAUT, TAUHFT, TAUHFT2 :63, allocated :1119-1126) and
// constants.F90:93 (FWTABLE), at WW3@bb08623a.
//
// Indexing rule, the one every kernel relies on: a View's index 0 is the Fortran
// array's lower bound. SIG(IK) is f.sig(IK), because SIG is SIG(0:MK+1); ESIN(IS)
// is f.esin(IS-1), because ESIN starts at 1. The lower bound of every array is
// in the table below, and ww_st4_set_real/int() refuse a registration whose
// LBOUND differs, so the rule cannot drift from the Fortran silently.
//
// Scope. Grid arrays live in W3GDATMD's per-grid structures (SGRDS(IMOD),
// MPARS(IMOD)%SRCPS) and the shim keeps them per grid. TAUT, TAUHFT, TAUHFT2,
// FWTABLE and the DEL* scalars are module variables: one copy per process, which
// the last INSIN4/W3IOGR to run overwrites for every grid. The shim keeps one
// copy too, and the last registration wins, as in the Fortran.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

// The Kokkos::Serial path for a caller already inside an OpenMP parallel region
// (W3SRCE under W3_OMPG, w3wavemd.F90:2240-2243); host builds only, as for
// W3SNL1 (snl1_config.hpp, WW_SNL1_SERIAL_NESTED).
#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_ST4_SERIAL_NESTED 1
#endif

namespace ww::st4 {

using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;

/// Array identifiers of the C ABI (ww_st4_set_real, ww_st4_set_int).
enum ArrayId : int {
  A_SIG = 0,     // SIG(0:MK+1)            real  grid
  A_SIG2,        // SIG2(1:MSPEC)          real  grid
  A_DSIP,        // DSIP(0:MK+1)           real  grid
  A_DDEN,        // DDEN(1:MK)             real  grid
  A_DDEN2,       // DDEN2(1:MSPEC)         real  grid
  A_ESIN,        // ESIN(1:MSPEC+MTH)      real  grid
  A_ECOS,        // ECOS(1:MSPEC+MTH)      real  grid
  A_TH,          // TH(1:MTH)              real  grid
  A_SATWEIGHTS,  // SATWEIGHTS(1:2*SDSNTH+1,1:MTH)  real grid
  A_CUMULW,      // CUMULW(1:MSPEC,1:MSPEC)         real grid
  A_DCKI,        // DCKI(1:NKHS,1:NKD)              real grid
  A_QBI,         // QBI(1:NKHS,1:NKD)               real grid
  A_SATINDICES,  // SATINDICES(1:2*SDSNTH+1,1:MTH)  int  grid
  A_IKTAB,       // IKTAB(1:MK,1:NDTAB)             int  grid
  A_TAUT,        // TAUT(0:ITAUMAX,0:JUMAX)         real module, when SINTAILPAR(1) > 0.5
  A_TAUHFT,      // TAUHFT(0:IUSTAR,0:IALPHA)       real module, when SINTAILPAR(1) > 0.5
  A_TAUHFT2,     // TAUHFT2(0:IUSTAR,0:IALPHA,0:ILEVTAIL) real module, and TTAUWSHELTER > 0
  A_FWTABLE,     // FWTABLE(0:SIZEFWTABLE)          real module (CONSTANTS)
  NARRAYS
};

struct ArraySpec {
  const char* name;
  bool is_int;
  int rank;
  int lbound;  ///< the same for every dimension of every ST4 array
  bool module; ///< one copy per process, last registration wins
};

inline constexpr ArraySpec kArraySpecs[NARRAYS] = {
    {"SIG", false, 1, 0, false},        {"SIG2", false, 1, 1, false},
    {"DSIP", false, 1, 0, false},       {"DDEN", false, 1, 1, false},
    {"DDEN2", false, 1, 1, false},      {"ESIN", false, 1, 1, false},
    {"ECOS", false, 1, 1, false},       {"TH", false, 1, 1, false},
    {"SATWEIGHTS", false, 2, 1, false}, {"CUMULW", false, 2, 1, false},
    {"DCKI", false, 2, 1, false},       {"QBI", false, 2, 1, false},
    {"SATINDICES", true, 2, 1, false},  {"IKTAB", true, 2, 1, false},
    {"TAUT", false, 2, 0, true},        {"TAUHFT", false, 2, 0, true},
    {"TAUHFT2", false, 3, 0, true},     {"FWTABLE", false, 1, 0, true},
};

/// Every array ST4 reads, in one memory space. Fields (device) is what a kernel
/// captures; HostArrays is what the shim registers and the table builders of
/// st4_tables_build.hpp fill. An array the Fortran does not allocate for this
/// configuration (TAUHFT2 with TTAUWSHELTER <= 0) is an empty View.
template <class Space>
struct ArraysT {
  template <class T>
  using V1 = Kokkos::View<T*, Kokkos::LayoutLeft, Space>;
  template <class T>
  using V2 = Kokkos::View<T**, Kokkos::LayoutLeft, Space>;
  template <class T>
  using V3 = Kokkos::View<T***, Kokkos::LayoutLeft, Space>;

  V1<Real> sig, sig2, dsip, dden, dden2, esin, ecos, th;
  V2<Real> satweights, cumulw, dcki, qbi;
  V2<int> satindices, iktab;
  V2<Real> taut, tauhft;
  V3<Real> tauhft2;
  V1<Real> fwtable;
};

using Fields = ArraysT<DeviceSpace>;
using HostArrays = ArraysT<Kokkos::HostSpace>;

/// Allocate, in `a`, the tables INSIN4 and TABU_* write, with the shapes and
/// under the conditions of the Fortran (w3gdatmd.F90:2089-2107,
/// w3src4md.F90:1118-1127). Zero-filled, as W3DIMX fills them; SATINDICES is
/// filled with 1 (w3gdatmd.F90:2104).
inline void allocate_tables(const Params& p, HostArrays& a) {
  const int sdsnth = p.mth / 2 - 1;
  a.iktab = HostArrays::V2<int>("ww_st4.IKTAB", p.mk, kNdtab);
  a.dcki = HostArrays::V2<Real>("ww_st4.DCKI", kNkhs, kNkd);
  a.qbi = HostArrays::V2<Real>("ww_st4.QBI", kNkhs, kNkd);
  a.satindices = HostArrays::V2<int>("ww_st4.SATINDICES", 2 * sdsnth + 1, p.mth);
  Kokkos::deep_copy(a.satindices, 1);
  a.satweights = HostArrays::V2<Real>("ww_st4.SATWEIGHTS", 2 * sdsnth + 1, p.mth);
  a.cumulw = HostArrays::V2<Real>("ww_st4.CUMULW", p.mspec, p.mspec);
  if (p.sintailpar[0] > static_cast<Real>(0.5)) {
    a.taut = HostArrays::V2<Real>("ww_st4.TAUT", kItauMax + 1, kJuMax + 1);
    a.tauhft = HostArrays::V2<Real>("ww_st4.TAUHFT", kIustar + 1, kIalpha + 1);
    if (p.ttauwshelter > 0)
      a.tauhft2 = HostArrays::V3<Real>("ww_st4.TAUHFT2", kIustar + 1, kIalpha + 1, kIlevTail + 1);
  }
}

}  // namespace ww::st4
