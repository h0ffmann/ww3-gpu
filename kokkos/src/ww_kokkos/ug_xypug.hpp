// kokkos/src/ww_kokkos/ug_xypug.hpp
// W3XYPUG with its explicit N-scheme W3XYPFSN2, and W3WAVE's W3GATH before and W3SCAT after it
// (W3_SHRD), for a batch of spectral components on an unstructured grid without PDLIB: one
// Kokkos team per component. Phase 1, translated, not regtested.
//
// WW3 heritage (WW3/ at h0ffmann/WW3 43601904, model/src):
//   W3XYPUG    w3profsmd.F90:69-272, A. Roland (FSN path only: :251-252)
//   W3XYPFSN2  w3profsmd.F90:470-727, A. Roland (non-PDLIB branch; W3_REF1 and W3_MGP not ported)
//   W3GATH     w3wavemd.F90:3293-3598 (W3_SHRD part: FIELD = 0., then FIELD(IXY) = A(ISPEC,ISEA))
//   W3SCAT     w3wavemd.F90:3614-3913 (W3_SHRD part: A(ISPEC,ISEA) = FIELD(IXY) where MAPSTA /= 0)
//   caller     w3wavemd.F90:2049-2143, section 3.6.3, FIELD = 0. (:2049) before the ISPEC loop
// SETDEPTH (w3profsmd.F90:1601-1678) is not here: the Fortran wrapper calls it once and passes
// IOBDP (xypug_c.hpp says why that gives the same IOBDP).
//
// Why one team per component is the Fortran's result. W3GATH sets all of FIELD (FIELD = 0., then
// the sea points), W3XYPUG and W3XYPFSN2 write only FIELD, their automatic arrays and
// ITER(IK,ITH) of their own component, and W3SCAT writes only VA(ISPEC,:). So the components are
// independent, and each team runs one in the Fortran's statement order.
//
// Inside a component, every loop the team splits writes elements of its own index only:
//  * W3GATH + W3XYPUG :207-236 per node IX. MAPSF(:,3) is one to one (the shim checks it), so the
//    Fortran's two loops over ISEA and its loops over IX give each node the same operations, which
//    run here in one pass over the nodes.
//  * W3XYPFSN2 :605-636 per triangle IE (KELEM, NM, FLALL), and :671 per IE (UTILDE).
//  * The scatters KKSUM(NI) = KKSUM(NI) + KELEM(:,IE) (:639-643) and ST(NI) = ST(NI) + KELEM(:,IE)*
//    (U(NI)-UTILDE) (:672) become gathers per node over the list node_ent of (IE, K) pairs of that
//    node in ascending IE, then K. The Fortran adds, for one node, the terms of IE = 1, 2, ... in
//    that order onto 0.d0 (memset), and within one IE the three K in order (GIMPLE: load, add,
//    store per K, no temporary), so the gather makes the same additions on the same operands in
//    the same order, even for a triangle that lists a node twice.
//  * The update :675-688 reads U(IP) and ST(IP) of its own node only, so it is fused with the ST
//    gather; AC = U (:690) makes U and AC one array: a node's U is read by the UTILDE pass of the
//    same sub-step, which ends at a barrier before any node is updated.
//  * DTSI(IP) = DBLE(DT)/DBLE(ITER)/SI(IP) (:662-664) is computed where it is used, from the same
//    operands in the same order, so it has the same bits.
// The CFL minimum over IP (:645-650), ITER (:651-659) and the boundary update over IBI
// (:694-720, duplicate IP: the last IBI wins) run on one thread of the team, in the Fortran's
// order. No reduction or atomic: the bits do not depend on the team size or the backend.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of w3profsmd.F90 (WW3's CMake
// Release flags, switch of ww3_tp2.6); evidence in the port notes of issue #196. gfortran's
// MIN(x,c) and MAX(x,c) of this file are minsd/maxsd with the constant as the source operand,
// x < c ? x : c and x > c ? x : c (a NaN or a +0/-0 tie gives c), and MIN(DTMAXGL, DTMAXEXP) is
// DTMAXGL < DTMAXEXP ? DTMAXGL : DTMAXEXP. NINT is a call of glibc's lround (low 32 bits kept),
// MOD(CFLXY,1.0d0) is x87 fprem (exact, as fmod), ABS of the integer is neg/cmovns.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <cstddef>
#include <string>

namespace ww::ug {

namespace xypug_detail {
/// gfortran's MIN/MAX with a constant source operand (minsd/maxsd x, c).
KOKKOS_INLINE_FUNCTION double min_sd(double x, double c) { return (x < c) ? x : c; }
KOKKOS_INLINE_FUNCTION double max_sd(double x, double c) { return (x > c) ? x : c; }
KOKKOS_INLINE_FUNCTION float min_ss(float x, float c) { return (x < c) ? x : c; }
KOKKOS_INLINE_FUNCTION float max_ss(float x, float c) { return (x > c) ? x : c; }

/// ONESIXTH = 1.0d0/6.0d0 (:563).
inline constexpr double kOneSixth = 1.0 / 6.0;
/// THR8 = TINY(1.0d0) (:564).
inline constexpr double kThr8 = 2.2250738585072014e-308;
/// DTMAXGL = DBLE(10.E10) (:600): the REAL literal 1e11 widened, 9.9999997952e10.
inline constexpr double kDtmaxGl0 = static_cast<double>(10.E10f);
/// MAX(DBLE(10.E-10), ...) (:647): the REAL literal 1e-9 widened.
inline constexpr double kKkFloor = static_cast<double>(10.E-10f);
/// RD2 .GT. 0.001 (:700), a REAL literal.
inline constexpr float kRd2Floor = 0.001f;

/// INTEGER(4) NINT of a double as the object computes it: glibc lround, low 32 bits. lround
/// equals round() converted to long for |x| < 2**63; beyond, and for NaN or Inf, glibc returns
/// LONG_MIN, whose low 32 bits are 0 (v: lround of 1e30, 9.3e18, NaN and Inf, glibc 2.44 of the
/// Nix ww3 shell, the glibc WW3's object links, 2026-10-18).
KOKKOS_INLINE_FUNCTION int nint32(double x) {
  if (!(Kokkos::fabs(x) < 9223372036854775808.0)) return 0;
  const long long l = static_cast<long long>(Kokkos::round(x));
  return static_cast<int>(static_cast<unsigned int>(static_cast<unsigned long long>(l)));
}

/// ABS of an INTEGER(4) as neg/cmovns: ABS(-2**31) stays -2**31.
KOKKOS_INLINE_FUNCTION int iabs32(int n) {
  return (n < 0) ? static_cast<int>(0u - static_cast<unsigned int>(n)) : n;
}
}  // namespace xypug_detail

/// The grid's geometry, uploaded when it changes. Fortran arrays are flattened column major,
/// 0-based:
///   trigp     TRIGP(3,NTRI) (w3gdatmd.F90:3235): TRIGP(K,IE) = trigp(3*(IE-1) + K-1)
///   ien       IEN(NTRI,6), REAL(8) (w3gdatmd.F90:3242): IEN(IE,J) = ien((J-1)*NTRI + IE-1)
///   si        SI(NX), REAL(8) (w3gdatmd.F90:3236): SI(IP) = si(IP-1)
///   node_isea ISEA with MAPSF(ISEA,3) = IP, or 0 for a node that is not a sea point (nx)
///   node_off, node_ent  the (IE, K) pairs of node IP, 3*(IE-1) + K-1, in ascending IE then K, at
///             node_ent(node_off(IP-1) .. node_off(IP)-1)
/// W3IOGR dimensions the grid with MTRI = NTRI and MX = NX (w3iogrmd.F90:1025), so the leading
/// extents are NTRI and NX.
template <class Mem>
struct XypugGrid {
  int nx = 0, ntri = 0;
  Kokkos::View<int*, Mem> trigp;
  Kokkos::View<double*, Mem> ien;
  Kokkos::View<double*, Mem> si;
  Kokkos::View<int*, Mem> node_isea;
  Kokkos::View<int*, Mem> node_off;
  Kokkos::View<int*, Mem> node_ent;
};

/// The per-call inputs, copied every call:
///   mapsta  MAPSTA(NY*NX), NY = 1: MAPSTA(IXY) = mapsta(IXY-1), read by W3SCAT
///   clats   CLATS(ISEA) = clats(ISEA-1)
///   ecos, esin  ECOS(ITH) = ecos(ITH-1)
///   cg      CG(0:NK+1,0:NSEA): CG(IK,ISEA) = cg(ISEA*(NK+2) + IK)
///   cx, cy  CX(0:NSEA): CX(ISEA) = cx(ISEA)
///   iobp, iobdp, iobpa  IOBP(IP) = iobp(IP-1), widened from INTEGER*2 / INTEGER*1
///   iobpd   IOBPD(NTH,NX): IOBPD(ITH,IP) = iobpd((IP-1)*NTH + ITH-1)
///   bnd_isea, bnd_ip  ISBPI(IBI) and MAPSF(ISBPI(IBI),1) for IBI = 1..NBI (flbpi only)
///   bbpi0, bbpin  BBPI0(NSPEC,0:NBI): BBPI0(ISP,IBI) = bbpi0(IBI*NSPEC + ISP-1)
///   iter    ITER(NK,NTH): ITER(IK,ITH) = iter((ITH-1)*NK + IK-1), read, and written under lcalc
///   lst     the ISPEC of each component of the batch
/// LOGICALs are int, tested != 0 as the object tests them.
template <class Mem>
struct XypugStep {
  int nsea = 0, nk = 0, nth = 0, nspec = 0, nbi = 0;
  int flcur = 0, flbpi = 0, fsbccfl = 0, lcalc = 0;
  /// Run-time switches of the bit-neutral speed-ups (WW_KOKKOS_XYPUG_*, xypug_shim.cpp); 0 is
  /// the translation as first written.
  int fuse_ut = 0;  ///< first sub-step's UTILDE computed in the per-triangle pass
  float facx = 0.f, facy = 0.f, dtg = 0.f, rd1 = 1.f, rd2 = 0.f;
  Kokkos::View<int*, Mem> mapsta, iobp, iobpd, iobdp, iobpa, bnd_isea, bnd_ip, iter, lst;
  Kokkos::View<float*, Mem> clats, ecos, esin, cg, cx, cy, bbpi0, bbpin;
};

/// The Fortran's automatic arrays, one slice per component of a launch, allocated by the
/// caller (never in a kernel): AC(NX) of W3XYPUG (also W3XYPFSN2's U), KKSUM(NX), C(NX,2),
/// KELEM(3,NTRI), FLALL(3,NTRI), NM(NTRI), and UTILDE per triangle (a scalar in the Fortran).
template <class Mem>
struct XypugWork {
  Kokkos::View<double*, Mem> ac, kksum, kelem, flall, nm, utilde;
  Kokkos::View<float*, Mem> c;
  /// Bytes of one component's slice.
  static std::size_t bytes_per_component(int nx, int ntri) {
    return static_cast<std::size_t>(nx) * (2 * sizeof(double) + 2 * sizeof(float)) +
           static_cast<std::size_t>(ntri) * 8 * sizeof(double);
  }
  /// Grows every slice array to hold `m` components; keeps it when it is large enough.
  void reserve(int nx, int ntri, int m) {
    const std::size_t n = static_cast<std::size_t>(nx) * static_cast<std::size_t>(m);
    const std::size_t t = static_cast<std::size_t>(ntri) * static_cast<std::size_t>(m);
    grow(ac, n, "ww_xypug.ac");
    grow(kksum, n, "ww_xypug.kksum");
    grow(c, 2 * n, "ww_xypug.c");
    grow(kelem, 3 * t, "ww_xypug.kelem");
    grow(flall, 3 * t, "ww_xypug.flall");
    grow(nm, t, "ww_xypug.nm");
    grow(utilde, t, "ww_xypug.utilde");
  }

 private:
  template <class V>
  static void grow(V& v, std::size_t n, const char* name) {
    if (v.extent(0) < n) {
      v = V();
      v = V(Kokkos::view_alloc(Kokkos::WithoutInitializing, std::string(name)), n > 0 ? n : 1);
    }
  }
};

/// @brief W3GATH, W3XYPUG (FSN), W3SCAT for components lst(k0) .. lst(k0+m-1), one team each.
/// @details va is the resident VA(NSPEC,0:NSEALM) of W3WDATMD in memory space Mem, column major:
/// VA(ISPEC,ISEA) = va[ISEA*NSPEC + ISPEC-1] (W3_SHRD: JSEA = ISEA). Each team reads and writes
/// row ISPEC of its own component only, and ITER(IK,ITH) of that component.
/// @param team_size the team size, or 0 for Kokkos::AUTO; the bits do not depend on it.
/// @pre g, s and w hold the sizes of this grid; w has room for m components; no two entries of
/// lst(k0 .. k0+m-1) are equal; MAPSF(:,3) is one to one onto node_isea.
/// @post VA(ISPEC,ISEA) of every sea point with MAPSTA /= 0, and ITER(IK,ITH) under lcalc, hold
/// the Fortran loop's values.
/// @author ww3-gpu
/// @date 2026-10-18
template <class ExecSpace>
void xypug_launch(const ExecSpace& exec, const XypugGrid<typename ExecSpace::memory_space>& g,
                  const XypugStep<typename ExecSpace::memory_space>& s,
                  const XypugWork<typename ExecSpace::memory_space>& w, float* va, int k0, int m,
                  int team_size) {
  using namespace xypug_detail;
  using Policy = Kokkos::TeamPolicy<ExecSpace>;
  using Member = typename Policy::member_type;
  if (m <= 0) return;

  const int nx = g.nx;
  const int ntri = g.ntri;
  const auto trigp = g.trigp;
  const auto ien = g.ien;
  const auto si = g.si;
  const auto node_isea = g.node_isea;
  const auto node_off = g.node_off;
  const auto node_ent = g.node_ent;

  const int nk = s.nk;
  const int nth = s.nth;
  const int nspec = s.nspec;
  const int nbi = s.nbi;
  const int flcur = s.flcur;
  const int flbpi = s.flbpi;
  const int fsbccfl = s.fsbccfl;
  const int lcalc = s.lcalc;
  const int fuse_ut = s.fuse_ut;
  const float facx = s.facx;
  const float facy = s.facy;
  const float dtg = s.dtg;
  const float rd10 = s.rd1;
  const float rd20 = s.rd2;
  const auto mapsta = s.mapsta;
  const auto iobp = s.iobp;
  const auto iobpd = s.iobpd;
  const auto iobdp = s.iobdp;
  const auto iobpa = s.iobpa;
  const auto bnd_isea = s.bnd_isea;
  const auto bnd_ip = s.bnd_ip;
  const auto iter_v = s.iter;
  const auto lst = s.lst;
  const auto clats = s.clats;
  const auto ecos = s.ecos;
  const auto esin = s.esin;
  const auto cg = s.cg;
  const auto cx = s.cx;
  const auto cy = s.cy;
  const auto bbpi0 = s.bbpi0;
  const auto bbpin = s.bbpin;

  const auto w_ac = w.ac;
  const auto w_kksum = w.kksum;
  const auto w_c = w.c;
  const auto w_kelem = w.kelem;
  const auto w_flall = w.flall;
  const auto w_nm = w.nm;
  const auto w_utilde = w.utilde;

  const Policy pol = team_size > 0 ? Policy(exec, m, team_size) : Policy(exec, m, Kokkos::AUTO);
  Kokkos::parallel_for(
      "ww_xypug_batch", pol, KOKKOS_LAMBDA(const Member& t) {
        const int i = t.league_rank();
        const int isp = lst(k0 + i);
        const int ith = 1 + (isp - 1) % nth;  // :187, :598
        const int ik = 1 + (isp - 1) / nth;   // :188, :599
        const int nk2 = nk + 2;
        const std::size_t sn = static_cast<std::size_t>(i) * static_cast<std::size_t>(nx);
        const std::size_t st3 = static_cast<std::size_t>(i) * 3 * static_cast<std::size_t>(ntri);
        const std::size_t st1 = static_cast<std::size_t>(i) * static_cast<std::size_t>(ntri);
        double* const ac = w_ac.data() + sn;
        double* const kksum = w_kksum.data() + sn;
        float* const c1 = w_c.data() + 2 * sn;  // C(:,1)
        float* const c2 = c1 + nx;              // C(:,2)
        double* const kelem = w_kelem.data() + st3;
        double* const flall = w_flall.data() + st3;
        double* const nm = w_nm.data() + st1;
        double* const utilde = w_utilde.data() + st1;
        float* const va_row = va + (isp - 1);  // VA(ISP,ISEA) = va_row[ISEA*NSPEC]

        // W3XYPUG 1.a (:190-193).
        const float ccos = facx * ecos(ith - 1);
        const float csin = facy * esin(ith - 1);

        // W3GATH (FIELD = 0., FIELD(IXY) = A(ISPEC,ISEA)) and W3XYPUG :197-236 per node.
        Kokkos::parallel_for(Kokkos::TeamThreadRange(t, nx), [&](const int p) {
          const int isea = node_isea(p);
          float vq = 0.0f;   // FIELD = 0.
          float vlx = 0.0f;  // VLCFLX = 0. (:197)
          float vly = 0.0f;  // VLCFLY = 0. (:198)
          if (isea > 0) {
            const float cgv = cg(static_cast<std::size_t>(isea) * nk2 + ik);
            const float cl = clats(isea - 1);
            // :209, (VQ/CG)*CLATS.
            vq = va_row[static_cast<std::size_t>(isea) * nspec] / cgv * cl;
            vlx = cgv * ccos / cl;  // :210, the object's (CG*CCOS)/CLATS
            vly = cgv * csin;       // :211
            // :218-229, IOBP is INTEGER*2 compared with 1 (cmpw $0x1).
            if (flcur != 0 && iobp(p) == 1) {
              vlx = vlx + facx * cx(isea) / cl;  // CCURX = FACX (:192)
              vly = vly + facy * cy(isea);       // CCURY = FACY (:193)
            }
          }
          const int d = iobdp(p);
          // :234, DBLE(VQ)*DBLE(IOBDP) (cvtss2sd, cvtsi2sd, mulsd).
          ac[p] = static_cast<double>(vq) * static_cast<double>(d);
          // :235-236, VLCFL*REAL(IOBDP) in REAL.
          c1[p] = vlx * static_cast<float>(d);
          c2[p] = vly * static_cast<float>(d);
        });
        t.team_barrier();

        // ITER(IK,ITH), column major (NK,NTH).
        const int iter_at = (ith - 1) * nk + (ik - 1);
        // FLALL is read by the sub-steps 2..ITER only when the first one is fused; ITER is known
        // here only without LCALC (it is read from the caller), else it is stored.
        const bool store_flall = fuse_ut == 0 || lcalc != 0 || iter_v(iter_at) > 1;

        // W3XYPFSN2 :605-636, one triangle per index.
        Kokkos::parallel_for(Kokkos::TeamThreadRange(t, ntri), [&](const int e) {
          const int i1 = trigp(3 * e) - 1;
          const int i2 = trigp(3 * e + 1) - 1;
          const int i3 = trigp(3 * e + 2) - 1;
          const double e1 = ien(e);
          const double e2 = ien(static_cast<std::size_t>(ntri) + e);
          const double e3 = ien(2 * static_cast<std::size_t>(ntri) + e);
          const double e4 = ien(3 * static_cast<std::size_t>(ntri) + e);
          const double e5 = ien(4 * static_cast<std::size_t>(ntri) + e);
          const double e6 = ien(5 * static_cast<std::size_t>(ntri) + e);
          // :611-612, ((C1+C2)+C3) in REAL, then widened, * ONESIXTH.
          const double lam1 = static_cast<double>((c1[i1] + c1[i2]) + c1[i3]) * kOneSixth;
          const double lam2 = static_cast<double>((c2[i1] + c2[i2]) + c2[i3]) * kOneSixth;
          // :613-615.
          const double k1 = lam1 * e1 + lam2 * e2;
          const double k2 = lam1 * e3 + lam2 * e4;
          const double k3 = lam1 * e5 + lam2 * e6;
          // :618, SUM(MIN(0.d0,KTMP)) is ((m1 + 0.) + m2) + m3: the + 0. turns a -0 into +0.
          double sum = min_sd(k1, 0.0) + 0.0;
          sum = sum + min_sd(k2, 0.0);
          sum = sum + min_sd(k3, 0.0);
          // :618, -1.D0/MIN(-THR8,SUM): {-1,-1}/den in the vector loop the contiguous C takes;
          // den is never NaN or zero, so it equals -(1/den) bit for bit.
          nm[e] = -1.0 / min_sd(sum, -kThr8);
          // :619.
          const double kk1 = max_sd(k1, 0.0);
          const double kk2 = max_sd(k2, 0.0);
          const double kk3 = max_sd(k3, 0.0);
          kelem[3 * e] = kk1;
          kelem[3 * e + 1] = kk2;
          kelem[3 * e + 2] = kk3;
          // :620-625, REAL times REAL(8): each C widened.
          const double c1i1 = c1[i1], c2i1 = c2[i1];
          const double c1i2 = c1[i2], c2i2 = c2[i2];
          const double c1i3 = c1[i3], c2i3 = c2[i3];
          const double fl11 = c1i2 * e1 + c2i2 * e2;
          const double fl12 = c1i3 * e1 + c2i3 * e2;
          const double fl21 = c1i3 * e3 + c2i3 * e4;
          const double fl22 = c1i1 * e3 + c2i1 * e4;
          const double fl31 = c1i1 * e5 + c2i1 * e6;
          const double fl32 = c1i2 * e5 + c2i2 * e6;
          // :626-631.
          const double fl111 = 2.0 * fl11 + fl12;
          const double fl112 = 2.0 * fl12 + fl11;
          const double fl211 = 2.0 * fl21 + fl22;
          const double fl212 = 2.0 * fl22 + fl21;
          const double fl311 = 2.0 * fl31 + fl32;
          const double fl312 = 2.0 * fl32 + fl31;
          // :632-634, (a + b) * ONESIXTH, then + KELEM after the MAX of :619.
          const double fa1 = (fl311 + fl212) * kOneSixth + kk1;
          const double fa2 = (fl111 + fl312) * kOneSixth + kk2;
          const double fa3 = (fl211 + fl112) * kOneSixth + kk3;
          if (store_flall) {
            flall[3 * e] = fa1;
            flall[3 * e + 1] = fa2;
            flall[3 * e + 2] = fa3;
          }
          if (fuse_ut != 0) {
            // :671 of the first sub-step with the registers just computed: the same products
            // and sums in the same order as the sub-step loop below; AC is complete (barrier).
            double d = fa1 * ac[i1] + 0.0;
            d = d + fa2 * ac[i2];
            d = d + fa3 * ac[i3];
            utilde[e] = nm[e] * d;
          }
        });
        t.team_barrier();
        int iter = 0;
        if (lcalc != 0) {
          // :638-643, KKSUM = 0.d0, then in IE order KKSUM(NI) + KELEM(:,IE).
          Kokkos::parallel_for(Kokkos::TeamThreadRange(t, nx), [&](const int p) {
            double acc = 0.0;
            for (int j = node_off(p); j < node_off(p + 1); ++j) acc = acc + kelem[node_ent(j)];
            kksum[p] = acc;
          });
          t.team_barrier();
          // :644-659 on one thread: the MIN chain depends on the order of IP.
          Kokkos::single(
              Kokkos::PerTeam(t),
              [&](int& out) {
                double dtmaxgl = kDtmaxGl0;  // :600
                for (int p = 0; p < nx; ++p) {
                  if (iobp(p) == 1 || fsbccfl != 0) {  // :646
                    // :647, KKSUM*IOBDP in REAL(8), MAX with 1e-9, SI / that.
                    const double q = kksum[p] * static_cast<double>(iobdp(p));
                    const double dtmaxexp = si(p) / max_sd(q, kKkFloor);
                    // :648, minsd: DTMAXGL < DTMAXEXP ? DTMAXGL : DTMAXEXP.
                    dtmaxgl = (dtmaxgl < dtmaxexp) ? dtmaxgl : dtmaxexp;
                  }
                }
                const double cflxy = static_cast<double>(dtg) / dtmaxgl;  // :651
                const double rest = Kokkos::fabs(Kokkos::fmod(cflxy, 1.0));  // :652
                const int an = iabs32(nint32(cflxy));
                int n = an;                                            // :653-654, :657-658
                if (!(rest < kThr8) && rest > kThr8 && rest < 0.5) {  // :655
                  n = static_cast<int>(static_cast<unsigned int>(an) + 1u);  // :656
                }
                iter_v(iter_at) = n;
                out = n;
              },
              iter);
        } else {
          iter = iter_v(iter_at);
        }

        // :662-664, DBLE(DT)/DBLE(ITER)/SI(IP) associates left to right in the source: DT/ITER
        // first, then / SI(IP), the same two divisions in the same order as here.
        const double dt_iter = static_cast<double>(dtg) / static_cast<double>(iter);
        const int ith0 = ith - 1;

        // :666-722, the sub-steps; the trip count is ITER(IK,ITH) read once.
        for (int it = 1; it <= iter; ++it) {
          // :671, UTILDE = NM*DOT_PRODUCT(FLALL(:,IE),U(NI)): ((f1*u1 + 0.) + f2*u2) + f3*u3.
          // The first sub-step's was made in the pre-pass when fuse_ut is on (U is still AC).
          if (it > 1 || fuse_ut == 0) {
            Kokkos::parallel_for(Kokkos::TeamThreadRange(t, ntri), [&](const int e) {
              double d = flall[3 * e] * ac[trigp(3 * e) - 1] + 0.0;
              d = d + flall[3 * e + 1] * ac[trigp(3 * e + 1) - 1];
              d = d + flall[3 * e + 2] * ac[trigp(3 * e + 2) - 1];
              utilde[e] = nm[e] * d;
            });
            t.team_barrier();
          }

          // :667-668 (U = AC, ST = 0.d0), :672 as a gather in IE order, :675-690 per node.
          Kokkos::parallel_for(Kokkos::TeamThreadRange(t, nx), [&](const int p) {
            const double u = ac[p];
            double st = 0.0;
            for (int j = node_off(p); j < node_off(p + 1); ++j) {
              const int ek = node_ent(j);
              st = st + kelem[ek] * (u - utilde[ek / 3]);
            }
            const double dtsi = dt_iter / si(p);
            // :680, (DTSI*ST)*DBLE(1-IOBPA) (the integer difference converted), U minus that,
            // MAX with 0, then DBLE(IOBPD(ITH,IP)) times it.
            const double a = (dtsi * st) * static_cast<double>(1 - iobpa(p));
            const double v = max_sd(u - a, 0.0);
            ac[p] = static_cast<double>(iobpd(static_cast<std::size_t>(p) * nth + ith0)) * v;
          });
          t.team_barrier();

          // :694-720 on one thread, in IBI order.
          if (flbpi != 0) {
            Kokkos::single(Kokkos::PerTeam(t), [&]() {
              float rd1 = 0.0f;
              float rd2 = rd20;  // :699
              if (rd2 > kRd2Floor) {
                // :698, DT*REAL(ITER-IT) / REAL(ITER), subtracted from RD10.
                rd1 = rd10 - dtg * static_cast<float>(iter - it) / static_cast<float>(iter);
                rd2 = min_ss(max_ss(rd1 / rd2, 0.0f), 1.0f);  // :701
                rd1 = 1.0f - rd2;                             // :702
              } else {
                rd1 = 0.0f;  // :704
                rd2 = 1.0f;  // :705
              }
              for (int ibi = 1; ibi <= nbi; ++ibi) {
                const int isb = bnd_isea(ibi - 1);
                const int p = bnd_ip(ibi - 1);  // MAPSF(ISBPI(IBI),1) (:711)
                const std::size_t q = static_cast<std::size_t>(ibi) * nspec + (isp - 1);
                // :715-716, in REAL, widened on the store.
                const float b = (rd1 * bbpi0(q) + rd2 * bbpin(q)) /
                                cg(static_cast<std::size_t>(isb) * nk2 + ik) * clats(isb - 1);
                ac[p - 1] = static_cast<double>(b);
              }
            });
            t.team_barrier();
          }
        }

        // W3XYPUG :261-271 and W3SCAT: VQ = REAL(AC), then for the sea points
        // MAX(0., (CG/CLATS)*VQ) (maxss with +0 as the source) where MAPSTA(IXY) /= 0.
        Kokkos::parallel_for(Kokkos::TeamThreadRange(t, nx), [&](const int p) {
          const int isea = node_isea(p);
          if (isea > 0 && mapsta(p) != 0) {
            const float vq = static_cast<float>(ac[p]);
            const float x = cg(static_cast<std::size_t>(isea) * nk2 + ik) / clats(isea - 1) * vq;
            va_row[static_cast<std::size_t>(isea) * nspec] = max_ss(x, 0.0f);
          }
        });
      });
}

}  // namespace ww::ug
