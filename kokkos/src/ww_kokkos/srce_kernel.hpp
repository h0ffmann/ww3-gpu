// kokkos/src/ww_kokkos/srce_kernel.hpp
// W3SRCE, ported to Kokkos: the source-term integration of one sea point, with
// every source term it calls on the device. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3srcemd.F90:190-2412 (W3SRCE) at NOAA-EMC/WW3
// 03adda91 (WW3-noaa/), as switches/switch_lab_shrd compiles it (ST4 NL1 LN1 BT1
// DB1 MLIM, srce_call = srce_direct); author H. L. Tolman and the authors listed
// in its header. The other branches stay in the Fortran: the caller hunk
// (W3KOKKOS_SRCE_ON on the fork) and ww_srce_init refuse them.
//
// Arithmetic: the -O3 GIMPLE of w3srcemd.F90 (gfortran 16.2, WW3's Release
// flags) is the reference, statement by statement; each statement below keeps
// its operand order and every rounding. Two places differ from a literal
// reading of the source and follow the build: OFFSET*x is folded to x
// (OFFSET = 1., :69), and "+ 0." at :1995 is kept, as the build keeps it.
// ⚠ The time-step MIN over IS (:1532) is a vectorised .REDUC_MIN in the build;
// for finite values a MIN gives the same bits in any order, for NaN it may not.
// MAX/MIN: mx(a,b) = a>b?a:b and mn(a,b) = a<b?a:b, so a NaN or a +0/-0 tie gives
// b; each site passes its operands in the order of the build's maxss/minss
// (read from w3srcemd.o), which is not always the GIMPLE order.
//
// Not ported, dead under this switch: PHINL, HSTOT, FACTOR2 outside the ice
// block, COSI, TAUSCX/Y, FHIGI, FIRST, TAUNUX/Y (-O3 drops them as well, or
// nothing reads them).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "snl1_config.hpp"
#include "snl1_tables.hpp"
#include "snl1_team.hpp"
#include "srce_bt1.hpp"
#include "srce_db1.hpp"
#include "srce_ln1.hpp"
#include "srce_params.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"
#include "st4_sds4.hpp"
#include "st4_sin4.hpp"
#include "st4_spr4.hpp"

namespace ww::srce {

namespace srce_detail {
KOKKOS_INLINE_FUNCTION Real mx(Real a, Real b) { return a > b ? a : b; }  // MAX_EXPR <a, b>
KOKKOS_INLINE_FUNCTION Real mn(Real a, Real b) { return a < b ? a : b; }  // MIN_EXPR <a, b>
KOKKOS_INLINE_FUNCTION int mxi(int a, int b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION int mni(int a, int b) { return a < b ? a : b; }
}  // namespace srce_detail

/// COS and SIN of one argument as -O3 computes them, one sincosf
/// (__builtin_cexpif). ⚠ On a device, Kokkos::cos/sin: not glibc's bits.
KOKKOS_INLINE_FUNCTION void sincos_r(Real x, Real& s, Real& c) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((::sincosf(x, &s, &c);))
  KOKKOS_IF_ON_DEVICE((c = Kokkos::cos(x); s = Kokkos::sin(x);))
#else
  c = Kokkos::cos(x);
  s = Kokkos::sin(x);
#endif
}

/// The grid arrays W3SRCE and its three Fortran callees read, on the device,
/// 0-based: sig = SIG(1:NK), dden = DDEN(1:NK), ecos/esin = ECOS/ESIN(1:NSPEC),
/// mapwn = MAPWN(1:NSPEC) (values 1-based, as in the Fortran).
struct Grid {
  const Real* sig;
  const Real* dden;
  const Real* ecos;
  const Real* esin;
  const int* mapwn;
};

/// Per-point inputs W3SRCE reads (by value) and the flags of the call.
struct PointIn {
  int it, inflags24, flogrd57, flogrd58;
  Real d_inp, u10abs, u10dir, as, ice, dtg, dair, coef;
};

/// Per-point INOUT and OUT scalars, W3SRCE's dummy names in lower case.
struct PointScalars {
  Real ustar, ustdir, tauwx, tauwy, dtdyn, fcut, tauox, tauoy, tauwix, tauwiy, tauwnx,
      tauwny, phiaw, charn, tws, phioc, phibbl, phice, tauocx, tauocy, wnmean;
  int shave;
};

/// W3SRCE's local scalars that live across the barriers of the team.
struct State {
  Real depth, drat, icescaleln, icescalein, icescalenl, icescaleds;
  Real emean, fmean, fmean1, amax, cd, z0, fmeanws, dlwmean, fage, fhigh;
  Real dttot, dt, hdt, tauwax, tauway;
  int nsteps, nkh, nkh1, nspech;
  int done;
};

/// Reals of per-point work (no allocation in a kernel): 18 arrays of NSPEC
/// (DAM, WN2, VSLN, VSIN, VDIN, VSNL, VDNL, VSDS, VDDS, VSBT, VDBT, VS, VD,
/// VSDB, VDDB, SPECINIT, SPEC2, BRLAMBDA), DIRF(NTH), WNF(NK), CBETA(NK), and
/// the largest of the ST4 routines' work.
inline int srce_work_reals(const Params& p, const st4::Params& q) {
  int st4w = st4::sds4_work_reals(q);
  if (st4::sin4_work_reals(q) > st4w) st4w = st4::sin4_work_reals(q);
  if (st4::spr4_work_reals(q) > st4w) st4w = st4::spr4_work_reals(q);
  return 18 * p.nspec + p.nth + 2 * p.nk + st4w;
}
/// Ints: LLWS(NSPEC) and the largest of the ST4 routines' int work.
inline int srce_work_ints(const Params& p, const st4::Params& q) {
  int st4w = st4::sds4_work_ints(q);
  if (st4::sin4_work_ints(q) > st4w) st4w = st4::sin4_work_ints(q);
  if (st4::spr4_work_ints(q) > st4w) st4w = st4::spr4_work_ints(q);
  return p.nspec + st4w;
}
/// Doubles: EB(NK) of W3SDB1 (REAL*8, w3sdb1md.F90).
inline int srce_work_doubles(const Params& p) { return p.nk; }

/// W3SRCE(srce_direct, ...) for one point, run by the whole team. spec =
/// SPEC(1:NSPEC) INOUT, wn1 = WN1(1:NK), cg1 = CG1(1:NK), whitecap(4),
/// taubbl(2), tauice(2) INOUT; st is the team's shared state; the serial parts
/// run on one thread, W3SNL1 on the team. The ST4 work buffers are reused by
/// each ST4 call in turn: the Fortran routines keep nothing between calls.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void w3srce_team(const TeamMember& team, const Params& p, const Grid& g,
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
  Real* dirf = brlambda + nspec;
  Real* wnf = dirf + nth;
  Real* cbeta = wnf + nk;
  Real* st4w = cbeta + nk;
  int* llws = iw;
  int* st4iw = iw + nspec;
  double* eb = dw;
  const Real sintail4 = q.sintailpar[3];

  // Both ST4 calls with W3SRCE's argument lists (:1080 and :1239 forms).
  auto spr4 = [&]() {
    st4::w3spr4_point(q, f, spec, cg1, wn1, in.u10abs, in.u10dir, o.tauwx, o.tauwy, llws,
                      o.ustar, o.ustdir, st.emean, st.fmean, st.fmean1, o.wnmean, st.amax,
                      st.cd, st.z0, o.charn, st.fmeanws, st.dlwmean, st4w, st4iw);
  };
  auto sin4 = [&]() {
    st4::w3sin4_point(q, f, spec, cg1, wn2, in.u10abs, o.ustar, st.drat, in.as, in.u10dir,
                      st.z0, st.cd, o.tauwx, o.tauwy, st.tauwax, st.tauway, vsin, vdin, llws,
                      st4w, st4iw);
  };

  // 1. Initialisation (:869-1179).
  Kokkos::single(Kokkos::PerTeam(team), [&]() {
    for (int is = 0; is < nspec; ++is) {  // :877-905
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
    }
    st.depth = mx(in.d_inp, p.dmin);  // :951, maxss: D_INP>DMIN?D_INP:DMIN
    st.drat = in.dair / kDwat;        // :952
    // :953-956: MAX(0.,MIN(1.,1.-ICE*ICESCALES(i)))
    st.icescaleln = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[0], 1), 0);
    st.icescalein = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[1], 1), 0);
    st.icescalenl = mx(mn(static_cast<Real>(1.) - in.ice * p.icescales[2], 1), 0);
    // :956 alone is emitted 0>M?0:M (the three above as M>0?M:0).
    st.icescaleds = mx(0, mn(static_cast<Real>(1.) - in.ice * p.icescales[3], 1));
    for (int ik = 0; ik < nk; ++ik) {  // :968-971, WN1**3 as w*(w*w) (the -O3 powmult)
      const Real w = wn1[ik];
      dam[ik * nth] = p.facp / (g.sig[ik] * (w * (w * w)));
      wn2[ik * nth] = wn1[ik];
    }
    for (int ik = 0; ik < nk; ++ik) {  // :973-979
      const int is0 = ik * nth;
      for (int ith = 1; ith < nth; ++ith) {
        dam[ith + is0] = dam[is0];
        wn2[ith + is0] = wn2[is0];
      }
    }
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
    for (int is = 0; is < nspec; ++is) brlambda[is] = 0;
    for (int j = 0; j < 4; ++j) whitecap[j] = 0;
    // :1069: "designed to keep the bug as an option" (TAUWBUG).
    if (sintail4 > static_cast<Real>(0.5)) {
      o.tauwx = 0;
      o.tauwy = 0;
    }
    if (in.it == 0) {  // :1073-1078
      for (int is = 0; is < nspec; ++is) llws[is] = 1;
      o.tauwx = 0;
      o.tauwy = 0;
      o.ustar = 0;
      o.ustdir = 0;
    } else {
      // :1080 reads LLWS before anything sets it; its only output that depends on
      // LLWS, FMEANWS, is overwritten at :1119. With TAUWBUG = 0 the :1119 call reads
      // the unset LLWS too: the caller sends that case to the Fortran (W3KOKKOS_SRCE_ON).
      spr4();
      if (sintail4 > static_cast<Real>(0.5)) sin4();  // :1100
    }
    spr4();                                           // :1119
    o.tws = static_cast<Real>(1.) / st.fmeanws;       // :1126
    for (int is = 0; is < nspec; ++is) specinit[is] = spec[is];  // :1134
    st.fage = 0;
    // :1178, FAGE = 0 kept as the build's "+ 0.0".
    st.fhigh = mx((p.ffxfm + static_cast<Real>(0.)) * mx(st.fmeanws, st.fmean1),
                  p.ffxpm / o.ustar);
    st.done = 0;
  });
  team.team_barrier();

  // 2. The dynamic integration loop (:1210-2042).
  while (true) {
    Kokkos::single(Kokkos::PerTeam(team), [&]() {
      st.nsteps = st.nsteps + 1;  // :1212
      w3sln1_point(p, g.sig, g.ecos, g.esin, wn1, st.fhigh, o.ustar, in.u10dir, vsln, dirf,
                   wnf);  // :1223
      sin4();             // :1239
    });
    team.team_barrier();
    // :1259-1263. IQTPE <= 0 (GQM) is left to the Fortran by the caller.
    snl1::snl1_team(team, nc, nt, nsig, spec, cg1, o.wnmean * st.depth, vsnl, vdnl, nw);
    team.team_barrier();
    Kokkos::single(Kokkos::PerTeam(team), [&]() {
      st4::w3sds4_point(q, f, spec, wn1, cg1, o.ustar, o.ustdir, st.depth, in.dair,
                        st.dlwmean, in.flogrd57, in.flogrd58, vsds, vdds, brlambda, whitecap,
                        st4w, st4iw);  // :1303
      bool lbreak = false;
      w3sdb1_point(p, g.sig, g.dden, spec, st.depth, st.emean, st.fmean, o.wnmean, cg1,
                   lbreak, vsdb, vddb, eb);  // :1321; LBREAK is not read by W3SRCE
      w3sbt1_point(p, g.sig, g.mapwn, spec, cg1, wn1, st.depth, vsbt, vdbt, cbeta);  // :1339

      // :1427-1437
      st.nkh = mni(nk, st4::fint(p.facti1 * Kokkos::log(mx(static_cast<Real>(1.E-7), st.fhigh)) +
                                 p.facti2));
      st.nkh1 = mni(nk, st.nkh + 1);
      st.nspech = st.nkh1 * nth;
      Real dt = mn(p.dtmax, in.dtg - st.dttot);  // :1436, minss: DTMAX<REST?DTMAX:REST
      const Real afilt = mx(dam[nspec - 1], p.xflt * st.amax);
      const int nspech = st.nspech;
      if (in.inflags24) {  // :1446-1453
        for (int is = 0; is < nspech; ++is) vsnl[is] = st.icescalenl * vsnl[is];
        for (int is = 0; is < nspech; ++is) vdnl[is] = st.icescalenl * vdnl[is];
        for (int is = 0; is < nspech; ++is) vsln[is] = st.icescaleln * vsln[is];
        for (int is = 0; is < nspech; ++is) vsin[is] = st.icescalein * vsin[is];
        for (int is = 0; is < nspech; ++is) vdin[is] = st.icescalein * vdin[is];
        for (int is = 0; is < nspech; ++is) vsds[is] = st.icescaleds * vsds[is];
        for (int is = 0; is < nspech; ++is) vdds[is] = st.icescaleds * vdds[is];
      }
      for (int is = 0; is < nspech; ++is) {  // :1484-1533
        vs[is] = (((vsln[is] + vsin[is]) + vsnl[is]) + vsds[is]) + vsbt[is];
        vd[is] = ((vdin[is] + vdnl[is]) + vdds[is]) + vdbt[is];
        const Real damax = mn(mx(p.xrel * specinit[is], afilt), dam[is]);  // :1523, as minps
        const Real afac =
            static_cast<Real>(1.) / mx(Kokkos::abs(vs[is] / damax), static_cast<Real>(1.E-10));
        // OFFSET*AFAC folded to AFAC (OFFSET = 1.): MIN(0.,VD)*AFAC + 1.
        dt = mn(dt, afac / mx(mn(vd[is], static_cast<Real>(0.)) * afac + static_cast<Real>(1.),
                              static_cast<Real>(1.E-10)));
      }
      dt = mx(dt, static_cast<Real>(0.5));  // :1539, the hard-coded floor
      o.dtdyn = o.dtdyn + dt;               // :1541
      const Real rest = in.dtg - st.dttot;
      const int idt = 1 + st4::fint((rest * static_cast<Real>(0.99)) / dt);  // :1545
      dt = rest / static_cast<Real>(idt);                                    // :1546
      const bool shave = dt < p.dtmin && dt < rest;                          // :1547
      o.shave = shave ? 1 : 0;                                               // :1548
      dt = mx(mn(p.dtmin, rest), dt);                                        // :1549, as maxss
      const Real hdt = dt;  // :1560, OFFSET*DT
      st.dttot = st.dttot + dt;  // :1564

      // :1761-1808, srce_call = srce_direct.
      if (shave) {
        for (int is = 0; is < nspech; ++is) {
          const Real einc1 = (dt * vs[is]) / mx(static_cast<Real>(1.) - hdt * vd[is], 1);
          const Real einc2 = Kokkos::copysign(mn(Kokkos::abs(einc1), dam[is]), einc1);
          spec[is] = mx(spec[is] + einc2, static_cast<Real>(0.));
        }
      } else {
        for (int is = 0; is < nspech; ++is) {
          const Real einc1 = (dt * vs[is]) / mx(static_cast<Real>(1.) - hdt * vd[is], 1);
          spec[is] = mx(spec[is] + einc1, static_cast<Real>(0.));
        }
      }
      for (int is = 0; is < nspech; ++is) {
        const Real einc1 = (dt * vsdb[is]) / mx(static_cast<Real>(1.) - hdt * vddb[is], 1);
        spec[is] = mx(spec[is] + einc1, static_cast<Real>(0.));
      }

      whitecap[2] = 0;  // :1815
      for (int ik = 0; ik < nk; ++ik) {  // :1817-1845, IK then ITH, serial
        const Real factor = g.dden[ik] / cg1[ik];
        for (int ith = 0; ith < nth; ++ith) {
          const int is = ik * nth + ith;
          o.phiaw = ((vsin[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdin[is] * dt, 1) +
                    o.phiaw;
          o.phibbl = o.phibbl -
                     ((vsbt[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdbt[is] * dt, 1);
          if (vsin[is] > static_cast<Real>(0.)) whitecap[2] = whitecap[2] + spec[is] * factor;
        }
      }
      whitecap[2] = Kokkos::sqrt(whitecap[2]) * static_cast<Real>(4.);  // :1846
      o.tauwix = o.tauwix + (o.tauwx * st.drat) * dt;                  // :1848-1851
      o.tauwiy = o.tauwiy + (o.tauwy * st.drat) * dt;
      o.tauwnx = o.tauwnx + (st.drat * st.tauwax) * dt;
      o.tauwny = o.tauwny + (st.drat * st.tauway) * dt;
      st.dt = dt;
      st.hdt = hdt;

      spr4();  // :1878
      // :1932-1937
      st.fage = p.ffxfa * Kokkos::tanh((((in.u10abs * static_cast<Real>(0.3)) * st.fmeanws) *
                                        kTpi) / kGrav);
      const Real fh1 = (p.ffxfm + st.fage) * st.fmean1;
      const Real fh2 = p.ffxpm / o.ustar;
      st.fhigh = mn(mx(fh1, fh2), g.sig[nk - 1]);
      st.nkh = mxi(mni(st.nkh1, st4::fint(p.facti1 *
                                              Kokkos::log(mx(static_cast<Real>(1.E-7), st.fhigh)) +
                                          p.facti2)),
                   2);
      // :1956-1965, MLIM
      if (st.dttot >= in.dtg * static_cast<Real>(0.9999)) {
        const Real hm = (p.fhmax * Kokkos::tanh(mx(static_cast<Real>(0.), in.d_inp) * o.wnmean)) /
                        mx(o.wnmean, static_cast<Real>(9.99999974737875163555145263671875e-5));
        const Real em = (hm * hm) * static_cast<Real>(6.25e-2);  // /16. as *0.0625
        if (st.emean > em && st.emean > static_cast<Real>(1.E-30)) {
          for (int is = 0; is < nspec; ++is) spec[is] = em * (spec[is] / st.emean);
          st.emean = em;
        }
      }
      // :1985-1997, the tail; "+ 0." kept (it turns -0. into +0.).
      for (int ik = st.nkh; ik < nk; ++ik)
        for (int ith = 0; ith < nth; ++ith)
          spec[ith + ik * nth] = spec[ith + (ik - 1) * nth] * p.fachfa + static_cast<Real>(0.);
      sin4();                                            // :2007
      if (sintail4 < static_cast<Real>(0.5)) spr4();     // :2012
      st.done = st.dttot >= static_cast<Real>(0.9999) * in.dtg ? 1 : 0;  // :2031
    });
    team.team_barrier();
    if (st.done) break;
  }

  // 3. After the loop (:2050-2350).
  Kokkos::single(Kokkos::PerTeam(team), [&]() {
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
    if (in.it == 0)  // :2348
      for (int is = 0; is < nspec; ++is) spec[is] = specinit[is];
    // :2350 is a 4-wide maxps (S>0?S:0) with a scalar remainder; the third remainder
    // element is emitted 0>S?0:S, so a -0. or NaN there survives. ⚠ Tied to the
    // build's vector width (SSE, -march=x86-64).
    const int nlast = (nspec % 4 == 3) ? nspec - 1 : nspec;
    for (int is = 0; is < nlast; ++is) spec[is] = mx(spec[is], static_cast<Real>(0.));
    if (nlast < nspec) spec[nlast] = mx(static_cast<Real>(0.), spec[nlast]);
  });
}

}  // namespace ww::srce
