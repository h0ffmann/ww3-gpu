// kokkos/src/ww_kokkos/adat_w3xeta.hpp
// W3XETA, ported to Kokkos: select model IMOD for the expanded (X*) output arrays
// only. Host-side, no View is created or read and no kernel runs. Phase 1,
// translated, not regtested.
//
// WW3 heritage: SUBROUTINE W3XETA, WW3/model/src/w3adatmd.F90:3112-3300 at the WW3/
// pin 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman,
// "Reduced version of W3SETA to point t expended output arrays" (:3127). It sits
// outside every #ifdef, so switches/switch_lab_shrd compiles it and it is ported
// as that switch compiles it; W3_S STRACE (:3136-3138, :3149-3152) and the W3_T
// print (:3167-3169) are not compiled and not ported. Every caller at the pin is under
// W3_MPI (w3initmd.F90:3447, :4876, w3iogomd.F90:2607, w3iorsmd.F90:1046) or in
// pdlib_field_vec.F90:1325 (PDLIB), so no lab-switch run reaches it.
//
// How it differs from W3SETA (adat_w3seta.hpp), the trap of this routine:
//  - W3SETA re-points the unconditional aliases, and each conditional group whose
//    condition holds, at WADATS(IMOD) and writes IADATA. W3XETA writes the same
//    IADATA (:3174, Wadats::iadata) and, when AINIT2 holds, re-points 93 of the AINIT
//    group's aliases (HS .. USERO) at the X components of WADATS(IMOD) instead of the
//    plain ones (kXetaAliases, target W3setaTargets::x). NSEALM, AINIT, AINIT2,
//    FL_ALL, DW .. TAUADIR, WN, CG and the rest keep the model of the last W3SETA.
//  - So after W3XETA, IADATA no longer names the model the unconditional aliases
//    point at: that model is W3setaTargets::base, not Wadats::iadata. W3WAVE
//    tests IADATA (w3wavemd.F90:700) and skips its W3SETA when it equals IMOD.
//  - AINIT2 (:3179) is read through the module pointer, which W3XETA does not
//    re-point: it is WADATS(J)%AINIT2 with J the model of the last W3SETA
//    (W3setaTargets::base, W3SETA :2759, unconditional), not necessarily IMOD's
//    and not W3setaTargets::ainit. W3IOGO calls W3SETA(IGRD) just before
//    (w3iogomd.F90:2605); W3MPIO calls W3XDMA(IMOD) and then W3XETA with no W3SETA
//    in between (w3initmd.F90:3440-3447), so there the alias is the model of
//    W3INIT's W3SETA (w3initmd.F90:550, W3MPIO at :1661).
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) of
// w3adatmd.F90 with WW3's Release flags and the switch_lab_shrd defines; the
// object's disassembly equals the CMake build's:
//  - NADATA is tested (:3157), then IMOD <= 0 and IMOD > NADATA with NADATA
//    reloaded (:3162); IMOD is loaded once and that value is stored to IADATA
//    (:3174) before the AINIT2 pointer is loaded and dereferenced (:3179).
//  - The 93 pointer assignments are descriptor copies of
//    WADATS(IMOD)%X<name>, the index from that same IMOD; no arithmetic.
//  - UNGTYPE is USE'd (:3133) and never read.
//
// Not ported, by construction of a C++ library call: the WRITE(NDSE,1001/1002)
// messages and EXTCDE (returned as the code EXTCDE would exit with,
// W3xetaStatus; EXTCDE ends in CALL EXIT, w3servmd.F90:1207); NDSE and NDST,
// which only those use. The X* storage is W3XDMA's (adat_w3xdma*.hpp): this
// header records which model's X arrays the aliases name, not the Views.
#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "adat_w3naux.hpp"
#include "adat_w3seta.hpp"
#include "adat_wadat.hpp"

namespace ww::adat {

/// EXTCDE exit codes of W3XETA, returned instead of stopping the program.
enum class W3xetaStatus : int {
  ok = 0,
  grids_not_initialized = 1,  ///< NADATA == -1, :3157-3160
  illegal_model = 2,          ///< IMOD < 1 or IMOD > NADATA, :3162-3165
  /// Not a WW3 code: :3179 would dereference the AINIT2 pointer before any W3SETA
  /// has associated it (W3setaTargets::base still 0; the pointer is declared
  /// without => NULL(), :690), undefined behaviour in the Fortran. IADATA is
  /// already written, as in the Fortran. No caller at the pin does this. Also
  /// returned when scalars has no entry t.base-1 (W3NAUX not run).
  ainit2_unassociated = -1,
};

/// @brief One pointer assignment of W3XETA section 3: the module pointer, the
///        WADAT component it is pointed at, its line, and the line where W3SETA
///        points the same pointer at the plain component instead.
struct XetaAlias {
  const char* pointer;  ///< W3ADATMD module pointer, e.g. "HS"
  const char* target;   ///< component of WADATS(IMOD), e.g. "XHS"
  int line;             ///< w3adatmd.F90 line of the W3XETA assignment
  int seta_line;        ///< w3adatmd.F90 line of W3SETA's "HS => WADATS(IMOD)%HS"
};

/// The 93 aliases W3XETA re-points when AINIT2 holds (:3181-3280), in its order.
/// All of them belong to W3SETA's AINIT group (:2826, W3setaTargets::ainit); while
/// W3setaTargets::x is nonzero they name its X components instead.
inline constexpr std::array<XetaAlias, 93> kXetaAliases = {{
    {"HS", "XHS", 3181, 2839},
    {"WLM", "XWLM", 3182, 2840},
    {"T02", "XT02", 3183, 2841},
    {"T0M1", "XT0M1", 3184, 2842},
    {"T01", "XT01", 3185, 2843},
    {"FP0", "XFP0", 3186, 2844},
    {"THM", "XTHM", 3187, 2845},
    {"THS", "XTHS", 3188, 2846},
    {"THP0", "XTHP0", 3189, 2847},
    {"HSIG", "XHSIG", 3190, 2848},
    {"STMAXE", "XSTMAXE", 3191, 2849},
    {"STMAXD", "XSTMAXD", 3192, 2850},
    {"HMAXE", "XHMAXE", 3193, 2851},
    {"HMAXD", "XHMAXD", 3194, 2852},
    {"HCMAXE", "XHCMAXE", 3195, 2853},
    {"HCMAXD", "XHCMAXD", 3196, 2854},
    {"QP", "XQP", 3197, 2855},
    {"WBT", "XWBT", 3198, 2856},
    {"WNMEAN", "XWNMEAN", 3199, 2857},
    {"EF", "XEF", 3201, 2859},
    {"TH1M", "XTH1M", 3202, 2860},
    {"STH1M", "XSTH1M", 3203, 2861},
    {"TH2M", "XTH2M", 3204, 2862},
    {"STH2M", "XSTH2M", 3205, 2863},
    {"PHS", "XPHS", 3207, 2865},
    {"PTP", "XPTP", 3208, 2866},
    {"PLP", "XPLP", 3209, 2867},
    {"PDIR", "XPDIR", 3210, 2868},
    {"PSI", "XPSI", 3211, 2869},
    {"PWS", "XPWS", 3212, 2870},
    {"PWST", "XPWST", 3213, 2871},
    {"PNR", "XPNR", 3214, 2872},
    {"PTHP0", "XPTHP0", 3215, 2873},
    {"PQP", "XPQP", 3216, 2874},
    {"PPE", "XPPE", 3217, 2875},
    {"PGW", "XPGW", 3218, 2876},
    {"PSW", "XPSW", 3219, 2877},
    {"PTM1", "XPTM1", 3220, 2878},
    {"PT1", "XPT1", 3221, 2879},
    {"PT2", "XPT2", 3222, 2880},
    {"PEP", "XPEP", 3223, 2881},
    {"CHARN", "XCHARN", 3225, 2883},
    {"TWS", "XTWS", 3226, 2884},
    {"CGE", "XCGE", 3227, 2885},
    {"PHIAW", "XPHIAW", 3228, 2886},
    {"TAUWIX", "XTAUWIX", 3229, 2887},
    {"TAUWIY", "XTAUWIY", 3230, 2888},
    {"TAUWNX", "XTAUWNX", 3231, 2889},
    {"TAUWNY", "XTAUWNY", 3232, 2890},
    {"WHITECAP", "XWHITECAP", 3233, 2891},
    {"SXX", "XSXX", 3235, 2893},
    {"SYY", "XSYY", 3236, 2894},
    {"SXY", "XSXY", 3237, 2895},
    {"TAUOX", "XTAUOX", 3238, 2896},
    {"TAUOY", "XTAUOY", 3239, 2897},
    {"BHD", "XBHD", 3240, 2898},
    {"PHIOC", "XPHIOC", 3241, 2899},
    {"TUSX", "XTUSX", 3242, 2900},
    {"TUSY", "XTUSY", 3243, 2901},
    {"USSX", "XUSSX", 3244, 2902},
    {"USSY", "XUSSY", 3245, 2903},
    {"PRMS", "XPRMS", 3246, 2904},
    {"TPMS", "XTPMS", 3247, 2905},
    {"P2SMS", "XP2SMS", 3248, 2906},
    {"US3D", "XUS3D", 3249, 2907},
    {"PHICE", "XPHICE", 3250, 2908},
    {"TAUICE", "XTAUICE", 3251, 2909},
    {"USSP", "XUSSP", 3252, 2910},
    {"TAUOCX", "XTAUOCX", 3253, 2911},
    {"TAUOCY", "XTAUOCY", 3254, 2912},
    {"ABA", "XABA", 3255, 2914},
    {"ABD", "XABD", 3256, 2915},
    {"UBA", "XUBA", 3257, 2916},
    {"UBD", "XUBD", 3258, 2917},
    {"BEDFORMS", "XBEDFORMS", 3259, 2918},
    {"PHIBBL", "XPHIBBL", 3260, 2919},
    {"TAUBBL", "XTAUBBL", 3261, 2920},
    {"MSSX", "XMSSX", 3263, 2922},
    {"MSSY", "XMSSY", 3264, 2923},
    {"MSSD", "XMSSD", 3265, 2924},
    {"MSCX", "XMSCX", 3266, 2925},
    {"MSCY", "XMSCY", 3267, 2926},
    {"MSCD", "XMSCD", 3268, 2927},
    {"QKK", "XQKK", 3269, 2928},
    {"SKEW", "XSKEW", 3270, 2929},
    {"EMBIA1", "XEMBIA1", 3271, 2930},
    {"EMBIA2", "XEMBIA2", 3272, 2931},
    {"DTDYN", "XDTDYN", 3274, 2933},
    {"FCUT", "XFCUT", 3275, 2934},
    {"CFLXYMAX", "XCFLXYMAX", 3276, 2935},
    {"CFLTHMAX", "XCFLTHMAX", 3277, 2936},
    {"CFLKMAX", "XCFLKMAX", 3278, 2937},
    {"USERO", "XUSERO", 3280, 2939},
}};

/// @brief W3XETA(IMOD, NDSE, NDST).
/// @param reg     WADATS and NADATA (read), IADATA (out: IMOD, :3174).
/// @param scalars W3NAUX's WadatScalars, parallel to reg.wadats (read: AINIT2).
/// @param t       the alias targets: base (read, the AINIT2 alias), x (out).
/// @param imod    model number, 1-based as in the Fortran.
/// @return W3xetaStatus::ok, or the code EXTCDE would have exited with. On 1 and
///         2 nothing has been written; on ainit2_unassociated only reg.iadata.
/// @post On ok: reg.iadata == imod; t.x == imod when WADATS(t.base)%AINIT2,
///       else unchanged. As in the Fortran, x is set whether or not
///       WADATS(IMOD)'s X arrays were allocated by W3XDMA; W3MPIO and
///       DO_OUTPUT_EXCHANGES call W3XDMA(IMOD) first (w3initmd.F90:3440-3442,
///       pdlib_field_vec.F90:1324).
/// @author ww3-gpu; heritage H. L. Tolman (W3ADATMD)
/// @date 2026-10-16
template <class ExecSpace>
W3xetaStatus w3xeta(Wadats<ExecSpace>& reg, const std::vector<WadatScalars>& scalars,
                    W3setaTargets& t, int imod) {
  if (reg.nadata == -1) return W3xetaStatus::grids_not_initialized;      // :3157
  if (imod < 1 || imod > reg.nadata) return W3xetaStatus::illegal_model;  // :3162

  reg.iadata = imod;  // :3174

  // :3179, AINIT2 through the alias of the last W3SETA (:2759), not WADATS(IMOD)'s.
  if (t.base < 1 || static_cast<std::size_t>(t.base) > scalars.size())
    return W3xetaStatus::ainit2_unassociated;
  if (scalars[static_cast<std::size_t>(t.base - 1)].ainit2) {
    t.x = imod;  // :3181-3280, kXetaAliases
  }
  return W3xetaStatus::ok;
}

}  // namespace ww::adat
