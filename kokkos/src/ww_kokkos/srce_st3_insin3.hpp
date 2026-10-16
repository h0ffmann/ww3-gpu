// kokkos/src/ww_kokkos/srce_st3_insin3.hpp
// INSIN3 of W3SRC3MD: the ST3 initialisation that builds TAUT and TAUHFT once per process,
// host code. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:726-813 (INSIN3) at NOAA-EMC/WW3 03adda91;
// author F. Ardhuin. Evidence: kokkos/port_report/INSIN3_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st3_tabu_stress.hpp"
#include "srce_st3_tabu_tauhf.hpp"

namespace ww::srce::st3 {

// TPIINV = 1./TPI (constants.F90:75), the 0x3e22f983 that w3src3md.o multiplies SIG(NK) by.
static_assert(ww::srce::kTpiInv == 0x1.45f306p-3f);

/// The W3SRC3MD module scalars INSIN3 sets beside TAUT and TAUHFT (:97-98).
struct Insin3Steps {
  Real delu;
  Real deltauw;
  Real delust;
  Real delalp;
};

/// INSIN3 (:726-813), host. Reads W3GDATMD NK (p.nk), SIG, ZZWND, AALPHA, ZZ0MAX, BBETA,
/// ZZALP of the grid whose mod_def is read first: its only caller, w3iogrmd.F90:1532, runs
/// it once per process behind the SAVE flag FLINP (w3iogrmd.F90:355), and TAUT, TAUHFT and
/// the steps are module variables of W3SRC3MD (:97-99), not per grid. sig = SIG(1:NK), the pointer at SIG(1), not SIG(0).
/// taut: written, kTautSize floats, TAUT as tabu_stress lays it out.
/// tauhft: written, (kTauhfIustar+1)*(kTauhfIalpha+1) floats, TAUHFT as tabu_tauhf lays it out.
inline Insin3Steps insin3(const ww::srce::Params& p, const Real* sig, Real zzwnd, Real aalpha,
                          Real zz0max, Real bbeta, Real zzalp, Real* taut, Real* tauhft) {
  const TabuStressSteps s = tabu_stress(zzwnd, aalpha, zz0max, taut);  // :808
  // :809, one mulss in the object: SIG(NK)*TPIINV rounded to REAL before the call.
  const Real frmax = sig[p.nk - 1] * ww::srce::kTpiInv;
  const TauhfSteps h = tabu_tauhf(frmax, aalpha, bbeta, zzalp, zz0max, tauhft);
  return Insin3Steps{s.delu, s.deltauw, h.delust, h.delalp};
}

}  // namespace ww::srce::st3
