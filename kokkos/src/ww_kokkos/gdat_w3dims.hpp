// kokkos/src/ww_kokkos/gdat_w3dims.hpp
// W3DIMS, ported to Kokkos: set the spectral dimensions MK, MTH of model IMOD and
// allocate its spectral arrays (SGRDS(IMOD)) and, under W3_ST4, the dissipation
// tables of MPARS(IMOD)%SRCPS. Phase 1, translated, not wired into WW3, not regtested.
//
// WW3 heritage: WW3/model/src/w3gdatmd.F90:1929-2154 at h0ffmann/WW3 8e433f99;
// author H. L. Tolman. Ported as switches/switch_lab_shrd compiles it: the W3_ST4
// block :2088-2106 is in; W3_S (STRACE, :2002-2004, :2021-2024) and the TEST_W3GDATMD writes
// (:2051-2053, :2109-2111, :2117-2119, :2128-2130) are not compiled and not ported.
//
// Checked against gfortran 16.2 -O3 GIMPLE of w3gdatmd.F90 (WW3's Release flags,
// switch_lab_shrd): W3DIMS does no floating-point arithmetic. The fills are scalar
// stores of the constant 0.0 (bits 0x00000000) into REAL arrays and 0 or 1 into
// INTEGER ones; the REAL literals 0. and 1. assigned to INTEGER arrays fold to 0 and 1
// at compile time. RADE is USEd (:2000) but only in a commented-out expression (:2097).
// SDSNTH is MTH/2-1 in integer division; GIMPLE builds the extent as (MTH/2)*2-1,
// the same integer.
//
// Errors: EXTCDE(n) stops the Fortran program. Here the same n is returned and the
// caller (the shim) writes the message and stops: a host routine of the port never
// prints. Codes: 1 grids not initialised (:2029-2032), 2 illegal IMOD (:2034-2037),
// 3 illegal MK/MTH (:2039-2042), 4 already allocated (:2044-2047), 99 ALLOCATE
// failed (CHECK_ALLOC_STATUS, w3macros.h:49-51, at :2073, :2093, :2102).
#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

#include "gdat_sgrd.hpp"
#include "real.hpp"

namespace ww::gdat {

inline constexpr int kW3dimsOk = 0;
inline constexpr int kW3dimsNoGrids = 1;
inline constexpr int kW3dimsBadImod = 2;
inline constexpr int kW3dimsBadDims = 3;
inline constexpr int kW3dimsAllocated = 4;
inline constexpr int kW3dimsAllocFailed = 99;

namespace detail {
// ALLOCATE leaves the memory undefined and W3DIMS fills it explicitly; allocate
// without Kokkos' initialisation and fill as the Fortran does, so the order and the
// values are the Fortran's, not Kokkos' default.
template <class View, class... Ext>
View alloc_noinit(const char* label, Ext... ext) {
  return View(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(label)),
              static_cast<std::size_t>(ext)...);
}
}  // namespace detail

/// W3DIMS(IMOD, MK, MTH, NDSE, NDST). gm is the module state (NGRIDS, NAUXGR read;
/// IGRID, ISGRD, IPARS written by the W3SETG call at :2116, as :2278-2280 do); sg and
/// sp are SGRDS(IMOD) and MPARS(IMOD)%SRCPS of this IMOD. Returns 0 or EXTCDE's code.
/// Host-only: allocation and the fills are launched from the host, outside any kernel.
template <class ExecSpace>
int w3dims(const ExecSpace& exec, int imod, int mk, int mth, GdatModule& gm,
           Sgrd<typename ExecSpace::memory_space>& sg,
           Srcps<typename ExecSpace::memory_space>& sp) {
  using S = Sgrd<typename ExecSpace::memory_space>;
  using P = Srcps<typename ExecSpace::memory_space>;
  using IntV = typename S::IntV;
  using RealV = typename S::RealV;
  using IntM = typename P::IntM;
  using RealM = typename P::RealM;

  // 1. Test input and module status, in the Fortran's order (:2029-2047).
  if (gm.ngrids == -1) return kW3dimsNoGrids;
  if (imod < -gm.nauxgr || imod > gm.ngrids) return kW3dimsBadImod;
  if (mk < 3 || mth < 4) return kW3dimsBadDims;
  if (sg.sinit) return kW3dimsAllocated;

  // :2049-2050. MK2 is a SAVE local nothing reads (GIMPLE drops it); MSPEC is INTEGER
  // and overflows for MK*MTH > 2^31-1 in both languages ⚠ (no grid comes close).
  const int mspec = mk * mth;

  try {
    // 2. Allocate (:2058-2072), in the ALLOCATE list's order. On a failed allocation
    // the Fortran stops with the arrays before it allocated; so does this.
    sg.mapwn = detail::alloc_noinit<IntV>("MAPWN", mspec + mth);
    sg.mapth = detail::alloc_noinit<IntV>("MAPTH", mspec + mth);
    sg.th = detail::alloc_noinit<RealV>("TH", mth);
    sg.esin = detail::alloc_noinit<RealV>("ESIN", mspec + mth);
    sg.ecos = detail::alloc_noinit<RealV>("ECOS", mspec + mth);
    sg.es2 = detail::alloc_noinit<RealV>("ES2", mspec + mth);
    sg.esc = detail::alloc_noinit<RealV>("ESC", mspec + mth);
    sg.ec2 = detail::alloc_noinit<RealV>("EC2", mspec + mth);
    sg.sig = detail::alloc_noinit<RealV>("SIG", mk + 2);    // SIG(0:MK+1)
    sg.sig2 = detail::alloc_noinit<RealV>("SIG2", mspec);
    sg.dsip = detail::alloc_noinit<RealV>("DSIP", mk + 2);  // DSIP(0:MK+1)
    sg.dsii = detail::alloc_noinit<RealV>("DSII", mk);
    sg.dden = detail::alloc_noinit<RealV>("DDEN", mk);
    sg.dden2 = detail::alloc_noinit<RealV>("DDEN2", mspec);
    // :2074-2087
    Kokkos::deep_copy(exec, sg.mapwn, 0);
    Kokkos::deep_copy(exec, sg.mapth, 0);
    Kokkos::deep_copy(exec, sg.th, Real(0));
    Kokkos::deep_copy(exec, sg.esin, Real(0));
    Kokkos::deep_copy(exec, sg.ecos, Real(0));
    Kokkos::deep_copy(exec, sg.es2, Real(0));
    Kokkos::deep_copy(exec, sg.esc, Real(0));
    Kokkos::deep_copy(exec, sg.ec2, Real(0));
    Kokkos::deep_copy(exec, sg.sig, Real(0));
    Kokkos::deep_copy(exec, sg.sig2, Real(0));
    Kokkos::deep_copy(exec, sg.dsip, Real(0));
    Kokkos::deep_copy(exec, sg.dsii, Real(0));
    Kokkos::deep_copy(exec, sg.dden, Real(0));
    Kokkos::deep_copy(exec, sg.dden2, Real(0));

    // W3_ST4 (:2088-2106).
    sp.iktab = detail::alloc_noinit<IntM>("IKTAB", mk, kNdtab);
    sp.dcki = detail::alloc_noinit<RealM>("DCKI", kNkhs, kNkd);
    sp.qbi = detail::alloc_noinit<RealM>("QBI", kNkhs, kNkd);
    Kokkos::deep_copy(exec, sp.iktab, 0);
    Kokkos::deep_copy(exec, sp.dcki, Real(0));
    Kokkos::deep_copy(exec, sp.qbi, Real(0));
    const int sdsnth = mth / 2 - 1;  // :2097; mth >= 4, so sdsnth >= 1
    sp.satindices = detail::alloc_noinit<IntM>("SATINDICES", 2 * sdsnth + 1, mth);
    sp.satweights = detail::alloc_noinit<RealM>("SATWEIGHTS", 2 * sdsnth + 1, mth);
    // CUMULW(MSPEC,MSPEC): the byte count is 64-bit in gfortran (MSPEC widened to
    // kind=8 before the product) and in Kokkos (size_t extents).
    sp.cumulw = detail::alloc_noinit<RealM>("CUMULW", mspec, mspec);
    Kokkos::deep_copy(exec, sp.satindices, 1);
    Kokkos::deep_copy(exec, sp.satweights, Real(0));
    Kokkos::deep_copy(exec, sp.cumulw, Real(0));
    exec.fence("ww::gdat::w3dims fills");
  } catch (const std::bad_alloc&) {
    return kW3dimsAllocFailed;
  } catch (const std::runtime_error&) {
    // Kokkos 5.2 reports a failed allocation as Kokkos::Experimental::BadAlloc, which
    // derives from std::runtime_error (impl/Kokkos_Error.hpp).
    return kW3dimsAllocFailed;
  }

  sg.sinit = true;  // :2108, before W3SETG, which tests SINIT (:2518)

  // 3. W3SETG(IMOD) (:2116). Its checks (:2262-2270) repeat the ones above and pass;
  // its pointer associations are the caller's choice of sg and sp; what is left is
  // the selection of IMOD (:2278-2280).
  gm.igrid = imod;
  gm.isgrd = imod;
  gm.ipars = imod;

  // 4. Update counters through W3SETG's pointers, i.e. SGRDS(IMOD) (:2124-2127).
  sg.nk = mk;
  sg.nk2 = mk + 2;
  sg.nth = mth;
  sg.nspec = mk * mth;
  return kW3dimsOk;
}

}  // namespace ww::gdat
