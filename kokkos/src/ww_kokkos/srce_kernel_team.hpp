// kokkos/src/ww_kokkos/srce_kernel_team.hpp
// W3SRCE with its element-wise loops split over the threads of its team: the body run_batch
// (srce_shim.cpp) launches on a device, where one thread per point leaves the GPU idle, and on a
// host only when WW_KOKKOS_SRCE_TEAM asks for it (srce_team.hpp). w3srce_team (srce_kernel.hpp)
// stays the code of every host run by default: its arithmetic is the reference, and this file
// repeats it element by element, sum by sum, in the same order. Heritage, quirks and operand
// orders: srce_kernel.hpp and the callees' headers.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "srce_bt1_team.hpp"
#include "srce_db1_team.hpp"
#include "srce_kernel.hpp"
#include "srce_ln1_team.hpp"
#include "srce_team.hpp"
#include "st4_sds4_team.hpp"
#include "st4_sin4_team.hpp"
#include "st4_spr4_team.hpp"

namespace ww::srce {

/// Reals of per-point work of w3srce_split: w3srce_team's (srce_work_reals) plus three arrays
/// of NSPEC, the per-IS terms of the time-step MIN, PHIAW and PHIBBL; the ST4 part is the
/// largest of the team forms' work (W3SIN4's adds the values its team shares).
inline int srce_split_work_reals(const Params& p, const st4::Params& q) {
  int st4w = st4::sds4_work_reals(q);
  if (st4::sin4_team_work_reals(q) > st4w) st4w = st4::sin4_team_work_reals(q);
  if (st4::spr4_work_reals(q) > st4w) st4w = st4::spr4_work_reals(q);
  return 21 * p.nspec + p.nth + 2 * p.nk + st4w;
}
/// Ints: w3srce_team's plus one, the IK where W3SBT1's CBETA loop stopped.
inline int srce_split_work_ints(const Params& p, const st4::Params& q) {
  return srce_work_ints(p, q) + 1;
}

/// W3SRCE(srce_direct, ...) for one point, run by the whole team. spec =
/// SPEC(1:NSPEC) INOUT, wn1 = WN1(1:NK), cg1 = CG1(1:NK), whitecap(4),
/// taubbl(2), tauice(2) INOUT; st is the team's shared state; the ST4 work
/// buffers are reused by each ST4 call in turn: the Fortran routines keep
/// nothing between calls.
///
/// Who computes what (the owner, 2026-10-17: element-wise work over the team,
/// parity kept). A loop whose iterations are independent (each writes only its
/// own elements and reads nothing another iteration writes) is split over the
/// team (TeamThreadRange, ex.for_), each element with the same operations in the
/// same order as on one thread. Every sum, MIN/MAX over IS and running value
/// stays on one thread (ex.one), in the Fortran's order; where its terms are
/// element-wise (the time-step candidates of :1523-1532, the PHIAW and PHIBBL
/// terms of :1827-1831) the terms are computed over the team into work arrays
/// first and summed or reduced in order after a barrier. No parallel_reduce, no
/// atomic. The callees do the same (their headers say which loops). A value one
/// thread computes reaches the others through st, o or the work arrays, after a
/// barrier. So the result does not depend on the team size.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void w3srce_split(const TeamMember& team, const Params& p, const Grid& g,
                                         const st4::Params& q, const st4::Fields& f,
                                         const snl1::Config& nc, const snl1::Tables& nt,
                                         const Real* nsig, const snl1::TeamScratch& nw,
                                         const PointIn& in, Real* spec, const Real* wn1,
                                         const Real* cg1, PointScalars& o, Real* whitecap,
                                         Real* taubbl, Real* tauice, State& st, Real* rw,
                                         int* iw, double* dw) {
  using srce_detail::mn;
  using srce_detail::mni;
  using srce_detail::mx;
  using srce_detail::mxi;
  const TeamExec<TeamMember> ex{team};
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  Real* dam = rw;
  Real* wn2 = dam + nspec;
  Real* vsln = wn2 + nspec;
  Real* vsin = vsln + nspec;
  Real* vdin = vsin + nspec;
  Real* vsnl = vdin + nspec;
  Real* vdnl = vsnl + nspec;
  Real* vsds = vdnl + nspec;
  Real* vdds = vsds + nspec;
  Real* vsbt = vdds + nspec;
  Real* vdbt = vsbt + nspec;
  Real* vs = vdbt + nspec;
  Real* vd = vs + nspec;
  Real* vsdb = vd + nspec;
  Real* vddb = vsdb + nspec;
  Real* specinit = vddb + nspec;
  Real* spec2 = specinit + nspec;
  Real* brlambda = spec2 + nspec;
  Real* dtc = brlambda + nspec;   // the candidate of each IS for the MIN of :1532
  Real* tphiaw = dtc + nspec;     // the PHIAW term of each IS (:1827)
  Real* tphibbl = tphiaw + nspec; // the PHIBBL term of each IS (:1830)
  Real* dirf = tphibbl + nspec;
  Real* wnf = dirf + nth;
  Real* cbeta = wnf + nk;
  Real* st4w = cbeta + nk;
  int* llws = iw;
  int* bt1ik = iw + nspec;  // the IK where W3SBT1's CBETA loop stopped
  int* st4iw = iw + nspec + 1;
  double* eb = dw;
  const Real sintail4 = q.sintailpar[3];

  // Both ST4 calls with W3SRCE's argument lists (:1080 and :1239 forms), by the team.
  auto spr4 = [&]() {
    st4::w3spr4_team(ex, q, f, spec, cg1, wn1, in.u10abs, in.u10dir, o.tauwx, o.tauwy, llws,
                     o.ustar, o.ustdir, st.emean, st.fmean, st.fmean1, o.wnmean, st.amax,
                     st.cd, st.z0, o.charn, st.fmeanws, st.dlwmean, st4w, st4iw);
  };
  auto sin4 = [&]() {
    st4::w3sin4_team(ex, q, f, spec, cg1, wn2, in.u10abs, o.ustar, st.drat, in.as, in.u10dir,
                     st.z0, st.cd, o.tauwx, o.tauwy, st.tauwax, st.tauway, vsin, vdin, llws,
                     st4w, st4iw);
  };

  // 1. Initialisation (:869-1179).
  ex.for_(nspec, [&](int is) {  // :877-905; BRLAMBDA = 0. (:1048) is the same kind of store
    vd[is] = 0;
    vs[is] = 0;
    vsbt[is] = 0;
    vdbt[is] = 0;
    vsln[is] = 0;
    vsin[is] = 0;
    vdin[is] = 0;
    vsnl[is] = 0;
    vdnl[is] = 0;
    vsds[is] = 0;
    vdds[is] = 0;
    vsdb[is] = 0;
    vddb[is] = 0;
    brlambda[is] = 0;
  });
  ex.one([&]() {
    st.depth = mx(in.d_inp, p.dmin);  // :951, maxss: D_INP>DMIN?D_INP:DMIN
    st.drat = in.dair / kDwat;        // :952
    // :953-956: MAX(0.,MIN(1.,1.-ICE*ICESCALES(i)))
    st.icescaleln = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[0], 1), 0);
    st.icescalein = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[1], 1), 0);
    st.icescalenl = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[2], 1), 0);
    // :956 alone is emitted 0>M?0:M (the three above as M>0?M:0).
    st.icescaleds = mx(0, mn(static_cast<Real>(1.) - in.ice * p.icescales[3], 1));
    o.dtdyn = 0;  // :983-1028
    st.dttot = 0;
    st.nsteps = 0;
    o.phiaw = 0;
    o.charn = 0;
    o.tws = 0;
    o.phibbl = 0;
    o.tauwix = 0;
    o.tauwiy = 0;
    o.tauwnx = 0;
    o.tauwny = 0;
    st.tauwax = 0;
    st.tauway = 0;
    taubbl[0] = 0;
    taubbl[1] = 0;
    tauice[0] = 0;
    tauice[1] = 0;
    o.phice = 0;
    o.tauocx = 0;
    o.tauocy = 0;
    o.wnmean = 0;
    st.dlwmean = 0;
    for (int j = 0; j < 4; ++j) whitecap[j] = 0;
    // :1069: "designed to keep the bug as an option" (TAUWBUG).
    if (sintail4 > static_cast<Real>(0.5)) {
      o.tauwx = 0;
      o.tauwy = 0;
    }
    if (in.it == 0) {  // :1073-1078
      o.tauwx = 0;
      o.tauwy = 0;
      o.ustar = 0;
      o.ustdir = 0;
    }
  });
  ex.for_(nk, [&](int ik) {  // :968-971, WN1**3 as w*(w*w) (the -O3 powmult)
    const Real w = wn1[ik];
    dam[ik * nth] = p.facp / (g.sig[ik] * (w * (w * w)));
    wn2[ik * nth] = wn1[ik];
  });
  if (in.it == 0) ex.for_(nspec, [&](int is) { llws[is] = 1; });  // :1074
  ex.sync();
  ex.for_(nk, [&](int ik) {  // :973-979, each row from its own first element
    const int is0 = ik * nth;
    for (int ith = 1; ith < nth; ++ith) {
      dam[ith + is0] = dam[is0];
      wn2[ith + is0] = wn2[is0];
    }
  });
  ex.sync();
  if (in.it != 0) {
    // :1080 reads LLWS before anything sets it; its only output that depends on
    // LLWS, FMEANWS, is overwritten at :1119. With TAUWBUG = 0 the :1119 call reads
    // the unset LLWS too: the caller sends that case to the Fortran (W3KOKKOS_SRCE_ON).
    spr4();
    ex.sync();
    if (sintail4 > static_cast<Real>(0.5)) {  // :1100
      sin4();
      ex.sync();
    }
  }
  spr4();  // :1119
  ex.sync();
  ex.for_(nspec, [&](int is) { specinit[is] = spec[is]; });  // :1134
  ex.one([&]() {
    o.tws = static_cast<Real>(1.) / st.fmeanws;  // :1126
    st.fage = 0;
    // :1178, FAGE = 0 kept as the build's "+ 0.0".
    st.fhigh = mx((p.ffxfm + static_cast<Real>(0.)) * mx(st.fmeanws, st.fmean1),
                  p.ffxpm / o.ustar);
    st.done = 0;
  });
  ex.sync();

  // 2. The dynamic integration loop (:1210-2042).
  while (true) {
    ex.one([&]() { st.nsteps = st.nsteps + 1; });  // :1212
    w3sln1_team(ex, p, g.sig, g.ecos, g.esin, wn1, st.fhigh, o.ustar, in.u10dir, vsln, dirf,
                wnf);  // :1223
    sin4();            // :1239
    ex.sync();
    // :1259-1263. IQTPE <= 0 (GQM) is left to the Fortran by the caller.
    snl1::snl1_team(team, nc, nt, nsig, spec, cg1, o.wnmean * st.depth, vsnl, vdnl, nw);
    ex.sync();
    st4::w3sds4_team(ex, q, f, spec, wn1, cg1, o.ustar, o.ustdir, st.depth, in.dair,
                     st.dlwmean, in.flogrd57, in.flogrd58, vsds, vdds, brlambda, whitecap, st4w,
                     st4iw);  // :1303
    ex.sync();
    bool lbreak = false;  // each thread's own: W3SRCE does not read LBREAK
    w3sdb1_team(ex, p, g.sig, g.dden, spec, st.depth, st.emean, st.fmean, o.wnmean, cg1,
                lbreak, vsdb, vddb, eb);  // :1321
    w3sbt1_team(ex, p, g.sig, g.mapwn, spec, cg1, wn1, st.depth, vsbt, vdbt, cbeta,
                bt1ik);  // :1339
    ex.sync();

    ex.one([&]() {
      // :1427-1437
      st.nkh = mni(nk, st4::fint(p.facti1 * dm::logf(mx(static_cast<Real>(1.E-7), st.fhigh)) +
                                 p.facti2));
      st.nkh1 = mni(nk, st.nkh + 1);
      st.nspech = st.nkh1 * nth;
      st.dt = mn(p.dtmax, in.dtg - st.dttot);  // :1436, minss: DTMAX<REST?DTMAX:REST
      st.afilt = mx(dam[nspec - 1], p.xflt * st.amax);
    });
    ex.sync();
    const int nspech = st.nspech;
    if (in.inflags24) {  // :1446-1453, seven element-wise array statements
      ex.for_(nspech, [&](int is) {
        vsnl[is] = st.icescalenl * vsnl[is];
        vdnl[is] = st.icescalenl * vdnl[is];
        vsln[is] = st.icescaleln * vsln[is];
        vsin[is] = st.icescalein * vsin[is];
        vdin[is] = st.icescalein * vdin[is];
        vsds[is] = st.icescaleds * vsds[is];
        vdds[is] = st.icescaleds * vdds[is];
      });
      ex.sync();
    }
    ex.for_(nspech, [&](int is) {  // :1484-1533, the terms of each IS
      vs[is] = (((vsln[is] + vsin[is]) + vsnl[is]) + vsds[is]) + vsbt[is];
      vd[is] = ((vdin[is] + vdnl[is]) + vdds[is]) + vdbt[is];
      const Real damax = mn(mx(p.xrel * specinit[is], st.afilt), dam[is]);  // :1523, as minps
      const Real afac =
          static_cast<Real>(1.) / mx(Kokkos::abs(vs[is] / damax), static_cast<Real>(1.E-10));
      // OFFSET*AFAC folded to AFAC (OFFSET = 1.): MIN(0.,VD)*AFAC + 1.
      dtc[is] = afac / mx(mn(vd[is], static_cast<Real>(0.)) * afac + static_cast<Real>(1.),
                          static_cast<Real>(1.E-10));
    });
    ex.sync();
    ex.one([&]() {
      Real dt = st.dt;
      for (int is = 0; is < nspech; ++is) dt = mn(dt, dtc[is]);  // :1532, in IS order
      dt = mx(dt, static_cast<Real>(0.5));  // :1539, the hard-coded floor
      o.dtdyn = o.dtdyn + dt;               // :1541
      const Real rest = in.dtg - st.dttot;
      const int idt = 1 + st4::fint((rest * static_cast<Real>(0.99)) / dt);  // :1545
      dt = rest / static_cast<Real>(idt);                                    // :1546
      const bool shave = dt < p.dtmin && dt < rest;                          // :1547
      o.shave = shave ? 1 : 0;                                               // :1548
      dt = mx(mn(p.dtmin, rest), dt);                                        // :1549, as maxss
      st.hdt = dt;  // :1560, OFFSET*DT
      st.dttot = st.dttot + dt;  // :1564
      st.dt = dt;
    });
    ex.sync();

    // :1761-1808, srce_call = srce_direct: the two element-wise updates of SPEC,
    // the second (VSDB, VDDB) after the first on each element, as the two loops.
    {
      const Real dt = st.dt, hdt = st.hdt;
      const bool shave = o.shave != 0;
      ex.for_(nspech, [&](int is) {
        if (shave) {
          const Real einc1 = (dt * vs[is]) / mx(static_cast<Real>(1.) - hdt * vd[is], 1);
          const Real einc2 = Kokkos::copysign(mn(Kokkos::abs(einc1), dam[is]), einc1);
          spec[is] = mx(spec[is] + einc2, static_cast<Real>(0.));
        } else {
          const Real einc1 = (dt * vs[is]) / mx(static_cast<Real>(1.) - hdt * vd[is], 1);
          spec[is] = mx(spec[is] + einc1, static_cast<Real>(0.));
        }
        const Real einc1 = (dt * vsdb[is]) / mx(static_cast<Real>(1.) - hdt * vddb[is], 1);
        spec[is] = mx(spec[is] + einc1, static_cast<Real>(0.));
      });
      // :1817-1845, the PHIAW and PHIBBL term of each IS; summed below in IK, ITH order.
      ex.for_(nspec, [&](int is) {
        const int ik = is / nth;
        const Real factor = g.dden[ik] / cg1[ik];
        tphiaw[is] = ((vsin[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdin[is] * dt, 1);
        tphibbl[is] = ((vsbt[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdbt[is] * dt, 1);
      });
    }
    ex.sync();
    ex.one([&]() {
      const Real dt = st.dt;
      whitecap[2] = 0;  // :1815
      for (int ik = 0; ik < nk; ++ik) {  // :1817-1845, IK then ITH, serial
        const Real factor = g.dden[ik] / cg1[ik];
        for (int ith = 0; ith < nth; ++ith) {
          const int is = ik * nth + ith;
          o.phiaw = tphiaw[is] + o.phiaw;
          o.phibbl = o.phibbl - tphibbl[is];
          if (vsin[is] > static_cast<Real>(0.)) whitecap[2] = whitecap[2] + spec[is] * factor;
        }
      }
      whitecap[2] = Kokkos::sqrt(whitecap[2]) * static_cast<Real>(4.);  // :1846
      o.tauwix = o.tauwix + (o.tauwx * st.drat) * dt;                  // :1848-1851
      o.tauwiy = o.tauwiy + (o.tauwy * st.drat) * dt;
      o.tauwnx = o.tauwnx + (st.drat * st.tauwax) * dt;
      o.tauwny = o.tauwny + (st.drat * st.tauway) * dt;
    });
    ex.sync();

    spr4();  // :1878
    ex.sync();
    ex.one([&]() {
      // :1932-1937
      st.fage = p.ffxfa * dm::tanhf((((in.u10abs * static_cast<Real>(0.3)) * st.fmeanws) *
                                        kTpi) / kGrav);
      const Real fh1 = (p.ffxfm + st.fage) * st.fmean1;
      const Real fh2 = p.ffxpm / o.ustar;
      st.fhigh = mn(mx(fh1, fh2), g.sig[nk - 1]);
      st.nkh = mxi(mni(st.nkh1, st4::fint(p.facti1 *
                                              dm::logf(mx(static_cast<Real>(1.E-7), st.fhigh)) +
                                          p.facti2)),
                   2);
      // :1956-1965, MLIM: the test and EM here, the rescaling of SPEC below.
      st.mlim = 0;
      if (st.dttot >= in.dtg * static_cast<Real>(0.9999)) {
        const Real hm = (p.fhmax * dm::tanhf(mx(static_cast<Real>(0.), in.d_inp) * o.wnmean)) /
                        mx(o.wnmean, static_cast<Real>(9.99999974737875163555145263671875e-5));
        const Real em = (hm * hm) * static_cast<Real>(6.25e-2);  // /16. as *0.0625
        if (st.emean > em && st.emean > static_cast<Real>(1.E-30)) {
          st.em = em;
          st.mlim = 1;
        }
      }
    });
    ex.sync();
    if (st.mlim) {
      ex.for_(nspec, [&](int is) { spec[is] = st.em * (spec[is] / st.emean); });
      ex.sync();
      ex.one([&]() { st.emean = st.em; });
    }
    // :1985-1997, the tail; "+ 0." kept (it turns -0. into +0.). Row IK reads row
    // IK-1, so the rows go in order, each split over ITH and closed by a barrier.
    for (int ik = st.nkh; ik < nk; ++ik) {
      ex.for_(nth, [&](int ith) {
        spec[ith + ik * nth] = spec[ith + (ik - 1) * nth] * p.fachfa + static_cast<Real>(0.);
      });
      ex.sync();
    }
    ex.sync();
    sin4();  // :2007
    ex.sync();
    if (sintail4 < static_cast<Real>(0.5)) {  // :2012
      spr4();
      ex.sync();
    }
    ex.one([&]() { st.done = st.dttot >= static_cast<Real>(0.9999) * in.dtg ? 1 : 0; });  // :2031
    ex.sync();
    if (st.done) break;
  }

  // 3. After the loop (:2050-2350).
  ex.one([&]() {
    o.dtdyn = o.dtdyn / static_cast<Real>(mxi(st.nsteps, 1));  // :2050
    o.fcut = st.fhigh * kTpiInv;                                // :2051
    Real efinish = 0, mwxfinish = 0, mwyfinish = 0;
    for (int ik = 0; ik < nk; ++ik) {  // :2066-2081
      Real eband = 0, a1band = 0, b1band = 0;
      for (int ith = 0; ith < nth; ++ith) {
        const Real diff = specinit[ith + ik * nth] - spec[ith + ik * nth];
        eband = diff + eband;
        a1band = g.ecos[ith] * diff + a1band;  // ECOS(ITH), not ECOS(IS), as written
        b1band = g.esin[ith] * diff + b1band;
      }
      efinish = (g.dden[ik] * eband) / cg1[ik] + efinish;
      mwxfinish = (((g.dden[ik] * a1band) / cg1[ik]) * wn1[ik]) / g.sig[ik] + mwxfinish;
      mwyfinish = (wn1[ik] * ((g.dden[ik] * b1band) / cg1[ik])) / g.sig[ik] + mwyfinish;
    }
    const Real dtg = in.dtg;
    o.tauox = ((o.tauwix + mwxfinish * kGrav) - taubbl[0]) / dtg;  // :2085
    o.tauoy = ((o.tauwiy + mwyfinish * kGrav) - taubbl[1]) / dtg;
    o.tauwix = o.tauwix / dtg;  // :2094-2098
    o.tauwiy = o.tauwiy / dtg;
    o.tauwnx = o.tauwnx / dtg;
    o.tauwny = o.tauwny / dtg;
    taubbl[0] = taubbl[0] / dtg;
    taubbl[1] = taubbl[1] / dtg;
    // :2099-2100. One sincosf in the build (__builtin_cexpif).
    Real c = 0, s = 0;
    sincos_r(o.ustdir, s, c);
    const Real k = o.ustar * (o.ustar * (in.coef * (in.dair * in.coef)));
    o.tauocx = k * c + (o.tauox - o.tauwix) * kDwat;
    o.tauocy = k * s + (o.tauoy - o.tauwiy) * kDwat;
    const Real dg = kDwat * kGrav;  // DWAT*GRAV folded to 9806.
    o.phioc = (((o.phiaw + efinish) - o.phibbl) * dg) / dtg;  // :2104
    o.phiaw = (o.phiaw * dg) / dtg;                           // :2105
    o.phibbl = (o.phibbl * dg) / dtg;                         // :2107

    // :2124-2259, the ice block without Liu's dispersion (the caller leaves
    // IICEDISP to the Fortran): ATT = 1., so SPEC is unchanged and the sums
    // below are of zeros; kept for their signed zeros.
    if (in.inflags24 && in.ice > static_cast<Real>(0.) && !p.ic_numerics) {
      for (int is = 0; is < nspec; ++is) spec2[is] = spec[is];
      tauice[0] = 0;
      tauice[1] = 0;
      o.phice = 0;
      for (int ik = 0; ik < nk; ++ik) {
        const double att = 1.;
        for (int ith = 0; ith < nth; ++ith)
          spec[ith + ik * nth] = static_cast<Real>(att * static_cast<double>(spec2[ith + ik * nth]));
        const Real factor = g.dden[ik] / cg1[ik];
        const Real factor2 = ((factor * kGrav) * wn1[ik]) / g.sig[ik];
        for (int ith = 0; ith < nth; ++ith) {
          const int is = ith + ik * nth;
          o.phice = o.phice + (spec[is] - spec2[is]) * factor;
          tauice[0] = tauice[0] - ((spec[is] - spec2[is]) * factor2) * g.ecos[is];
          tauice[1] = tauice[1] - ((spec[is] - spec2[is]) * factor2) * g.esin[is];
        }
      }
      o.phice = (((static_cast<Real>(-1.) * kDwat) * kGrav) * o.phice) / dtg;
      tauice[0] = tauice[0] / dtg;
      tauice[1] = tauice[1] / dtg;
    }
  });
  ex.sync();
  if (in.it == 0) {  // :2348
    ex.for_(nspec, [&](int is) { spec[is] = specinit[is]; });
    ex.sync();
  }
  // :2350 is a 4-wide maxps (S>0?S:0) with a scalar remainder; the third remainder
  // element is emitted 0>S?0:S, so a -0. or NaN there survives. ⚠ Tied to the
  // build's vector width (SSE, -march=x86-64). Each element's form is fixed by
  // its index, so the elements are independent.
  const int nlast = (nspec % 4 == 3) ? nspec - 1 : nspec;
  ex.for_(nspec, [&](int is) {
    spec[is] = is < nlast ? mx(spec[is], static_cast<Real>(0.)) : mx(static_cast<Real>(0.), spec[is]);
  });
}

/// w3srce_split when Split, else w3srce_team, with the same arguments: the choice at compile
/// time, outside the launch's lambda (nvcc refuses a device lambda's first capture inside an
/// if constexpr).
template <bool Split, class TeamMember>
KOKKOS_INLINE_FUNCTION void w3srce_body(const TeamMember& team, const Params& p, const Grid& g,
                                        const st4::Params& q, const st4::Fields& f,
                                        const snl1::Config& nc, const snl1::Tables& nt,
                                        const Real* nsig, const snl1::TeamScratch& nw,
                                        const PointIn& in, Real* spec, const Real* wn1,
                                        const Real* cg1, PointScalars& o, Real* whitecap,
                                        Real* taubbl, Real* tauice, State& st, Real* rw,
                                        int* iw, double* dw) {
  if constexpr (Split)
    w3srce_split(team, p, g, q, f, nc, nt, nsig, nw, in, spec, wn1, cg1, o, whitecap, taubbl,
                 tauice, st, rw, iw, dw);
  else
    w3srce_team(team, p, g, q, f, nc, nt, nsig, nw, in, spec, wn1, cg1, o, whitecap, taubbl,
                tauice, st, rw, iw, dw);
}

}  // namespace ww::srce
