// kokkos/src/ww_kokkos/snl1_tables.cpp
// INSNL1 ported to C++: the DIA's quadruplet addressing and interpolation weights.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE INSNL1, lines 483-779; the numbered section comments below are that
// routine's own. This is phase-1 work: same expressions, same order, same float32
// arithmetic. Nothing is regrouped, hoisted or "simplified", because the point of
// the file is that it produces the Fortran's bits.
//
// It runs once at set-up on the host and deep-copies the result to the device,
// which is why it may use std:: math and std::vector freely.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ww_kokkos/snl1_tables.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ww_kokkos/snl1_config.hpp"

namespace ww::snl1 {
namespace {

/// Fortran's `x ** n` for a runtime INTEGER n, in the same order gfortran uses.
///
/// This is not std::pow: gfortran lowers a real**integer to the binary-exponent
/// chain of libgcc's __powisf2 (and GCC's own inline expansion follows the same
/// addition chain), while powf goes through libm's general algorithm. The two
/// agree to a ULP or so, not exactly -- and "or so" is the whole budget of a
/// parity test. INSNL1 needs XFR**IFRP with IFRP in {-4,-3,2,3}, so the negative
/// branch matters too.
Real powi(Real x, int n) {
  unsigned int m = (n < 0) ? static_cast<unsigned int>(-n) : static_cast<unsigned int>(n);
  Real y = (m & 1u) ? x : static_cast<Real>(1);
  while ((m >>= 1u) != 0u) {
    x = x * x;
    if ((m & 1u) != 0u) y = y * x;
  }
  return (n < 0) ? static_cast<Real>(1) / y : y;
}

/// x**11 by the same addition chain gfortran emits for a literal exponent:
/// x^2, x^3 = x*x^2, x^4, x^8, x^11 = x^3 * x^8.
Real pow11(Real x) {
  const Real x2 = x * x;
  const Real x3 = x * x2;
  const Real x4 = x2 * x2;
  const Real x8 = x4 * x4;
  return x3 * x8;
}

/// The host-side mirror type of an index table. Spelled with decltype rather
/// than a hand-written View type so it stays correct when the device memory
/// space is CudaSpace (mirror in HostSpace) and when it is HostSpace (mirror is
/// the view itself).
using HostIndexMirror = decltype(Kokkos::create_mirror_view(IntView1D()));

/// Allocate a device index table and hand back a host mirror to fill.
struct IndexTable {
  IntView1D device;
  HostIndexMirror host;
};

IndexTable make_index_table(const char* label, int n) {
  IndexTable t;
  t.device = IntView1D(label, n);
  t.host = Kokkos::create_mirror_view(t.device);
  return t;
}

}  // namespace

Tables make_tables(const Config& c, ConstHostRealView1D sig) {
  Tables t{};
  const int nth = c.nth;
  const int nspec = c.nspec();

  t.nfr = c.nk;  // NFR = NK

  // 1.  Internal angles of quadruplet.
  const Real one = static_cast<Real>(1);
  const Real lamm2_b = one - c.lam;
  const Real lamp2_b = one + c.lam;
  const Real lamm2 = lamm2_b * lamm2_b;
  const Real lamp2 = lamp2_b * lamp2_b;
  const Real delth3 = std::acos((lamm2 * lamm2 + static_cast<Real>(4) - lamp2 * lamp2) /
                                (static_cast<Real>(4) * lamm2));
  const Real delth4 = std::asin(-std::sin(delth3) * lamm2 / lamp2);

  // 2.  Lambda dependend weight factors.
  const Real p2 = lamp2_b * lamp2_b;
  const Real m2 = lamm2_b * lamm2_b;
  t.dal1 = one / (p2 * p2);  // 1. / (1.+LAM)**4
  t.dal2 = one / (m2 * m2);  // 1. / (1.-LAM)**4
  t.dal3 = static_cast<Real>(2) * t.dal1 * t.dal2;

  // 3.  Directional indices. INT() truncates toward zero, like a C cast.
  const Real cthp = std::fabs(delth4 / c.dth);
  const int ithp = static_cast<int>(cthp);
  const int ithp1 = ithp + 1;
  const Real wthp = cthp - static_cast<Real>(ithp);
  const Real wthp1 = one - wthp;

  const Real cthm = std::fabs(delth3 / c.dth);
  const int ithm = static_cast<int>(cthm);
  const int ithm1 = ithm + 1;
  const Real wthm = cthm - static_cast<Real>(ithm);
  const Real wthm1 = one - wthm;

  // 4.  Frequency indices.
  const Real xfrln = std::log(c.xfr);

  const int ifrp = static_cast<int>(std::log(one + c.lam) / xfrln);
  const int ifrp1 = ifrp + 1;
  const Real wfrp = (one + c.lam - powi(c.xfr, ifrp)) / (powi(c.xfr, ifrp1) - powi(c.xfr, ifrp));
  const Real wfrp1 = one - wfrp;

  const int ifrm = static_cast<int>(std::log(one - c.lam) / xfrln);
  const int ifrm1 = ifrm - 1;
  const Real wfrm = (powi(c.xfr, ifrm) - (one - c.lam)) / (powi(c.xfr, ifrm) - powi(c.xfr, ifrm1));
  const Real wfrm1 = one - wfrm;

  // 5.  Range of calculations.
  t.nfrhgh = t.nfr + ifrp1 - ifrm1;
  t.nfrchg = t.nfr - ifrm1;
  t.nspecy = t.nfrhgh * nth;
  t.nspecx = t.nfrchg * nth;
  t.nspec = nspec;

  // 6.  Allocate arrays. In WW3 this is CALL W3DMNL, which sizes the W3ADATMD
  //     tables; here it is the View allocations plus their host mirrors.
  static const char* const kIpLabels[2][4] = {{"snl1.ip11", "snl1.ip12", "snl1.ip13", "snl1.ip14"},
                                              {"snl1.ip21", "snl1.ip22", "snl1.ip23", "snl1.ip24"}};
  static const char* const kImLabels[2][4] = {{"snl1.im11", "snl1.im12", "snl1.im13", "snl1.im14"},
                                              {"snl1.im21", "snl1.im22", "snl1.im23", "snl1.im24"}};
  static const char* const kIcLabels[8][2] = {
      {"snl1.ic11", "snl1.ic12"}, {"snl1.ic21", "snl1.ic22"}, {"snl1.ic31", "snl1.ic32"},
      {"snl1.ic41", "snl1.ic42"}, {"snl1.ic51", "snl1.ic52"}, {"snl1.ic61", "snl1.ic62"},
      {"snl1.ic71", "snl1.ic72"}, {"snl1.ic81", "snl1.ic82"}};

  IndexTable ip[2][4];
  IndexTable im[2][4];
  IndexTable ic[8][2];
  for (int k = 0; k < 2; ++k) {
    for (int j = 0; j < 4; ++j) {
      ip[k][j] = make_index_table(kIpLabels[k][j], t.nspecx);
      im[k][j] = make_index_table(kImLabels[k][j], t.nspecx);
    }
  }
  for (int j = 0; j < 8; ++j) {
    for (int col = 0; col < 2; ++col) ic[j][col] = make_index_table(kIcLabels[j][col], nspec);
  }
  t.af11 = RealView1D("snl1.af11", t.nspecx);
  auto h_af11 = Kokkos::create_mirror_view(t.af11);

  // 7.  Spectral addresses. IF*/IT* stay in WW3's 1-based world for the whole
  //     section -- they are the Fortran's own scratch -- and only the values
  //     written into the tables are shifted to 0-based, once, at the end.
  std::vector<int> if_(8 * static_cast<std::size_t>(t.nfrchg));
  const auto IFn = [&](int n, int ifr1) -> int& {
    return if_[static_cast<std::size_t>((n - 1) * t.nfrchg + (ifr1 - 1))];
  };
  for (int ifr = 1; ifr <= t.nfrchg; ++ifr) {
    IFn(1, ifr) = ifr + ifrp;
    IFn(2, ifr) = ifr + ifrp1;
    IFn(3, ifr) = std::max(0, ifr + ifrm);
    IFn(4, ifr) = std::max(0, ifr + ifrm1);
    IFn(5, ifr) = std::max(0, ifr - ifrp);
    IFn(6, ifr) = std::max(0, ifr - ifrp1);
    IFn(7, ifr) = ifr - ifrm;
    IFn(8, ifr) = ifr - ifrm1;
  }

  std::vector<int> it_(8 * static_cast<std::size_t>(nth));
  const auto ITn = [&](int n, int ith1) -> int& {
    return it_[static_cast<std::size_t>((n - 1) * nth + (ith1 - 1))];
  };
  for (int ith = 1; ith <= nth; ++ith) {
    ITn(1, ith) = ith + ithp;
    ITn(2, ith) = ith + ithp1;
    ITn(3, ith) = ith + ithm;
    ITn(4, ith) = ith + ithm1;
    ITn(5, ith) = ith - ithp;
    ITn(6, ith) = ith - ithp1;
    ITn(7, ith) = ith - ithm;
    ITn(8, ith) = ith - ithm1;
    for (int n = 1; n <= 4; ++n) {
      if (ITn(n, ith) > nth) ITn(n, ith) -= nth;  // directions wrap, frequencies do not
    }
    for (int n = 5; n <= 8; ++n) {
      if (ITn(n, ith) < 1) ITn(n, ith) += nth;
    }
  }

  // Fortran writes IP11(ISP) = IT2(ITH) + (IF2(IFR)-1)*NTH, a 1-based address.
  // The port stores that address minus one; see the index-convention note in
  // snl1_tables.hpp for why the result may legitimately be negative.
  const auto addr = [&](int itn, int ifn, int ith1, int ifr1) -> int {
    return ITn(itn, ith1) + (IFn(ifn, ifr1) - 1) * nth - 1;
  };

  for (int isp = 0; isp < t.nspecx; ++isp) {
    const int ifr = 1 + isp / nth;      // Fortran IFR = 1 + (ISP-1)/NTH
    const int ith = 1 + (isp % nth);    // Fortran ITH = 1 + MOD(ISP-1,NTH)
    ip[0][0].host(isp) = addr(2, 2, ith, ifr);  // IP11
    ip[0][1].host(isp) = addr(1, 2, ith, ifr);  // IP12
    ip[0][2].host(isp) = addr(2, 1, ith, ifr);  // IP13
    ip[0][3].host(isp) = addr(1, 1, ith, ifr);  // IP14
    im[0][0].host(isp) = addr(8, 4, ith, ifr);  // IM11
    im[0][1].host(isp) = addr(7, 4, ith, ifr);  // IM12
    im[0][2].host(isp) = addr(8, 3, ith, ifr);  // IM13
    im[0][3].host(isp) = addr(7, 3, ith, ifr);  // IM14
    ip[1][0].host(isp) = addr(6, 2, ith, ifr);  // IP21
    ip[1][1].host(isp) = addr(5, 2, ith, ifr);  // IP22
    ip[1][2].host(isp) = addr(6, 1, ith, ifr);  // IP23
    ip[1][3].host(isp) = addr(5, 1, ith, ifr);  // IP24
    im[1][0].host(isp) = addr(4, 4, ith, ifr);  // IM21
    im[1][1].host(isp) = addr(3, 4, ith, ifr);  // IM22
    im[1][2].host(isp) = addr(4, 3, ith, ifr);  // IM23
    im[1][3].host(isp) = addr(3, 3, ith, ifr);  // IM24
  }

  for (int isp = 0; isp < nspec; ++isp) {
    const int ifr = 1 + isp / nth;
    const int ith = 1 + (isp % nth);
    ic[0][0].host(isp) = addr(6, 6, ith, ifr);  // IC11
    ic[1][0].host(isp) = addr(5, 6, ith, ifr);  // IC21
    ic[2][0].host(isp) = addr(6, 5, ith, ifr);  // IC31
    ic[3][0].host(isp) = addr(5, 5, ith, ifr);  // IC41
    ic[4][0].host(isp) = addr(4, 8, ith, ifr);  // IC51
    ic[5][0].host(isp) = addr(3, 8, ith, ifr);  // IC61
    ic[6][0].host(isp) = addr(4, 7, ith, ifr);  // IC71
    ic[7][0].host(isp) = addr(3, 7, ith, ifr);  // IC81
    ic[0][1].host(isp) = addr(2, 6, ith, ifr);  // IC12
    ic[1][1].host(isp) = addr(1, 6, ith, ifr);  // IC22
    ic[2][1].host(isp) = addr(2, 5, ith, ifr);  // IC32
    ic[3][1].host(isp) = addr(1, 5, ith, ifr);  // IC42
    ic[4][1].host(isp) = addr(8, 8, ith, ifr);  // IC52
    ic[5][1].host(isp) = addr(7, 8, ith, ifr);  // IC62
    ic[6][1].host(isp) = addr(8, 7, ith, ifr);  // IC72
    ic[7][1].host(isp) = addr(7, 7, ith, ifr);  // IC82
  }

  // 8.  Fill scaling array (f**11).
  for (int ifr = 0; ifr < t.nfr; ++ifr) {
    const Real af11a = pow11(sig(ifr) * kTpiInv);
    for (int ith = 0; ith < nth; ++ith) h_af11(ith + ifr * nth) = af11a;
  }
  Real fr = sig(t.nfr - 1) * kTpiInv;
  for (int ifr = t.nfr; ifr < t.nfrchg; ++ifr) {
    fr = fr * c.xfr;
    const Real af11a = pow11(fr);
    for (int ith = 0; ith < nth; ++ith) h_af11(ith + ifr * nth) = af11a;
  }

  // 9.  Interpolation weights.
  t.awg[0] = wthp * wfrp;
  t.awg[1] = wthp1 * wfrp;
  t.awg[2] = wthp * wfrp1;
  t.awg[3] = wthp1 * wfrp1;
  t.awg[4] = wthm * wfrm;
  t.awg[5] = wthm1 * wfrm;
  t.awg[6] = wthm * wfrm1;
  t.awg[7] = wthm1 * wfrm1;
  for (int j = 0; j < 8; ++j) t.swg[j] = t.awg[j] * t.awg[j];

  // Publish: one deep_copy per table, then hand back the device handles.
  for (int k = 0; k < 2; ++k) {
    for (int j = 0; j < 4; ++j) {
      Kokkos::deep_copy(ip[k][j].device, ip[k][j].host);
      Kokkos::deep_copy(im[k][j].device, im[k][j].host);
      t.ip[k][j] = ip[k][j].device;
      t.im[k][j] = im[k][j].device;
    }
  }
  for (int j = 0; j < 8; ++j) {
    for (int col = 0; col < 2; ++col) {
      Kokkos::deep_copy(ic[j][col].device, ic[j][col].host);
      t.ic[j][col] = ic[j][col].device;
    }
  }
  Kokkos::deep_copy(t.af11, h_af11);
  Kokkos::fence();  // the host mirrors above die at the end of this scope

  return t;
}

}  // namespace ww::snl1
