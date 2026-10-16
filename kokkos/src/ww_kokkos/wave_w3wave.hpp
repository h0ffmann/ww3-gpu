// kokkos/src/ww_kokkos/wave_w3wave.hpp
// W3WAVE, ported to Kokkos: WW3's time-step driver for one grid, from the
// current TIME to TEND. A host-side sequencer: forcing updates, the CFL
// sub-step counts, the intra-spectral / spatial / intra-spectral split of the
// propagation, the source terms, the output and restart triggers, and the
// integer-date bookkeeping, in the Fortran's order. Phase 1, translated, not
// wired into WW3, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3wavemd.F90:229-2997 (W3WAVE) at
// NOAA-EMC/WW3 03adda91; author H. L. Tolman and the authors in its header.
// Only what switches/switch_lab_shrd compiles is ported: SHRD (no MPI, no
// OMPG/OMPH, no PDLIB bodies), PR3 + UQ, ST4 NL1 LN1 BT1 DB1 MLIM through
// srce_kernel.hpp, WNT1/RWND through updt_uwnd.hpp. W3_SEC1, W3_REFRX, W3_IC3,
// W3_SMC, W3_OASIS, W3_SBS, W3_TIMINGS, W3_DEBUG*, W3_T and W3_S are not
// compiled there and not ported. GTYPE = SMCTYPE is still a run-time value
// (ww3_grid sets it for 'SMCG' without W3_SMC, w3gridmd.F90:3594-3596): every
// SMC body is empty and FIELD is never allocated (:756-764), so the Fortran
// runs until FIELD = 0. on the unallocated array (:1942), which is undefined;
// the driver translates the empty branches and returns kErrUnsupported there.
//
// Callees. Translated ones are called here, on the device: W3DZXY
// (updt_dzxy.hpp), W3UWND (updt_uwnd.hpp), W3MAP3, W3UTRN, W3MAPT
// (prop_map3.hpp, updt_utrn.hpp, prop_mapt.hpp), W3CFLXY (prop_cflxy.hpp),
// W3KTP3 (prop_ktp3.hpp), W3GATH, W3SCAT (wave_gath_scat.hpp), W3XYP3
// (prop_xyp3.hpp), W3SRCE (srce_kernel.hpp). The rest, including DSEC21 and
// TICK21, go through wave_w3wave_hooks.hpp at the same place in the sequence.
//
// Arithmetic: the -O3 GIMPLE and object of w3wavemd.F90 (gfortran 16.2, WW3's
// CMake Release flags, switch_lab_shrd; byte-identical to the CMake object)
// are the reference. W3WAVE's own floating point is a few host scalars:
//   :1094, :1771  INT(x/y - 0.001), cvttss2si: st4::fint
//   :1147, :1784  NINT, a call of lroundf truncated to INTEGER(4): st4::nint
//   :1147-1148    s = DTGA + DTRES once (the GIMPLE has one sum for both lines;
//                 IEEE addition commutes), DTG = REAL(NINT(s + 0.0001)), DTRES = s - DTG
//   :1167         MAX(1., DTTST2) is maxss DTTST2,1.: 1. > DTTST2 ? 1. : DTTST2
//   :1168-1169    one sincosf (__builtin_cexpif) for COS and SIN of one angle
//   :1819, :2131  MAX(DMIN, DW(ISEA)) is maxss DMIN,DW: DW > DMIN ? DW : DMIN
//   :753          SIG(NK) * TPIINV, TPIINV folded to 0x3e22f983
//   :1343 etc.    DTTST .LE. 0.5*DTI0 is 0.5*DTI0 >= DTTST, same for NaN
// No FMA (contraction off), no vectorised loop of W3WAVE's own.
//
// Parallel launches give the serial bits because each iterates over points
// that are independent in the Fortran too: W3CFLXY, W3KTP3 and W3SRCE write only
// their own JSEA column and read module arrays the loop does not write (the
// same loops are !$OMP PARALLEL DO under OMPG, :1697, :1798, :2241). The
// spatial propagation keeps the Fortran's serial ISPEC loop, one W3XYP3
// (a serial body, prop_xyp3.hpp) per bin between W3GATH and W3SCAT.
#pragma once

#include <cmath>
#include <cstddef>
#include <type_traits>

#include <Kokkos_Core.hpp>

#include "prop_cflxy.hpp"
#include "prop_ktp3.hpp"
#include "prop_map3.hpp"
#include "prop_mapt.hpp"
#include "prop_params.hpp"
#include "prop_xyp3.hpp"
#include "real.hpp"
#include "snl1_team.hpp"
#include "srce_kernel.hpp"
#include "st4_params.hpp"
#include "updt_dzxy.hpp"
#include "updt_utrn.hpp"
#include "updt_uwnd.hpp"
#include "wave_gath_scat.hpp"
#include "wave_w3wave_hooks.hpp"
#include "wave_w3wave_state.hpp"

namespace ww::wave::w3wave {

/// Return codes. WW3 has none: EXTCDE stops the program.
inline constexpr int kOk = 0;
inline constexpr int kErrHook = 1;         ///< a required hook is null
inline constexpr int kErrExtcde = 2;       ///< EXTCDE was called and returned
inline constexpr int kErrUnsupported = 3;  ///< GTYPE = SMCTYPE reached :1942 (see the header)

namespace detail {

// Literals as the GIMPLE holds them (REAL constants of w3wavemd.F90).
inline constexpr Real k1em3 = static_cast<Real>(0.001);   // :1094, :1149, :1771
inline constexpr Real k1em4 = static_cast<Real>(0.0001);  // :1147
inline constexpr Real kHalf = static_cast<Real>(0.5);     // :1343, :1373, :1426
static_assert(k1em3 == 1.000000047497451305389404296875e-3f, "0.001 as REAL");
static_assert(k1em4 == 9.99999974737875163555145263671875e-5f, "0.0001 as REAL");
static_assert(st4::kTpiInv == 1.5915493667125701904296875e-1f, "TPIINV as folded at :753");
// FACX = 1./(DERA*RADIUS) (:769), folded by gfortran (the :1894 store holds it).
inline constexpr Real kFacxLL = 9.0000003183376975357532501220703125e-6f;
inline constexpr Real kDair = static_cast<Real>(1.225);  // DAIR, constants.F90:64, :1280

/// MAX_EXPR as the object's maxss computes it: a > b ? a : b (a NaN `a` gives b).
KOKKOS_INLINE_FUNCTION Real mx(Real a, Real b) { return a > b ? a : b; }

/// COS and SIN of one angle as :1168-1169 computes them: one sincosf.
inline void sincos_host(Real x, Real& s, Real& c) {
#if defined(__GLIBC__)
  ::sincosf(x, &s, &c);
#else
  c = std::cos(x);
  s = std::sin(x);
#endif
}

/// CHARACTER(LEN=N), blank padded, as IDACT (21) and OUTID (16).
template <int N>
struct Text {
  char c[N];
  void blank() {
    for (int i = 0; i < N; ++i) c[i] = ' ';
  }
  /// Fortran X(i:i) = ch, i 1-based.
  void set(int i, char ch) { c[i - 1] = ch; }
  char at(int i) const { return c[i - 1]; }
};

inline void copy2(int* d, const int* s) {
  d[0] = s[0];
  d[1] = s[1];
}

inline bool hooks_complete(const ww_w3wave_hooks& h) {
  return h.dsec21 && h.tick21 && h.set_model && h.extcde && h.write_error && h.log_backspace &&
         h.screen_step && h.screen_end && h.log_step && h.log_footer && h.w3ucur &&
         h.ug_gradients && h.w3uini && h.w3utau && h.w3urho && h.w3iobc_read && h.w3ubpt &&
         h.w3uice && h.w3uic1 && h.w3ulev && h.w3cflug && h.w3xypug && h.srce_on_kokkos &&
         h.w3srce_fortran &&
         h.w3cprt && h.w3outg && h.w3iogo && h.w3iope && h.w3iopo && h.w3iotr && h.w3iors_hot &&
         h.w3iobc_write && h.w3iosf;
}

}  // namespace detail

/// W3WAVE(IMOD, ODAT, TEND, STAMP, NO_OUT) for grid IMOD under switch_lab_shrd.
/// p, m, a, c describe grid IMOD (wave_w3wave_state.hpp); h the Fortran hooks.
/// odat is ODAT(35), read only under W3_OASIS and so never read here. stamp
/// and no_out are the OPTIONAL LOGICALs: null when not PRESENT (:713-723).
template <class ExecSpace>
int w3wave(const ExecSpace& exec, const Params& p, const Module& m,
           const Arrays<typename ExecSpace::memory_space>& a, Callees& c,
           const ww_w3wave_hooks& h, int imod, [[maybe_unused]] const int* odat,
           const int* tend, const int* stamp, const int* no_out) {
  using Mem = typename ExecSpace::memory_space;
  static_assert(std::is_same_v<Mem, Kokkos::DefaultExecutionSpace::memory_space>,
                "W3GATH/W3SCAT and W3SRCE take Views of the default memory space");
  using detail::mx;
  using st4::fint;
  using st4::nint;
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, Mem>;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, Mem>;
  using CI1 = Kokkos::View<const int*, Kokkos::LayoutLeft, Mem>;
  using TeamPolicy = Kokkos::TeamPolicy<ExecSpace>;
  using Member = typename TeamPolicy::member_type;
  using ScratchR =
      Kokkos::View<Real*, typename ExecSpace::scratch_memory_space, Kokkos::MemoryUnmanaged>;

  if (!detail::hooks_complete(h)) return kErrHook;

  void* const ctx = h.ctx;
  const int nx = p.nx, ny = p.ny, nsea = p.nsea, nseal = p.nseal, nk = p.nk, nth = p.nth,
            nspec = p.nspec;
  const bool ung = p.gtype == kUngtype;
  const bool smc = p.gtype == kSmctype;  // its W3_SMC bodies are not compiled: empty branches
  const bool lpdlib = p.lpdlib != 0;

  auto dsec = [&](const int* t1, const int* t2) { return h.dsec21(ctx, t1, t2); };
  auto memcheck = [&](int unit, const char* tag) {
    if (h.memcheck) h.memcheck(ctx, unit, tag);
  };
  // A Fortran hook that may read or write module arrays: fence, sync, call, sync.
  auto fortran = [&](int id, auto&& call) {
    exec.fence("ww::wave::w3wave before a Fortran hook");
    if (h.sync_to_host) h.sync_to_host(ctx, id);
    call();
    if (h.sync_to_device) h.sync_to_device(ctx, id);
  };
  // IF (IAPROC .EQ. NAPERR) WRITE (NDSE,label); CALL EXTCDE(code).
  auto stop = [&](int label, int code) {
    if (p.iaproc == p.naperr) h.write_error(ctx, label);
    h.extcde(ctx, code);
    return kErrExtcde;
  };
  auto flogrd = [&](int i, int j) { return m.flogrd[(j - 1) * kNogrp + (i - 1)] != 0; };
  auto flogr2 = [&](int i, int j) { return m.flogr2[(j - 1) * kNogrp + (i - 1)] != 0; };
  auto tonext = [&](int j) { return m.tonext + 2 * (j - 1); };
  auto tolast = [&](int j) { return m.tolast + 2 * (j - 1); };

  // Views of the same memory under the shapes the callees take.
  const CI1 mapsta_flat(a.mapsta.data(), static_cast<std::size_t>(ny) * nx);
  auto sea = [&](const R1& v) { return Kokkos::subview(v, Kokkos::make_pair(1, nsea + 1)); };

  // 0. Initialisations (:686-785).
  Real xxx = p.undef;                   // :686
  const int memunit = 40000 + p.iaproc;  // :687
  h.set_model(ctx, imod);               // :694-698

  R1 tauwx("ww::wave::w3wave.tauwx", nseal);  // :708 ALLOCATE TAUWX, TAUWY
  R1 tauwy("ww::wave::w3wave.tauwy", nseal);

  const bool tstamp = stamp ? *stamp != 0 : true;   // :713-717
  const bool skip_o = no_out ? *no_out != 0 : false;  // :719-723

  *m.ipass = *m.ipass + 1;  // :737
  detail::Text<21> idact;
  detail::Text<16> outid;
  idact.blank();  // :738
  outid.blank();  // :739
  bool flact = *m.itime == 0;  // :740
  bool flmap = *m.itime == 0;  // :741
  bool flddir = *m.itime == 0 && (p.flcth || p.fsrefraction || p.flck || p.fsfreqshift);  // :742

  bool flpfld = false;  // :744-747
  for (int j = 1; j <= m.noge[3]; ++j) flpfld = flpfld || flogrd(4, j) || flogr2(4, j);

  if (p.iaproc == p.naplog) h.log_backspace(ctx);  // :749

  if (*m.flcold != 0) {  // :751-754
    Kokkos::deep_copy(exec, a.dtdyn, Real(0));
    Kokkos::deep_copy(exec, a.fcut, p.sig_nk * st4::kTpiInv);
  }

  // :756-764: FIELD(1-NY:NY*(NX+2)), NY*(NX+3) entries; field(I + NY - 1).
  // Not allocated for SMCTYPE.
  R1 field(Kokkos::view_alloc("ww::wave::w3wave.field", Kokkos::WithoutInitializing),
           smc ? std::size_t{0} : static_cast<std::size_t>(ny) * (nx + 3));

  const bool local = p.iaproc <= p.naproc;  // :766
  int ugdtupdate = 0;                       // :767, INOUT of W3XYPUG
  Real facx = p.flagll ? detail::kFacxLL : Real(1);  // :768-772

  Kokkos::deep_copy(exec, tauwx, Real(0));  // :784
  Kokkos::deep_copy(exec, tauwy, Real(0));  // :785

  // 1. Check the times of the forcing fields (:797-947).
  Real dttst = dsec(m.time, tend);  // :797
  const bool flzero = dttst == Real(0);
  if (dttst < Real(0)) return stop(WW_W3WAVE_ERR_TEND, 1);  // :802-805

  Real dtl0 = 0;  // :809-824
  if (p.fllev) {
    dtl0 = m.tlev[0] >= 0 ? dsec(m.tlev, m.tln) : Real(1);
    if (dtl0 < Real(0)) return stop(WW_W3WAVE_ERR_LEVEL, 2);
  }

  Real dttst1 = 0, dttst2 = 0, dttst3 = 0;
  // :831-865, :894-928: the same check for currents, wind, momentum, air density.
  auto check_interval = [&](const int* t0, const int* tn, int label, int code, int pos) {
    dttst1 = dsec(t0, tn);
    dttst2 = dsec(t0, m.time);
    dttst3 = dsec(tend, tn);
    if (dttst1 < Real(0) || dttst2 < Real(0) || dttst3 < Real(0)) return stop(label, code);
    if (dttst2 == Real(0) && *m.itime == 0) {
      idact.set(pos, 'F');
      detail::copy2(m.tofrst, m.time);
    }
    return kOk;
  };
  if (p.flcur && check_interval(m.tc0, m.tcn, WW_W3WAVE_ERR_CURRENT, 3, 7) != kOk)
    return kErrExtcde;
  if (p.flwind && check_interval(m.tw0, m.twn, WW_W3WAVE_ERR_WIND, 4, 3) != kOk)
    return kErrExtcde;

  Real dti0 = 0;  // :872-887
  if (p.flice) {
    dti0 = m.tice[0] >= 0 ? dsec(m.tice, m.tin) : Real(1);
    if (dti0 < Real(0)) return stop(WW_W3WAVE_ERR_ICE, 5);
  }
  if (p.fltaua && check_interval(m.tu0, m.tun, WW_W3WAVE_ERR_TAUA, 3, 9) != kOk)
    return kErrExtcde;
  if (p.flrhoa && check_interval(m.tr0, m.trn, WW_W3WAVE_ERR_RHOA, 2, 11) != kOk)
    return kErrExtcde;

  Real dti10 = 0;  // :932-947
  if (p.flic1) {
    dti10 = m.tic1[0] >= 0 ? dsec(m.tic1, m.ti1) : Real(1);
    if (dti10 < Real(0)) return stop(WW_W3WAVE_ERR_IC1, 5);
  }

  // Work of the device callees, allocated once per call (no allocation in a kernel).
  R1 xy_wr("ww::wave::w3wave.xyp3_wr", prop::xyp3_work_reals(nx, ny, nsea));
  I1 xy_wi("ww::wave::w3wave.xyp3_wi", prop::xyp3_work_ints(nx, ny, nsea));
  const int srce_l = nseal > 0 ? (c.srce_league < 1 ? 1 : (c.srce_league < nseal ? c.srce_league : nseal)) : 1;
  const int srce_wr = srce::srce_work_reals(c.sp, c.q);
  const int srce_wi = srce::srce_work_ints(c.sp, c.q);
  const int srce_wd = srce::srce_work_doubles(c.sp);
  R1 srce_rw, srce_tmp;
  I1 srce_iw;
  Kokkos::View<double*, Mem> srce_dw;
  Kokkos::View<srce::PointScalars*, Mem> srce_sc;
  Kokkos::View<srce::State*, Mem> srce_st;

  // 2. Loop over output intervals (:973-2895).
  bool flfrst = true;  // :973
  int tcalc[2];
  int tout[2];
  int tlst[2];
  int itest = 0;
  for (;;) {
    // :1008, :1023 JSEA = 1, ISEA = JSEA: dead stores of locals (no IC3), not kept.

    // 2.c Time steps to the next output or the end (:1081-1105).
    dttst = m.tofrst[0] > 0 ? dsec(tend, m.tofrst) : Real(0);
    if (dttst >= Real(0))
      detail::copy2(tcalc, tend);
    else
      detail::copy2(tcalc, m.tofrst);
    dttst = dsec(m.time, tcalc);                     // :1093
    int nt = 1 + fint(dttst / p.dtmax - detail::k1em3);  // :1094
    const Real dtga = dttst / static_cast<Real>(nt);     // :1095
    int it0;
    if (dttst == Real(0)) {
      it0 = 0;
      if (!flzero) {
        *m.itime = *m.itime - 1;
        *m.itstep = *m.itstep - 1;
      }
      nt = 0;
    } else {
      it0 = 1;
    }
    memcheck(memunit, "memcheck_____: WW3_WAVE");  // :1106

    Real dtres = 0;  // :1116
    Real dtg = 0;
    Real vgx = 0, vgy = 0;
    for (int it = it0; it <= nt; ++it) {  // :1119
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 0");
      *m.itime = *m.itime + 1;    // :1144
      *m.itstep = *m.itstep + 1;  // :1145
      const Real s = dtga + dtres;                        // :1147 DTGA+DTRES, :1148 the same sum
      dtg = static_cast<Real>(nint(s + detail::k1em4));   // :1147
      dtres = s - dtg;                                    // :1148
      if (std::fabs(dtres) < detail::k1em3) dtres = 0;    // :1149
      h.tick21(ctx, m.time, dtg);                         // :1150
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 1");
      if (tstamp && p.screen != p.ndso && p.iaproc == p.napout) h.screen_step(ctx, m.time);
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 2");

      // 3.1 Moving-grid speed (:1162-1170).
      vgx = 0;
      vgy = 0;
      if (p.inflags1_10) {
        dttst1 = dsec(m.time, m.tgn);
        dttst2 = dsec(m.tg0, m.tgn);
        const Real fac = dttst1 / mx(Real(1), dttst2);
        const Real om = Real(1) - fac;
        const Real speed = (fac * *m.ga0) + (om * *m.gan);
        const Real dir = (fac * *m.gd0) + (om * *m.gdn);
        Real sn = 0, cs = 0;
        detail::sincos_host(dir, sn, cs);
        vgx = speed * cs;
        vgy = speed * sn;
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 3a");

      // 3.2 Currents (:1187-1224).
      if (p.flcur) {
        fortran(WW_W3WAVE_W3UCUR, [&] { h.w3ucur(ctx, flfrst ? 1 : 0); });
        memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 3b");
        if (smc) {
          // :1198-1203, W3SMCDCXY not compiled.
        } else if (ung) {
          fortran(WW_W3WAVE_UG_GRADIENTS, [&] { h.ug_gradients(ctx, WW_W3WAVE_UG_CX); });
          fortran(WW_W3WAVE_UG_GRADIENTS, [&] { h.ug_gradients(ctx, WW_W3WAVE_UG_CY); });
          ugdtupdate = 1;
          Kokkos::deep_copy(exec, a.cflxymax, Real(0));
        } else {
          updt::w3dzxy<ExecSpace>(exec, sea(a.cx), a.mapsta, a.mapfs, a.dpdx, a.dpdy, a.dqdx, a.dqdy,
                       p.flagll != 0, p.iclose, a.dcxdx, a.dcxdy);  // :1213
          updt::w3dzxy<ExecSpace>(exec, sea(a.cy), a.mapsta, a.mapfs, a.dpdx, a.dpdy, a.dqdx, a.dqdy,
                       p.flagll != 0, p.iclose, a.dcydx, a.dcydy);  // :1214
        }
        memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 4");
      } else if (flfrst) {  // :1219-1224
        ugdtupdate = 1;
        Kokkos::deep_copy(exec, a.cflxymax, Real(0));
        Kokkos::deep_copy(exec, a.cx, Real(0));
        Kokkos::deep_copy(exec, a.cy, Real(0));
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 5");

      // 3.3 Wind (:1231-1239).
      if (p.flwind) {
        if (flfrst) Kokkos::deep_copy(exec, a.asf, Real(1));  // :1232
        const Real dt0n = dsec(m.tw0, m.twn);   // w3updtmd.F90:677
        const Real dt0t = dsec(m.tw0, m.time);  // w3updtmd.F90:678
        updt::w3uwnd<ExecSpace>(exec, flfrst, dt0n, dt0t, p.flcur != 0, c.rwndc, a.mapsf, a.wx0, a.wy0,
                     a.dt0, a.wxn, a.wyn, a.dtn, sea(a.cx), sea(a.cy), a.ua0, a.uai, a.ud0,
                     a.udi, a.as0, a.asi, sea(a.ua), sea(a.ud), sea(a.as), a.u10, a.u10d);
        exec.fence("ww::wave::w3wave after W3UWND");
      } else if (flfrst) {  // :1234-1239
        Kokkos::deep_copy(exec, a.u10, static_cast<Real>(0.01));
        Kokkos::deep_copy(exec, a.u10d, Real(0));
        Kokkos::deep_copy(exec, a.ust, static_cast<Real>(0.05));
        Kokkos::deep_copy(exec, a.ustdir, static_cast<Real>(0.05));
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 6");

      if (*m.fliwnd != 0 && local) fortran(WW_W3WAVE_W3UINI, [&] { h.w3uini(ctx); });  // :1268

      if (p.fltaua) {  // :1270-1275
        fortran(WW_W3WAVE_W3UTAU, [&] { h.w3utau(ctx, flfrst ? 1 : 0); });
      } else if (flfrst) {
        Kokkos::deep_copy(exec, a.taua, static_cast<Real>(0.01));
        Kokkos::deep_copy(exec, a.tauadir, Real(0));
      }
      if (p.flrhoa) {  // :1277-1281
        fortran(WW_W3WAVE_W3URHO, [&] { h.w3urho(ctx, flfrst ? 1 : 0); });
      } else if (flfrst) {
        Kokkos::deep_copy(exec, a.rhoair, detail::kDair);
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 7");

      // 3.4 Boundary input (:1293-1316). W3IOBC may change TBPIN and FLBPI.
      if (*m.flbpi != 0 && local) {
        for (;;) {
          bool readbc;
          if (m.tbpin[0] == -1) {
            readbc = true;
            idact.set(1, 'F');
          } else {
            readbc = dsec(m.time, m.tbpin) < Real(0);
            if (readbc && idact.at(1) == ' ') idact.set(1, 'X');
          }
          flact = readbc || flact;
          if (readbc) {
            fortran(WW_W3WAVE_W3IOBC_READ, [&] {
              itest = h.w3iobc_read(ctx, imod);
              if (itest != 1) h.w3ubpt(ctx);
            });
          } else {
            itest = 0;
          }
          if (itest < 0) idact.set(1, 'L');
          if (itest > 0) idact.set(1, ' ');
          if (!(readbc && *m.flbpi != 0)) break;
        }
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 7");
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 8");

      // 3.5 Ice, IC1 and water level (:1336-1443).
      if (p.flice && dti0 != Real(0)) {
        if (m.tice[0] >= 0) {
          if (dti0 < Real(0)) {
            idact.set(13, 'B');
          } else {
            dttst = dsec(m.time, m.tin);
            if (detail::kHalf * dti0 >= dttst) idact.set(13, 'U');
          }
        } else {
          idact.set(13, 'I');
        }
        if (idact.at(13) != ' ') {
          fortran(WW_W3WAVE_W3UICE, [&] { h.w3uice(ctx); });
          dti0 = 0;
          flact = true;
          flmap = true;
        }
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 9");

      if (p.flic1 && dti10 != Real(0)) {
        if (m.tic1[0] >= 0) {
          if (dti10 < Real(0)) {
            idact.set(15, 'B');
          } else {
            dttst = dsec(m.time, m.ti1);
            if (detail::kHalf * dti10 >= dttst) idact.set(15, 'U');
          }
        } else {
          idact.set(15, 'I');
        }
        if (idact.at(15) != ' ') {
          fortran(WW_W3WAVE_W3UIC1, [&] { h.w3uic1(ctx, flfrst ? 1 : 0); });
          dti10 = 0;
          flact = true;
          flmap = true;
        }
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 10");
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 11a");

      if (p.fllev && dtl0 != Real(0)) {
        if (m.tlev[0] >= 0) {
          if (dtl0 < Real(0)) {
            idact.set(5, 'B');
          } else {
            dttst = dsec(m.time, m.tln);
            if (detail::kHalf * dtl0 >= dttst) idact.set(5, 'U');
          }
        } else {
          idact.set(5, 'I');
        }
        if (idact.at(5) != ' ') {
          fortran(WW_W3WAVE_W3ULEV, [&] { h.w3ulev(ctx); });
          ugdtupdate = 1;
          Kokkos::deep_copy(exec, a.cflxymax, Real(0));
          dtl0 = 0;
          flact = true;
          flmap = true;
          flddir = flddir || p.flcth || p.fsrefraction || p.flck || p.fsfreqshift;
        }
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 11b");

      // 3.5 Maps and depth gradients (:1454-1494).
      if (flmap) {
        if (!smc) {
          const prop::Map3Views<Mem> mv{mapsta_flat, a.mapsf,  a.mapx2,  a.mapy2,  a.mapaxy,
                                        a.mapcxy,    a.mapth2, a.mapwn2, a.map3_work};
          prop::w3map3<ExecSpace>(exec, c.map3, mv, *m.map3_counts);  // :1463
          const updt::UtrnViews<Mem> uv{a.trnx,  a.trny,  a.ice,   a.berg,  a.mapsf, a.mapsta,
                                        a.hpfac, a.hqfac, a.atrnx, a.atrny, a.trix,  a.triy};
          updt::w3utrn<ExecSpace>(exec, c.utrn, uv);                              // :1465
          prop::w3mapt<ExecSpace>(exec, nsea, a.mapsf, a.atrnx, a.atrny, a.maptrn);  // :1467
        }
        flmap = false;  // :1477
      }
      if (flddir) {
        if (smc) {
          // :1483-1487, W3SMCDHXY not compiled.
        } else if (ung) {
          fortran(WW_W3WAVE_UG_GRADIENTS, [&] { h.ug_gradients(ctx, WW_W3WAVE_UG_DW); });
        } else {
          updt::w3dzxy<ExecSpace>(exec, sea(a.dw), a.mapsta, a.mapfs, a.dpdx, a.dpdy, a.dqdx, a.dqdy,
                       p.flagll != 0, p.iclose, a.dddx, a.dddy);  // :1491
          exec.fence("ww::wave::w3wave after W3DZXY");
        }
        flddir = false;
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 12");
      Kokkos::deep_copy(exec, a.dcdx, Real(0));  // :1499
      Kokkos::deep_copy(exec, a.dcdy, Real(0));  // :1500
      *m.fliwnd = 0;   // :1517
      flfrst = false;  // :1518
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 13");
      memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 14");

      if (flzero) break;  // :1675-1680

      // 3.6 Propagation (:1682-2212).
      if (it != 0 && !p.fldry && p.iaproc <= p.naproc) {
        // CFL diagnostic of the spatial advection (:1689-1732).
        if (flogrd(9, 3) && ugdtupdate != 0) {
          if (!p.fstotalimp) {
            const int nkcfl = nk;
            if (ung) {
              if (flogrd(9, 3)) {
                fortran(WW_W3WAVE_W3CFLUG, [&] {
                  for (int jsea = 1; jsea <= nseal; ++jsea)
                    h.w3cflug(ctx, jsea, jsea, nkcfl, facx, dtg, vgx, vgy);
                });
              }
            } else if (nseal > 0) {
              const prop::Params xp = c.xyp;
              const prop::Grid xg = c.xyg;
              const prop::Step xs = c.xys;
              const Real dtgc = dtg, vgxc = vgx, vgyc = vgy;
              const auto cflxymax = a.cflxymax;
              const int* msta = a.mapsta.data();
              const int* mfs = a.mapfs.data();
              Kokkos::parallel_for(
                  "ww::wave::w3wave.w3cflxy", Kokkos::RangePolicy<ExecSpace>(exec, 1, nseal + 1),
                  KOKKOS_LAMBDA(const int jsea) {
                    prop::w3cflxy<false>(jsea, dtgc, msta, mfs, cflxymax(jsea - 1), vgxc, vgyc,
                                         xp, xg, xs);
                  });
              exec.fence("ww::wave::w3wave after W3CFLXY");
            }
          }
        }
        memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 15");

        // 3.6.1 Sub-steps (:1771-1784).
        const int ntloc = 1 + fint(dtg / p.dtcfli - detail::k1em3);
        const Real rntloc = static_cast<Real>(ntloc);
        const Real facth = dtg / (p.dth * rntloc);  // :1778
        const Real fack = dtg / rntloc;             // :1779
        const int ttest[2] = {m.time[0], 0};
        const int dttest = fint(dsec(ttest, m.time));  // :1783, INTEGER DTTEST
        const int itloch = (ntloc + 1 - nint(static_cast<Real>(dttest) / dtg) % 2) / 2;

        // 3.6.2 / 3.6.4 Intra-spectral part 1 and 2 (:1794-1878, :2118-2201).
        auto intra_spectral = [&](int itloc_first, int itloc_last) {
          if (nseal <= 0) return;
          const prop::Ktp3Params kp = c.ktp;
          const prop::Ktp3Grid kg = c.ktg;
          const int itstep = *m.itstep;
          const int gtype = p.gtype;
          const bool lpd = lpdlib;
          const Real dmin = p.dmin;
          const auto mapsf = a.mapsf;
          const auto mapsta = a.mapsta;
          const auto iobp = a.iobp;
          const auto dw = a.dw;
          const auto cthg0s = a.cthg0s;
          const auto cg = a.cg;
          const auto wn = a.wn;
          const auto dddx = a.dddx, dddy = a.dddy, dcxdx = a.dcxdx, dcxdy = a.dcxdy,
                     dcydx = a.dcydx, dcydy = a.dcydy;
          const auto dcdx = a.dcdx, dcdy = a.dcdy;
          const auto cx = a.cx, cy = a.cy;
          const auto va = a.va;
          const auto cflthmax = a.cflthmax, cflkmax = a.cflkmax;
          const int nwork = prop::ktp3_work_reals(nk, nth);
          for (int itloc = itloc_first; itloc <= itloc_last; ++itloc) {
            TeamPolicy policy(exec, nseal, 1);
            policy.set_scratch_size(1, Kokkos::PerTeam(ScratchR::shmem_size(nwork)));
            Kokkos::parallel_for(
                "ww::wave::w3wave.w3ktp3", policy, KOKKOS_LAMBDA(const Member& team) {
                  const int jsea = team.league_rank() + 1;
                  const int isea = jsea;  // INIT_GET_ISEA under SHRD
                  const int ix = mapsf(isea - 1, 0);
                  const int iy = mapsf(isea - 1, 1);
                  if (gtype == kUngtype) {
                    if (lpd) {
                      // The IOBP_LOC test is W3_PDLIB, not compiled: no CYCLE.
                    } else if (iobp(isea - 1) != 1) {
                      return;  // CYCLE
                    }
                  }
                  if (mapsta(iy - 1, ix - 1) != 1) return;
                  // :1819 (part 1) and :2131 (part 2, before the IOBP test,
                  // the same value): maxss DMIN,DW.
                  const Real depth = detail::mx(dw(isea), dmin);
                  const int ixrel = lpd ? jsea : ix;
                  if (gtype == kSmctype) return;  // W3KRTN not compiled
                  ScratchR buf(team.team_scratch(1), nwork);
                  const prop::Ktp3Work w = prop::ktp3_work(buf.data(), kp.nk, kp.nth);
                  prop::w3ktp3(kp, kg, itstep, isea, facth, fack, cthg0s(isea), &cg(0, isea),
                               &wn(0, isea), depth, dddx(iy - 1, ixrel - 1),
                               dddy(iy - 1, ixrel - 1), cx(isea), cy(isea),
                               dcxdx(iy - 1, ixrel - 1), dcxdy(iy - 1, ixrel - 1),
                               dcydx(iy - 1, ixrel - 1), dcydy(iy - 1, ixrel - 1),
                               &dcdx(0, iy - 1, ixrel - 1), &dcdy(0, iy - 1, ixrel - 1),
                               &va(0, jsea), cflthmax(jsea - 1), cflkmax(jsea - 1), w);
                });
            exec.fence("ww::wave::w3wave after W3KTP3");
          }
        };
        if (p.flcth || p.flck) intra_spectral(1, itloch);
        memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 16");

        // 3.6.3 Spatial propagation (:1892-2107).
        if (ung) facx = p.flagll ? detail::kFacxLL : Real(1);  // :1892-1898
        if (lpdlib) {
          // :1900-1929, W3_PDLIB not compiled.
        } else if (p.flcx || p.flcy) {
          if (smc) return kErrUnsupported;  // :1942 on the unallocated FIELD: undefined
          Kokkos::deep_copy(exec, field, Real(0));  // :1942
          // What W3XYP3 reads from the modules this step (w3pro3md.F90:928-930,
          // 1395-1398): the DSEC21 results, one value for every ISPEC and ITLOC of
          // the step. A device kernel cannot call the hook; DSEC21 is a pure
          // function of its dates and CALTYPE (w3timemd.F90:330-568), and TIME, TBPI0, TBPIN do
          // not change in this loop, so the hoisted calls give the same values.
          prop::Grid xg = c.xyg;
          const prop::Map3Counts& mc = *m.map3_counts;
          xg.nmx0 = mc.nmx0;
          xg.nmx1 = mc.nmx1;
          xg.nmx2 = mc.nmx2;
          xg.nmy0 = mc.nmy0;
          xg.nmy1 = mc.nmy1;
          xg.nmy2 = mc.nmy2;
          xg.nact = mc.nact;
          xg.ncent = mc.ncent;
          prop::Step xs = c.xys;
          xs.dsec_day = dsec(ttest, m.time);
          xs.flbpi = *m.flbpi != 0 ? 1 : 0;
          if (xs.flbpi) {
            xs.dsec_bpi0 = dsec(m.tbpi0, m.time);
            xs.dsec_bpin = dsec(m.tbpi0, m.tbpin);
          }
          const prop::Params xp = c.xyp;
          for (int ispec = 1; ispec <= nspec; ++ispec) {
            if (m.iappro[ispec - 1] != p.iaproc) continue;
            if (smc) {
              // W3GATHSMC not compiled.
            } else if (!lpdlib) {
              wave::w3gath<ExecSpace>(exec, ispec, nsea, ny, a.va, a.mapsf, field);  // :1954
            }
            if (smc) {
              // W3PSMC not compiled.
            } else if (ung) {
              auto hfield = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), field);
              fortran(WW_W3WAVE_W3XYPUG, [&] {
                h.w3xypug(ctx, ispec, facx, dtg, hfield.data(), vgx, vgy, &ugdtupdate);
              });
              Kokkos::deep_copy(exec, field, hfield);
              exec.fence("ww::wave::w3wave after W3XYPUG");
            } else {
              if (xp.iclose == prop::kIcloseTrpl) return stop(WW_W3WAVE_ERR_XYP3_TRIPOLE, 1);
              const Real dtgc = dtg;
              const int isp = ispec;
              const int* msta = mapsta_flat.data();
              Real* vq = field.data();
              Real* wr = xy_wr.data();
              int* wi = xy_wi.data();
              Kokkos::parallel_for(
                  "ww::wave::w3wave.w3xyp3", Kokkos::RangePolicy<ExecSpace>(exec, 0, 1),
                  KOKKOS_LAMBDA(const int) { prop::w3xyp3(xp, xg, xs, isp, dtgc, msta, vq, wr, wi); });
              exec.fence("ww::wave::w3wave after W3XYP3");
            }
            if (smc) {
              // W3SCATSMC not compiled.
            } else if (!lpdlib) {
              wave::w3scat<ExecSpace>(exec, ispec, nsea, ny, mapsta_flat, field, a.mapsf, a.va);  // :2003
            }
          }
          memcheck(memunit, "memcheck_____: WW3_WAVE TIME LOOP 17");
          // :2020-2022 IK = IX = IY = 1: dead stores of locals, not kept.
        }

        if (p.flcth || p.flck) intra_spectral(itloch + 1, ntloc);
        ugdtupdate = 0;  // :2209
      }

      // 3.7 Source terms (:2216-2379).
      if (it == 0) dtg = 1;  // :2216
      if (!p.fldry && p.iaproc <= p.naproc) {
        if (p.flsou) {
          // Not W3WAVE's: the seam of the fork's W3KOKKOS hunk (W3KOKKOS_SRCE_ON, called
          // there per point but a function of IT and grid constants only). When it says
          // no, the hook runs W3WAVE's own loop :2246-2349 unchanged.
          const bool on_kokkos = h.srce_on_kokkos(ctx, it) != 0;
          if (!on_kokkos) {
            auto htx = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), tauwx);
            auto hty = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), tauwy);
            fortran(WW_W3WAVE_W3SRCE_FORTRAN, [&] {
              for (int jsea = 1; jsea <= nseal; ++jsea)
                h.w3srce_fortran(ctx, it, jsea, jsea, imod, dtg, &htx(jsea - 1), &hty(jsea - 1));
            });
            Kokkos::deep_copy(exec, tauwx, htx);
            Kokkos::deep_copy(exec, tauwy, hty);
            exec.fence("ww::wave::w3wave after the Fortran W3SRCE");
          } else if (nseal > 0) {
            if (srce_rw.extent(0) == 0) {
              srce_rw = R1("ww::wave::w3wave.srce_rw", static_cast<std::size_t>(srce_l) * srce_wr);
              srce_iw = I1("ww::wave::w3wave.srce_iw", static_cast<std::size_t>(srce_l) * srce_wi);
              srce_dw = Kokkos::View<double*, Mem>("ww::wave::w3wave.srce_dw",
                                                   static_cast<std::size_t>(srce_l) * srce_wd);
              srce_tmp = R1("ww::wave::w3wave.srce_tmp", static_cast<std::size_t>(srce_l) * 8);
              srce_sc = Kokkos::View<srce::PointScalars*, Mem>("ww::wave::w3wave.srce_sc", srce_l);
              srce_st = Kokkos::View<srce::State*, Mem>("ww::wave::w3wave.srce_st", srce_l);
            }
            const std::size_t scratch = snl1::snl1_team_scratch_bytes<ExecSpace>(c.nc, c.nt);
            TeamPolicy policy(exec, srce_l, Kokkos::AUTO);
            const int level = scratch <= static_cast<std::size_t>(policy.scratch_size_max(0)) ? 0 : 1;
            policy.set_scratch_size(level, Kokkos::PerTeam(scratch));
            const srce::Params sp = c.sp;
            const srce::Grid sg = c.sg;
            const st4::Params q = c.q;
            const st4::Fields f = c.f;
            const snl1::Config nlc = c.nc;
            const snl1::Tables nlt = c.nt;
            const Real* nsig = c.nsig.data();
            const int itc = it, i24 = c.inflags2_4, f57 = c.flogrd_5_7, f58 = c.flogrd_5_8;
            const Real dtgc = dtg, undef = p.undef;
            const int league = srce_l, nsl = nseal;
            const auto mapsf = a.mapsf;
            const auto mapsta = a.mapsta;
            const auto flagst = a.flagst;
            const auto va = a.va;
            const auto wn = a.wn, cg = a.cg;
            const auto dw = a.dw, u10 = a.u10, u10d = a.u10d, as = a.as, ice = a.ice,
                       rhoair = a.rhoair, asf = a.asf, ust = a.ust, ustdir = a.ustdir;
            const auto dtdyn = a.dtdyn, fcut = a.fcut, tauox = a.tauox, tauoy = a.tauoy,
                       tauwix = a.tauwix, tauwiy = a.tauwiy, tauwnx = a.tauwnx,
                       tauwny = a.tauwny, phiaw = a.phiaw, charn = a.charn, tws = a.tws,
                       phioc = a.phioc, phibbl = a.phibbl, phice = a.phice, tauocx = a.tauocx,
                       tauocy = a.tauocy, wnmean = a.wnmean;
            const auto whitecap = a.whitecap, taubbl = a.taubbl, tauice = a.tauice;
            const auto twx = tauwx, twy = tauwy;
            const auto rw = srce_rw, tmpv = srce_tmp;
            const auto iw = srce_iw;
            const auto dwk = srce_dw;
            const auto scv = srce_sc;
            const auto stv = srce_st;
            const int wr_n = srce_wr, wi_n = srce_wi, wd_n = srce_wd;
            Kokkos::parallel_for(
                "ww::wave::w3wave.w3srce", policy, KOKKOS_LAMBDA(const Member& team) {
                  const int t = team.league_rank();
                  const snl1::TeamScratch w = snl1::snl1_team_scratch(team, level, nlc, nlt);
                  Real* tmp = tmpv.data() + 8 * t;  // TMP1(4), TMP3(2), TMP4(2) (:636)
                  for (int jsea = t + 1; jsea <= nsl; jsea += league) {
                    const int isea = jsea;
                    const int ix = mapsf(isea - 1, 0);  // :2249
                    const int iy = mapsf(isea - 1, 1);  // :2250
                    // :2251-2253 DELA = DELX = DELY = 1.: read only under W3_REF1.
                    if (mapsta(iy - 1, ix - 1) == 1 && flagst(isea - 1) != 0) {  // :2278
                      Kokkos::single(Kokkos::PerTeam(team), [&]() {
                        for (int k = 0; k < 4; ++k) tmp[k] = whitecap(jsea - 1, k);  // :2279
                        // :2280 TMP2 = BEDFORMS: W3SRCE never assigns its BEDFORM
                        // under this switch (w3srcemd.F90:684-688, used nowhere in
                        // :190-2412 once preprocessed), so :2339 writes the same
                        // values back; not copied.
                        for (int k = 0; k < 2; ++k) tmp[4 + k] = taubbl(jsea - 1, k);  // :2281
                        for (int k = 0; k < 2; ++k) tmp[6 + k] = tauice(jsea - 1, k);  // :2282
                        srce::PointScalars& o = scv(t);
                        o.ustar = ust(isea);
                        o.ustdir = ustdir(isea);
                        o.tauwx = twx(jsea - 1);
                        o.tauwy = twy(jsea - 1);
                        o.dtdyn = dtdyn(jsea - 1);
                        o.fcut = fcut(jsea - 1);
                        o.tauox = tauox(jsea - 1);
                        o.tauoy = tauoy(jsea - 1);
                        o.tauwix = tauwix(jsea - 1);
                        o.tauwiy = tauwiy(jsea - 1);
                        o.tauwnx = tauwnx(jsea - 1);
                        o.tauwny = tauwny(jsea - 1);
                        o.phiaw = phiaw(jsea - 1);
                        o.charn = charn(jsea - 1);
                        o.tws = tws(jsea - 1);
                        o.phioc = phioc(jsea - 1);
                        o.phibbl = phibbl(jsea - 1);
                        o.phice = phice(jsea - 1);
                        o.tauocx = tauocx(jsea - 1);
                        o.tauocy = tauocy(jsea - 1);
                        o.wnmean = wnmean(jsea - 1);
                        o.shave = 0;  // SHAVETOTioDummy, OUT, never read (:627)
                      });
                      team.team_barrier();
                      // W3SRCE's D_INP, U10ABS, U10DIR, AS, ICE, DAIR = RHOAIR, COEF = ASF.
                      const srce::PointIn in{itc,       i24,        f57,         f58,
                                             dw(isea),  u10(isea - 1), u10d(isea - 1), as(isea),
                                             ice(isea), dtgc,       rhoair(isea - 1), asf(isea - 1)};
                      srce::w3srce_team(team, sp, sg, q, f, nlc, nlt, nsig, w, in, &va(0, jsea),
                                        &wn(1, isea), &cg(1, isea), scv(t), tmp, tmp + 4,
                                        tmp + 6, stv(t), rw.data() + static_cast<std::size_t>(t) * wr_n,
                                        iw.data() + static_cast<std::size_t>(t) * wi_n,
                                        dwk.data() + static_cast<std::size_t>(t) * wd_n);
                      team.team_barrier();
                      Kokkos::single(Kokkos::PerTeam(team), [&]() {
                        const srce::PointScalars& o = scv(t);
                        ust(isea) = o.ustar;
                        ustdir(isea) = o.ustdir;
                        twx(jsea - 1) = o.tauwx;
                        twy(jsea - 1) = o.tauwy;
                        dtdyn(jsea - 1) = o.dtdyn;
                        fcut(jsea - 1) = o.fcut;
                        tauox(jsea - 1) = o.tauox;
                        tauoy(jsea - 1) = o.tauoy;
                        tauwix(jsea - 1) = o.tauwix;
                        tauwiy(jsea - 1) = o.tauwiy;
                        tauwnx(jsea - 1) = o.tauwnx;
                        tauwny(jsea - 1) = o.tauwny;
                        phiaw(jsea - 1) = o.phiaw;
                        charn(jsea - 1) = o.charn;
                        tws(jsea - 1) = o.tws;
                        phioc(jsea - 1) = o.phioc;
                        phibbl(jsea - 1) = o.phibbl;
                        phice(jsea - 1) = o.phice;
                        tauocx(jsea - 1) = o.tauocx;
                        tauocy(jsea - 1) = o.tauocy;
                        wnmean(jsea - 1) = o.wnmean;
                        for (int k = 0; k < 4; ++k) whitecap(jsea - 1, k) = tmp[k];  // :2338
                        for (int k = 0; k < 2; ++k) taubbl(jsea - 1, k) = tmp[4 + k];  // :2340
                        for (int k = 0; k < 2; ++k) tauice(jsea - 1, k) = tmp[6 + k];  // :2341
                      });
                      team.team_barrier();
                    } else {
                      Kokkos::single(Kokkos::PerTeam(team), [&]() {
                        ust(isea) = undef;         // :2343
                        ustdir(isea) = undef;      // :2344
                        dtdyn(jsea - 1) = undef;   // :2345
                        fcut(jsea - 1) = undef;    // :2346
                      });
                    }
                  }
                });
            exec.fence("ww::wave::w3wave after W3SRCE");
          }
        }
      }

      // 3.8 Global time step and log (:2395-2410).
      if (it != nt) {
        dttst = dsec(m.time, tcalc);
        dtg = dttst / static_cast<Real>(nt - it);
      }
      if (flact && it != nt && p.iaproc == p.naplog) {
        const int full = *m.idlast != m.time[0] ? 1 : 0;
        h.log_step(ctx, *m.itime, *m.ipass, m.time, idact.c, outid.c, full);
        if (full) *m.idlast = m.time[0];
        flact = false;
        idact.blank();
      }
    }

    if (!flzero) memcheck(memunit, "memcheck_____: WW3_WAVE END TIME LOOP");  // :2422-2431

    // 4. Output (:2441-2843).
    dttst = m.tofrst[0] == -1 ? Real(1) : dsec(m.time, m.tofrst);
    dttst1 = m.tdn[0] == -1 ? Real(1) : dsec(m.time, m.tdn);
    dttst2 = dsec(m.time, tend);
    const bool flag_o = !skip_o || (skip_o && dttst2 != Real(0));  // :2454

    if (dttst <= Real(0) && dttst1 != Real(0) && flag_o) {
      const bool floutg = m.flout[0] != 0 ? dsec(m.time, tonext(1)) == Real(0) : false;
      const bool floutg2 = m.flout[6] != 0 ? dsec(m.time, tonext(7)) == Real(0) : false;
      bool flpart = false;  // :2480-2482
      if (m.flout[0] != 0 && flpfld) flpart = flpart || dsec(m.time, tonext(1)) == Real(0);
      if (m.flout[5] != 0) flpart = flpart || dsec(m.time, tonext(6)) == Real(0);
      if (local && flpart) fortran(WW_W3WAVE_W3CPRT, [&] { h.w3cprt(ctx, imod); });
      if (local && (floutg || floutg2))
        fortran(WW_W3WAVE_W3OUTG, [&] { h.w3outg(ctx, flpfld ? 1 : 0, floutg ? 1 : 0, floutg2 ? 1 : 0); });
      memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 1");
      memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 2");

      m.tofrst[0] = -1;  // :2626
      m.tofrst[1] = 0;   // :2627

      // :2750-2778 and :2797-2825, the same tail for every output type J.
      auto advance = [&](int j) {
        h.tick21(ctx, tout, m.dtout[j - 1]);
        detail::copy2(tonext(j), tout);
        detail::copy2(tlst, tolast(j));
        dttst = dsec(tout, tlst);
        m.flout[j - 1] = dttst >= Real(0) ? 1 : 0;
        outid.set(2 * j - 1, m.flout[j - 1] != 0 ? 'X' : 'L');
      };
      auto first_output = [&](int j) {
        if (m.flout[j - 1] != 0) {
          if (m.tofrst[0] == -1) {
            detail::copy2(m.tofrst, tout);
          } else {
            dttst = dsec(tout, m.tofrst);
            if (dttst > Real(0)) detail::copy2(m.tofrst, tout);
          }
        }
      };

      for (int j = 1; j <= p.notype; ++j) {  // :2629-2782
        if (m.flout[j - 1] == 0) continue;
        detail::copy2(tout, tonext(j));
        dttst = dsec(m.time, tout);
        if (dttst == Real(0)) {
          if (j == 1) {
            if (p.iaproc == p.napfld) fortran(WW_W3WAVE_W3IOGO, [&] { h.w3iogo(ctx, imod); });
          } else if (j == 2) {
            if (p.iaproc == p.nappnt) {
              fortran(WW_W3WAVE_W3IOPE_IOPO, [&] {
                h.w3iope(ctx);
                h.w3iopo(ctx, imod);
              });
            }
          } else if (j == 3) {
            fortran(WW_W3WAVE_W3IOTR, [&] { h.w3iotr(ctx, imod); });
          } else if (j == 4) {
            fortran(WW_W3WAVE_W3IORS, [&] { h.w3iors_hot(ctx, &xxx, imod); });
            // :2709 ITEST = RSTYPE: ITEST is not read again before its next store.
          } else if (j == 5) {
            if (p.iaproc == p.napbpt)
              fortran(WW_W3WAVE_W3IOBC_WRITE, [&] { h.w3iobc_write(ctx, imod); });
          } else if (j == 6) {
            fortran(WW_W3WAVE_W3IOSF, [&] { h.w3iosf(ctx, imod); });
          }
          advance(j);
        }
        first_output(j);
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 3");

      {  // :2787-2826, restart type 8 (checkpoint)
        const int j = 8;
        if (m.flout[j - 1] != 0) {
          detail::copy2(tout, tonext(j));
          dttst = dsec(m.time, tout);
          if (dttst == Real(0)) {
            fortran(WW_W3WAVE_W3IORS, [&] { h.w3iors_hot(ctx, &xxx, imod); });
            advance(j);
          }
          first_output(j);
        }
      }
      memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 3");
    }
    memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 4");

    // 5. Log file (:2851-2886).
    if (p.iaproc == p.naplog) {
      if (p.flcur && dsec(m.time, m.tcn) == Real(0)) idact.set(7, 'X');
      if (p.flwind && dsec(m.time, m.twn) == Real(0)) idact.set(3, 'X');
      if (p.fltaua && dsec(m.time, m.tun) == Real(0)) idact.set(9, 'X');
      if (p.flrhoa && dsec(m.time, m.trn) == Real(0)) idact.set(11, 'X');
      if (m.tdn[0] > 0 && dsec(m.time, m.tdn) == Real(0)) idact.set(21, 'X');
      const int full = *m.idlast != m.time[0] ? 1 : 0;
      h.log_step(ctx, *m.itime, *m.ipass, m.time, idact.c, outid.c, full);
      if (full) *m.idlast = m.time[0];
    }
    idact.blank();  // :2884
    outid.blank();  // :2885
    flact = false;  // :2886

    dttst = dsec(m.time, tend);  // :2890
    if (dttst == Real(0)) break;
  }
  memcheck(memunit, "memcheck_____: WW3_WAVE AFTER TIME LOOP 5");

  if (tstamp && p.screen != p.ndso && p.iaproc == p.napout) h.screen_end(ctx);  // :2899-2902
  if (p.iaproc == p.naplog) h.log_footer(ctx);                                   // :2904
  // :2906-2907 DEALLOCATE FIELD, TAUWX, TAUWY: the Views go out of scope.
  memcheck(memunit, "memcheck_____: WW3_WAVE END W3WAVE");
  return kOk;
}

}  // namespace ww::wave::w3wave
