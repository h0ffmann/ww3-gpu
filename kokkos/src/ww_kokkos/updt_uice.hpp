// kokkos/src/ww_kokkos/updt_uice.hpp
// W3UICE, ported to Kokkos: the ice concentration and iceberg damping of each
// sea point from the input fields and, under IC0, the masking of points that
// freeze (MAPSTA < 0, spectrum and Charnock reset) and the unmasking of points
// that thaw. Phase 1, translated, not wired, not regtested.
//
// WW3 heritage: WW3/model/src/w3updtmd.F90:1806-2052 at h0ffmann/WW3 96892c95
// (byte-identical to NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD --
// model/src/w3updtmd.F90 is empty); author H. L. Tolman. Called from W3WAVE
// (w3wavemd.F90:1392-1398 at the pin, where the fork's W3KOKKOS_UICE (:1392) already
// repeats the VA(:,JSEA) = 0. columns on a resident VA, PATCH.md section 18).
// Evidence: kokkos/port_report/W3UICE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Only the switch_lab_shrd branch is ported: IC0 (:1937-1940, :1955-2028,
// :2034-2036) and ST4, so CHARN(JSEA) = AALPHA (:1967, :2003). Not compiled there
// and not ported: the non-ST3/ST4 CHARN = 0. branches (:1969, :2005), T (test
// prints only), S. Without PDLIB, INIT_GET_JSEA_ISPROC is its round-robin branch
// (w3parall.F90:1236-1237, parall_init_get_jsea_isproc.hpp). TICE = TIN
// (:1932-1933) is integer time on the host.
//
// Arithmetic: none in floating point. Checked against gfortran 16.2 -O3 GIMPLE
// and the object of w3updtmd.F90 (WW3's own flags, switch_lab_shrd; the
// recompiled object's disassembly equals the CMake build's), 2026-11-12 (v):
// - :1956 ICEI(IX,IY) >= FICEN and :1987 ICEI(IX,IY) < FICEN are ordered
//   compares: a NaN ICEI takes neither branch, as with C++ >= and <.
// - :1938 MAPICE = MOD(MAPST2,2) is the one vectorised loop of the routine
//   (integer, truncating remainder, as C++ %); :1965, :2001 VA(:,JSEA) = 0. is a
//   memset (+0.0 bits); :1990 UST = 0.05 is the REAL 0x3d4ccccd.
// - ABS of MAPSTA is integer abs; MAPSTA values are small (no INT_MIN).
#pragma once

#include <Kokkos_Core.hpp>

#include "parall_init_get_jsea_isproc.hpp"
#include "real.hpp"

namespace ww::updt {

/// The scalars of W3UICE: FICEN (w3gdatmd.F90:670, per grid), AALPHA (ST4,
/// w3gdatmd.F90:1328), NAPROC and IAPROC (W3ODATMD, used at w3updtmd.F90:148).
struct UiceScalars {
  Real ficen = 0;
  Real aalpha = 0;
  int naproc = 1;
  int iaproc = 1;
};

/// W3UICE(VA) under switch_lab_shrd. tice and tin are TICE(2) and TIN(2) (date,
/// time), host arrays. Module state comes in as Views, LayoutLeft, 0-based:
///   mapsf         MAPSF(MSEA,3), w3gdatmd.F90:1736, Fortran 1-based IX, IY values
///   icei, bergi   ICEI(NX,NY), BERGI(NX,NY), w3idatmd.F90:699-700
///   mapsta, mapst2 MAPSTA(NY,NX), MAPST2(NY,NX), w3gdatmd.F90:1733-1734, INOUT
///   mapice        MAPICE(NY,NX), the Fortran's automatic local (w3updtmd.F90:1910),
///                 caller's work array, fully written before it is read
///   ice           ICE(0:NSEA), w3wdatmd.F90:561, element ISEA
///   berg          BERG(NSEA), w3wdatmd.F90:566, element ISEA-1
///   ust           UST(0:NSEATM), w3wdatmd.F90:570, element ISEA
///   charn         CHARN(NSEALM), w3adatmd.F90:1161, element JSEA-1
///   va            VA(NSPEC,0:NSEALM), w3wdatmd.F90:491, element (ISP-1, JSEA); it is
///                 written only in the columns the Fortran zeroes, so a resident VA
///                 needs no copy
///
/// Why parallel gives the serial bits: on a regular grid MAPSF(ISEA,1:2) is set
/// once per sea cell of the loop over cells (w3gridmd.F90:5010-5028; :5182-5184 is
/// the SMC branch, not compiled), so ISEA -> (IX,IY) is one-to-one and point ISEA reads and
/// writes MAPSTA, MAPST2, MAPICE only at its own cell; it writes ICE, BERG, UST
/// at ISEA, and VA, CHARN at JSEA only when ISPROC = IAPROC, where (JSEA, ISPROC)
/// determines ISEA (w3parall.F90:1236-1237). No reduction, no cross-point read.
/// The three passes run in the Fortran's order on one execution space instance.
template <class ExecSpace>
void w3uice(const ExecSpace& exec, int* tice, const int* tin, const UiceScalars& sc, int nsea,
            Kokkos::View<const int**, Kokkos::LayoutLeft, ExecSpace> mapsf,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> icei,
            Kokkos::View<const Real**, Kokkos::LayoutLeft, ExecSpace> bergi,
            Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace> mapsta,
            Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace> mapst2,
            Kokkos::View<int**, Kokkos::LayoutLeft, ExecSpace> mapice,
            Kokkos::View<Real*, ExecSpace> ice, Kokkos::View<Real*, ExecSpace> berg,
            Kokkos::View<Real*, ExecSpace> ust, Kokkos::View<Real*, ExecSpace> charn,
            Kokkos::View<Real**, Kokkos::LayoutLeft, ExecSpace> va) {
  const bool local = sc.iaproc <= sc.naproc;  // :1919
  tice[0] = tin[0];  // :1932-1933
  tice[1] = tin[1];

  const int ny = static_cast<int>(mapst2.extent(0));
  const int nx = static_cast<int>(mapst2.extent(1));
  const int nspec = static_cast<int>(va.extent(0));
  using Cells = Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2, Kokkos::Iterate::Left>>;

  Kokkos::parallel_for(  // :1938-1939
      "ww::updt::w3uice_mapice", Cells(exec, {0, 0}, {ny, nx}),
      KOKKOS_LAMBDA(const int iy, const int ix) {
        const int m = mapst2(iy, ix) % 2;
        mapice(iy, ix) = m;
        mapst2(iy, ix) = mapst2(iy, ix) - m;
      });

  const Real ficen = sc.ficen;
  const Real aalpha = sc.aalpha;
  const int naproc = sc.naproc;
  const int iaproc = sc.iaproc;
  Kokkos::parallel_for(  // :1944-2030
      "ww::updt::w3uice", Kokkos::RangePolicy<ExecSpace>(exec, 0, nsea),
      KOKKOS_LAMBDA(const int i) {
        const int isea = i + 1;
        const int ix = mapsf(i, 0) - 1;  // :1948-1949
        const int iy = mapsf(i, 1) - 1;
        const Real c = icei(ix, iy);
        ice(isea) = c;  // :1950
        berg(i) = bergi(ix, iy);  // :1951
        int jsea = 0;
        int isproc = 0;
        if (c >= ficen && mapice(iy, ix) == 0) {  // :1956
          const int a = mapsta(iy, ix);
          mapsta(iy, ix) = -(a < 0 ? -a : a);  // :1957
          mapice(iy, ix) = 1;  // :1958
          ww::parall::init_get_jsea_isproc(isea, jsea, isproc, naproc, false, iaproc, nullptr,
                                           nullptr, nullptr);  // :1959
          if (local && iaproc == isproc) {  // :1960
            for (int isp = 0; isp < nspec; ++isp) va(isp, jsea) = 0;  // :1965
            charn(jsea - 1) = aalpha;  // :1967
          }
        }
        if (c < ficen && mapice(iy, ix) == 1) {  // :1987
          mapice(iy, ix) = 0;  // :1989
          ust(isea) = static_cast<Real>(0.05);  // :1990
          if (mapst2(iy, ix) == 0) {  // :1992
            const int a = mapsta(iy, ix);
            mapsta(iy, ix) = a < 0 ? -a : a;  // :1993
            ww::parall::init_get_jsea_isproc(isea, jsea, isproc, naproc, false, iaproc, nullptr,
                                             nullptr, nullptr);  // :1995
            if (local && iaproc == isproc) {  // :1996
              for (int isp = 0; isp < nspec; ++isp) va(isp, jsea) = 0;  // :2001
              charn(jsea - 1) = aalpha;  // :2003
            }
          }
        }
      });

  Kokkos::parallel_for(  // :2035
      "ww::updt::w3uice_mapst2", Cells(exec, {0, 0}, {ny, nx}),
      KOKKOS_LAMBDA(const int iy, const int ix) {
        mapst2(iy, ix) = mapst2(iy, ix) + mapice(iy, ix);
      });
}

}  // namespace ww::updt
