// kokkos/src/ww_kokkos/updt_utrn.hpp
// W3UTRN, ported to Kokkos: cell-boundary transparencies ATRNX/ATRNY for the
// propagation, from the sub-grid obstructions TRNX/TRNY, ice (IC0) and icebergs.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3updtmd.F90:2809-3194 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman (run-time grid selection W. E. Rogers, T. J. Campbell, NRL).
// Called from W3WAVE (w3wavemd.F90:1465). Only the switch_lab_shrd branch is
// ported: IC0 is compiled (:3080-3112); W3_S and W3_T (the LEVS statistics and
// the test prints) are not, and a kernel never prints. RLGTYPE and CLGTYPE are in
// the USE list (:2877-2878) but never read. Every TRFLAG value is translated, since
// TRFLAG is a run-time grid setting.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3updtmd.F90 (WW3's own flags, switch_lab_shrd):
//   :3002-3005  ((1+T(IXY)) * T(N)) / (1+T(N)), a true division, 1+T(IXY) shared;
//   :3075-3076  (DX * RADIUS) * DERA, both REAL PARAMETERs folded (kRadius, kDera);
//   :3083-3084  expf(-((ICE*DX) / FICEL)), a true division;
//   :3091,3105  (LICEN - ICE*DX) / (LICEN - LICE0);
//   :3110-3111  minss T,1 then maxss M,0 (operand order read from the object);
//   :3118-3119  TRIX * expf(-(((BERG*FFACBERG) * DX) * 1.E-4)), BERG*FFACBERG shared;
//   :3169-3175  (((ATRN * (1+T(IXY))) * T(N)) / (1+T(N)).
// No FMA (SSE baseline object, contraction off), no reciprocal multiply. The only
// vectorised loops are the TRIX = 1. and TRIY = 1. fills (:3063-3064), which change
// no bits. Every expf is a scalar call of libm's expf.
//
// Why the parallel passes give the Fortran's bits: MAPSF(ISEA,3) = IY+(IX-1)*NY
// (w3gridmd.F90:5184) is one cell per sea point, so each pass writes each output
// element at most once, reads only inputs a previous pass finished, and has no
// reduction, atomic or accumulation across points. The one cross-point read is
// :3169-3175 reading TRIX/TRIY of the neighbours, which runs after a fence that
// follows the pass writing TRIX/TRIY (:3066-3127).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::updt {

namespace utrn_detail {
// Fortran MIN/MAX of two REALs: fmin2(a,b) = b<a?b:a, fmax2(a,b) = b>a?b:a. A NaN or
// a +0/-0 tie gives `a`; each call passes the operands in the order of the build's
// minss/maxss (w3updtmd.o), not the GIMPLE order.
KOKKOS_INLINE_FUNCTION Real fmin2(Real a, Real b) { return (b < a) ? b : a; }
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }

// RADIUS = 4.E7*TPIINV and DERA = PI/180. (constants.F90:78-80), REAL PARAMETERs
// that gfortran folds to 6.3661975e+6 and 1.745329238474369049072265625e-2 (the
// GIMPLE of :3075; .rodata.cst4 0x4ac247eb, 0x3c8efa35). Written as those bits.
inline constexpr Real kRadius = 0x1.848fd6p+22f;
inline constexpr Real kDera = 0x1.1df46ap-6f;
// The REAL literals 1.E-6 (:3050, 0x358637bd) and 0.0001 (:3118, 0x38d1b717).
inline constexpr Real kFiceTol = 0x1.0c6f7ap-20f;
inline constexpr Real kBergScale = 0x1.a36e2ep-14f;

// IXN, IXP, IYN, IYP of :2978-2998, repeated verbatim at :3146-3166. Fortran
// 1-based cell indices (IXY = IY + (IX-1)*NY). IX == 1 is tested before IX == NX,
// so NX == 1 takes the first branch, as in the Fortran.
struct Neighbours {
  int ixn, ixp, iyn, iyp;
};
KOKKOS_INLINE_FUNCTION Neighbours neighbours(int ix, int iy, int ixy, int nx, int ny) {
  Neighbours n{};
  if (ix == 1) {
    n.ixn = iy + (nx - 1) * ny;
    n.ixp = iy + ix * ny;
  } else if (ix == nx) {
    n.ixn = iy + (ix - 2) * ny;
    n.ixp = iy;
  } else {
    n.ixn = iy + (ix - 2) * ny;
    n.ixp = iy + ix * ny;
  }
  if (iy == 1) {
    n.iyn = ixy;
    n.iyp = ixy + 1;
  } else if (iy == ny) {
    n.iyn = ixy - 1;
    n.iyp = ixy;
  } else {
    n.iyn = ixy - 1;
    n.iyp = ixy + 1;
  }
  return n;
}
}  // namespace utrn_detail

/// Grid scalars of W3UTRN (w3gdatmd.F90:648,705,720; the per-grid GRIDS(IMOD)
/// values W3SETG points at). trny_before is the REAL that sits in memory just
/// before TRNY(1): with TRFLAG = 1, 3 or 5, :2954 reads TRNY(IXY-1), which is
/// TRNY(0), outside the array, when the cell IXY = 1 is a sea point. The caller
/// reads it from WW3's own TRNY address so the port reproduces that read without
/// an out-of-bounds access in the kernel.
struct UtrnScalars {
  int nx = 0;
  int ny = 0;
  int nsea = 0;
  int trflag = 0;
  Real fice0 = 0;
  Real ficen = 0;
  Real ficel = 0;
  Real ffacberg = 0;
  bool flagll = false;
  Real trny_before = 0;
};

/// The arrays of W3UTRN, all LayoutLeft (Fortran order), 0-based:
///   TRNX(IXY), TRNY(IXY)        -> trnx(IXY-1), trny(IXY-1), extent NY*NX
///   MAPSF(ISEA,J)               -> mapsf(ISEA-1, J-1), extent (>=NSEA, 3)
///   MAPSTA(IY,IX)               -> mapsta(IY-1, IX-1), (NY, NX)
///   HPFAC/HQFAC(IY,IX)          -> hpfac/hqfac(IY-1, IX-1), (NY, NX)
///   ICE(ISEA), ICE(0:NSEA)      -> ice(ISEA), extent NSEA+1 (w3wdatmd.F90:561)
///   BERG(ISEA), BERG(NSEA)      -> berg(ISEA-1) (w3wdatmd.F90:566)
///   ATRNX/ATRNY(IXY,K), K=-1:1  -> atrnx/atrny(IXY-1, K+1), (NY*NX, 3)
///                                  (w3adatmd.F90:1370-1371)
///   TRIX/TRIY(IXY), locals      -> trix/triy(IXY-1), extent >= NY*NX, caller's work
///                                  (w3updtmd.F90:2900)
template <class MemSpace>
struct UtrnViews {
  using R1 = Kokkos::View<const Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
  using I2 = Kokkos::View<const int**, Kokkos::LayoutLeft, MemSpace>;
  using W1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using W2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  R1 trnx, trny, ice, berg;
  I2 mapsf, mapsta;
  R2 hpfac, hqfac;
  W2 atrnx, atrny;
  W1 trix, triy;
};

/// :2917-2918, ATRNX = 1., ATRNY = 1., one cell (all three K).
template <class W2>
KOKKOS_INLINE_FUNCTION void w3utrn_init_point(int i, const W2& atrnx, const W2& atrny) {
  for (int k = 0; k < 3; ++k) {
    atrnx(i, k) = Real(1.);
    atrny(i, k) = Real(1.);
  }
}

/// :2939-2962, TRFLAG = 1, 3, 5: transparencies at cell boundaries, sea point ISEA.
template <class I2, class R1, class W2>
KOKKOS_INLINE_FUNCTION void w3utrn_bound_point(int isea, const UtrnScalars& s, const I2& mapsf,
                                               const R1& trnx, const R1& trny, const W2& atrnx,
                                               const W2& atrny) {
  const int ix = mapsf(isea - 1, 0);
  const int iy = mapsf(isea - 1, 1);
  const int ixy = mapsf(isea - 1, 2);
  const int nx = s.nx;
  const int ny = s.ny;
  if (ix == 1) {
    atrnx(ixy - 1, 0) = trnx(iy + (nx - 1) * ny - 1);
    atrnx(ixy - 1, 2) = trnx(ixy - 1);
  } else if (ix == nx) {
    atrnx(ixy - 1, 0) = trnx(ixy - ny - 1);
    atrnx(ixy - 1, 2) = trnx(iy - 1);
  } else {
    atrnx(ixy - 1, 0) = trnx(ixy - ny - 1);
    atrnx(ixy - 1, 2) = trnx(ixy - 1);
  }
  // :2954, TRNY(IXY-1): TRNY(0) for IXY = 1, see UtrnScalars::trny_before.
  atrny(ixy - 1, 0) = (ixy >= 2) ? trny(ixy - 2) : s.trny_before;
  atrny(ixy - 1, 2) = trny(ixy - 1);
}

/// :2972-3035, TRFLAG = 2, 4 (any value not 0, 1, 3, 5): transparencies at cell
/// centres, sea point ISEA. The 0.5 and 2. of the averaging cancel (:3000).
template <class I2, class R1, class W2>
KOKKOS_INLINE_FUNCTION void w3utrn_center_point(int isea, const UtrnScalars& s, const I2& mapsf,
                                                const I2& mapsta, const R1& trnx, const R1& trny,
                                                const W2& atrnx, const W2& atrny) {
  const int ix = mapsf(isea - 1, 0);
  const int iy = mapsf(isea - 1, 1);
  const int ixy = mapsf(isea - 1, 2);
  const int nx = s.nx;
  const int ny = s.ny;
  const utrn_detail::Neighbours n = utrn_detail::neighbours(ix, iy, ixy, nx, ny);

  const Real tx = Real(1.) + trnx(ixy - 1);  // :3002-3003, shared by -O3
  const Real txn = trnx(n.ixn - 1);
  atrnx(ixy - 1, 0) = tx * txn / (Real(1.) + txn);
  const Real txp = trnx(n.ixp - 1);
  atrnx(ixy - 1, 2) = tx * txp / (Real(1.) + txp);
  const Real ty = Real(1.) + trny(ixy - 1);  // :3004-3005
  const Real tyn = trny(n.iyn - 1);
  atrny(ixy - 1, 0) = ty * tyn / (Real(1.) + tyn);
  const Real typ = trny(n.iyp - 1);
  atrny(ixy - 1, 2) = ty * typ / (Real(1.) + typ);

  // :3007-3028, MAPSTA = 2 (an active boundary point): an inflow side next to the
  // grid edge or to land is fully transparent.
  if (mapsta(iy - 1, ix - 1) == 2) {
    if (ix == 1) {
      atrnx(ixy - 1, 0) = Real(1.);
    } else if (mapsta(iy - 1, ix - 2) <= 0) {
      atrnx(ixy - 1, 0) = Real(1.);
    }
    if (ix == nx) {
      atrnx(ixy - 1, 2) = Real(1.);
    } else if (mapsta(iy - 1, ix) <= 0) {
      atrnx(ixy - 1, 2) = Real(1.);
    }
    if (iy == 1) {
      atrny(ixy - 1, 0) = Real(1.);
    } else if (mapsta(iy - 2, ix - 1) <= 0) {
      atrny(ixy - 1, 0) = Real(1.);
    }
    if (iy == ny) {
      atrny(ixy - 1, 2) = Real(1.);
    } else if (mapsta(iy, ix - 1) <= 0) {
      atrny(ixy - 1, 2) = Real(1.);
    }
  }
}

/// :3063-3064, TRIX = 1., TRIY = 1., one cell.
template <class W1>
KOKKOS_INLINE_FUNCTION void w3utrn_tri_init_point(int i, const W1& trix, const W1& triy) {
  trix(i) = Real(1.);
  triy(i) = Real(1.);
}

/// :3066-3127, ice (IC0) and iceberg transparencies of sea point ISEA, written to
/// TRIX(IXY), TRIY(IXY) only.
template <class I2, class R1, class R2, class W1>
KOKKOS_INLINE_FUNCTION void w3utrn_ice_point(int isea, const UtrnScalars& s, const I2& mapsf,
                                             const R2& hpfac, const R2& hqfac, const R1& ice,
                                             const R1& berg, const W1& trix, const W1& triy) {
  using utrn_detail::fmax2;
  using utrn_detail::fmin2;
  const int ix = mapsf(isea - 1, 0);
  const int iy = mapsf(isea - 1, 1);
  const int ixy = mapsf(isea - 1, 2);

  Real dx = hpfac(iy - 1, ix - 1);  // :3072-3077
  Real dy = hqfac(iy - 1, ix - 1);
  if (s.flagll) {
    dx = dx * utrn_detail::kRadius * utrn_detail::kDera;
    dy = dy * utrn_detail::kRadius * utrn_detail::kDera;
  }

  const Real ic = ice(isea);  // ICE(0:NSEA)
  if (ic > Real(0.)) {         // :3081
    if (s.ficel > Real(0.)) {  // :3082-3084
      trix(ixy - 1) = Kokkos::exp(-(ic * dx / s.ficel));
      triy(ixy - 1) = Kokkos::exp(-(ic * dy / s.ficel));
    } else {  // :3089-3105, Tolman (2003)
      Real lice0 = s.fice0 * dx;
      Real licen = s.ficen * dx;
      trix(ixy - 1) = (licen - ic * dx) / (licen - lice0);
      lice0 = s.fice0 * dy;
      licen = s.ficen * dy;
      triy(ixy - 1) = (licen - ic * dy) / (licen - lice0);
    }
    // :3110-3111, MAX(0., MIN(1., T)) as minss T,1 (T<1?T:1) then maxss M,0
    // (M>0?M:0): a NaN T gives 1.
    trix(ixy - 1) = fmax2(Real(0.), fmin2(Real(1.), trix(ixy - 1)));
    triy(ixy - 1) = fmax2(Real(0.), fmin2(Real(1.), triy(ixy - 1)));
  }

  const Real bg = berg(isea - 1);  // :3117-3120
  if (bg > Real(0.)) {
    const Real bf = bg * s.ffacberg;
    trix(ixy - 1) = trix(ixy - 1) * Kokkos::exp(-(bf * dx * utrn_detail::kBergScale));
    triy(ixy - 1) = triy(ixy - 1) * Kokkos::exp(-(bf * dy * utrn_detail::kBergScale));
  }
}

/// :3140-3177, ice combined into ATRNX/ATRNY of sea point ISEA; reads TRIX/TRIY of
/// the neighbours, so it runs after every w3utrn_ice_point has finished.
template <class I2, class W1, class W2>
KOKKOS_INLINE_FUNCTION void w3utrn_combine_point(int isea, const UtrnScalars& s, const I2& mapsf,
                                                 const W1& trix, const W1& triy, const W2& atrnx,
                                                 const W2& atrny) {
  const int ix = mapsf(isea - 1, 0);
  const int iy = mapsf(isea - 1, 1);
  const int ixy = mapsf(isea - 1, 2);
  const utrn_detail::Neighbours n = utrn_detail::neighbours(ix, iy, ixy, s.nx, s.ny);

  const Real tx = Real(1.) + trix(ixy - 1);
  const Real txn = trix(n.ixn - 1);
  atrnx(ixy - 1, 0) = atrnx(ixy - 1, 0) * tx * txn / (Real(1.) + txn);
  const Real txp = trix(n.ixp - 1);
  atrnx(ixy - 1, 2) = atrnx(ixy - 1, 2) * tx * txp / (Real(1.) + txp);
  const Real ty = Real(1.) + triy(ixy - 1);
  const Real tyn = triy(n.iyn - 1);
  atrny(ixy - 1, 0) = atrny(ixy - 1, 0) * ty * tyn / (Real(1.) + tyn);
  const Real typ = triy(n.iyp - 1);
  atrny(ixy - 1, 2) = atrny(ixy - 1, 2) * ty * typ / (Real(1.) + typ);
}

/// W3UTRN(TRNX, TRNY): fills v.atrnx, v.atrny. The branch tests on TRFLAG, FICEN
/// and FICE0 are host scalars, taken as the Fortran takes them (:2925, :2933,
/// :3050). Kernels on one execution space instance run in order; the fences make
/// the dependency explicit.
template <class ExecSpace>
void w3utrn(const ExecSpace& exec, const UtrnScalars& s,
            const UtrnViews<typename ExecSpace::memory_space>& v) {
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  const UtrnScalars sc = s;
  const auto atrnx = v.atrnx;
  const auto atrny = v.atrny;
  const auto mapsf = v.mapsf;
  const auto trnx = v.trnx;
  const auto trny = v.trny;

  const int ncell = static_cast<int>(atrnx.extent(0));
  Kokkos::parallel_for(
      "w3utrn_init", Policy(exec, 0, ncell),
      KOKKOS_LAMBDA(const int i) { w3utrn_init_point(i, atrnx, atrny); });
  exec.fence();

  if (sc.trflag == 0) return;  // :2925-2929

  if (sc.nsea > 0) {
    if (sc.trflag == 1 || sc.trflag == 3 || sc.trflag == 5) {  // :2933
      Kokkos::parallel_for(
          "w3utrn_bound", Policy(exec, 1, sc.nsea + 1), KOKKOS_LAMBDA(const int isea) {
            w3utrn_bound_point(isea, sc, mapsf, trnx, trny, atrnx, atrny);
          });
    } else {
      const auto mapsta = v.mapsta;
      Kokkos::parallel_for(
          "w3utrn_center", Policy(exec, 1, sc.nsea + 1), KOKKOS_LAMBDA(const int isea) {
            w3utrn_center_point(isea, sc, mapsf, mapsta, trnx, trny, atrnx, atrny);
          });
    }
    exec.fence();
  }

  // :3050, FICEN-FICE0 in REAL on the host, as the Fortran forms it.
  if (sc.trflag < 3 || sc.ficen - sc.fice0 < utrn_detail::kFiceTol) return;

  const auto trix = v.trix;
  const auto triy = v.triy;
  Kokkos::parallel_for(
      "w3utrn_tri_init", Policy(exec, 0, sc.ny * sc.nx),
      KOKKOS_LAMBDA(const int i) { w3utrn_tri_init_point(i, trix, triy); });
  exec.fence();
  if (sc.nsea <= 0) return;

  const auto hpfac = v.hpfac;
  const auto hqfac = v.hqfac;
  const auto ice = v.ice;
  const auto berg = v.berg;
  Kokkos::parallel_for(
      "w3utrn_ice", Policy(exec, 1, sc.nsea + 1), KOKKOS_LAMBDA(const int isea) {
        w3utrn_ice_point(isea, sc, mapsf, hpfac, hqfac, ice, berg, trix, triy);
      });
  exec.fence();  // :3169-3175 read TRIX/TRIY of the neighbours
  Kokkos::parallel_for(
      "w3utrn_combine", Policy(exec, 1, sc.nsea + 1), KOKKOS_LAMBDA(const int isea) {
        w3utrn_combine_point(isea, sc, mapsf, trix, triy, atrnx, atrny);
      });
  exec.fence();
}

}  // namespace ww::updt
