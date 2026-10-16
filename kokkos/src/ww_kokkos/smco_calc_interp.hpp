// kokkos/src/ww_kokkos/smco_calc_interp.hpp
// CALC_INTERP: for each cell of an arbitrary regular output grid, the SMC sea point that contains
// its centre and the east and north distances (m) between the two centres. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:622-696 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/CALC_INTERP_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <type_traits>
#include <vector>

#include "real.hpp"

namespace ww::smco {

namespace calc_interp_detail {
// DERA and RADIUS as constants.F90:72-80 build them; gfortran keeps them apart because
// ((lon-mlon)*DERA)*RADIUS is left to right (:688-689).
inline constexpr Real kTpi = static_cast<Real>(2.0) * kPi;
inline constexpr Real kTpiInv = static_cast<Real>(1.) / kTpi;
inline constexpr Real kDera = kPi / static_cast<Real>(180.);
inline constexpr Real kRadius = static_cast<Real>(4.E7) * kTpiInv;
static_assert(kDera == 0x1.1df46ap-6f, "DERA as in the object, 1.745329238474369049072265625e-2");
static_assert(kRadius == 6.3661975e+6f, "RADIUS as in the object");
inline constexpr Real kHalf = static_cast<Real>(0.5);
inline constexpr Real k360 = static_cast<Real>(360.0);

// 2**(NRLv-1) as the object computes it (:640): 1 << n for 0 <= n <= 31, else 0.
inline int cfac_of(int nrlv) {
  const unsigned n = static_cast<unsigned>(nrlv - 1);
  return n <= 31u ? static_cast<int>(1u << n) : 0;
}
}  // namespace calc_interp_detail

/// CALC_INTERP, host only: it allocates its automatic arrays (:633) and runs once per output
/// setup. Nothing in WW3 calls it (see the port report).
///
/// Module state read: nsea NSEA, nrlv NRLv, sx SX, sy SY, x0 X0, y0 Y0, nx NX, ny NY (w3gdatmd);
/// ijkcel: IJKCel(4,-9:MCel) column-major, pointer to IJKCel(1,-9) (w3gdatmd.F90:1770);
/// clats: CLATS(0:MSEA), pointer to CLATS(0) (w3gdatmd.F90:1741);
/// sxo, syo, dxo, dyo, nxo, nyo: the output grid (w3smcomd.F90:77-84).
/// Module state written: dlat DLAT, dlon DLON (w3smcomd.F90:106-107);
/// nnidx: NNIDX column-major with its allocated extents nnidx_n1 x nnidx_n2, lower bounds 1;
/// xdist: XDIST as :646 allocates it, (NX,NY); ydist: YDIST as :646 allocates it, (NY,NX).
/// The caller performs the ALLOCATE of :646 (and its "already allocated" check).
/// ⚠ :688-689 index XDIST(I,J), YDIST(I,J) with I <= NXO, J <= NYO into those (NX,NY) and
/// (NY,NX) arrays; the index arithmetic is kept, so the caller must size them for it.
///
/// W3_RTD (:665-668): pass lltoeq, a callable with W3LLTOEQ's dummies
/// (const Real* phi, const Real* lambda, Real* phi_eq, Real* lambda_eq, Real* angled,
///  Real& phi_pole, Real& lambda_pole, int points), and polat, polon (PoLat, PoLon, module state;
/// LAMBDA_POLE is INOUT). W3LLTOEQ itself is not part of this port. Without W3_RTD pass nullptr.
/// The PRINT progress lines (:665, :668, :675) are not reproduced.
template <class LlToEq = std::nullptr_t>
inline void calc_interp(int nsea, int nrlv, Real sx, Real sy, Real x0, Real y0, int nx, int ny,
                        const int* ijkcel, const Real* clats, double sxo, double syo, double dxo,
                        double dyo, int nxo, int nyo, Real& dlat, Real& dlon, int* nnidx,
                        long nnidx_n1, long nnidx_n2, Real* xdist, Real* ydist,
                        LlToEq lltoeq = nullptr, Real* polat = nullptr, Real* polon = nullptr) {
  using namespace calc_interp_detail;
  constexpr bool kRtd = !std::is_null_pointer_v<LlToEq>;

  auto ijk = [ijkcel](int k, int isea) {
    return ijkcel[static_cast<long>(k - 1) + 4L * static_cast<long>(isea + 9)];
  };
  const std::size_t nsea_sz = nsea > 0 ? static_cast<std::size_t>(nsea) : 0u;
  const long ld_o = nxo > 0 ? static_cast<long>(nxo) : 0L;
  const std::size_t no_sz =
      (nxo > 0 && nyo > 0) ? static_cast<std::size_t>(nxo) * static_cast<std::size_t>(nyo) : 0u;
  std::vector<Real> mlon(nsea_sz), mlat(nsea_sz), olon(no_sz), olat(no_sz);

  const int cfac = cfac_of(nrlv);

  const Real dlat_v = sy / static_cast<Real>(cfac);
  const Real dlon_v = sx / static_cast<Real>(cfac);
  dlat = dlat_v;
  dlon = dlon_v;

  for (int isea = 1; isea <= nsea; ++isea) {
    mlon[isea - 1] = (x0 - kHalf * sx) +
                     (static_cast<Real>(ijk(1, isea)) + kHalf * static_cast<Real>(ijk(3, isea))) * dlon_v;
    mlat[isea - 1] = (y0 - kHalf * sy) +
                     (static_cast<Real>(ijk(2, isea)) + kHalf * static_cast<Real>(ijk(4, isea))) * dlat_v;
  }

  for (int i = 1; i <= nxo; ++i) {
    for (int j = 1; j <= nyo; ++j) {
      const long o = static_cast<long>(i - 1) + ld_o * static_cast<long>(j - 1);
      olon[static_cast<std::size_t>(o)] = static_cast<Real>(sxo + static_cast<double>(i - 1) * dxo);
      olat[static_cast<std::size_t>(o)] = static_cast<Real>(syo + static_cast<double>(j - 1) * dyo);
    }
  }

  if constexpr (kRtd) {
    std::vector<Real> tmplon(olon), tmplat(olat), ang(no_sz);
    lltoeq(tmplat.data(), tmplon.data(), olat.data(), olon.data(), ang.data(), *polat, *polon,
           nxo * nyo);
  }

  for (long jj = 0; jj < nnidx_n2; ++jj) {
    for (long ii = 0; ii < nnidx_n1; ++ii) nnidx[ii + nnidx_n1 * jj] = -1;
  }

  for (int i = 1; i <= nxo; ++i) {
    for (int j = 1; j <= nyo; ++j) {
      const long o = static_cast<long>(i - 1) + ld_o * static_cast<long>(j - 1);
      Real lon = olon[static_cast<std::size_t>(o)];
      const Real lat = olat[static_cast<std::size_t>(o)];
      if (lon < x0 - sx / static_cast<Real>(2)) lon = lon + k360;
      if (lon > (x0 + static_cast<Real>(nx - 1) * sx) + kHalf * sx) lon = lon - k360;
      for (int isea = 1; isea <= nsea; ++isea) {
        const Real ml = mlon[isea - 1];
        const Real mp = mlat[isea - 1];
        const Real hx = kHalf * static_cast<Real>(ijk(3, isea)) * dlon_v;
        const Real hy = kHalf * static_cast<Real>(ijk(4, isea)) * dlat_v;
        if (ml - hx <= lon && ml + hx >= lon && mp - hy <= lat && mp + hy >= lat) {
          const long ci = static_cast<long>(i - 1);
          const long cj = static_cast<long>(j - 1);
          nnidx[ci + nnidx_n1 * cj] = isea;
          xdist[ci + static_cast<long>(nx) * cj] = (lon - ml) * kDera * kRadius * clats[isea];
          ydist[ci + static_cast<long>(ny) * cj] = (lat - mp) * kDera * kRadius;
          break;
        }
      }
    }
  }
}

}  // namespace ww::smco
