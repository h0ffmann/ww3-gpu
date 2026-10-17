// kokkos/src/ww_kokkos/gdat_w3setg.hpp
// W3SETG, ported to Kokkos: select grid/model IMOD in W3GDATMD. Host-side, no View
// is created or read and no kernel runs. Phase 1, translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3SETG, WW3/model/src/w3gdatmd.F90:2156-2893 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); authors H. L. Tolman and
// J. H. Alves. Ported as switches/switch_lab_shrd compiles it: of the #ifdef blocks
// inside the routine, W3_RWND :2554-2556, W3_PR3 :2567-2570, W3_LN1 :2615-2619,
// W3_ST4 :2672-2714, W3_NL1 :2737-2753, W3_BT1 :2818-2820 and W3_DB1 :2827-2832 are
// in; W3_S, the TEST_W3GDATMD writes, W3_SMC, W3_REF1, W3_IG1, W3_IC2..IC5, W3_IS2,
// W3_SEC1, W3_RTD, W3_BT4, W3_WCOR, W3_PR2, W3_FLD1/2, W3_FLX2/3/4, W3_ST1/2/3/6,
// W3_NL2..NL5, W3_NLS, W3_UOST and W3_IS1 are not compiled and not ported. W3SETG
// has no ASSOCIATED test; its only guards are the two checks of section 1 and the
// GINIT (:2434) and SINIT (:2518) conditions.
//
// What a pointer alias becomes here, as in W3SETA's port (adat_w3seta.hpp): the
// Fortran re-points 252 module pointers (kW3setgAliases) at the components of
// GRIDS(IMOD), SGRDS(IMOD) and MPARS(IMOD); the C++ has no module pointers, a
// consumer holds the struct of a model. What W3SETG leaves is one model number per
// group of aliases re-pointed together under one condition:
//  - the unconditional GRIDS, SGRDS and MPARS aliases name the models IGRID, ISGRD
//    and IPARS (GdatModule::igrid, isgrd, ipars), which W3SETG sets to IMOD in the
//    same call (:2278-2280). They are one fact, not two: at the pin only W3SETG
//    writes IGRID, ISGRD and IPARS (the IGRID of ww3_gint.F90:1303, :1374 is a
//    local of W3EXGI, :1059);
//  - MAPSTA .. GSU (:2436-2491) only when GRIDS(IMOD)%GINIT holds, W3setgTargets::ginit;
//  - MAPWN .. DDEN2 (:2520-2534) only when SGRDS(IMOD)%SINIT holds, W3setgTargets::sinit.
// A group whose condition is false keeps the model of an earlier W3SETG: after
// W3SETG(2) with GRIDS(2)%GINIT false, MAPSTA still names grid 1's. The aliases are
// declared without => NULL() (w3gdatmd.F90:1097-1431), so their status is undefined
// until a W3SETG associates them: std::nullopt here.
// Models run from NLOW = -NAUXGR to NGRIDS (W3NMOD, :1528-1547; NAUXGR = -1 when
// W3NMOD has no NAUX, so then 1..NGRIDS), so 0 and negative numbers are grids
// (auxiliary ones) and cannot stand for "none"; selected() maps a model number to
// the slot of a caller's vector over NLOW..NGRIDS.
// A model number stands for that model's components as they are when read; the
// Fortran copies each array component's descriptor at the =>. The two differ when a
// component is re-allocated between a W3SETG and a read through its alias (sites at
// the pin in the port report); W3DIMX, W3DIMS and W3DIMUG call W3SETG after they
// allocate (:1900, :2116, :3273).
//
// The conditions are read as the Fortran reads them, through the aliases set just
// before (GINIT => GRIDS(IMOD)%GINIT :2384, SINIT => SGRDS(IMOD)%SINIT :2516), so
// from model IMOD; the GIMPLE loads both straight from GRIDS(IMOD) and SGRDS(IMOD).
// The C++ has two homes for those flags until the shim merges them (IogrState::ginit,
// sinit and Sgrd::sinit), so the caller passes readers (W3setgInputs), called after
// the checks, as the Fortran reads them after the checks.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3gdatmd.F90 with WW3's Release flags and the switch_lab_shrd defines; the
// object's .text and disassembly equal the CMake build's:
//  - NGRIDS == -1 is tested (:2262), then IMOD < -NAUXGR, then IMOD > NGRIDS with
//    NGRIDS reloaded (:2267); IMOD is loaded once and that value is stored to IGRID,
//    ISGRD and IPARS (:2278-2280), then the 93 unconditional GRIDS aliases, GINIT,
//    the 22 of its block, the 17 SGRDS aliases, SINIT, the 14 of its block and the
//    106 MPARS aliases, each at its source line in that order; no floating-point
//    instruction, only address arithmetic and descriptor copies.
//  - EXTCDE is called with 1 and 2 (the .rodata constants), after the WRITE.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE,1001/1002) and
// EXTCDE (returned as the code EXTCDE would exit with, W3setgStatus; EXTCDE ends in
// CALL EXIT, w3servmd.F90:1207); w3setg_message() returns the text the WRITE would
// print, for the shim; NDSE and NDST, which only the WRITEs use.
#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "gdat_sgrd.hpp"

namespace ww::gdat {

/// EXTCDE exit codes of W3SETG, returned instead of stopping the program.
enum class W3setgStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NGRIDS == -1, :2262-2265
  illegal_model = 2,          ///< IMOD < -NAUXGR or IMOD > NGRIDS, :2267-2270
  /// Not a WW3 code: a reader of W3setgInputs is empty, so GINIT (:2434) or SINIT
  /// (:2518) cannot be read. Tested after the two Fortran checks and before any
  /// write; the value is ww::iogr::kExitMissingHook's.
  missing_input = -101,
};

/// The group of an alias: re-pointed always, or only under GINIT or SINIT.
enum class W3setgGroup : int { base, ginit, sinit };

/// @brief One pointer assignment of W3SETG: the module pointer, the component of
///        GRIDS(IMOD), SGRDS(IMOD) or MPARS(IMOD) it is pointed at, its line, and
///        its group.
struct W3setgAlias {
  const char* pointer;  ///< W3GDATMD module pointer, e.g. "MAPSTA"
  const char* target;   ///< structure and component, e.g. "GRIDS%MAPSTA"
  int line;             ///< w3gdatmd.F90 line of the assignment
  W3setgGroup group;
};

/// The 252 pointer assignments switch_lab_shrd compiles (:2285-2876), in the
/// Fortran's order, which the GIMPLE keeps.
inline constexpr std::array<W3setgAlias, 252> kW3setgAliases = {{
    {"GTYPE", "GRIDS%GTYPE", 2285, W3setgGroup::base},
    {"RSTYPE", "GRIDS%RSTYPE", 2286, W3setgGroup::base},
    {"ICLOSE", "GRIDS%ICLOSE", 2287, W3setgGroup::base},
    {"NX", "GRIDS%NX", 2289, W3setgGroup::base},
    {"NY", "GRIDS%NY", 2290, W3setgGroup::base},
    {"NSEA", "GRIDS%NSEA", 2291, W3setgGroup::base},
    {"NSEAL", "GRIDS%NSEAL", 2292, W3setgGroup::base},
    {"TRFLAG", "GRIDS%TRFLAG", 2293, W3setgGroup::base},
    {"FLAGLL", "GRIDS%FLAGLL", 2294, W3setgGroup::base},
    {"E3DF", "GRIDS%E3DF", 2309, W3setgGroup::base},
    {"P2MSF", "GRIDS%P2MSF", 2310, W3setgGroup::base},
    {"US3DF", "GRIDS%US3DF", 2311, W3setgGroup::base},
    {"USSPF", "GRIDS%USSPF", 2312, W3setgGroup::base},
    {"USSP_WN", "GRIDS%USSP_WN", 2313, W3setgGroup::base},
    {"FFACBERG", "GRIDS%FFACBERG", 2314, W3setgGroup::base},
    {"SX", "GRIDS%SX", 2344, W3setgGroup::base},
    {"SY", "GRIDS%SY", 2345, W3setgGroup::base},
    {"X0", "GRIDS%X0", 2346, W3setgGroup::base},
    {"Y0", "GRIDS%Y0", 2347, W3setgGroup::base},
    {"DTCFL", "GRIDS%DTCFL", 2349, W3setgGroup::base},
    {"DTCFLI", "GRIDS%DTCFLI", 2350, W3setgGroup::base},
    {"DTMAX", "GRIDS%DTMAX", 2351, W3setgGroup::base},
    {"DTMIN", "GRIDS%DTMIN", 2352, W3setgGroup::base},
    {"DMIN", "GRIDS%DMIN", 2353, W3setgGroup::base},
    {"CTMAX", "GRIDS%CTMAX", 2357, W3setgGroup::base},
    {"FICE0", "GRIDS%FICE0", 2358, W3setgGroup::base},
    {"GRIDSHIFT", "GRIDS%GRIDSHIFT", 2359, W3setgGroup::base},
    {"CMPRTRCK", "GRIDS%CMPRTRCK", 2360, W3setgGroup::base},
    {"FICEN", "GRIDS%FICEN", 2366, W3setgGroup::base},
    {"FICEL", "GRIDS%FICEL", 2367, W3setgGroup::base},
    {"IICEHMIN", "GRIDS%IICEHMIN", 2368, W3setgGroup::base},
    {"IICEHDISP", "GRIDS%IICEHDISP", 2369, W3setgGroup::base},
    {"IICEFDISP", "GRIDS%IICEFDISP", 2370, W3setgGroup::base},
    {"IICEDDISP", "GRIDS%IICEDDISP", 2371, W3setgGroup::base},
    {"IICEHFAC", "GRIDS%IICEHFAC", 2372, W3setgGroup::base},
    {"IICEHINIT", "GRIDS%IICEHINIT", 2373, W3setgGroup::base},
    {"ICESCALES", "GRIDS%ICESCALES", 2374, W3setgGroup::base},
    {"PFMOVE", "GRIDS%PFMOVE", 2375, W3setgGroup::base},
    {"STEXU", "GRIDS%STEXU", 2376, W3setgGroup::base},
    {"STEYU", "GRIDS%STEYU", 2377, W3setgGroup::base},
    {"STEDU", "GRIDS%STEDU", 2378, W3setgGroup::base},
    {"BTBETA", "GRIDS%BTBETA", 2379, W3setgGroup::base},
    {"AAIRGB", "GRIDS%AAIRGB", 2380, W3setgGroup::base},
    {"FETCH", "GRIDS%FETCH", 2381, W3setgGroup::base},
    {"AAIRCMIN", "GRIDS%AAIRCMIN", 2382, W3setgGroup::base},
    {"GINIT", "GRIDS%GINIT", 2384, W3setgGroup::base},
    {"GUGINIT", "GRIDS%GUGINIT", 2385, W3setgGroup::base},
    {"FLDRY", "GRIDS%FLDRY", 2386, W3setgGroup::base},
    {"FLCX", "GRIDS%FLCX", 2387, W3setgGroup::base},
    {"FLCY", "GRIDS%FLCY", 2388, W3setgGroup::base},
    {"FLCTH", "GRIDS%FLCTH", 2389, W3setgGroup::base},
    {"FLCK", "GRIDS%FLCK", 2390, W3setgGroup::base},
    {"FLSOU", "GRIDS%FLSOU", 2391, W3setgGroup::base},
    {"IICEDISP", "GRIDS%IICEDISP", 2392, W3setgGroup::base},
    {"IICESMOOTH", "GRIDS%IICESMOOTH", 2393, W3setgGroup::base},
    {"IC_NUMERICS", "GRIDS%IC_NUMERICS", 2394, W3setgGroup::base},
    {"GNAME", "GRIDS%GNAME", 2396, W3setgGroup::base},
    {"FILEXT", "GRIDS%FILEXT", 2397, W3setgGroup::base},
    {"TRIGP", "GRIDS%TRIGP", 2398, W3setgGroup::base},
    {"NTRI", "GRIDS%NTRI", 2399, W3setgGroup::base},
    {"COUNTRI", "GRIDS%COUNTRI", 2400, W3setgGroup::base},
    {"SI", "GRIDS%SI", 2401, W3setgGroup::base},
    {"COUNTOT", "GRIDS%COUNTOT", 2402, W3setgGroup::base},
    {"IEN", "GRIDS%IEN", 2403, W3setgGroup::base},
    {"LEN", "GRIDS%LEN", 2404, W3setgGroup::base},
    {"ANGLE", "GRIDS%ANGLE", 2405, W3setgGroup::base},
    {"ANGLE0", "GRIDS%ANGLE0", 2406, W3setgGroup::base},
    {"CCON", "GRIDS%CCON", 2407, W3setgGroup::base},
    {"COUNTCON", "GRIDS%COUNTCON", 2408, W3setgGroup::base},
    {"INDEX_CELL", "GRIDS%INDEX_CELL", 2409, W3setgGroup::base},
    {"IE_CELL", "GRIDS%IE_CELL", 2410, W3setgGroup::base},
    {"POS_CELL", "GRIDS%POS_CELL", 2411, W3setgGroup::base},
    {"IOBP", "GRIDS%IOBP", 2412, W3setgGroup::base},
    {"IAA", "GRIDS%IAA", 2413, W3setgGroup::base},
    {"JAA", "GRIDS%JAA", 2414, W3setgGroup::base},
    {"POSI", "GRIDS%POSI", 2415, W3setgGroup::base},
    {"I_DIAG", "GRIDS%I_DIAG", 2416, W3setgGroup::base},
    {"JA_IE", "GRIDS%JA_IE", 2417, W3setgGroup::base},
    {"NBEDGE", "GRIDS%NBEDGE", 2418, W3setgGroup::base},
    {"EDGES", "GRIDS%EDGES", 2419, W3setgGroup::base},
    {"NEIGH", "GRIDS%NEIGH", 2420, W3setgGroup::base},
    {"NNZ", "GRIDS%NNZ", 2421, W3setgGroup::base},
    {"IOBPD", "GRIDS%IOBPD", 2422, W3setgGroup::base},
    {"IOBDP", "GRIDS%IOBDP", 2423, W3setgGroup::base},
    {"IOBPA", "GRIDS%IOBPA", 2424, W3setgGroup::base},
    {"TRIA", "GRIDS%TRIA", 2425, W3setgGroup::base},
    {"CROSSDIFF", "GRIDS%CROSSDIFF", 2426, W3setgGroup::base},
    {"MAXX", "GRIDS%MAXX", 2427, W3setgGroup::base},
    {"MAXY", "GRIDS%MAXY", 2428, W3setgGroup::base},
    {"DXYMAX", "GRIDS%DXYMAX", 2429, W3setgGroup::base},
    {"XGRD", "GRIDS%XGRD", 2430, W3setgGroup::base},
    {"YGRD", "GRIDS%YGRD", 2431, W3setgGroup::base},
    {"ZB", "GRIDS%ZB", 2432, W3setgGroup::base},
    {"MAPSTA", "GRIDS%MAPSTA", 2436, W3setgGroup::ginit},
    {"MAPST2", "GRIDS%MAPST2", 2437, W3setgGroup::ginit},
    {"MAPFS", "GRIDS%MAPFS", 2438, W3setgGroup::ginit},
    {"MAPSF", "GRIDS%MAPSF", 2439, W3setgGroup::ginit},
    {"FLAGST", "GRIDS%FLAGST", 2440, W3setgGroup::ginit},
    {"CLATS", "GRIDS%CLATS", 2445, W3setgGroup::ginit},
    {"CLATIS", "GRIDS%CLATIS", 2446, W3setgGroup::ginit},
    {"CTHG0S", "GRIDS%CTHG0S", 2447, W3setgGroup::ginit},
    {"TRNX", "GRIDS%TRNX", 2448, W3setgGroup::ginit},
    {"TRNY", "GRIDS%TRNY", 2449, W3setgGroup::ginit},
    {"DXDP", "GRIDS%DXDP", 2451, W3setgGroup::ginit},
    {"DXDQ", "GRIDS%DXDQ", 2452, W3setgGroup::ginit},
    {"DYDP", "GRIDS%DYDP", 2453, W3setgGroup::ginit},
    {"DYDQ", "GRIDS%DYDQ", 2454, W3setgGroup::ginit},
    {"DPDX", "GRIDS%DPDX", 2455, W3setgGroup::ginit},
    {"DPDY", "GRIDS%DPDY", 2456, W3setgGroup::ginit},
    {"DQDX", "GRIDS%DQDX", 2457, W3setgGroup::ginit},
    {"DQDY", "GRIDS%DQDY", 2458, W3setgGroup::ginit},
    {"GSQRT", "GRIDS%GSQRT", 2459, W3setgGroup::ginit},
    {"HPFAC", "GRIDS%HPFAC", 2460, W3setgGroup::ginit},
    {"HQFAC", "GRIDS%HQFAC", 2461, W3setgGroup::ginit},
    {"GSU", "GRIDS%GSU", 2491, W3setgGroup::ginit},
    {"NK", "SGRDS%NK", 2498, W3setgGroup::base},
    {"NK2", "SGRDS%NK2", 2499, W3setgGroup::base},
    {"NTH", "SGRDS%NTH", 2500, W3setgGroup::base},
    {"NSPEC", "SGRDS%NSPEC", 2501, W3setgGroup::base},
    {"DTH", "SGRDS%DTH", 2503, W3setgGroup::base},
    {"XFR", "SGRDS%XFR", 2504, W3setgGroup::base},
    {"FR1", "SGRDS%FR1", 2505, W3setgGroup::base},
    {"FTE", "SGRDS%FTE", 2506, W3setgGroup::base},
    {"FTF", "SGRDS%FTF", 2507, W3setgGroup::base},
    {"FTWN", "SGRDS%FTWN", 2508, W3setgGroup::base},
    {"FTTR", "SGRDS%FTTR", 2509, W3setgGroup::base},
    {"FTWL", "SGRDS%FTWL", 2510, W3setgGroup::base},
    {"FACTI1", "SGRDS%FACTI1", 2511, W3setgGroup::base},
    {"FACTI2", "SGRDS%FACTI2", 2512, W3setgGroup::base},
    {"FACHFA", "SGRDS%FACHFA", 2513, W3setgGroup::base},
    {"FACHFE", "SGRDS%FACHFE", 2514, W3setgGroup::base},
    {"SINIT", "SGRDS%SINIT", 2516, W3setgGroup::base},
    {"MAPWN", "SGRDS%MAPWN", 2520, W3setgGroup::sinit},
    {"MAPTH", "SGRDS%MAPTH", 2521, W3setgGroup::sinit},
    {"TH", "SGRDS%TH", 2523, W3setgGroup::sinit},
    {"ESIN", "SGRDS%ESIN", 2524, W3setgGroup::sinit},
    {"ECOS", "SGRDS%ECOS", 2525, W3setgGroup::sinit},
    {"ES2", "SGRDS%ES2", 2526, W3setgGroup::sinit},
    {"ESC", "SGRDS%ESC", 2527, W3setgGroup::sinit},
    {"EC2", "SGRDS%EC2", 2528, W3setgGroup::sinit},
    {"SIG", "SGRDS%SIG", 2529, W3setgGroup::sinit},
    {"SIG2", "SGRDS%SIG2", 2530, W3setgGroup::sinit},
    {"DSIP", "SGRDS%DSIP", 2531, W3setgGroup::sinit},
    {"DSII", "SGRDS%DSII", 2532, W3setgGroup::sinit},
    {"DDEN", "SGRDS%DDEN", 2533, W3setgGroup::sinit},
    {"DDEN2", "SGRDS%DDEN2", 2534, W3setgGroup::sinit},
    {"PINIT", "MPARS%PINIT", 2541, W3setgGroup::base},
    {"FACP", "MPARS%NPARS%FACP", 2545, W3setgGroup::base},
    {"XREL", "MPARS%NPARS%XREL", 2546, W3setgGroup::base},
    {"XFLT", "MPARS%NPARS%XFLT", 2547, W3setgGroup::base},
    {"FXFM", "MPARS%NPARS%FXFM", 2548, W3setgGroup::base},
    {"FXPM", "MPARS%NPARS%FXPM", 2549, W3setgGroup::base},
    {"XFT", "MPARS%NPARS%XFT", 2550, W3setgGroup::base},
    {"XFC", "MPARS%NPARS%XFC", 2551, W3setgGroup::base},
    {"FACSD", "MPARS%NPARS%FACSD", 2552, W3setgGroup::base},
    {"FHMAX", "MPARS%NPARS%FHMAX", 2553, W3setgGroup::base},
    {"RWINDC", "MPARS%NPARS%RWINDC", 2555, W3setgGroup::base},
    {"WDCG", "MPARS%PROPS%WDCG", 2568, W3setgGroup::base},
    {"WDTH", "MPARS%PROPS%WDTH", 2569, W3setgGroup::base},
    {"SLNC1", "MPARS%SLNPS%SLNC1", 2616, W3setgGroup::base},
    {"FSPM", "MPARS%SLNPS%FSPM", 2617, W3setgGroup::base},
    {"FSHF", "MPARS%SLNPS%FSHF", 2618, W3setgGroup::base},
    {"WWNMEANPTAIL", "MPARS%SRCPS%WWNMEANPTAIL", 2623, W3setgGroup::base},
    {"SSTXFTFTAIL", "MPARS%SRCPS%SSTXFTFTAIL", 2624, W3setgGroup::base},
    {"ZZWND", "MPARS%SRCPS%ZZWND", 2673, W3setgGroup::base},
    {"AALPHA", "MPARS%SRCPS%AALPHA", 2674, W3setgGroup::base},
    {"BBETA", "MPARS%SRCPS%BBETA", 2675, W3setgGroup::base},
    {"SSINTHP", "MPARS%SRCPS%SSINTHP", 2676, W3setgGroup::base},
    {"ZZ0MAX", "MPARS%SRCPS%ZZ0MAX", 2677, W3setgGroup::base},
    {"ZZ0RAT", "MPARS%SRCPS%ZZ0RAT", 2678, W3setgGroup::base},
    {"ZZALP", "MPARS%SRCPS%ZZALP", 2679, W3setgGroup::base},
    {"TTAUWSHELTER", "MPARS%SRCPS%TTAUWSHELTER", 2680, W3setgGroup::base},
    {"SINTAILPAR", "MPARS%SRCPS%SINTAILPAR", 2681, W3setgGroup::base},
    {"CAPCHNK", "MPARS%SRCPS%CAPCHNK", 2682, W3setgGroup::base},
    {"SSWELLFPAR", "MPARS%SRCPS%SSWELLFPAR", 2683, W3setgGroup::base},
    {"SSWELLF", "MPARS%SRCPS%SSWELLF", 2684, W3setgGroup::base},
    {"SSDSC", "MPARS%SRCPS%SSDSC", 2685, W3setgGroup::base},
    {"SSDSBR", "MPARS%SRCPS%SSDSBR", 2686, W3setgGroup::base},
    {"SSDSBT", "MPARS%SRCPS%SSDSBT", 2687, W3setgGroup::base},
    {"SSDSBRF1", "MPARS%SRCPS%SSDSBRF1", 2688, W3setgGroup::base},
    {"SSDSBRF2", "MPARS%SRCPS%SSDSBRF2", 2689, W3setgGroup::base},
    {"SSDSBRFDF", "MPARS%SRCPS%SSDSBRFDF", 2690, W3setgGroup::base},
    {"SSDSBM", "MPARS%SRCPS%SSDSBM", 2691, W3setgGroup::base},
    {"SSDSBCK", "MPARS%SRCPS%SSDSBCK", 2692, W3setgGroup::base},
    {"SSDSABK", "MPARS%SRCPS%SSDSABK", 2693, W3setgGroup::base},
    {"SSDSPBK", "MPARS%SRCPS%SSDSPBK", 2694, W3setgGroup::base},
    {"SSDSHCK", "MPARS%SRCPS%SSDSHCK", 2695, W3setgGroup::base},
    {"SSDSBINT", "MPARS%SRCPS%SSDSBINT", 2696, W3setgGroup::base},
    {"SSDSP", "MPARS%SRCPS%SSDSP", 2697, W3setgGroup::base},
    {"WWNMEANP", "MPARS%SRCPS%WWNMEANP", 2698, W3setgGroup::base},
    {"FFXFM", "MPARS%SRCPS%FFXFM", 2699, W3setgGroup::base},
    {"FFXFA", "MPARS%SRCPS%FFXFA", 2700, W3setgGroup::base},
    {"FFXPM", "MPARS%SRCPS%FFXPM", 2701, W3setgGroup::base},
    {"SSDSDTH", "MPARS%SRCPS%SSDSDTH", 2702, W3setgGroup::base},
    {"SSTXFTF", "MPARS%SRCPS%SSTXFTF", 2703, W3setgGroup::base},
    {"SSTXFTWN", "MPARS%SRCPS%SSTXFTWN", 2704, W3setgGroup::base},
    {"SSDSCOS", "MPARS%SRCPS%SSDSCOS", 2705, W3setgGroup::base},
    {"SSDSISO", "MPARS%SRCPS%SSDSISO", 2706, W3setgGroup::base},
    {"IKTAB", "MPARS%SRCPS%IKTAB", 2707, W3setgGroup::base},
    {"DCKI", "MPARS%SRCPS%DCKI", 2708, W3setgGroup::base},
    {"QBI", "MPARS%SRCPS%QBI", 2709, W3setgGroup::base},
    {"CUMULW", "MPARS%SRCPS%CUMULW", 2710, W3setgGroup::base},
    {"SATINDICES", "MPARS%SRCPS%SATINDICES", 2711, W3setgGroup::base},
    {"SATWEIGHTS", "MPARS%SRCPS%SATWEIGHTS", 2712, W3setgGroup::base},
    {"SSINBR", "MPARS%SRCPS%SSINBR", 2713, W3setgGroup::base},
    {"SNLC1", "MPARS%SNLPS%SNLC1", 2738, W3setgGroup::base},
    {"LAM", "MPARS%SNLPS%LAM", 2739, W3setgGroup::base},
    {"KDCON", "MPARS%SNLPS%KDCON", 2740, W3setgGroup::base},
    {"KDMN", "MPARS%SNLPS%KDMN", 2741, W3setgGroup::base},
    {"SNLS1", "MPARS%SNLPS%SNLS1", 2742, W3setgGroup::base},
    {"SNLS2", "MPARS%SNLPS%SNLS2", 2743, W3setgGroup::base},
    {"SNLS3", "MPARS%SNLPS%SNLS3", 2744, W3setgGroup::base},
    {"IQTPE", "MPARS%SNLPS%IQTPE", 2745, W3setgGroup::base},
    {"GQNF1", "MPARS%SNLPS%GQNF1", 2746, W3setgGroup::base},
    {"GQNT1", "MPARS%SNLPS%GQNT1", 2747, W3setgGroup::base},
    {"GQNQ_OM2", "MPARS%SNLPS%GQNQ_OM2", 2748, W3setgGroup::base},
    {"NLTAIL", "MPARS%SNLPS%NLTAIL", 2749, W3setgGroup::base},
    {"GQTHRSAT", "MPARS%SNLPS%GQTHRSAT", 2750, W3setgGroup::base},
    {"GQTHRCOU", "MPARS%SNLPS%GQTHRCOU", 2751, W3setgGroup::base},
    {"GQAMP", "MPARS%SNLPS%GQAMP", 2752, W3setgGroup::base},
    {"SBTC1", "MPARS%SBTPS%SBTC1", 2819, W3setgGroup::base},
    {"SDBC1", "MPARS%SDBPS%SDBC1", 2828, W3setgGroup::base},
    {"SDBC2", "MPARS%SDBPS%SDBC2", 2829, W3setgGroup::base},
    {"FDONLY", "MPARS%SDBPS%FDONLY", 2830, W3setgGroup::base},
    {"SDBSC", "MPARS%SDBPS%SDBSC", 2831, W3setgGroup::base},
    {"FSBCCFL", "MPARS%SCHMS%FSBCCFL", 2850, W3setgGroup::base},
    {"FSN", "MPARS%SCHMS%FSN", 2851, W3setgGroup::base},
    {"FSPSI", "MPARS%SCHMS%FSPSI", 2852, W3setgGroup::base},
    {"FSFCT", "MPARS%SCHMS%FSFCT", 2853, W3setgGroup::base},
    {"FSNIMP", "MPARS%SCHMS%FSNIMP", 2854, W3setgGroup::base},
    {"FSTOTALIMP", "MPARS%SCHMS%FSTOTALIMP", 2855, W3setgGroup::base},
    {"FSTOTALEXP", "MPARS%SCHMS%FSTOTALEXP", 2856, W3setgGroup::base},
    {"FSREFRACTION", "MPARS%SCHMS%FSREFRACTION", 2857, W3setgGroup::base},
    {"FSFREQSHIFT", "MPARS%SCHMS%FSFREQSHIFT", 2858, W3setgGroup::base},
    {"FSSOURCE", "MPARS%SCHMS%FSSOURCE", 2859, W3setgGroup::base},
    {"DO_CHANGE_WLV", "MPARS%SCHMS%DO_CHANGE_WLV", 2860, W3setgGroup::base},
    {"SOLVERTHR_STP", "MPARS%SCHMS%SOLVERTHR_STP", 2861, W3setgGroup::base},
    {"CRIT_DEP_STP", "MPARS%SCHMS%CRIT_DEP_STP", 2862, W3setgGroup::base},
    {"B_JGS_TERMINATE_MAXITER", "MPARS%SCHMS%B_JGS_TERMINATE_MAXITER", 2863, W3setgGroup::base},
    {"B_JGS_TERMINATE_DIFFERENCE", "MPARS%SCHMS%B_JGS_TERMINATE_DIFFERENCE", 2864,
     W3setgGroup::base},
    {"B_JGS_TERMINATE_NORM", "MPARS%SCHMS%B_JGS_TERMINATE_NORM", 2865, W3setgGroup::base},
    {"B_JGS_LIMITER", "MPARS%SCHMS%B_JGS_LIMITER", 2866, W3setgGroup::base},
    {"B_JGS_USE_JACOBI", "MPARS%SCHMS%B_JGS_USE_JACOBI", 2867, W3setgGroup::base},
    {"B_JGS_BLOCK_GAUSS_SEIDEL", "MPARS%SCHMS%B_JGS_BLOCK_GAUSS_SEIDEL", 2868, W3setgGroup::base},
    {"B_JGS_MAXITER", "MPARS%SCHMS%B_JGS_MAXITER", 2869, W3setgGroup::base},
    {"B_JGS_LIMITER_FUNC", "MPARS%SCHMS%B_JGS_LIMITER_FUNC", 2870, W3setgGroup::base},
    {"B_JGS_PMIN", "MPARS%SCHMS%B_JGS_PMIN", 2871, W3setgGroup::base},
    {"B_JGS_DIFF_THR", "MPARS%SCHMS%B_JGS_DIFF_THR", 2872, W3setgGroup::base},
    {"B_JGS_NORM_THR", "MPARS%SCHMS%B_JGS_NORM_THR", 2873, W3setgGroup::base},
    {"B_JGS_NLEVEL", "MPARS%SCHMS%B_JGS_NLEVEL", 2874, W3setgGroup::base},
    {"B_JGS_SOURCE_NONLINEAR", "MPARS%SCHMS%B_JGS_SOURCE_NONLINEAR", 2875, W3setgGroup::base},
    {"B_JGS_TRUNK_DIGITS", "MPARS%SCHMS%B_JGS_TRUNK_DIGITS", 2876, W3setgGroup::base},
}};

/// @brief The alias groups W3SETG re-points under a condition: the model each now
///        names, std::nullopt while no W3SETG has associated it. The unconditional
///        groups are GdatModule::igrid, isgrd and ipars.
struct W3setgTargets {
  std::optional<int> ginit;  ///< MAPSTA .. GSU, :2436-2491
  std::optional<int> sinit;  ///< MAPWN .. DDEN2, :2520-2534
};

/// @brief The state of the selected model W3SETG reads, from wherever the caller
///        keeps it. Each is called with IMOD, once, after the checks of section 1.
struct W3setgInputs {
  std::function<bool(int)> ginit;  ///< GRIDS(IMOD)%GINIT, :2384, :2434
  std::function<bool(int)> sinit;  ///< SGRDS(IMOD)%SINIT, :2516, :2518
};

/// @brief W3SETG(IMOD, NDSE, NDST) as switch_lab_shrd compiles it.
/// @param gm   NGRIDS, NAUXGR (read); IGRID, ISGRD, IPARS (written on ok).
/// @param t    the conditional alias targets (in/out).
/// @param imod model number, -NAUXGR..NGRIDS as in the Fortran.
/// @param in   readers of GINIT and SINIT of model imod.
/// @return W3setgStatus::ok, or the code EXTCDE would have exited with; on any code
///         other than ok nothing has been written.
/// @author ww3-gpu; heritage H. L. Tolman, J. H. Alves (W3GDATMD)
/// @date 2026-10-16
inline W3setgStatus w3setg(GdatModule& gm, W3setgTargets& t, int imod,
                           const W3setgInputs& in) {
  if (gm.ngrids == -1) return W3setgStatus::grids_not_initialized;  // :2262
  if (imod < -gm.nauxgr || imod > gm.ngrids) return W3setgStatus::illegal_model;  // :2267
  if (!in.ginit || !in.sinit) return W3setgStatus::missing_input;

  gm.igrid = imod;  // :2278; GTYPE .. ZB, :2285-2432
  gm.isgrd = imod;  // :2279; NK .. SINIT, :2498-2516
  gm.ipars = imod;  // :2280; PINIT .. B_JGS_TRUNK_DIGITS, :2541-2876

  if (in.ginit(imod)) t.ginit = imod;  // :2434
  if (in.sinit(imod)) t.sinit = imod;  // :2518
  return W3setgStatus::ok;
}

/// The model of a group, as the slot of a caller's vector over NLOW..NGRIDS
/// (element k + NAUXGR for model k), nullptr when the group has no model or v has
/// no such slot: selected(sgrds, gm, gm.isgrd) for NK, selected(sgrds, gm, t.sinit)
/// for SIG. ⚠ Before any W3SETG gm.igrid is -1 (:619), which is also auxiliary grid
/// -1 when W3NMOD made one; the Fortran aliases are undefined then.
template <class T>
T* selected(std::vector<T>& v, const GdatModule& gm, std::optional<int> target) {
  if (!target) return nullptr;
  const long slot = static_cast<long>(*target) + gm.nauxgr;
  return slot >= 0 && static_cast<std::size_t>(slot) < v.size()
             ? &v[static_cast<std::size_t>(slot)]
             : nullptr;
}

namespace detail {

// Fortran I10: right-justified in 10 characters, ten asterisks when it does not fit.
inline std::string fortran_i10(int v) {
  std::string s = std::to_string(v);
  return s.size() > 10 ? std::string(10, '*') : std::string(10 - s.size(), ' ') + s;
}

}  // namespace detail

/// The text WRITE(NDSE,1001) or WRITE(NDSE,1002) prints before EXTCDE (formats at
/// :2881-2886), one '\n' per record, the leading and trailing '/' as empty records;
/// 1002 prints -NAUXGR under the label NAUXGR, as the Fortran does (:2268). Empty for
/// a status the Fortran prints nothing for.
inline std::string w3setg_message(W3setgStatus st, int imod, const GdatModule& gm) {
  switch (st) {
    case W3setgStatus::grids_not_initialized:
      return "\n *** ERROR W3SETG : GRIDS NOT INITIALIZED *** \n"
             "                    RUN W3NMOD FIRST \n\n";
    case W3setgStatus::illegal_model:
      return "\n *** ERROR W3SETG : ILLEGAL MODEL NUMBER *** \n"
             "                    IMOD   = " + detail::fortran_i10(imod) + "\n"
             "                    NAUXGR = " + detail::fortran_i10(-gm.nauxgr) + "\n"
             "                    NGRIDS = " + detail::fortran_i10(gm.ngrids) + "\n\n";
    default:
      return {};
  }
}

}  // namespace ww::gdat
