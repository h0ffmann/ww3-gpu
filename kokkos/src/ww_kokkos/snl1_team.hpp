// kokkos/src/ww_kokkos/snl1_team.hpp
// W3SNL1 for one sea point, run by one Kokkos team: the DIA nonlinear
// interactions and their diagonal, callable from any team kernel.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE W3SNL1, lines 115-473; the numbered section comments below are that
// routine's own (sections 1-4, lines 338-440). Phase-1 rules apply: the
// expressions, their order and their float32 arithmetic are the Fortran's. The
// only thing that changed is who executes them.
//
// snl1() (snl1_dia.cpp) launches one team per point and calls snl1_team(); a
// W3SRCE team kernel calls it on its own point, between team barriers, with the
// point's spectrum in its own arrays. Inside a team the sections are
// data-parallel over ISP, with one exception: the high-frequency extension in
// section 2 marches upward in frequency reading the row below, so its frequency
// loop is sequential and each row ends in a team_barrier.
//
// The body contains no reduction: every output element is written by exactly one
// thread from inputs no thread modifies, so the result is bit-identical across
// backends, team sizes and thread counts.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <Kokkos_Core.hpp>

#include "libm_device.hpp"
#include "real.hpp"
#include "snl1_config.hpp"
#include "snl1_tables.hpp"

// GCC 16 at -O3 leaves the bodies of sections 3 and 4 out of line inside the batched W3SRCE
// kernel: TeamThreadRange then calls a function per spectral bin, which reloads every captured
// variable through the closure on each call (2026-10-17, profile of that kernel on OpenMP).
// always_inline puts the body back in the loop; it changes where the code sits, not one floating-
// point operation (-ffp-contract=off still holds). Device compilers inline these on their own.
#if defined(__GNUC__) && !defined(__CUDACC__) && !defined(__HIPCC__) && !defined(__SYCL_DEVICE_ONLY__)
#define WW_SNL1_INLINE_BODY __attribute__((always_inline))
#else
#define WW_SNL1_INLINE_BODY
#endif

// Sections 3 and 4 as `#pragma omp simd` loops on a host team. Each bin writes only its own slot
// (sa*/da* in section 3, s and d in section 4) and reads only ue, the tables and the slots of the
// section before, so the loop carries no dependence, and it has no reduction a lane split could
// reorder: every lane does one bin's operations in the bin's order. Without the pragma GCC 16
// vectorises neither loop (it cannot tell the gathers through the index tables from the stores);
// gfortran -O3 vectorises section 3 of W3SNL1 (w3snl1md.F90:380-412) on four SSE lanes and leaves
// section 4 scalar (2026-10-17, the hot loops of the build's w3snl1md.F90.o).
#if defined(__GNUC__) && !defined(__CUDACC__) && !defined(__HIPCC__) && !defined(__SYCL_DEVICE_ONLY__) && defined(_OPENMP)
#define WW_SNL1_HOST_SIMD 1
#endif

namespace ww::snl1 {

/// parallel_for(TeamThreadRange(team, n), body), with the host team's range run as a simd loop
/// (WW_SNL1_HOST_SIMD). The range of each team member is Kokkos' own.
template <class TeamMember, class Body>
KOKKOS_FORCEINLINE_FUNCTION void team_bins(const TeamMember& team, const int n, const Body& body) {
#if defined(WW_SNL1_HOST_SIMD)
  const auto r = Kokkos::TeamThreadRange(team, n);
#pragma omp simd
  for (int i = r.start; i < r.end; ++i) body(i);
#else
  Kokkos::parallel_for(Kokkos::TeamThreadRange(team, n), body);
#endif
}

/// W3SNL1's working arrays for one point, in team scratch.
///
/// Fortran's UE(1-NTH:NSPECY) and SA*/DA*(1-NTH:NSPECX) become 0-based arrays
/// of length (upper + NTH): a Fortran index J lives at slot J - 1 + NTH, and a
/// table entry (already Fortran-minus-one, snl1_tables.hpp) at slot entry + NTH.
/// `con` is CON(1:NSPEC).
struct TeamScratch {
  Real* ue;    ///< NSPECY + NTH
  Real* sa1;   ///< NSPECX + NTH, as are the eight below
  Real* sa2;
  Real* da1c;
  Real* da1p;
  Real* da1m;
  Real* da2c;
  Real* da2p;
  Real* da2m;
  Real* con;   ///< NSPEC
  Real* uep;   ///< UE in the halo layout, (NFRHGH+1) rows of NTH+2*halo; host halo path only
};

/// The host kernel reads the DIA's arrays in a halo layout when the tables allow it: row r of
/// the Fortran array (the bins (r-1)*NTH+1 .. r*NTH, row 0 the zero bins 1-NTH..0) holds its NTH
/// values at slots halo .. halo+NTH-1 and copies of the wrapped neighbours on both sides. An
/// address IT(ITH) + (IF-1)*NTH is then IF*W + halo + ith0 + shift, the same element for every
/// ITH of a row, so the loads of a row are contiguous instead of gathers. The values read and the
/// arithmetic are those of the gather form; only where the copies sit changes.
#if defined(WW_SNL1_HOST_SIMD)
inline constexpr bool kHostHalo = true;
#define WW_SNL1_SIMD _Pragma("omp simd")
#else
inline constexpr bool kHostHalo = false;
#define WW_SNL1_SIMD
#endif

/// Unmanaged because team scratch is a bump allocator: the view never owns it.
template <class ScratchSpace>
using ScratchRealView = Kokkos::View<Real*, Kokkos::LayoutRight, ScratchSpace,
                                     Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// Bytes of team scratch snl1_team_scratch() takes from one team, alignment
/// padding included: (NSPECY + NTH) + 8*(NSPECX + NTH) + NSPEC floats. A caller
/// requests this on top of its own scratch, at the level it passes to
/// snl1_team_scratch(). For the lab's NK=25/NTH=24 grid it is ~28 KB, ~52 KB at
/// NK=32/NTH=36.
///
/// The level is the caller's choice. TeamPolicy::scratch_size_max(0) is, for
/// the Kokkos 5.2 CUDA backend, the device's opt-in shared-memory limit minus
/// ~24.6 KB (about 75 KB on an RTX 4090); the host backends' level-0 ceiling is
/// smaller: WW3's ww3_ts1 grid (NK=36, NTH=24) asks for 39 KB, which the OpenMP
/// backend refused at level 0 (2026-10-14). Level 1 holds the same buffers in
/// global memory on a GPU and in the same host memory on the CPU backends; the
/// level changes where the buffers live, not the arithmetic.
template <class ExecSpace>
std::size_t snl1_team_scratch_bytes(const Config& c, const Tables& t) {
  using ScratchReal = ScratchRealView<typename ExecSpace::scratch_memory_space>;
  if (kHostHalo && t.halo > 0) {
    const int w = c.nth + 2 * t.halo;
    return ScratchReal::shmem_size(t.nspecy + c.nth) +
           8 * ScratchReal::shmem_size((t.nfrchg + 1) * w) + ScratchReal::shmem_size(t.nspec) +
           ScratchReal::shmem_size((t.nfrhgh + 1) * w);
  }
  return ScratchReal::shmem_size(t.nspecy + c.nth) +
         8 * ScratchReal::shmem_size(t.nspecx + c.nth) +
         ScratchReal::shmem_size(t.nspec);
}

/// Takes W3SNL1's working arrays from `team`'s scratch at `level`. Team scratch
/// is a bump allocator that a kernel cannot give back, so a caller that runs
/// snl1_team() more than once per team (W3SRCE's integration loop) calls this
/// once and passes the same TeamScratch to every call.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION TeamScratch snl1_team_scratch(const TeamMember& team, const int level,
                                                     const Config& c, const Tables& t) {
  using ScratchReal = ScratchRealView<typename TeamMember::execution_space::scratch_memory_space>;
  const bool halo = kHostHalo && t.halo > 0;
  const int w = c.nth + 2 * t.halo;
  const int ue_len = t.nspecy + c.nth;
  const int sa_len = halo ? (t.nfrchg + 1) * w : t.nspecx + c.nth;
  // Taken in this order and these lengths so the sum is snl1_team_scratch_bytes().
  ScratchReal ue(team.team_scratch(level), ue_len);
  ScratchReal sa1(team.team_scratch(level), sa_len);
  ScratchReal sa2(team.team_scratch(level), sa_len);
  ScratchReal da1c(team.team_scratch(level), sa_len);
  ScratchReal da1p(team.team_scratch(level), sa_len);
  ScratchReal da1m(team.team_scratch(level), sa_len);
  ScratchReal da2c(team.team_scratch(level), sa_len);
  ScratchReal da2p(team.team_scratch(level), sa_len);
  ScratchReal da2m(team.team_scratch(level), sa_len);
  ScratchReal con(team.team_scratch(level), t.nspec);
  Real* uep = nullptr;
  if (halo) uep = ScratchReal(team.team_scratch(level), (t.nfrhgh + 1) * w).data();
  return TeamScratch{ue.data(),   sa1.data(),  sa2.data(),  da1c.data(), da1p.data(),
                     da1m.data(), da2c.data(), da2p.data(), da2m.data(), con.data(), uep};
}

/// Sections 3 and 4 of W3SNL1 in the halo layout (kHostHalo, t.halo > 0): the statements of
/// snl1_team() below, with each `x[TABLE(isp) + nth]` read as `x_row[ith0]`, the row and shift
/// that make_tables checked equal that address. Each row is one team thread's, its ITH loop a
/// simd loop. Sections 1 and 2 ran before, into `ue`; this copies it into the halo layout.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void snl1_team_halo(const TeamMember& team, const Tables& t, const int nth,
                                           const Real cons, const Real* ue, const TeamScratch& w,
                                           Real* s, Real* d) {
  if constexpr (kHostHalo) {
    const int h = t.halo;
    const int wd = nth + 2 * h;
    const int nfr = t.nfr;
    const int nfrchg = t.nfrchg;
    const Real dal1 = t.dal1;
    const Real dal2 = t.dal2;
    const Real dal3 = t.dal3;
    Real* const uep = w.uep;
    Real* const out[8] = {w.sa1, w.sa2, w.da1c, w.da1p, w.da1m, w.da2c, w.da2p, w.da2m};
    // Slot j of row r is direction j - h, wrapped once (|shift| <= h < nth).
    const auto wrap = [nth](int j) { return j < 0 ? j + nth : (j >= nth ? j - nth : j); };

    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, t.nfrhgh + 1), [&](const int r) {
      for (int j = 0; j < wd; ++j) uep[r * wd + j] = ue[r * nth + wrap(j - h)];
    });
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, 8), [&](const int a) {
      for (int j = 0; j < wd; ++j) out[a][j] = static_cast<Real>(0);  // row 0: the zero bins
    });
    team.team_barrier();

    // Row of table (itn, ifn) for frequency row ifr0, shifted so that [ith0] is its entry.
    const auto at = [&](const Real* x, int itn, int ifn, int ifr0) {
      return x + t.ifrow(ifn - 1, ifr0) * wd + h + t.itd[itn - 1];
    };

    // 3.  Calculate interactions for extended spectrum ---------------- *
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nfrchg), [&](const int ifr0) {
      const Real* const u = uep + (ifr0 + 1) * wd + h;
      const Real* const p11 = at(uep, 2, 2, ifr0);  // IP11 .. IP14
      const Real* const p12 = at(uep, 1, 2, ifr0);
      const Real* const p13 = at(uep, 2, 1, ifr0);
      const Real* const p14 = at(uep, 1, 1, ifr0);
      const Real* const m11 = at(uep, 8, 4, ifr0);  // IM11 .. IM14
      const Real* const m12 = at(uep, 7, 4, ifr0);
      const Real* const m13 = at(uep, 8, 3, ifr0);
      const Real* const m14 = at(uep, 7, 3, ifr0);
      const Real* const p21 = at(uep, 6, 2, ifr0);  // IP21 .. IP24
      const Real* const p22 = at(uep, 5, 2, ifr0);
      const Real* const p23 = at(uep, 6, 1, ifr0);
      const Real* const p24 = at(uep, 5, 1, ifr0);
      const Real* const m21 = at(uep, 4, 4, ifr0);  // IM21 .. IM24
      const Real* const m22 = at(uep, 3, 4, ifr0);
      const Real* const m23 = at(uep, 4, 3, ifr0);
      const Real* const m24 = at(uep, 3, 3, ifr0);
      const int o = (ifr0 + 1) * wd + h;
      Real* const sa1 = w.sa1 + o;
      Real* const sa2 = w.sa2 + o;
      Real* const da1c = w.da1c + o;
      Real* const da1p = w.da1p + o;
      Real* const da1m = w.da1m + o;
      Real* const da2c = w.da2c + o;
      Real* const da2p = w.da2p + o;
      Real* const da2m = w.da2m + o;
      WW_SNL1_SIMD
      for (int i = 0; i < nth; ++i) {
        const int isp = i + ifr0 * nth;
        // 3.a Energy at interacting bins
        const Real e00 = u[i];
        const Real ep1 = t.awg[0] * p11[i] + t.awg[1] * p12[i] + t.awg[2] * p13[i] + t.awg[3] * p14[i];
        const Real em1 = t.awg[4] * m11[i] + t.awg[5] * m12[i] + t.awg[6] * m13[i] + t.awg[7] * m14[i];
        const Real ep2 = t.awg[0] * p21[i] + t.awg[1] * p22[i] + t.awg[2] * p23[i] + t.awg[3] * p24[i];
        const Real em2 = t.awg[4] * m21[i] + t.awg[5] * m22[i] + t.awg[6] * m23[i] + t.awg[7] * m24[i];

        // 3.b Contribution to interactions
        const Real factor = cons * t.af11(isp) * e00;

        const Real sa1a = e00 * (ep1 * dal1 + em1 * dal2);
        const Real sa1b = sa1a - ep1 * em1 * dal3;
        const Real sa2a = e00 * (ep2 * dal1 + em2 * dal2);
        const Real sa2b = sa2a - ep2 * em2 * dal3;

        sa1[i] = factor * sa1b;
        sa2[i] = factor * sa2b;

        da1c[i] = cons * t.af11(isp) * (sa1a + sa1b);
        da1p[i] = factor * (dal1 * e00 - dal3 * em1);
        da1m[i] = factor * (dal2 * e00 - dal3 * ep1);

        da2c[i] = cons * t.af11(isp) * (sa2a + sa2b);
        da2p[i] = factor * (dal1 * e00 - dal3 * em2);
        da2m[i] = factor * (dal2 * e00 - dal3 * ep2);
      }
    });
    team.team_barrier();

    // The wrapped copies of the rows section 3 wrote (row 0 stays zero).
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, 8 * nfrchg), [&](const int q) {
      Real* const row = out[q / nfrchg] + (q % nfrchg + 1) * wd;
      for (int j = 0; j < h; ++j) {
        row[j] = row[j + nth];
        row[h + nth + j] = row[h + j];
      }
    });
    team.team_barrier();

    // 4.  Put source and diagonal term together ----------------------- *
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nfr), [&](const int ifr0) {
      const int o = (ifr0 + 1) * wd + h;
      const Real* const sa1 = w.sa1 + o;
      const Real* const sa2 = w.sa2 + o;
      const Real* const da1c = w.da1c + o;
      const Real* const da2c = w.da2c + o;
      // IC(j)1 reads SA1, DA1P (j <= 4) or DA1M; IC(j)2 reads SA2, DA2P or DA2M.
      const Real* const s11 = at(w.sa1, 6, 6, ifr0);
      const Real* const s21 = at(w.sa1, 5, 6, ifr0);
      const Real* const s31 = at(w.sa1, 6, 5, ifr0);
      const Real* const s41 = at(w.sa1, 5, 5, ifr0);
      const Real* const s51 = at(w.sa1, 4, 8, ifr0);
      const Real* const s61 = at(w.sa1, 3, 8, ifr0);
      const Real* const s71 = at(w.sa1, 4, 7, ifr0);
      const Real* const s81 = at(w.sa1, 3, 7, ifr0);
      const Real* const s12 = at(w.sa2, 2, 6, ifr0);
      const Real* const s22 = at(w.sa2, 1, 6, ifr0);
      const Real* const s32 = at(w.sa2, 2, 5, ifr0);
      const Real* const s42 = at(w.sa2, 1, 5, ifr0);
      const Real* const s52 = at(w.sa2, 8, 8, ifr0);
      const Real* const s62 = at(w.sa2, 7, 8, ifr0);
      const Real* const s72 = at(w.sa2, 8, 7, ifr0);
      const Real* const s82 = at(w.sa2, 7, 7, ifr0);
      const Real* const p11 = at(w.da1p, 6, 6, ifr0);
      const Real* const p21 = at(w.da1p, 5, 6, ifr0);
      const Real* const p31 = at(w.da1p, 6, 5, ifr0);
      const Real* const p41 = at(w.da1p, 5, 5, ifr0);
      const Real* const n51 = at(w.da1m, 4, 8, ifr0);
      const Real* const n61 = at(w.da1m, 3, 8, ifr0);
      const Real* const n71 = at(w.da1m, 4, 7, ifr0);
      const Real* const n81 = at(w.da1m, 3, 7, ifr0);
      const Real* const p12 = at(w.da2p, 2, 6, ifr0);
      const Real* const p22 = at(w.da2p, 1, 6, ifr0);
      const Real* const p32 = at(w.da2p, 2, 5, ifr0);
      const Real* const p42 = at(w.da2p, 1, 5, ifr0);
      const Real* const n52 = at(w.da2m, 8, 8, ifr0);
      const Real* const n62 = at(w.da2m, 7, 8, ifr0);
      const Real* const n72 = at(w.da2m, 8, 7, ifr0);
      const Real* const n82 = at(w.da2m, 7, 7, ifr0);
      Real* const sr = s + ifr0 * nth;
      Real* const dr = d + ifr0 * nth;
      const Real* const cr = w.con + ifr0 * nth;
      WW_SNL1_SIMD
      for (int i = 0; i < nth; ++i) {
        sr[i] = cr[i] * (-static_cast<Real>(2) * (sa1[i] + sa2[i]) +
                         t.awg[0] * (s11[i] + s12[i]) +
                         t.awg[1] * (s21[i] + s22[i]) +
                         t.awg[2] * (s31[i] + s32[i]) +
                         t.awg[3] * (s41[i] + s42[i]) +
                         t.awg[4] * (s51[i] + s52[i]) +
                         t.awg[5] * (s61[i] + s62[i]) +
                         t.awg[6] * (s71[i] + s72[i]) +
                         t.awg[7] * (s81[i] + s82[i]));

        dr[i] = -static_cast<Real>(2) * (da1c[i] + da2c[i]) +
                t.swg[0] * (p11[i] + p12[i]) +
                t.swg[1] * (p21[i] + p22[i]) +
                t.swg[2] * (p31[i] + p32[i]) +
                t.swg[3] * (p41[i] + p42[i]) +
                t.swg[4] * (n51[i] + n52[i]) +
                t.swg[5] * (n61[i] + n62[i]) +
                t.swg[6] * (n71[i] + n72[i]) +
                t.swg[7] * (n81[i] + n82[i]);
      }
    });
  } else {
    (void)team; (void)t; (void)nth; (void)cons; (void)ue; (void)w; (void)s; (void)d;
  }
}

/// W3SNL1 for one point, executed by the whole of `team`.
///
/// `sig` is SIG(1:NK), `a` the point's action spectrum A(1:NSPEC), `cg`
/// CG(1:NK), `kdmean` its mean relative depth; `s` and `d` (NSPEC each) are
/// overwritten, not accumulated, as in the Fortran. All pointers are to
/// contiguous memory the team's execution space can read. `w` comes from
/// snl1_team_scratch(). The caller provides the barriers around the call:
/// snl1_team() reads `a` with no barrier first and returns without one after
/// writing `s` and `d`.
template <class TeamMember>
KOKKOS_INLINE_FUNCTION void snl1_team(const TeamMember& team, const Config& c, const Tables& t,
                                      const Real* sig, const Real* a, const Real* cg,
                                      const Real kdmean, Real* s, Real* d, const TeamScratch& w) {
  const int nth = c.nth;
  const int nfr = t.nfr;
  const int nfrhgh = t.nfrhgh;
  const int nspecx = t.nspecx;
  const int nspec = t.nspec;

  const Real snlc1 = c.snlc1;
  const Real snls1 = c.snls1;
  const Real snls2 = c.snls2;
  const Real snls3 = c.snls3;
  const Real kdcon = c.kdcon;
  const Real kdmn = c.kdmn;
  const Real fachfe = c.fachfe;
  const Real dal1 = t.dal1;
  const Real dal2 = t.dal2;
  const Real dal3 = t.dal3;

  Real* const ue = w.ue;
  Real* const sa1 = w.sa1;
  Real* const sa2 = w.sa2;
  Real* const da1c = w.da1c;
  Real* const da1p = w.da1p;
  Real* const da1m = w.da1m;
  Real* const da2c = w.da2c;
  Real* const da2p = w.da2p;
  Real* const da2m = w.da2m;
  Real* const con = w.con;

  // 1.  Calculate prop. constant ----------------------------------- *
  //     Three flops, recomputed by every thread rather than broadcast
  //     through scratch: cheaper than the barrier it would cost.
  const Real x = Kokkos::max(kdcon * kdmean, kdmn);
  const Real x2 = Kokkos::max(static_cast<Real>(-1.e15), snls3 * x);
  const Real cons =
      snlc1 * (static_cast<Real>(1) +
               snls1 / x * (static_cast<Real>(1) - snls2 * x) * dm::expf(x2));

  // 2.  Prepare auxiliary spectrum and arrays ----------------------- *
  Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nfr), [&](const int ifr) {
    const Real conx = kTpiInv / sig[ifr] * cg[ifr];
    for (int ith = 0; ith < nth; ++ith) {
      const int isp = ith + ifr * nth;  // Fortran ISP = ITH + (IFR-1)*NTH
      ue[isp + nth] = a[isp] / conx;
      con[isp] = conx;
    }
  });

  // The Fortran DO ISP=1-NTH,0 zeroing loop: scratch slots [0, NTH).
  Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nth), [&](const int i) {
    const Real zero = static_cast<Real>(0);
    ue[i] = zero;
    sa1[i] = zero;
    sa2[i] = zero;
    da1c[i] = zero;
    da1p[i] = zero;
    da1m[i] = zero;
    da2c[i] = zero;
    da2p[i] = zero;
    da2m[i] = zero;
  });
  team.team_barrier();

  // The parametric tail, DO IFR=NFR+1,NFRHGH. Row IFR reads row IFR-1, so
  // the frequency loop stays sequential and each row ends in a barrier.
  for (int ifr = nfr; ifr < nfrhgh; ++ifr) {
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nth), [&](const int ith) {
      const int isp = ith + ifr * nth;
      // UE(ISP) = UE(ISP-NTH) * FACHFE; (isp-nth) + nth == isp.
      ue[isp + nth] = ue[isp] * fachfe;
    });
    team.team_barrier();
  }

  if (kHostHalo && t.halo > 0) {
    snl1_team_halo(team, t, nth, cons, ue, w, s, d);
    return;
  }

  // 3.  Calculate interactions for extended spectrum ---------------- *
  team_bins(team, nspecx, [&](const int isp) WW_SNL1_INLINE_BODY {
    // 3.a Energy at interacting bins
    const Real e00 = ue[isp + nth];
    const Real ep1 = t.awg[0] * ue[t.ip[0][0](isp) + nth] +
                     t.awg[1] * ue[t.ip[0][1](isp) + nth] +
                     t.awg[2] * ue[t.ip[0][2](isp) + nth] +
                     t.awg[3] * ue[t.ip[0][3](isp) + nth];
    const Real em1 = t.awg[4] * ue[t.im[0][0](isp) + nth] +
                     t.awg[5] * ue[t.im[0][1](isp) + nth] +
                     t.awg[6] * ue[t.im[0][2](isp) + nth] +
                     t.awg[7] * ue[t.im[0][3](isp) + nth];
    const Real ep2 = t.awg[0] * ue[t.ip[1][0](isp) + nth] +
                     t.awg[1] * ue[t.ip[1][1](isp) + nth] +
                     t.awg[2] * ue[t.ip[1][2](isp) + nth] +
                     t.awg[3] * ue[t.ip[1][3](isp) + nth];
    const Real em2 = t.awg[4] * ue[t.im[1][0](isp) + nth] +
                     t.awg[5] * ue[t.im[1][1](isp) + nth] +
                     t.awg[6] * ue[t.im[1][2](isp) + nth] +
                     t.awg[7] * ue[t.im[1][3](isp) + nth];

    // 3.b Contribution to interactions
    const Real factor = cons * t.af11(isp) * e00;

    const Real sa1a = e00 * (ep1 * dal1 + em1 * dal2);
    const Real sa1b = sa1a - ep1 * em1 * dal3;
    const Real sa2a = e00 * (ep2 * dal1 + em2 * dal2);
    const Real sa2b = sa2a - ep2 * em2 * dal3;

    sa1[isp + nth] = factor * sa1b;
    sa2[isp + nth] = factor * sa2b;

    da1c[isp + nth] = cons * t.af11(isp) * (sa1a + sa1b);
    da1p[isp + nth] = factor * (dal1 * e00 - dal3 * em1);
    da1m[isp + nth] = factor * (dal2 * e00 - dal3 * ep1);

    da2c[isp + nth] = cons * t.af11(isp) * (sa2a + sa2b);
    da2p[isp + nth] = factor * (dal1 * e00 - dal3 * em2);
    da2m[isp + nth] = factor * (dal2 * e00 - dal3 * ep2);
  });
  team.team_barrier();

  // 4.  Put source and diagonal term together ----------------------- *
  team_bins(team, nspec, [&](const int isp) WW_SNL1_INLINE_BODY {
    s[isp] = con[isp] * (-static_cast<Real>(2) * (sa1[isp + nth] + sa2[isp + nth]) +
                         t.awg[0] * (sa1[t.ic[0][0](isp) + nth] + sa2[t.ic[0][1](isp) + nth]) +
                         t.awg[1] * (sa1[t.ic[1][0](isp) + nth] + sa2[t.ic[1][1](isp) + nth]) +
                         t.awg[2] * (sa1[t.ic[2][0](isp) + nth] + sa2[t.ic[2][1](isp) + nth]) +
                         t.awg[3] * (sa1[t.ic[3][0](isp) + nth] + sa2[t.ic[3][1](isp) + nth]) +
                         t.awg[4] * (sa1[t.ic[4][0](isp) + nth] + sa2[t.ic[4][1](isp) + nth]) +
                         t.awg[5] * (sa1[t.ic[5][0](isp) + nth] + sa2[t.ic[5][1](isp) + nth]) +
                         t.awg[6] * (sa1[t.ic[6][0](isp) + nth] + sa2[t.ic[6][1](isp) + nth]) +
                         t.awg[7] * (sa1[t.ic[7][0](isp) + nth] + sa2[t.ic[7][1](isp) + nth]));

    d[isp] = -static_cast<Real>(2) * (da1c[isp + nth] + da2c[isp + nth]) +
             t.swg[0] * (da1p[t.ic[0][0](isp) + nth] + da2p[t.ic[0][1](isp) + nth]) +
             t.swg[1] * (da1p[t.ic[1][0](isp) + nth] + da2p[t.ic[1][1](isp) + nth]) +
             t.swg[2] * (da1p[t.ic[2][0](isp) + nth] + da2p[t.ic[2][1](isp) + nth]) +
             t.swg[3] * (da1p[t.ic[3][0](isp) + nth] + da2p[t.ic[3][1](isp) + nth]) +
             t.swg[4] * (da1m[t.ic[4][0](isp) + nth] + da2m[t.ic[4][1](isp) + nth]) +
             t.swg[5] * (da1m[t.ic[5][0](isp) + nth] + da2m[t.ic[5][1](isp) + nth]) +
             t.swg[6] * (da1m[t.ic[6][0](isp) + nth] + da2m[t.ic[6][1](isp) + nth]) +
             t.swg[7] * (da1m[t.ic[7][0](isp) + nth] + da2m[t.ic[7][1](isp) + nth]);
  });
}

}  // namespace ww::snl1
