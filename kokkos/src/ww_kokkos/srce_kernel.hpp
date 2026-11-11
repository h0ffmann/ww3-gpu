// kokkos/src/ww_kokkos/srce_kernel.hpp
// W3SRCE, ported to Kokkos: the source-term integration of one sea point, with
// every source term it calls on the device. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3srcemd.F90:190-2412 (W3SRCE) at NOAA-EMC/WW3
// 03adda91 (WW3-noaa/), as switches/switch_lab_shrd compiles it (ST4 NL1 LN1 BT1
// DB1 MLIM, srce_call = srce_direct); author H. L. Tolman and the authors listed
// in its header. Three more branches are translated, each selected at run time by the flag
// the Fortran side derives from the same switch (srce_params.hpp I_LN1, I_SEED, I_FLD2): no
// W3SLN1 (VSLN stays 0, :884-886 and no call at :1222), SEED (:1970-1982) and FLD2 (CALC_FPI and
// W3FLD2, :2278-2301, with HSTOT of :1816-1847); lines of WW3-noaa/ at 03adda91. The other
// branches stay in the Fortran: the caller hunk (W3KOKKOS_SRCE_ON on the fork) and ww_srce_init
// refuse them.
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
// Not ported, dead under these switches: PHINL, FACTOR2 outside the ice
// block, COSI, TAUSCX/Y, FHIGI, FIRST, TAUNUX/Y (-O3 drops them as well, or
// nothing reads them). HSTOT is read by the FLD2 branch only; it is summed in every call of the
// kernel, which changes no other value.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "libm_device.hpp"
#include "real.hpp"
#include "snl1_config.hpp"
#include "snl1_tables.hpp"
#include "snl1_team.hpp"
#include "srce_bt1.hpp"
#include "srce_calc_fpi.hpp"
#include "srce_db1.hpp"
#include "srce_fld2.hpp"
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
/// (__builtin_cexpif); on a device, ww::dm's copy of glibc's sincosf.
KOKKOS_INLINE_FUNCTION void sincos_r(Real x, Real& s, Real& c) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((::sincosf(x, &s, &c);))
  KOKKOS_IF_ON_DEVICE((dm::dev::sincosf(x, &s, &c);))
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
  /// FPI of W3SRCE: INOUT, set by CALC_FPI under FLD2 and passed through otherwise.
  Real fpi;
  /// ww::fld2::Fld2Status bits of the W3FLD2 call (0 when none or none set).
  int fld2_status;
};

/// W3SRCE's local scalars that live across the barriers of the team.
struct State {
  Real depth, drat, icescaleln, icescalein, icescalenl, icescaleds;
  Real emean, fmean, fmean1, amax, cd, z0, fmeanws, dlwmean, fage, fhigh;
  Real dttot, dt, hdt, tauwax, tauway;
  Real hstot;  ///< HSTOT of the last step (:1816-1847), read by the FLD2 branch
  int nsteps, nkh, nkh1, nspech;
  int done;
};

/// Reals of per-point work (no allocation in a kernel): 18 arrays of NSPEC
/// (DAM, WN2, VSLN, VSIN, VDIN, VSNL, VDNL, VSDS, VDDS, VSBT, VDBT, VS, VD,
/// VSDB, VDDB, SPECINIT, SPEC2, BRLAMBDA), DIRF(NTH), WNF(NK), CBETA(NK), and
/// the largest of the ST4 routines' work (and W3FLD2's, with FLD2).
inline int srce_work_reals(const Params& p, const st4::Params& q) {
  int st4w = st4::sds4_work_reals(q);
  if (st4::sin4_work_reals(q) > st4w) st4w = st4::sin4_work_reals(q);
  if (st4::spr4_work_reals(q) > st4w) st4w = st4::spr4_work_reals(q);
  // W3FLD2 runs after the last ST4 call of the point, in the same slice.
  if (p.fld2 && ww::fld2::work_reals(p.nth, p.nktmax) > st4w)
    st4w = ww::fld2::work_reals(p.nth, p.nktmax);
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

/// One point of W3SRCE(srce_direct, ...): its arguments and its work arrays, under the names
/// the routine's locals had, and W3SRCE's sections as member functions whose bodies are the
/// scalar routine's statements, unchanged. spec = SPEC(1:NSPEC) INOUT, wn1 = WN1(1:NK), cg1 =
/// CG1(1:NK), whitecap(4), taubbl(2), tauice(2) INOUT; st is the point's state across the
/// team's barriers. The ST4 work buffers are reused by each ST4 call in turn: the Fortran
/// routines keep nothing between calls. A point reads and writes only its own arguments and
/// work arrays, so the order in which the points of a team run their sections cannot change a
/// bit (plan 2026-10-18-005, "The parity argument").
struct SrcePoint {
  const Params& p;
  const Grid& g;
  const st4::Params& q;
  const st4::Fields& f;
  const PointIn& in;
  Real* spec;
  const Real* wn1;
  const Real* cg1;
  PointScalars& o;
  Real* whitecap;
  Real* taubbl;
  Real* tauice;
  State& st;
  int nk, nth, nspec;
  Real *dam, *wn2, *vsln, *vsin, *vdin, *vsnl, *vdnl, *vsds, *vdds, *vsbt, *vdbt, *vs, *vd,
      *vsdb, *vddb, *specinit, *spec2, *brlambda, *dirf, *wnf, *cbeta, *st4w;
  int *llws, *st4iw;
  double* eb;
  Real sintail4;

  KOKKOS_INLINE_FUNCTION SrcePoint(const Params& p_, const Grid& g_, const st4::Params& q_,
                                   const st4::Fields& f_, const PointIn& in_, Real* spec_,
                                   const Real* wn1_, const Real* cg1_, PointScalars& o_,
                                   Real* whitecap_, Real* taubbl_, Real* tauice_, State& st_,
                                   Real* rw, int* iw, double* dw)
      : p(p_), g(g_), q(q_), f(f_), in(in_), spec(spec_), wn1(wn1_), cg1(cg1_), o(o_),
        whitecap(whitecap_), taubbl(taubbl_), tauice(tauice_), st(st_), nk(p_.nk),
        nth(p_.nth), nspec(p_.nspec) {
    dam = rw;
    wn2 = dam + nspec;
    vsln = wn2 + nspec;
    vsin = vsln + nspec;
    vdin = vsin + nspec;
    vsnl = vdin + nspec;
    vdnl = vsnl + nspec;
    vsds = vdnl + nspec;
    vdds = vsds + nspec;
    vsbt = vdds + nspec;
    vdbt = vsbt + nspec;
    vs = vdbt + nspec;
    vd = vs + nspec;
    vsdb = vd + nspec;
    vddb = vsdb + nspec;
    specinit = vddb + nspec;
    spec2 = specinit + nspec;
    brlambda = spec2 + nspec;
    dirf = brlambda + nspec;
    wnf = dirf + nth;
    cbeta = wnf + nk;
    st4w = cbeta + nk;
    llws = iw;
    st4iw = iw + nspec;
    eb = dw;
    sintail4 = q.sintailpar[3];
  }

  // Both ST4 calls with W3SRCE's argument lists (:1080 and :1239 forms).
  KOKKOS_INLINE_FUNCTION void spr4() const {
    st4::w3spr4_point(q, f, spec, cg1, wn1, in.u10abs, in.u10dir, o.tauwx, o.tauwy, llws,
                      o.ustar, o.ustdir, st.emean, st.fmean, st.fmean1, o.wnmean, st.amax,
                      st.cd, st.z0, o.charn, st.fmeanws, st.dlwmean, st4w, st4iw);
  }
  KOKKOS_INLINE_FUNCTION void sin4() const {
    st4::w3sin4_point(q, f, spec, cg1, wn2, in.u10abs, o.ustar, st.drat, in.as, in.u10dir,
                      st.z0, st.cd, o.tauwx, o.tauwy, st.tauwax, st.tauway, vsin, vdin, llws,
                      st4w, st4iw);
  }
  /// 1. Initialisation (:869-1179), on one thread.
  KOKKOS_INLINE_FUNCTION void init() const {
    using srce_detail::mn;
    using srce_detail::mni;
    using srce_detail::mx;
    using srce_detail::mxi;
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
  }

  /// 2. One step of the dynamic integration loop (:1210-2042), up to the DIA: step_a_sln1,
  /// then W3SIN4 (:1239).
  KOKKOS_INLINE_FUNCTION void step_a_sln1() const {
    st.nsteps = st.nsteps + 1;  // :1212
    // :1222-1224, W3_LN1 only; without it VSLN keeps the 0 of :884-886 for the whole call.
    if (p.ln1)
      w3sln1_point(p, g.sig, g.ecos, g.esin, wn1, st.fhigh, o.ustar, in.u10dir, vsln, dirf,
                   wnf);
  }
  KOKKOS_INLINE_FUNCTION void step_a() const {
    step_a_sln1();
    sin4();  // :1239
  }

  /// The step's DIA (:1259-1263), run by the whole team. IQTPE <= 0 (GQM) is left to the
  /// Fortran by the caller.
  template <class TeamMember>
  KOKKOS_INLINE_FUNCTION void dia(const TeamMember& team, const snl1::Config& nc,
                                  const snl1::Tables& nt, const Real* nsig,
                                  const snl1::TeamScratch& nw) const {
    snl1::snl1_team(team, nc, nt, nsig, spec, cg1, o.wnmean * st.depth, vsnl, vdnl, nw);
  }
  /// The rest of the step, after the DIA: step_c_main, W3SIN4 (:2007), step_c_end, which
  /// sets st.done after the last step.
  KOKKOS_INLINE_FUNCTION void step_c() const {
    step_c_main();
    sin4();  // :2007
    step_c_end();
  }
  KOKKOS_INLINE_FUNCTION void sds4() const {
    st4::w3sds4_point(q, f, spec, wn1, cg1, o.ustar, o.ustdir, st.depth, in.dair,
                      st.dlwmean, in.flogrd57, in.flogrd58, vsds, vdds, brlambda, whitecap,
                      st4w, st4iw);  // :1303
  }
  /// The same arguments, for st4::w3sds4_lanes.
  KOKKOS_INLINE_FUNCTION st4::Sds4Args sds4_args() const {
    return st4::Sds4Args{spec, wn1, cg1, o.ustar, o.ustdir, st.depth, in.dair, st.dlwmean,
                         in.flogrd57, in.flogrd58, vsds, vdds, brlambda, whitecap};
  }
  KOKKOS_INLINE_FUNCTION st4::Sds4Work sds4_work() const { return st4::sds4_layout(q, st4w, st4iw); }
  /// The step after the DIA, W3SDS4 (:1303) and then step_c_after_sds4.
  KOKKOS_INLINE_FUNCTION void step_c_main() const {
    sds4();
    step_c_after_sds4();
  }
  KOKKOS_INLINE_FUNCTION void step_c_after_sds4() const {
    using srce_detail::mn;
    using srce_detail::mni;
    using srce_detail::mx;
    using srce_detail::mxi;
    bool lbreak = false;
    w3sdb1_point(p, g.sig, g.dden, spec, st.depth, st.emean, st.fmean, o.wnmean, cg1,
                 lbreak, vsdb, vddb, eb);  // :1321; LBREAK is not read by W3SRCE
    w3sbt1_point(p, g.sig, g.mapwn, spec, cg1, wn1, st.depth, vsbt, vdbt, cbeta);  // :1339

    // :1427-1437
    st.nkh = mni(nk, st4::fint(p.facti1 * dm::logf(mx(static_cast<Real>(1.E-7), st.fhigh)) +
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
    Real hstot = 0;   // :1816
    for (int ik = 0; ik < nk; ++ik) {  // :1817-1845, IK then ITH, serial
      const Real factor = g.dden[ik] / cg1[ik];
      for (int ith = 0; ith < nth; ++ith) {
        const int is = ik * nth + ith;
        o.phiaw = ((vsin[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdin[is] * dt, 1) +
                  o.phiaw;
        o.phibbl = o.phibbl -
                   ((vsbt[is] * dt) * factor) / mx(static_cast<Real>(1.) - vdbt[is] * dt, 1);
        if (vsin[is] > static_cast<Real>(0.)) whitecap[2] = whitecap[2] + spec[is] * factor;
        hstot = hstot + spec[is] * factor;  // :1843
      }
    }
    whitecap[2] = Kokkos::sqrt(whitecap[2]) * static_cast<Real>(4.);  // :1846
    st.hstot = Kokkos::sqrt(hstot) * static_cast<Real>(4.);           // :1847
    o.tauwix = o.tauwix + (o.tauwx * st.drat) * dt;                  // :1848-1851
    o.tauwiy = o.tauwiy + (o.tauwy * st.drat) * dt;
    o.tauwnx = o.tauwnx + (st.drat * st.tauwax) * dt;
    o.tauwny = o.tauwny + (st.drat * st.tauway) * dt;
    st.dt = dt;
    st.hdt = hdt;

    spr4();  // :1878
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
    // :1956-1965, MLIM
    if (st.dttot >= in.dtg * static_cast<Real>(0.9999)) {
      const Real hm = (p.fhmax * dm::tanhf(mx(static_cast<Real>(0.), in.d_inp) * o.wnmean)) /
                      mx(o.wnmean, static_cast<Real>(9.99999974737875163555145263671875e-5));
      const Real em = (hm * hm) * static_cast<Real>(6.25e-2);  // /16. as *0.0625
      if (st.emean > em && st.emean > static_cast<Real>(1.E-30)) {
        for (int is = 0; is < nspec; ++is) spec[is] = em * (spec[is] / st.emean);
        st.emean = em;
      }
    }
    // :1970-1982, SEED: the floor of the spectrum above the cut-off. UC = FACSD*GRAV/SIG is one
    // product and one quotient; WN1**3 is (w*w)*w; MAX(0.,x) and MIN(1.,.) are maxss/minss with
    // the operands of the object (x first, then 0 or 1); the spectrum MAX takes the new value
    // first, so a tie (or a NaN) keeps the old one.
    if (p.seed) {
      for (int ik = mni(nk, st.nkh); ik <= nk; ++ik) {
        const Real uc = (p.facsd * kGrav) / g.sig[ik - 1];
        const Real w = wn1[ik - 1];
        Real slev = ((mn(mx(in.u10abs / uc - static_cast<Real>(1.), static_cast<Real>(0.)),
                         static_cast<Real>(1.)) *
                      6.25E-4F) /
                     ((w * w) * w)) /
                    g.sig[ik - 1];
        if (in.inflags24) slev = (static_cast<Real>(1.) - in.ice) * slev;
        for (int ith = 1; ith <= nth; ++ith) {
          const Real c = mx(dm::cosf(in.u10dir - f.th(ith - 1)), static_cast<Real>(0.));
          const int is = ith + (ik - 1) * nth - 1;
          spec[is] = mx(slev * (c * c), spec[is]);
        }
      }
    }
    // :1985-1997, the tail; "+ 0." kept (it turns -0. into +0.).
    for (int ik = st.nkh; ik < nk; ++ik)
      for (int ith = 0; ith < nth; ++ith)
        spec[ith + ik * nth] = spec[ith + (ik - 1) * nth] * p.fachfa + static_cast<Real>(0.);
  }
  KOKKOS_INLINE_FUNCTION void step_c_end() const {
    if (sintail4 < static_cast<Real>(0.5)) spr4();     // :2012
    st.done = st.dttot >= static_cast<Real>(0.9999) * in.dtg ? 1 : 0;  // :2031
  }

  /// 3. After the loop (:2050-2350).
  KOKKOS_INLINE_FUNCTION void finish() const {
    using srce_detail::mn;
    using srce_detail::mni;
    using srce_detail::mx;
    using srce_detail::mxi;
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
    // :2278-2301, FLD2: the equivalent peak frequency of the input, then the wind stress of the
    // wave spectrum from W3FLD2 above 10 m/s and a significant wave height of 0.5 m, else the
    // constant FLDALPHA as Charnock parameter. FPI/TPI is MIN(.,2.) with FPI/TPI first (minss);
    // COEF*U10ABS is one product, COS and SIN of U10DIR one sincosf. WNF (NK) is CALC_FPI's work.
    if (p.fld2) {
      calc_fpi(spec, cg1, o.fpi, vsin, nk, nth, q.xfr, g.sig, g.dden, q.fte, p.fttr, wnf);
      if (in.u10abs > static_cast<Real>(10.) && st.hstot > static_cast<Real>(0.5)) {
        const Real fpia = mn(o.fpi / kTpi, static_cast<Real>(2.));
        Real su = 0, cu = 0;
        sincos_r(in.u10dir, su, cu);
        const Real cw = in.u10abs * in.coef;
        o.fld2_status = ww::fld2::w3fld2(spec, fpia, cw * cu, cw * su, q.zzwnd, st.depth, in.dair,
                                         nk, nth, q.xfr, q.dth, g.sig, f.th, p.tail_id,
                                         p.tail_lev, p.tail_tran1, p.tail_tran2, o.ustar,
                                         o.ustdir, o.charn, st4w, p.nktmax);
      } else {
        o.charn = p.fldalpha;
      }
    }
    if (in.it == 0)  // :2348
      for (int is = 0; is < nspec; ++is) spec[is] = specinit[is];
    // :2350 is a 4-wide maxps (S>0?S:0) with a scalar remainder; the third remainder
    // element is emitted 0>S?0:S, so a -0. or NaN there survives. ⚠ Tied to the
    // build's vector width (SSE, -march=x86-64).
    const int nlast = (nspec % 4 == 3) ? nspec - 1 : nspec;
    for (int is = 0; is < nlast; ++is) spec[is] = mx(spec[is], static_cast<Real>(0.));
    if (nlast < nspec) spec[nlast] = mx(static_cast<Real>(0.), spec[nlast]);
  }
};

/// W3SRCE(srce_direct, ...) for one point, run by the whole team: the serial parts on one
/// thread, W3SNL1 on the team.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void w3srce_team(const TeamMember& team, const Params& p, const Grid& g,
                                        const st4::Params& q, const st4::Fields& f,
                                        const snl1::Config& nc, const snl1::Tables& nt,
                                        const Real* nsig, const snl1::TeamScratch& nw,
                                        const PointIn& in, Real* spec, const Real* wn1,
                                        const Real* cg1, PointScalars& o, Real* whitecap,
                                        Real* taubbl, Real* tauice, State& st, Real* rw,
                                        int* iw, double* dw) {
  const SrcePoint x(p, g, q, f, in, spec, wn1, cg1, o, whitecap, taubbl, tauice, st, rw, iw, dw);
  Kokkos::single(Kokkos::PerTeam(team), [&]() { x.init(); });
  team.team_barrier();
  while (true) {
    Kokkos::single(Kokkos::PerTeam(team), [&]() { x.step_a(); });
    team.team_barrier();
    x.dia(team, nc, nt, nsig, nw);
    team.team_barrier();
    Kokkos::single(Kokkos::PerTeam(team), [&]() { x.step_c(); });
    team.team_barrier();
    if (st.done) break;
  }
  Kokkos::single(Kokkos::PerTeam(team), [&]() { x.finish(); });
}

/// The largest number of points one team of w3srce_lanes takes.
inline constexpr int kSrceMaxLanes = 8;

/// W3SRCE for the `nl` points of `x` (1 <= nl <= kSrceMaxLanes), run by the whole team: each
/// section runs for every point still in the loop, one point after the other, and each point's
/// DIA on the whole team in the team's scratch. Every point runs the statements and the number
/// of steps it runs alone in w3srce_team; the points only take turns.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void w3srce_lanes(const TeamMember& team, const snl1::Config& nc,
                                         const snl1::Tables& nt, const Real* nsig,
                                         const snl1::TeamScratch& nw, const SrcePoint* x,
                                         const int nl) {
  Kokkos::single(Kokkos::PerTeam(team), [&]() {
    for (int l = 0; l < nl; ++l) x[l].init();
  });
  team.team_barrier();
  // W3SIN4 for the points listed in `on`, one point per vector lane (st4::w3sin4_lanes), with
  // each point's arguments of SrcePoint::sin4.
  const auto sin4_lanes = [&](const int* on, const int n) {
    if (n == 0) return;
    const Real *a[kSrceMaxLanes] = {}, *cg[kSrceMaxLanes] = {}, *k[kSrceMaxLanes] = {};
    Real u[kSrceMaxLanes] = {}, ustar[kSrceMaxLanes] = {}, drat[kSrceMaxLanes] = {},
         as[kSrceMaxLanes] = {}, usdir[kSrceMaxLanes] = {}, z0[kSrceMaxLanes] = {};
    Real *tauwx[kSrceMaxLanes] = {}, *tauwy[kSrceMaxLanes] = {}, *tauwnx[kSrceMaxLanes] = {},
         *tauwny[kSrceMaxLanes] = {}, *s[kSrceMaxLanes] = {}, *d[kSrceMaxLanes] = {},
         *work[kSrceMaxLanes] = {};
    int* llws[kSrceMaxLanes] = {};
    for (int j = 0; j < n; ++j) {
      const SrcePoint& y = x[on[j]];
      a[j] = y.spec;
      cg[j] = y.cg1;
      k[j] = y.wn2;
      u[j] = y.in.u10abs;
      ustar[j] = y.o.ustar;
      drat[j] = y.st.drat;
      as[j] = y.in.as;
      usdir[j] = y.in.u10dir;
      z0[j] = y.st.z0;
      tauwx[j] = &y.o.tauwx;
      tauwy[j] = &y.o.tauwy;
      tauwnx[j] = &y.st.tauwax;
      tauwny[j] = &y.st.tauway;
      s[j] = y.vsin;
      d[j] = y.vdin;
      llws[j] = y.llws;
      work[j] = y.st4w;
    }
    st4::w3sin4_lanes<kSrceMaxLanes>(x[on[0]].q, x[on[0]].f, n, a, cg, k, u, ustar, drat, as,
                                     usdir, z0, tauwx, tauwy, tauwnx, tauwny, s, d, llws, work);
  };
  // W3SDS4 for the points listed in `on`, one point per vector lane (st4::w3sds4_lanes).
  const auto sds4_lanes = [&](const int* on, const int n) {
    if (n == 0) return;
    st4::Sds4Args pt[kSrceMaxLanes];
    st4::Sds4Work wk[kSrceMaxLanes];
    for (int j = 0; j < n; ++j) {
      pt[j] = x[on[j]].sds4_args();
      wk[j] = x[on[j]].sds4_work();
    }
    st4::w3sds4_lanes<kSrceMaxLanes>(x[on[0]].q, x[on[0]].f, n, pt, wk);
  };
  while (true) {
    // st.done changes only in step_c_end, so the points in the loop are the same for the whole
    // step.
    int on[kSrceMaxLanes];
    int non = 0;
    for (int l = 0; l < nl; ++l)
      if (!x[l].st.done) on[non++] = l;
    Kokkos::single(Kokkos::PerTeam(team), [&]() {
      for (int j = 0; j < non; ++j) x[on[j]].step_a_sln1();
      sin4_lanes(on, non);  // :1239
    });
    team.team_barrier();
    for (int j = 0; j < non; ++j) {
      x[on[j]].dia(team, nc, nt, nsig, nw);
      team.team_barrier();
    }
    Kokkos::single(Kokkos::PerTeam(team), [&]() {
      sds4_lanes(on, non);  // :1303
      for (int j = 0; j < non; ++j) x[on[j]].step_c_after_sds4();
      sin4_lanes(on, non);  // :2007
      for (int j = 0; j < non; ++j) x[on[j]].step_c_end();
    });
    team.team_barrier();
    bool all = true;
    for (int l = 0; l < nl; ++l) all = all && x[l].st.done != 0;
    if (all) break;
  }
  Kokkos::single(Kokkos::PerTeam(team), [&]() {
    for (int l = 0; l < nl; ++l) x[l].finish();
  });
}

}  // namespace ww::srce
