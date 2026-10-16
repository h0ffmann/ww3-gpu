// kokkos/src/ww_kokkos/pdlib_do_output_exchanges.hpp
// DO_OUTPUT_EXCHANGES: under PDLIB, gathers the gridded output fields of every computing process's
// local sea points onto the field output process NAPFLD, which stores them, by global sea point,
// into the full-grid output arrays (the X arrays W3XETA points HS, WLM, ... at). No arithmetic:
// the routine copies REAL words. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/pdlib_field_vec.F90:784-1733 at h0ffmann/WW3 8e433f99;
// A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/DO_OUTPUT_EXCHANGES_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (DIST, MPI, PDLIB;
// no S). The body has no preprocessor branch; W3_S only declares an unused IENT (:854-856).
// Not translated, behind the hook table (OutputExchangeHooks): GET_ARRAY_SIZE (:877), the six MPI
// calls (:1303-1315), W3XDMA, W3XETA and W3SETA (:1324-1325, :1729), and the read of
// WADATS(IMOD)%AINIT2 (:1324). INIT_GET_ISEA (:881) is parall_init_get_isea.hpp.
//
// Loop nesting as in the Fortran: the packing is one launch over JSEA that walks the columns in
// order (:880-1286); the unpacking one whole-array assignment per column (:1326-1728). The two IF
// chains of the Fortran list the same fields in the same order with the same loop bounds
// (mechanical diff of :883-1287 against :1326-1728); one walker, walk_columns, carries that order
// for both.
#pragma once

#include <Kokkos_Core.hpp>

#include "parall_init_get_isea.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace dooutx_detail {
// W3ODATMD PARAMETERs NOGRP, NGRPP, NOEXTR (w3odatmd.F90:330-335).
inline constexpr int kNogrp = 10;
inline constexpr int kNgrpp = 20;
inline constexpr int kNoextr = 2;
}  // namespace dooutx_detail

/// The MPI datatypes the Fortran names; the shim maps them to MPI_INTEGER and MPI_REAL
/// (Open MPI handles 7 and 13 in the -O3 object).
enum OutputMpiDatatype : int { kOutputMpiInteger = 0, kOutputMpiReal = 1 };

/// FLGRDALL(NOGRP,NGRPP) = FLOGRD .OR. FLOGR2 (:867-871), LOGICAL(4) as 0/1, column-major as the
/// Fortran stores it, so W3XDMA receives the same 800 bytes.
struct OutputFlags {
  int all[dooutx_detail::kNogrp * dooutx_detail::kNgrpp] = {};
  /// FLGRDALL(j,k), 1-based.
  [[nodiscard]] KOKKOS_INLINE_FUNCTION bool operator()(int j, int k) const {
    return all[(j - 1) + (k - 1) * dooutx_detail::kNogrp] != 0;
  }
};

/// The output fields, in the order the Fortran exchanges them. One instance holds the arrays the
/// W3SETA pointers name at entry (local, NSEALM rows, read by the packing); another the arrays
/// after W3XETA (the X arrays, NSEALM*NAPROC rows, written by the unpacking), from
/// OutputExchangeHooks::output_fields. LayoutLeft, row 0 is Fortran row 1, except ust and
/// ustdir: W3WDATMD's UST(0:NSEATM), USTDIR(0:NSEATM) (w3wdatmd.F90:570-571), row 0 is index 0;
/// asf is ASF(NSEATM). These three are not remapped by W3XETA: both instances name the same
/// W3WDATMD arrays. The second dimension of a 2-D field is 0-based here from the Fortran lower
/// bound: EF..STH2M from E3DF(2,k) (w3adatmd.F90:1084-1105, :1752-1790), PHS..PEP from 0
/// (:1119, :1798), P2SMS from P2MSF(2) (:1211, :2115), the others from 1.
/// A field whose flag is off is never touched and may be empty.
template <class MemSpace>
struct OutputExchangeFields {
  using F1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using F2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  F1 hs, wlm, t02, t0m1, t01, fp0, thm, ths, thp0, hsig;                  ///< group 2, 1-10
  F1 stmaxe, stmaxd, hmaxe, hcmaxe, hmaxd, hcmaxd, wbt, wnmean;           ///< 2, 11-17 and 19
  F2 ef, th1m, sth1m, th2m, sth2m;                                       ///< group 3
  F2 phs, ptp, plp, pdir, psi, pws, pthp0, pqp, ppe, pgw, psw, ptm1, pt1, pt2, pep;  ///< 4, 1-15
  F1 pwst, pnr;                                                          ///< 4, 16-17
  F1 ust, ustdir, asf;                                                   ///< 5, 1 (W3WDATMD)
  F1 charn, cge, phiaw, tauwix, tauwiy, tauwnx, tauwny;                  ///< 5, 2-6
  F2 whitecap;                                                           ///< 5, 7-10
  F1 sxx, syy, sxy, tauox, tauoy, bhd, phioc, tusx, tusy, ussx, ussy, prms, tpms;  ///< 6, 1-7
  F2 us3d, p2sms, tauice;                                                ///< 6, 8-10
  F1 phice;                                                              ///< 6, 11
  F2 ussp;                                                               ///< 6, 12
  F1 tauocx, tauocy;                                                     ///< 6, 13
  F1 aba, abd, uba, ubd;                                                 ///< 7, 1-2
  F2 bedforms;                                                           ///< 7, 3
  F1 phibbl;                                                             ///< 7, 4
  F2 taubbl;                                                             ///< 7, 5
  F1 mssx, mssy, mscx, mscy, mssd, mscd, qp;                             ///< group 8
  F1 dtdyn, fcut, cflxymax, cflthmax, cflkmax;                           ///< group 9
  F2 usero;                                                              ///< group 10
};

/// The scalars the routine reads from W3GDATMD, W3ODATMD and CONSTANTS.
struct OutputExchangeConfig {
  int imod = 1;                       ///< argument IMOD
  int nsea = 0, nseal = 0, nk = 0;    ///< W3GDATMD NSEA, NSEAL, NK
  int e3df[3 * 5] = {};               ///< W3GDATMD E3DF(3,5), column-major
  int p2msf[3] = {};                  ///< W3GDATMD P2MSF(3)
  int noswll = 0;                     ///< W3ODATMD NOSWLL
  int iaproc = 1, naproc = 1, napfld = 1;  ///< W3ODATMD IAPROC, NAPROC, NAPFLD
  int flout1 = 0, flout7 = 0;         ///< W3ODATMD FLOUT(1), FLOUT(7)
  const int* flogrd = nullptr;        ///< W3ODATMD FLOGRD(NOGRP,NGRPP), LOGICAL(4)
  const int* flogr2 = nullptr;        ///< W3ODATMD FLOGR2(NOGRP,NGRPP), LOGICAL(4)
  int lpdlib = 1, gtype = 3;          ///< CONSTANTS LPDLIB, W3GDATMD GTYPE, for INIT_GET_ISEA
  [[nodiscard]] KOKKOS_INLINE_FUNCTION int e3(int i, int j) const {
    return e3df[(i - 1) + (j - 1) * 3];
  }
};

/// The module variables the routine writes: W3ODATMD NRQGO, NRQGO2 (set to 0, :872-873) and its
/// own SAVE counter indexOutput (:863, :1732), which nothing reads. gfortran keeps the static in
/// .bss; its first value is 0 in practice, undefined by the standard.
struct OutputExchangeModule {
  int* nrqgo = nullptr;
  int* nrqgo2 = nullptr;
  int index_output = 0;
};

/// The calls that stay in Fortran or MPI, filled by a future shim. Buffers are host memory;
/// counts are in elements of the named datatype; ranks are 0-based as the Fortran passes them
/// (NAPFLD-1, IPROC-1). MPI_COMM_WAVE, the status ISTATUS and IERR are the shim's: the Fortran
/// checks neither. A hook needed on a path and left null aborts.
template <class MemSpace>
struct OutputExchangeHooks {
  void* ctx = nullptr;
  int (*get_array_size)(void* ctx) = nullptr;  ///< GET_ARRAY_SIZE(TheSize), :877
  void (*mpi_send)(void* ctx, const void* buf, int count, int datatype, int dest,
                   int tag) = nullptr;  ///< MPI_SEND, :1303-1305
  void (*mpi_recv)(void* ctx, void* buf, int count, int datatype, int source,
                   int tag) = nullptr;  ///< MPI_RECV, :1311-1315
  int (*ainit2)(void* ctx, int imod) = nullptr;                          ///< WADATS(IMOD)%AINIT2
  void (*w3xdma)(void* ctx, int imod, const int* flgrdall) = nullptr;    ///< :1324, NDSE, NDST
  void (*w3xeta)(void* ctx, int imod) = nullptr;                         ///< :1325
  void (*w3seta)(void* ctx, int imod) = nullptr;                         ///< :1729
  /// The X arrays W3XETA has just pointed the module at, wrapped as views.
  OutputExchangeFields<MemSpace> (*output_fields)(void* ctx, int imod) = nullptr;
};

namespace dooutx_detail {

/// How a 1-D field is indexed: by JSEA when packing and ISEA when unpacking, both 1-based on a
/// 1-based array; or by ISEA in both, on UST/USTDIR(0:) or ASF(1:) (:1084-1090, :1527-1534).
enum Index1 : int { kJsea = 0, kIsea0 = 1, kIsea1 = 2 };

inline void require(bool ok, const char* what) {
  if (!ok) Kokkos::abort(what);
}

/// The column order of :883-1287 and :1326-1728. c1(ih, view, Index1) for a 1-D field,
/// c2(ih, view, col) for column col (0-based from the field's lower bound) of a 2-D one.
/// Returns the last IH. Runs inside the packing kernel and on the host for the unpacking.
template <class Fields, class C1, class C2>
KOKKOS_INLINE_FUNCTION int walk_columns(const OutputExchangeConfig& c, const OutputFlags& fl,
                                        const Fields& f, C1&& c1, C2&& c2) {
  int ih = 0;
  auto one = [&](const auto& v, Index1 ix = kJsea) { c1(++ih, v, ix); };
  auto e3 = [&](int k, const auto& v) {  // DO IK=E3DF(2,k),E3DF(3,k)
    for (int ik = c.e3(2, k); ik <= c.e3(3, k); ++ik) c2(++ih, v, ik - c.e3(2, k));
  };
  auto swell = [&](const auto& v) {  // DO IK=0,NOSWLL
    for (int ik = 0; ik <= c.noswll; ++ik) c2(++ih, v, ik);
  };
  auto cols = [&](const auto& v, int lo, int hi) {  // DO IK=lo,hi on a lo-based dimension
    for (int ik = lo; ik <= hi; ++ik) c2(++ih, v, ik - lo);
  };
  if (fl(2, 1)) one(f.hs);
  if (fl(2, 2)) one(f.wlm);
  if (fl(2, 3)) one(f.t02);
  if (fl(2, 4)) one(f.t0m1);
  if (fl(2, 5)) one(f.t01);
  if (fl(2, 6)) one(f.fp0);
  if (fl(2, 7)) one(f.thm);
  if (fl(2, 8)) one(f.ths);
  if (fl(2, 9)) one(f.thp0);
  if (fl(2, 10)) one(f.hsig);
  if (fl(2, 11)) one(f.stmaxe);
  if (fl(2, 12)) one(f.stmaxd);
  if (fl(2, 13)) one(f.hmaxe);
  if (fl(2, 14)) one(f.hcmaxe);
  if (fl(2, 15)) one(f.hmaxd);
  if (fl(2, 16)) one(f.hcmaxd);
  if (fl(2, 17)) one(f.wbt);
  if (fl(2, 19)) one(f.wnmean);  // (2,18) is not exchanged (:948-955)
  if (fl(3, 1)) e3(1, f.ef);
  if (fl(3, 2)) e3(2, f.th1m);
  if (fl(3, 3)) e3(3, f.sth1m);
  if (fl(3, 4)) e3(4, f.th2m);
  if (fl(3, 5)) e3(5, f.sth2m);
  if (fl(4, 1)) swell(f.phs);
  if (fl(4, 2)) swell(f.ptp);
  if (fl(4, 3)) swell(f.plp);
  if (fl(4, 4)) swell(f.pdir);
  if (fl(4, 5)) swell(f.psi);
  if (fl(4, 6)) swell(f.pws);
  if (fl(4, 7)) swell(f.pthp0);
  if (fl(4, 8)) swell(f.pqp);
  if (fl(4, 9)) swell(f.ppe);
  if (fl(4, 10)) swell(f.pgw);
  if (fl(4, 11)) swell(f.psw);
  if (fl(4, 12)) swell(f.ptm1);
  if (fl(4, 13)) swell(f.pt1);
  if (fl(4, 14)) swell(f.pt2);
  if (fl(4, 15)) swell(f.pep);
  if (fl(4, 16)) one(f.pwst);
  if (fl(4, 17)) one(f.pnr);
  if (fl(5, 1)) {  // packed as UST(ISEA), USTDIR(ISEA), ASF(ISEA) (:1084-1090)
    one(f.ust, kIsea0);
    one(f.ustdir, kIsea0);
    one(f.asf, kIsea1);
  }
  if (fl(5, 2)) one(f.charn);
  if (fl(5, 3)) one(f.cge);
  if (fl(5, 4)) one(f.phiaw);
  if (fl(5, 5)) {
    one(f.tauwix);
    one(f.tauwiy);
  }
  if (fl(5, 6)) {
    one(f.tauwnx);
    one(f.tauwny);
  }
  if (fl(5, 7)) c2(++ih, f.whitecap, 0);
  if (fl(5, 8)) c2(++ih, f.whitecap, 1);
  if (fl(5, 9)) c2(++ih, f.whitecap, 2);
  if (fl(5, 10)) c2(++ih, f.whitecap, 3);
  if (fl(6, 1)) {
    one(f.sxx);
    one(f.syy);
    one(f.sxy);
  }
  if (fl(6, 2)) {
    one(f.tauox);
    one(f.tauoy);
  }
  if (fl(6, 3)) one(f.bhd);
  if (fl(6, 4)) one(f.phioc);
  if (fl(6, 5)) {
    one(f.tusx);
    one(f.tusy);
  }
  if (fl(6, 6)) {
    one(f.ussx);
    one(f.ussy);
  }
  if (fl(6, 7)) {
    one(f.prms);
    one(f.tpms);
  }
  if (fl(6, 8)) cols(f.us3d, 1, 2 * c.nk);
  if (fl(6, 9)) cols(f.p2sms, c.p2msf[1], c.p2msf[2]);
  if (fl(6, 10)) {
    c2(++ih, f.tauice, 0);
    c2(++ih, f.tauice, 1);
  }
  if (fl(6, 11)) one(f.phice);
  if (fl(6, 12)) cols(f.ussp, 1, 2 * c.nk);
  if (fl(6, 13)) {
    one(f.tauocx);
    one(f.tauocy);
  }
  if (fl(7, 1)) {
    one(f.aba);
    one(f.abd);
  }
  if (fl(7, 2)) {
    one(f.uba);
    one(f.ubd);
  }
  if (fl(7, 3)) {
    c2(++ih, f.bedforms, 0);
    c2(++ih, f.bedforms, 1);
    c2(++ih, f.bedforms, 2);
  }
  if (fl(7, 4)) one(f.phibbl);
  if (fl(7, 5)) {
    c2(++ih, f.taubbl, 0);
    c2(++ih, f.taubbl, 1);
  }
  if (fl(8, 1)) {
    one(f.mssx);
    one(f.mssy);
  }
  if (fl(8, 2)) {
    one(f.mscx);
    one(f.mscy);
  }
  if (fl(8, 3)) one(f.mssd);
  if (fl(8, 4)) one(f.mscd);
  if (fl(8, 5)) one(f.qp);
  if (fl(9, 1)) one(f.dtdyn);
  if (fl(9, 2)) one(f.fcut);
  if (fl(9, 3)) one(f.cflxymax);
  if (fl(9, 4)) one(f.cflthmax);
  if (fl(9, 5)) one(f.cflkmax);
  for (int i = 1; i <= kNoextr; ++i) {
    if (fl(10, i)) c2(++ih, f.usero, i - 1);
  }
  return ih;
}

}  // namespace dooutx_detail

/// DO_OUTPUT_EXCHANGES(IMOD). local: the fields at entry (W3SETA's pointers). iplg: yowNodepool
/// IPLG(1:NPA), 0-based, for INIT_GET_ISEA. On the root NAPFLD the X arrays come from
/// hooks.output_fields after W3XDMA/W3XETA, and W3SETA is called last, as at :1324-1729.
template <class ExecSpace>
void do_output_exchanges(const ExecSpace& exec, const OutputExchangeConfig& c,
                         const OutputExchangeFields<typename ExecSpace::memory_space>& local,
                         const Kokkos::View<const int*, typename ExecSpace::memory_space>& iplg,
                         OutputExchangeModule& m,
                         const OutputExchangeHooks<typename ExecSpace::memory_space>& hooks) {
  using namespace dooutx_detail;
  using MemSpace = typename ExecSpace::memory_space;
  using Policy = Kokkos::RangePolicy<ExecSpace>;
  using A2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  using F1 = typename OutputExchangeFields<MemSpace>::F1;
  using F2 = typename OutputExchangeFields<MemSpace>::F2;

  OutputFlags fl;  // :867-871
  for (int j = 1; j <= kNogrp; ++j) {
    for (int k = 1; k <= kNgrpp; ++k) {
      const int n = (j - 1) + (k - 1) * kNogrp;
      fl.all[n] = (c.flogrd[n] != 0 || c.flogr2[n] != 0) ? 1 : 0;
    }
  }
  *m.nrqgo = 0;  // :872-873; IT0 and IROOT (:874-875) are never read
  *m.nrqgo2 = 0;

  if (c.flout1 != 0 || c.flout7 != 0) {  // :876
    require(hooks.get_array_size != nullptr, "do_output_exchanges: get_array_size hook missing");
    const int the_size = hooks.get_array_size(hooks.ctx);
    const bool computes = c.iaproc <= c.naproc;
    const bool root = c.iaproc == c.napfld;
    const int nseal = c.nseal;

    A2 arrexch;
    I1 arrpos;
    if (computes) {  // :878-1287
      arrexch = A2(Kokkos::view_alloc(exec, Kokkos::WithoutInitializing, "dooutx_arrexch"),
                   the_size, nseal);
      arrpos = I1(Kokkos::view_alloc(exec, Kokkos::WithoutInitializing, "dooutx_arrpos"), nseal);
      // :880-1286, DO JSEA: ARRpos(JSEA) = ISEA, then the columns of JSEA in order
      const bool lpdlib = c.lpdlib != 0;
      const int* ip = iplg.data();
      Kokkos::parallel_for(
          "dooutx_pack", Policy(exec, 1, nseal + 1), KOKKOS_LAMBDA(const int jsea) {
            int isea = 0;
            ww::parall::init_get_isea(isea, jsea, true, true, lpdlib, c.gtype, c.iaproc,
                                      c.naproc, ip);
            arrpos(jsea - 1) = isea;
            walk_columns(
                c, fl, local,
                [&](int ih, const F1& v, Index1 ix) {
                  const int idx = ix == kJsea ? jsea - 1 : ix == kIsea0 ? isea : isea - 1;
                  arrexch(ih - 1, jsea - 1) = v(idx);
                },
                [&](int ih, const F2& v, int col) {
                  arrexch(ih - 1, jsea - 1) = v(jsea - 1, col);
                });
          });
    }

    // DO I: ARRtotal(:,ARRpos(I)) = ARRexch(:,I) (:1296-1298, :1316-1318). The positions of one
    // process are IPLG of distinct local nodes, residents then ghosts with no repeat
    // (PDLIB/yowpdlibmain.F90:947-1012), so the I of one call are independent. A ghost repeats
    // another process's point; the calls run in the Fortran's process order on one instance, so
    // the last process keeps the column, as in the Fortran.
    auto scatter = [&](const A2& total, const A2& src, const I1& pos, int n) {
      const int rows = the_size;
      Kokkos::parallel_for(
          "dooutx_scatter", Policy(exec, 0, n), KOKKOS_LAMBDA(const int i) {
            for (int r = 0; r < rows; ++r) total(r, pos(i) - 1) = src(r, i);
          });
    };

    A2 arrtotal;
    if (root) {  // :1293-1300; columns no process sends stay undefined, as after ALLOCATE
      arrtotal = A2(Kokkos::view_alloc(exec, Kokkos::WithoutInitializing, "dooutx_arrtotal"),
                    the_size, c.nsea);
      if (computes) scatter(arrtotal, arrexch, arrpos, nseal);
    }

    if (computes && !root) {  // :1301-1307
      require(hooks.mpi_send != nullptr, "do_output_exchanges: mpi_send hook missing");
      const int dest = c.napfld - 1;
      int eent = nseal;
      hooks.mpi_send(hooks.ctx, &eent, 1, kOutputMpiInteger, dest, 23);
      const auto hpos = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), arrpos);
      const auto hexch = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), arrexch);
      exec.fence();
      hooks.mpi_send(hooks.ctx, hpos.data(), nseal, kOutputMpiInteger, dest, 29);
      hooks.mpi_send(hooks.ctx, hexch.data(), nseal * the_size, kOutputMpiReal, dest, 37);
    }

    if (root) {
      for (int iproc = 1; iproc <= c.naproc; ++iproc) {  // :1308-1322
        if (iproc == c.iaproc) continue;
        require(hooks.mpi_recv != nullptr, "do_output_exchanges: mpi_recv hook missing");
        int eent = 0;
        hooks.mpi_recv(hooks.ctx, &eent, 1, kOutputMpiInteger, iproc - 1, 23);
        const int nseal_loc = eent;
        const I1 pos_loc(
            Kokkos::view_alloc(exec, Kokkos::WithoutInitializing, "dooutx_arrpos_loc"), nseal_loc);
        const A2 exch_loc(
            Kokkos::view_alloc(exec, Kokkos::WithoutInitializing, "dooutx_arrexch_loc"), the_size,
            nseal_loc);
        const auto hpos = Kokkos::create_mirror_view(Kokkos::HostSpace(), pos_loc);
        const auto hexch = Kokkos::create_mirror_view(Kokkos::HostSpace(), exch_loc);
        exec.fence();
        hooks.mpi_recv(hooks.ctx, hpos.data(), nseal_loc, kOutputMpiInteger, iproc - 1, 29);
        // :1315 receives the REAL words with MPI_INTEGER; the shim keeps that datatype.
        hooks.mpi_recv(hooks.ctx, hexch.data(), nseal_loc * the_size, kOutputMpiInteger,
                       iproc - 1, 37);
        Kokkos::deep_copy(exec, pos_loc, hpos);
        Kokkos::deep_copy(exec, exch_loc, hexch);
        scatter(arrtotal, exch_loc, pos_loc, nseal_loc);
        exec.fence();  // the host buffers die at the end of the iteration
      }

      // :1323-1729
      require(hooks.ainit2 && hooks.w3xdma && hooks.w3xeta && hooks.w3seta && hooks.output_fields,
              "do_output_exchanges: a root hook is missing");
      exec.fence();
      if (hooks.ainit2(hooks.ctx, c.imod) == 0) hooks.w3xdma(hooks.ctx, c.imod, fl.all);
      hooks.w3xeta(hooks.ctx, c.imod);
      const OutputExchangeFields<MemSpace> out = hooks.output_fields(hooks.ctx, c.imod);
      const int nsea = c.nsea;
      walk_columns(
          c, fl, out,
          [&](int ih, const F1& v, Index1 ix) {  // X(1:NSEA) = ARRtotal(IH,:)
            const int r = ih - 1;
            const int base = ix == kIsea0 ? 0 : 1;
            Kokkos::parallel_for(
                "dooutx_unpack1", Policy(exec, 1, nsea + 1), KOKKOS_LAMBDA(const int isea) {
                  v(isea - base) = arrtotal(r, isea - 1);
                });
          },
          [&](int ih, const F2& v, int col) {  // X(1:NSEA,K) = ARRtotal(IH,:)
            const int r = ih - 1;
            Kokkos::parallel_for(
                "dooutx_unpack2", Policy(exec, 1, nsea + 1), KOKKOS_LAMBDA(const int isea) {
                  v(isea - 1, col) = arrtotal(r, isea - 1);
                });
          });
      exec.fence();
      hooks.w3seta(hooks.ctx, c.imod);
    }
  }
  exec.fence();
  m.index_output = m.index_output + 1;  // :1732
}

}  // namespace ww::pdlib
