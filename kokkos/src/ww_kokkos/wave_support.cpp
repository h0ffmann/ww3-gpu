// kokkos/src/ww_kokkos/wave_support.cpp
// W3WAVE's support headers (W3GATH, W3SCAT, W3DZXY, W3UWND, W3UTRN, DISTAB with WAVNU2, and the
// PR3 maps W3MAP3, W3MAPT) are header-only device functions with no caller yet: the shims and the
// W3WAVE and W3IOGR wiring come with the unit's checkpoint
// (docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md). This translation unit instantiates every launcher
// on the default execution space, so each preset generates their code (device code on CUDA) until
// a shim calls them. Nothing calls ww_support_instantiate.
#include "disp_distab.hpp"
#include "disp_wavnu3.hpp"
#include "pdlib_boundary.hpp"
#include "pdlib_boundary_va.hpp"
#include "pdlib_explicit_block.hpp"
#include "pdlib_fsfct2.hpp"
#include "pdlib_fsn2.hpp"
#include "pdlib_fspsi2.hpp"
#include "pdlib_jacobi_spectral.hpp"
#include "pdlib_jacobi_vec.hpp"
#include "pdlib_jgs_block.hpp"
#include "pdlib_w3xypug.hpp"
#include "prop_map3.hpp"
#include "prop_mapt.hpp"
#include "prop_freq_shift.hpp"
#include "prop_parall.hpp"
#include "srce_bt4.hpp"
#include "srce_ic1.hpp"
#include "srce_ic4.hpp"
#include "srce_is1.hpp"
#include "srce_ref1.hpp"
#include "srce_tr1.hpp"
#include "srce_flx1.hpp"
#include "srce_flx2.hpp"
#include "srce_flx3.hpp"
#include "srce_flx4.hpp"
#include "srce_flx5.hpp"
#include "srce_st0_spr0.hpp"
#include "srce_st1_sds1.hpp"
#include "srce_st1_sin1.hpp"
#include "srce_st1_spr1.hpp"
#include "srce_st2_inptab.hpp"
#include "srce_st2_sds2.hpp"
#include "srce_st2_sin2.hpp"
#include "srce_st2_spr2.hpp"
#include "srce_st3_calc_ustar.hpp"
#include "srce_st3_insin3.hpp"
#include "srce_st3_sds3.hpp"
#include "srce_st3_sin3.hpp"
#include "srce_st3_spr3.hpp"
#include "srce_st3_tabu_stress.hpp"
#include "srce_st3_tabu_tauhf.hpp"
#include "srce_st6_lfactor.hpp"
#include "srce_st6_sds6.hpp"
#include "srce_st6_spr6.hpp"
#include "srce_st6_tau_wave_atmos.hpp"
#include "srce_st6_util.hpp"
#include "srce_swl4.hpp"
#include "srce_swl6.hpp"
#include "srce_swl_util.hpp"
#include "updt_dzxy.hpp"
#include "updt_utrn.hpp"
#include "updt_uwnd.hpp"
#include "wave_gath_scat.hpp"

void ww_support_instantiate();
void ww_support_instantiate() {
  using E = Kokkos::DefaultExecutionSpace;
  (void)&ww::wave::w3gath<E>;
  (void)&ww::wave::w3scat<E>;
  (void)&ww::updt::w3dzxy<E>;
  (void)&ww::updt::w3uwnd<E>;
  (void)&ww::updt::w3utrn<E>;
  (void)&ww::disp::distab<E>;
  (void)&ww::prop::w3map3<E>;
  (void)&ww::prop::w3mapt<E>;
  (void)&ww::srce::st3::w3sin3_point<false>;
  (void)&ww::srce::st3::w3sin3_point<true>;
}
