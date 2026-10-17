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

namespace ww::snl1 {

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
};

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
  const int ue_len = t.nspecy + c.nth;
  const int sa_len = t.nspecx + c.nth;
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
  return TeamScratch{ue.data(),   sa1.data(),  sa2.data(),  da1c.data(), da1p.data(),
                     da1m.data(), da2c.data(), da2p.data(), da2m.data(), con.data()};
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
               snls1 / x * (static_cast<Real>(1) - snls2 * x) * Kokkos::exp(x2));

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

  // 3.  Calculate interactions for extended spectrum ---------------- *
  Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nspecx), [&](const int isp) WW_SNL1_INLINE_BODY {
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
  Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nspec), [&](const int isp) WW_SNL1_INLINE_BODY {
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
