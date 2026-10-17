// kokkos/src/ww_kokkos/prop.cpp
// The propagation headers (prop_*.hpp: W3XYP3, W3KTP3, W3QCK1, W3QCK2, W3QCK3, WAVNU1, W3XYP1,
// W3KTP1, W3XYP2, W3KTP2, and W3XYPUG, a host function) are header-only functions. W3XYP3 and
// W3KTP3, with the three W3QCKn, have a caller, fortran_iface/prop3_shim.cpp; the others none yet.
// This translation unit makes every preset compile all of them, host and CUDA.
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
