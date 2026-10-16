// kokkos/src/ww_kokkos/srce_params.hpp
// The scalars and grid arrays W3SRCE and its Fortran callees W3SLN1, W3SBT1 and
// W3SDB1 read, and how they cross the C ABI.
//
// WW3 heritage: WW3/model/src/w3srcemd.F90:190-2412 (W3SRCE, the USE lists at
// :497-579), w3sln1md.F90:56-211, w3sbt1md.F90:88-285, w3sdb1md.F90:96-382, at
// NOAA-EMC/WW3 03adda91 (WW3-noaa/), as switches/switch_lab_shrd compiles them.
// They read W3GDATMD, W3IDATMD's INFLAGS2 and CONSTANTS. ST4's own scalars stay in st4_params.hpp, W3SNL1's in
// snl1_config.hpp: the W3SRCE kernel reads them from those contexts.
//
// As for ST4, the Fortran side packs the scalars into REAL(C_FLOAT) RPAR and
// INTEGER(C_INT) IPAR at the 0-based indices below; W3KOKKOSMD on the fork
// mirrors them, and ww_srce_init() refuses a packet whose length differs.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

// Kokkos::Serial for a caller inside an OpenMP parallel region (W3WAVE's
// source-term loop under W3_OMPG); host builds only, as WW_SNL1_SERIAL_NESTED.
#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_SRCE_SERIAL_NESTED 1
#endif

namespace ww::srce {

// CONSTANTS (constants.F90:62-75). W3SRCEMD's OFFSET (w3srcemd.F90:69) is 1. and
// folded away by -O3; the kernel writes HDT = DT.
inline constexpr Real kGrav = static_cast<Real>(9.806);      // GRAV :62
inline constexpr Real kDwat = static_cast<Real>(1000.);      // DWAT :63
inline constexpr Real kTpi = static_cast<Real>(2.0) * ww::kPi;   // TPI :72
inline constexpr Real kTpiInv = static_cast<Real>(1.) / kTpi;    // TPIINV :75

/// RPAR indices (0-based). The comment names the Fortran variable packed there.
enum Rpar : int {
  R_DMIN = 0,  // W3GDATMD DMIN
  R_DTMAX,     // W3GDATMD DTMAX
  R_DTMIN,     // W3GDATMD DTMIN
  R_FACTI1,    // W3GDATMD FACTI1
  R_FACTI2,    // W3GDATMD FACTI2
  R_FACHFA,    // W3GDATMD FACHFA
  R_FACP,      // W3GDATMD FACP
  R_XFLT,      // W3GDATMD XFLT
  R_XREL,      // W3GDATMD XREL
  R_FHMAX,     // W3GDATMD FHMAX
  R_FFXFM,     // W3GDATMD FFXFM
  R_FFXPM,     // W3GDATMD FFXPM
  R_FFXFA,     // W3GDATMD FFXFA
  R_ICESCALES, // W3GDATMD ICESCALES(1:4): R_ICESCALES + 0..3
  R_SLNC1 = R_ICESCALES + 4,  // W3GDATMD SLNC1
  R_FSPM,      // W3GDATMD FSPM
  R_FSHF,      // W3GDATMD FSHF
  R_SBTC1,     // W3GDATMD SBTC1
  R_SDBC1,     // W3GDATMD SDBC1
  R_SDBC2,     // W3GDATMD SDBC2
  NRPAR
};

/// IPAR indices (0-based).
enum Ipar : int {
  I_NK = 0,        // W3GDATMD NK
  I_NTH,           // W3GDATMD NTH
  I_NSPEC,         // W3GDATMD NSPEC
  I_IQTPE,         // W3GDATMD IQTPE
  I_IICEDISP,      // W3GDATMD IICEDISP, 0/1
  I_IC_NUMERICS,   // W3GDATMD IC_NUMERICS, 0/1
  I_FDONLY,        // W3GDATMD FDONLY, 0/1
  I_LAB_SWITCH,    // 1 when the build has exactly the source-term switches the kernel
                   // translates (ST4 NL1 LN1 BT1 DB1 MLIM FLX0 STAB0 IC0 IS0 REF0 TR0 BS0)
  NIPAR
};

/// Everything above, unpacked, under WW3's names in lower case.
struct Params {
  int nk, nth, nspec;
  int iqtpe, iicedisp, ic_numerics, fdonly;
  Real dmin, dtmax, dtmin, facti1, facti2, fachfa;
  Real facp, xflt, xrel, fhmax, ffxfm, ffxpm, ffxfa;
  Real icescales[4];  ///< ICESCALES(1:4): icescales[i-1]
  Real slnc1, fspm, fshf, sbtc1, sdbc1, sdbc2;
};

inline Params unpack(const float* r, const int* i) {
  Params p{};
  p.nk = i[I_NK];
  p.nth = i[I_NTH];
  p.nspec = i[I_NSPEC];
  p.iqtpe = i[I_IQTPE];
  p.iicedisp = i[I_IICEDISP];
  p.ic_numerics = i[I_IC_NUMERICS];
  p.fdonly = i[I_FDONLY];
  p.dmin = r[R_DMIN];
  p.dtmax = r[R_DTMAX];
  p.dtmin = r[R_DTMIN];
  p.facti1 = r[R_FACTI1];
  p.facti2 = r[R_FACTI2];
  p.fachfa = r[R_FACHFA];
  p.facp = r[R_FACP];
  p.xflt = r[R_XFLT];
  p.xrel = r[R_XREL];
  p.fhmax = r[R_FHMAX];
  p.ffxfm = r[R_FFXFM];
  p.ffxpm = r[R_FFXPM];
  p.ffxfa = r[R_FFXFA];
  for (int j = 0; j < 4; ++j) p.icescales[j] = r[R_ICESCALES + j];
  p.slnc1 = r[R_SLNC1];
  p.fspm = r[R_FSPM];
  p.fshf = r[R_FSHF];
  p.sbtc1 = r[R_SBTC1];
  p.sdbc1 = r[R_SDBC1];
  p.sdbc2 = r[R_SDBC2];
  return p;
}

}  // namespace ww::srce
