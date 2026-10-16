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
}
