// kokkos/src/ww_kokkos/prop_mapt.hpp
// W3MAPT, ported to Kokkos: the PR3 map MAPTRN that switches the garden-sprinkler
// averaging off at a cell whose least transparent side is below TRNMIN.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:527-625 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Called from W3WAVE after W3MAP3 and W3UTRN
// (w3wavemd.F90:1467 in the fork). Under switch_lab_shrd only W3_S (STRACE) is
// not compiled; the rest is the loop of :604-617. The comment of :607-613 says the
// first operand was changed in 2012 from ATRNY(IXY,-1) to ATRNX(IXY,-1); the port
// follows the code, not the old line.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3pro3md.F90 (WW3's own flags, switch_lab_shrd; object identical to the CMake
// one): no floating-point arithmetic, three MIN_EXPR and one compare.
//   :615-616  GIMPLE MIN(MIN(ATRNY(+1),ATRNY(-1)), MIN(ATRNX(+1),ATRNX(-1))), emitted
//             as minss ATRNY(-1) into ATRNY(+1), minss ATRNX(-1) into ATRNX(+1),
//             then minss X into Y (relocations of __w3pro3md_MOD_w3mapt: r11 is
//             ATRNX, r9 is ATRNY). minss d,s is d<s?d:s, which is min_r(d, s), so a
//             NaN or a signed-zero tie gives the second operand, as in the object.
//   :616      TRNMIN = 0.95 (w3pro3md.F90:129), folded to 0x3f733333; the compare is
//             comiss/seta (0.95 > M), false for a NaN M, as M < kTrnMin is.
// MAPTRN is LOGICAL(4) written as 0 or 1 (seta into a zeroed register), here an
// int, the convention of prop_params.hpp (Geometry::maptrn).
//
// Why the parallel pass gives the Fortran's bits: MAPSF(ISEA,3) = IY+(IX-1)*NY
// (w3gridmd.F90:5184) is one cell per sea point, so each ISEA writes one MAPTRN
// element no other ISEA writes, and reads only ATRNX/ATRNY, which W3UTRN finished.
// No reduction, no accumulation across points. Cells that are not sea points keep
// what MAPTRN held, as in the Fortran.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"  // min_r

namespace ww::prop {

namespace mapt_detail {
// TRNMIN = 0.95 (w3pro3md.F90:129), a REAL PARAMETER; .rodata.cst4 0x3f733333.
inline constexpr Real kTrnMin = 0x1.e66666p-1f;
}  // namespace mapt_detail

/// :605-616 for sea point ISEA (1-based). Layouts as updt_utrn.hpp (all LayoutLeft):
///   MAPSF(ISEA,J)               -> mapsf(ISEA-1, J-1), extent (>=NSEA, 3)
///   ATRNX/ATRNY(IXY,K), K=-1:1  -> atrnx/atrny(IXY-1, K+1), (NY*NX, 3)
///   MAPTRN(IXY)                 -> maptrn(IXY-1), extent NY*NX, 0 or 1
template <class I2, class R2, class W1>
KOKKOS_INLINE_FUNCTION void w3mapt_point(int isea, const I2& mapsf, const R2& atrnx,
                                         const R2& atrny, const W1& maptrn) {
  using ww::st4::min_r;
  const int ixy = mapsf(isea - 1, 2);
  const Real my = min_r(atrny(ixy - 1, 2), atrny(ixy - 1, 0));
  const Real mx = min_r(atrnx(ixy - 1, 2), atrnx(ixy - 1, 0));
  const Real m = min_r(my, mx);
  maptrn(ixy - 1) = (m < mapt_detail::kTrnMin) ? 1 : 0;
}

/// W3MAPT: fills maptrn at the NSEA sea cells. ATRNX/ATRNY are W3UTRN's output
/// (ww::updt::UtrnViews::atrnx/atrny, non-const there; a const view converts).
template <class ExecSpace>
void w3mapt(const ExecSpace& exec, int nsea,
            const Kokkos::View<const int**, Kokkos::LayoutLeft,
                               typename ExecSpace::memory_space>& mapsf,
            const Kokkos::View<const Real**, Kokkos::LayoutLeft,
                               typename ExecSpace::memory_space>& atrnx,
            const Kokkos::View<const Real**, Kokkos::LayoutLeft,
                               typename ExecSpace::memory_space>& atrny,
            const Kokkos::View<int*, Kokkos::LayoutLeft, typename ExecSpace::memory_space>&
                maptrn) {
  if (nsea <= 0) return;  // :604, DO ISEA=1, NSEA
  Kokkos::parallel_for(
      "w3mapt", Kokkos::RangePolicy<ExecSpace>(exec, 1, nsea + 1),
      KOKKOS_LAMBDA(const int isea) { w3mapt_point(isea, mapsf, atrnx, atrny, maptrn); });
  exec.fence();
}

}  // namespace ww::prop
