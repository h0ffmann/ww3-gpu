// kokkos/src/ww_kokkos/prop.cpp
// The propagation headers (prop_*.hpp: W3XYP3, W3KTP3, W3QCK1, W3QCK2, W3QCK3, WAVNU1) are
// header-only device functions with no caller yet: the shim and the W3WAVE wiring come with the
// unit's checkpoint (docs/PROPAGATION_KOKKOS_PLAN_202610.md). This translation unit makes every
// preset compile them, host and CUDA, until a shim includes them.
#include "prop_ktp3.hpp"
#include "prop_params.hpp"
#include "prop_qck1.hpp"
#include "prop_qck2.hpp"
#include "prop_qck3.hpp"
#include "prop_wavnu1.hpp"
#include "prop_xyp3.hpp"
