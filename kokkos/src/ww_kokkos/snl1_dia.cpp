// kokkos/src/ww_kokkos/snl1_dia.cpp
// W3SNL1 ported to Kokkos: the DIA nonlinear interactions and their diagonal.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE W3SNL1, lines 115-473; the numbered section comments below are that
// routine's own (sections 1-4, lines 338-440). Phase-1 rules apply: the
// expressions, their order and their float32 arithmetic are the Fortran's. The
// only thing that changed is who executes them.
//
// Parallel decomposition: one team per sea point, because W3SNL1's working set is
// per-point (an extended spectrum plus nine helper arrays) and wants to live in
// team scratch. Inside a team the sections are data-parallel over ISP, with one
// exception: the high-frequency extension in section 2 marches upward in
// frequency reading the row below, so its frequency loop is sequential and each
// row ends in a team_barrier.
//
// WW_DETERMINISTIC does not change this file. The kernel contains no reduction --
// every output element is written by exactly one thread from inputs no thread
// modifies -- so there is no summation order to pin down and the result is
// already bit-identical across backends and thread counts.
//
// Scratch budget: (NSPECY + NTH) + 8*(NSPECX + NTH) + NSPEC floats. For the
// lab's NK=25/NTH=24 grid that is ~28 KB, inside the 48 KB a CUDA block gets;
// a much larger spectral grid would need level-1 scratch instead, which is why
// the size is checked against the policy's maximum before the launch.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ww_kokkos/snl1_dia.hpp"

#include <stdexcept>
#include <string>

namespace ww::snl1 {
namespace {

using ExecSpace = Kokkos::DefaultExecutionSpace;
using TeamPolicy = Kokkos::TeamPolicy<ExecSpace>;
using TeamMember = TeamPolicy::member_type;
/// Unmanaged because team scratch is a bump allocator: the view never owns it.
using ScratchReal = Kokkos::View<Real*, Kokkos::LayoutRight, ExecSpace::scratch_memory_space,
                                 Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

}  // namespace

void snl1(const Config& c, const Tables& t, ConstRealView1D sig, ConstRealView2D a,
          ConstRealView2D cg, ConstRealView1D kdmean, RealView2D s, RealView2D d) {
  const int npts = a.extent_int(1);
  if (npts == 0) return;

  const int nth = c.nth;
  const int nfr = t.nfr;
  const int nfrhgh = t.nfrhgh;
  const int nspecx = t.nspecx;
  const int nspecy = t.nspecy;
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
  const Tables tab = t;  // Views are handles; the copy is what the lambda captures

  // Fortran's UE(1-NTH:NSPECY) and SA*/DA*(1-NTH:NSPECX) become 0-based scratch
  // of length (upper + NTH): a Fortran index J lives at scratch slot J - 1 + NTH,
  // and a table entry (already Fortran-minus-one) at slot entry + NTH.
  const int ue_len = nspecy + nth;
  const int sa_len = nspecx + nth;
  const size_t scratch_bytes = ScratchReal::shmem_size(ue_len) +
                               8 * ScratchReal::shmem_size(sa_len) +
                               ScratchReal::shmem_size(nspec);

  TeamPolicy policy(npts, Kokkos::AUTO);
  policy.set_scratch_size(0, Kokkos::PerTeam(scratch_bytes));
  if (scratch_bytes > static_cast<size_t>(policy.scratch_size_max(0))) {
    throw std::runtime_error("ww::snl1::snl1: the extended spectrum needs " +
                             std::to_string(scratch_bytes) +
                             " B of level-0 team scratch, more than this backend offers");
  }

  Kokkos::parallel_for(
      "srce.snl1.dia", policy, KOKKOS_LAMBDA(const TeamMember& team) {
        const int ipt = team.league_rank();

        ScratchReal ue(team.team_scratch(0), ue_len);
        ScratchReal sa1(team.team_scratch(0), sa_len);
        ScratchReal sa2(team.team_scratch(0), sa_len);
        ScratchReal da1c(team.team_scratch(0), sa_len);
        ScratchReal da1p(team.team_scratch(0), sa_len);
        ScratchReal da1m(team.team_scratch(0), sa_len);
        ScratchReal da2c(team.team_scratch(0), sa_len);
        ScratchReal da2p(team.team_scratch(0), sa_len);
        ScratchReal da2m(team.team_scratch(0), sa_len);
        ScratchReal con(team.team_scratch(0), nspec);

        // 1.  Calculate prop. constant ----------------------------------- *
        //     Three flops, recomputed by every thread rather than broadcast
        //     through scratch: cheaper than the barrier it would cost.
        const Real x = Kokkos::max(kdcon * kdmean(ipt), kdmn);
        const Real x2 = Kokkos::max(static_cast<Real>(-1.e15), snls3 * x);
        const Real cons =
            snlc1 * (static_cast<Real>(1) +
                     snls1 / x * (static_cast<Real>(1) - snls2 * x) * Kokkos::exp(x2));

        // 2.  Prepare auxiliary spectrum and arrays ----------------------- *
        Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nfr), [&](const int ifr) {
          const Real conx = kTpiInv / sig(ifr) * cg(ifr, ipt);
          for (int ith = 0; ith < nth; ++ith) {
            const int isp = ith + ifr * nth;  // Fortran ISP = ITH + (IFR-1)*NTH
            ue(isp + nth) = a(isp, ipt) / conx;
            con(isp) = conx;
          }
        });

        // The Fortran DO ISP=1-NTH,0 zeroing loop: scratch slots [0, NTH).
        Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nth), [&](const int i) {
          const Real zero = static_cast<Real>(0);
          ue(i) = zero;
          sa1(i) = zero;
          sa2(i) = zero;
          da1c(i) = zero;
          da1p(i) = zero;
          da1m(i) = zero;
          da2c(i) = zero;
          da2p(i) = zero;
          da2m(i) = zero;
        });
        team.team_barrier();

        // The parametric tail, DO IFR=NFR+1,NFRHGH. Row IFR reads row IFR-1, so
        // the frequency loop stays sequential and each row ends in a barrier.
        for (int ifr = nfr; ifr < nfrhgh; ++ifr) {
          Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nth), [&](const int ith) {
            const int isp = ith + ifr * nth;
            // UE(ISP) = UE(ISP-NTH) * FACHFE; (isp-nth) + nth == isp.
            ue(isp + nth) = ue(isp) * fachfe;
          });
          team.team_barrier();
        }

        // 3.  Calculate interactions for extended spectrum ---------------- *
        Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nspecx), [&](const int isp) {
          // 3.a Energy at interacting bins
          const Real e00 = ue(isp + nth);
          const Real ep1 = tab.awg[0] * ue(tab.ip[0][0](isp) + nth) +
                           tab.awg[1] * ue(tab.ip[0][1](isp) + nth) +
                           tab.awg[2] * ue(tab.ip[0][2](isp) + nth) +
                           tab.awg[3] * ue(tab.ip[0][3](isp) + nth);
          const Real em1 = tab.awg[4] * ue(tab.im[0][0](isp) + nth) +
                           tab.awg[5] * ue(tab.im[0][1](isp) + nth) +
                           tab.awg[6] * ue(tab.im[0][2](isp) + nth) +
                           tab.awg[7] * ue(tab.im[0][3](isp) + nth);
          const Real ep2 = tab.awg[0] * ue(tab.ip[1][0](isp) + nth) +
                           tab.awg[1] * ue(tab.ip[1][1](isp) + nth) +
                           tab.awg[2] * ue(tab.ip[1][2](isp) + nth) +
                           tab.awg[3] * ue(tab.ip[1][3](isp) + nth);
          const Real em2 = tab.awg[4] * ue(tab.im[1][0](isp) + nth) +
                           tab.awg[5] * ue(tab.im[1][1](isp) + nth) +
                           tab.awg[6] * ue(tab.im[1][2](isp) + nth) +
                           tab.awg[7] * ue(tab.im[1][3](isp) + nth);

          // 3.b Contribution to interactions
          const Real factor = cons * tab.af11(isp) * e00;

          const Real sa1a = e00 * (ep1 * dal1 + em1 * dal2);
          const Real sa1b = sa1a - ep1 * em1 * dal3;
          const Real sa2a = e00 * (ep2 * dal1 + em2 * dal2);
          const Real sa2b = sa2a - ep2 * em2 * dal3;

          sa1(isp + nth) = factor * sa1b;
          sa2(isp + nth) = factor * sa2b;

          da1c(isp + nth) = cons * tab.af11(isp) * (sa1a + sa1b);
          da1p(isp + nth) = factor * (dal1 * e00 - dal3 * em1);
          da1m(isp + nth) = factor * (dal2 * e00 - dal3 * ep1);

          da2c(isp + nth) = cons * tab.af11(isp) * (sa2a + sa2b);
          da2p(isp + nth) = factor * (dal1 * e00 - dal3 * em2);
          da2m(isp + nth) = factor * (dal2 * e00 - dal3 * ep2);
        });
        team.team_barrier();

        // 4.  Put source and diagonal term together ----------------------- *
        Kokkos::parallel_for(Kokkos::TeamThreadRange(team, nspec), [&](const int isp) {
          s(isp, ipt) =
              con(isp) * (-static_cast<Real>(2) * (sa1(isp + nth) + sa2(isp + nth)) +
                          tab.awg[0] * (sa1(tab.ic[0][0](isp) + nth) + sa2(tab.ic[0][1](isp) + nth)) +
                          tab.awg[1] * (sa1(tab.ic[1][0](isp) + nth) + sa2(tab.ic[1][1](isp) + nth)) +
                          tab.awg[2] * (sa1(tab.ic[2][0](isp) + nth) + sa2(tab.ic[2][1](isp) + nth)) +
                          tab.awg[3] * (sa1(tab.ic[3][0](isp) + nth) + sa2(tab.ic[3][1](isp) + nth)) +
                          tab.awg[4] * (sa1(tab.ic[4][0](isp) + nth) + sa2(tab.ic[4][1](isp) + nth)) +
                          tab.awg[5] * (sa1(tab.ic[5][0](isp) + nth) + sa2(tab.ic[5][1](isp) + nth)) +
                          tab.awg[6] * (sa1(tab.ic[6][0](isp) + nth) + sa2(tab.ic[6][1](isp) + nth)) +
                          tab.awg[7] * (sa1(tab.ic[7][0](isp) + nth) + sa2(tab.ic[7][1](isp) + nth)));

          d(isp, ipt) =
              -static_cast<Real>(2) * (da1c(isp + nth) + da2c(isp + nth)) +
              tab.swg[0] * (da1p(tab.ic[0][0](isp) + nth) + da2p(tab.ic[0][1](isp) + nth)) +
              tab.swg[1] * (da1p(tab.ic[1][0](isp) + nth) + da2p(tab.ic[1][1](isp) + nth)) +
              tab.swg[2] * (da1p(tab.ic[2][0](isp) + nth) + da2p(tab.ic[2][1](isp) + nth)) +
              tab.swg[3] * (da1p(tab.ic[3][0](isp) + nth) + da2p(tab.ic[3][1](isp) + nth)) +
              tab.swg[4] * (da1m(tab.ic[4][0](isp) + nth) + da2m(tab.ic[4][1](isp) + nth)) +
              tab.swg[5] * (da1m(tab.ic[5][0](isp) + nth) + da2m(tab.ic[5][1](isp) + nth)) +
              tab.swg[6] * (da1m(tab.ic[6][0](isp) + nth) + da2m(tab.ic[6][1](isp) + nth)) +
              tab.swg[7] * (da1m(tab.ic[7][0](isp) + nth) + da2m(tab.ic[7][1](isp) + nth));
        });
      });
}

}  // namespace ww::snl1
