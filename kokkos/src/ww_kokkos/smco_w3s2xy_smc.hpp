// kokkos/src/ww_kokkos/smco_w3s2xy_smc.hpp
// W3S2XY_SMC: put a field on the SMC sea points onto the output grid of ww3_ounf, by the
// SMC output type (sea-point array, area-weighted regridding, nearest neighbour with or without
// gradient correction). Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3smcomd.F90:816-863 at NOAA-EMC/WW3 03adda91;
// author C. Bunney. Evidence: kokkos/port_report/W3S2XY_SMC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "smco_w3s2xy_smcnn.hpp"
#include "smco_w3s2xy_smcnn_int.hpp"
#include "smco_w3s2xy_smcrg.hpp"
#include "smco_w3s2xy_smcrg_dir.hpp"

namespace ww::smco {

namespace s2xy_smc_detail {
/// Returned where the Fortran writes "Uknonwn SMC type!" SMCOTYPE to unit * and STOPs
/// (:857-860): the port neither prints nor stops, the shim does both. XY is not written.
inline constexpr int kUnknownType = 1;
}  // namespace s2xy_smc_detail

/// W3S2XY_SMC, :816-863, host only (type 3 calls SMCGradn through w3s2xy_smcnn_int).
/// Returns 0, or kUnknownType when smcotype is not 1 to 4.
/// s: S(:) 1-based in Fortran, contiguous here (the shim packs a strided section), size_s
/// elements (SIZE(S)). xy: XY(NXO,NYO) column-major, 0-based. dir: the OPTIONAL DIR, nullptr
/// when absent (DIRN = .FALSE., :827-831).
/// Module state (w3smcomd.F90:82-119): smcotype SMCOTYPE; nxo, nyo NXO, NYO; smcmask
/// SMCMASK(NSEA), LOGICAL(4), 0-based; undef UNDEF (constants.F90:85, a variable); noval
/// NOVAL; nsmc, smcidx, xidx, yidx, xspan, yspan, wts, cov, mapsmc and nnidx, xdist, ydist
/// as in w3s2xy_smcrg and w3s2xy_smcnn_int. Work: aux1, aux2 (NXO*NYO) for
/// w3s2xy_smcrg_dir; cvq (NSEA+10), grdx, grdy (NSEA) and smcgradn for w3s2xy_smcnn_int.
/// An array a type does not read may be nullptr.
template <class SmcGradn>
inline int w3s2xy_smc(const Real* s, long size_s, Real* xy, const bool* dir, int smcotype,
                      int nxo, int nyo, const int* smcmask, Real undef, Real noval, int nsmc,
                      const int* smcidx, const int* xidx, const int* yidx, const int* xspan,
                      const int* yspan, const Real* wts, Real* cov, const int* mapsmc,
                      const int* nnidx, const Real* xdist, const Real* ydist, Real* aux1,
                      Real* aux2, Real* cvq, Real* grdx, Real* grdy, SmcGradn&& smcgradn) {
  using namespace s2xy_smcnn_detail;
  const bool dirn = (dir != nullptr) ? *dir : false;

  if (smcotype == 1) {
    // :835, XY(:,1) = PACK(S, SMCMASK): a call of _gfortran_pack, which walks S in order and
    // copies where the low byte of the LOGICAL(4) mask is non-zero. Without bounds checking it
    // writes COUNT(SMCMASK) elements into XY(:,1) and checks no size; ww3_ounf.F90:591-592
    // sets NXO = COUNT(SMCMASK).
    long k = 0;
    for (long i = 0; i < size_s; ++i) {
      if (static_cast<unsigned char>(smcmask[i]) != 0) {
        xy[k] = s[i];
        ++k;
      }
    }
    if (dirn) {
      // :838-842; RADE folded as in w3s2xy_smcnn; MOD of REALs is fprem inline, fmod is exact.
      for (int isea = 1; isea <= nxo; ++isea) {
        if (xy[isea - 1] != undef) {
          xy[isea - 1] = Kokkos::fmod(k630 - kRade * xy[isea - 1], k360);
        }
      }
    }
  } else if (smcotype == 2) {
    if (dirn) {
      w3s2xy_smcrg_dir(s, xy, nxo, nyo, nsmc, smcidx, xidx, yidx, xspan, yspan, wts, mapsmc,
                       undef, noval, cov, aux1, aux2);
    } else {
      w3s2xy_smcrg(s, xy, nxo, nyo, nsmc, smcidx, xidx, yidx, xspan, yspan, wts, cov, mapsmc,
                   undef, noval);
    }
  } else if (smcotype == 3) {
    w3s2xy_smcnn_int(s, size_s, xy, dirn, nxo, nyo, nnidx, xdist, ydist, undef, cvq, grdx, grdy,
                     smcgradn);
  } else if (smcotype == 4) {
    w3s2xy_smcnn(s, xy, dirn, nxo, nyo, nnidx, undef, noval);
  } else {
    return s2xy_smc_detail::kUnknownType;
  }
  return 0;
}

}  // namespace ww::smco
