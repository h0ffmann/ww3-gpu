// kokkos/src/ww_kokkos/wave_gath_scat.hpp
// W3GATH and W3SCAT, ported to Kokkos: copy one spectral component ISPEC of the
// sea-point spectra VA into a 2-D field over the grid (W3GATH) and back (W3SCAT),
// around the spatial propagation in W3WAVE. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3wavemd.F90:3024-3329 (W3GATH) and :3345-3644
// (W3SCAT) at NOAA-EMC/WW3 03adda91; author H. L. Tolman. Only the shared-memory
// branch is ported (W3_SHRD, switch_lab_shrd): W3GATH :3158-3166, W3SCAT :3478-3481.
// The W3_MPI buffer management, the W3_MPIT test print and W3_S STRACE are not
// compiled under that switch (no STRACE call in the -O3 GIMPLE) and not ported.
//
// Checked against gfortran 16.2 -O3 GIMPLE and objdump of w3wavemd.o (WW3's flags,
// switch_lab_shrd): FIELD = 0. is one __builtin_memset of the whole extent
// (:3158); both ISEA loops are scalar (not vectorised) and do no float arithmetic,
// only an integer load of MAPSF(ISEA,3), a test of MAPSTA (W3SCAT) and a movss
// load/store of the REAL. A movss load/store does not canonicalise NaN payloads or
// change -0.0, so the copy moves the bits unchanged.
//
// Why parallel_for over ISEA gives the same bits: each iteration is a pure copy,
// and the targets do not collide. W3SCAT writes VA(ISPEC,ISEA), distinct per ISEA.
// W3GATH writes FIELD(MAPSF(ISEA,3)); MAPSF(:,3) = MAPSF(:,2) + (MAPSF(:,1)-1)*NY
// (w3iogrmd.F90:1073) is injective in ISEA: on a rectilinear grid ww3_grid gives
// each sea point (IX,IY) one new ISEA in a single pass over the grid
// (w3gridmd.F90:5020-5028). Grids built elsewhere (SMC, w3gridmd.F90:5181-5184, uses
// W3GATHSMC, w3wavemd.F90:1951; unstructured) are not checked here ⚠.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::wave {

// Index maps (Fortran -> C++), the trap of these routines:
//  FIELD(1-NY:NY*(NX+2)) (:3132, :3447):  FIELD(i)      -> field(i - (1-NY)), extent NY*(NX+3)
//  VA(NSPEC,0:NSEALM)    (w3wdatmd.F90:491): A(ISPEC,ISEA) -> va(ISPEC-1, ISEA). The second
//                        dimension starts at 0, so the column index is ISEA, not ISEA-1;
//                        pass the whole VA, column 0 included.
//  MAPSF(MSEA,3)         (w3gdatmd.F90:1736): MAPSF(ISEA,3) -> mapsf(ISEA-1, 2)
//  MAPSTA(NY*NX) dummy   (:3446), actual MAPSTA(MY,MX) by sequence association
//                        (w3gdatmd.F90:1733): MAPSTA(IXY) -> mapsta(IXY-1)
using FieldView = Kokkos::View<Real*, Kokkos::LayoutLeft>;
using ConstFieldView = Kokkos::View<const Real*, Kokkos::LayoutLeft>;
using VaView = Kokkos::View<Real**, Kokkos::LayoutLeft>;
using ConstVaView = Kokkos::View<const Real**, Kokkos::LayoutLeft>;
using MapsfView = Kokkos::View<const int**, Kokkos::LayoutLeft>;
using MapstaView = Kokkos::View<const int*, Kokkos::LayoutLeft>;

/// :3164-3165 for one ISEA (1-based). ny gives FIELD's lower bound 1-NY.
KOKKOS_INLINE_FUNCTION void w3gath_point(int isea, int ispec, int ny, const ConstVaView& va,
                                         const MapsfView& mapsf, const FieldView& field) {
  // 64-bit offsets as gfortran's (integer(kind=8)) index arithmetic: NSPEC*NSEA can pass 2^31.
  const long ixy = mapsf(isea - 1, 2);
  field(ixy - (1 - long{ny})) = va(long{ispec} - 1, long{isea});
}

/// :3479-3480 for one ISEA (1-based).
KOKKOS_INLINE_FUNCTION void w3scat_point(int isea, int ispec, int ny, const MapstaView& mapsta,
                                         const ConstFieldView& field, const MapsfView& mapsf,
                                         const VaView& va) {
  const long ixy = mapsf(isea - 1, 2);
  if (mapsta(ixy - 1) != 0) va(long{ispec} - 1, long{isea}) = field(ixy - (1 - long{ny}));
}

/// W3GATH(ISPEC, FIELD), W3_SHRD. FIELD is INTENT(OUT): its whole extent is zeroed
/// first (:3158), then the sea points are scattered into it. The fill and the copy
/// run on the same instance with a fence between, so no copy can be overwritten.
template <class ExecSpace>
void w3gath(const ExecSpace& exec, int ispec, int nsea, int ny, const ConstVaView& va,
            const MapsfView& mapsf, const FieldView& field) {
  Kokkos::parallel_for(
      "ww::wave::w3gath_zero", Kokkos::RangePolicy<ExecSpace>(exec, 0, field.extent(0)),
      KOKKOS_LAMBDA(const std::size_t i) { field(i) = Real(0); });
  exec.fence("ww::wave::w3gath zero before scatter");
  Kokkos::parallel_for(
      "ww::wave::w3gath", Kokkos::RangePolicy<ExecSpace>(exec, 1, nsea + 1),
      KOKKOS_LAMBDA(const int isea) { w3gath_point(isea, ispec, ny, va, mapsf, field); });
  exec.fence("ww::wave::w3gath");
}

/// W3SCAT(ISPEC, MAPSTA, FIELD), W3_SHRD: VA(ISPEC,ISEA) = FIELD(MAPSF(ISEA,3)) where
/// MAPSTA of that point is nonzero; other points keep their VA.
template <class ExecSpace>
void w3scat(const ExecSpace& exec, int ispec, int nsea, int ny, const MapstaView& mapsta,
            const ConstFieldView& field, const MapsfView& mapsf, const VaView& va) {
  Kokkos::parallel_for(
      "ww::wave::w3scat", Kokkos::RangePolicy<ExecSpace>(exec, 1, nsea + 1),
      KOKKOS_LAMBDA(const int isea) { w3scat_point(isea, ispec, ny, mapsta, field, mapsf, va); });
  exec.fence("ww::wave::w3scat");
}

}  // namespace ww::wave
