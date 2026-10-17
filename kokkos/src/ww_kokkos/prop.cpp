// kokkos/src/ww_kokkos/prop.cpp
// The propagation headers (prop_*.hpp: W3XYP3, W3KTP3, W3QCK1, W3QCK2, W3QCK3, WAVNU1, W3XYP1,
// W3KTP1, W3XYP2, W3KTP2, and W3XYPUG, a host function) are header-only functions with no caller
// yet: the shim and the W3WAVE wiring come with the unit's checkpoint
// (docs/PROPAGATION_KOKKOS_PLAN_202610.md). This translation unit makes every preset compile them,
// host and CUDA, until a shim includes them.
#include "prop_ktp1.hpp"
#include "prop_ktp2.hpp"
#include "prop_ktp3.hpp"
#include "prop_params.hpp"
#include "prop_qck1.hpp"
#include "prop_qck2.hpp"
#include "prop_qck3.hpp"
#include "prop_wavnu1.hpp"
#include "prop_xyp1.hpp"
#include "prop_xyp2.hpp"
#include "prop_xyp3.hpp"
#include "prop_xypug.hpp"
