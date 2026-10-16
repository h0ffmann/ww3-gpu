// kokkos/src/ww_kokkos/wave_w3wave_hooks.hpp
// The calls of W3WAVE that stay in Fortran, as a table of C function pointers
// that a future bind(C) shim fills with the WW3 routines. Phase 1: declared and
// documented here; nothing in this repository fills it yet, and WW3/ is not
// touched.
//
// WW3 heritage: the CALL and WRITE statements of W3WAVE,
// WW3-noaa/model/src/w3wavemd.F90:229-2997 at NOAA-EMC/WW3 03adda91, as
// switches/switch_lab_shrd compiles it (the preprocessed source and the
// relocations of the -O3 object, which list every external call: DSEC21 45
// times, TICK21 3, STME21 3, EXTCDE 8, PRINT_MEMCHECK 37, and each routine
// below). Those callees have no Kokkos translation; the hooks keep each call
// at its place in W3WAVE's sequence, with the arguments the Fortran passes.
//
// Memory contract (phase 1). The arrays W3WAVE and its translated callees
// touch live in Kokkos Views owned by the shim (wave_w3wave_state.hpp,
// Arrays). A Fortran routine reads and writes WW3's module arrays instead.
// Before every hook marked [arrays] the driver fences and calls
// sync_to_host(ctx, id); after it, sync_to_device(ctx, id). The shim copies
// what that call reads and writes (or everything; phase 1 copies per call and
// exists to validate). On a host build whose Views alias the module memory,
// both may be null. Never Unified Memory (AGENTS.md, "Porting").
//
// Scalars and the integer dates are not copied: wave_w3wave_state.hpp Module
// holds pointers to the module variables themselves (C_LOC), so a hook that
// changes TICE, TBPIN or FLBPI is seen by the driver at once, as in WW3.
//
// LOGICAL arguments are int: gfortran's default LOGICAL is 4 bytes, .TRUE. is
// 1, and W3WAVE's tests are `!= 0` in the -O3 GIMPLE.
#pragma once

extern "C" {

/// Which Fortran call a sync_to_host / sync_to_device brackets.
enum ww_w3wave_hook_id {
  WW_W3WAVE_W3UCUR = 1,      ///< :1194
  WW_W3WAVE_UG_GRADIENTS,    ///< :1208-1209, :1489
  WW_W3WAVE_W3UINI,          ///< :1268
  WW_W3WAVE_W3UTAU,          ///< :1271
  WW_W3WAVE_W3URHO,          ///< :1278
  WW_W3WAVE_W3IOBC_READ,     ///< :1306, with W3UBPT at :1307
  WW_W3WAVE_W3UICE,          ///< :1350
  WW_W3WAVE_W3UIC1,          ///< :1381
  WW_W3WAVE_W3ULEV,          ///< :1434
  WW_W3WAVE_W3CFLUG,         ///< :1713, the whole JSEA loop
  WW_W3WAVE_W3XYPUG,         ///< :1976
  WW_W3WAVE_W3SRCE_FORTRAN,  ///< :2246-2349, the whole JSEA loop
  WW_W3WAVE_W3CPRT,          ///< :2488
  WW_W3WAVE_W3OUTG,          ///< :2490
  WW_W3WAVE_W3IOGO,          ///< :2656
  WW_W3WAVE_W3IOPE_IOPO,     ///< :2690-2694
  WW_W3WAVE_W3IOTR,          ///< :2706
  WW_W3WAVE_W3IORS,          ///< :2708, :2795
  WW_W3WAVE_W3IOBC_WRITE,    ///< :2715
  WW_W3WAVE_W3IOSF           ///< :2719
};

/// The field UG_GRADIENTS differentiates (:1208, :1209, :1489).
enum ww_w3wave_ug_field { WW_W3WAVE_UG_CX = 0, WW_W3WAVE_UG_CY = 1, WW_W3WAVE_UG_DW = 2 };

/// Error messages W3WAVE writes to NDSE before EXTCDE: the FORMAT labels of
/// :2926-2941, and -1 for W3XYP3's list-directed tripole message
/// (w3pro3md.F90:868-874), which the driver checks at the call site because
/// the device function cannot stop the program.
enum ww_w3wave_error_label {
  WW_W3WAVE_ERR_XYP3_TRIPOLE = -1,
  WW_W3WAVE_ERR_TEND = 1000,     ///< ending time before starting time
  WW_W3WAVE_ERR_LEVEL = 1001,    ///< new water level before old
  WW_W3WAVE_ERR_CURRENT = 1002,  ///< illegal current interval
  WW_W3WAVE_ERR_WIND = 1003,     ///< illegal wind interval
  WW_W3WAVE_ERR_ICE = 1004,      ///< new ice field before old
  WW_W3WAVE_ERR_IC1 = 1005,      ///< new IC1 field before old
  WW_W3WAVE_ERR_TAUA = 1007,     ///< new atmospheric momentum before old
  WW_W3WAVE_ERR_RHOA = 1008      ///< new air density before old
};

/// The hook table. ctx is the shim's, passed back unchanged as the first
/// argument of every hook. Times are WW3's integer dates, t[0] = YYYYMMDD and
/// t[1] = HHMMSS (W3TIMEMD). Every hook not marked "may be null" is required;
/// the driver returns kErrHook before doing anything when one is missing.
struct ww_w3wave_hooks {
  void* ctx;

  // W3TIMEMD (time arithmetic, not translated).
  /// DSEC21(T1, T2), seconds from T1 to T2 as the REAL WW3 returns.
  float (*dsec21)(void* ctx, const int* t1, const int* t2);
  /// TICK21(T, DTIME), T advanced by DTIME seconds in place.
  void (*tick21)(void* ctx, int* t, float dtime);

  // W3SERVMD and the data modules.
  /// :694-698: the five IF (IOUTP/IGRID/IWDATA/IADATA/IIDATA .NE. IMOD)
  /// CALL W3SETO/W3SETG/W3SETW/W3SETA/W3SETI (IMOD, NDSE, NDST).
  void (*set_model)(void* ctx, int imod);
  /// EXTCDE(code). Does not return in WW3; if it does, the driver returns
  /// kErrExtcde.
  void (*extcde)(void* ctx, int code);
  /// WRITE (NDSE, label), called only when IAPROC .EQ. NAPERR, as in WW3.
  void (*write_error)(void* ctx, int label);
  /// PRINT_MEMCHECK(memunit, tag) with W3WAVE's tags verbatim. May be null: its
  /// body is under W3_MEMCHECK (w3servmd.F90:2501-2516), not in switch_lab_shrd.
  void (*memcheck)(void* ctx, int memunit, const char* tag);

  // Log and screen output (formats 900, 901, 902, 950, 951; :2915-2925).
  /// :749 BACKSPACE (NDSO), called when IAPROC .EQ. NAPLOG.
  void (*log_backspace)(void* ctx);
  /// :1155-1157 WWTIME, STME21(TIME), WRITE (SCREEN,950).
  void (*screen_step)(void* ctx, const int* time);
  /// :2900-2901 WWTIME, WRITE (SCREEN,951).
  void (*screen_end)(void* ctx);
  /// :2401-2407 and :2853-2880: STME21(TIME) then WRITE (NDSO,900) when
  /// full != 0, else (NDSO,901). idact is CHARACTER(21), outid CHARACTER(16),
  /// blank padded, not NUL terminated.
  void (*log_step)(void* ctx, int itime, int ipass, const int* time, const char* idact,
                   const char* outid, int full);
  /// :2904 WRITE (NDSO,902), called when IAPROC .EQ. NAPLOG.
  void (*log_footer)(void* ctx);

  // Sync around every [arrays] hook (see the memory contract above). May be null.
  void (*sync_to_host)(void* ctx, int hook_id);
  void (*sync_to_device)(void* ctx, int hook_id);

  // W3UPDTMD, W3TRIAMD, W3IOBCMD: forcing and boundaries [arrays].
  void (*w3ucur)(void* ctx, int flfrst);                ///< W3UCUR(FLFRST), :1194
  void (*ug_gradients)(void* ctx, int field);           ///< UG_GRADIENTS, :1208-1209, :1489
  void (*w3uini)(void* ctx);                            ///< W3UINI(VA), :1268
  void (*w3utau)(void* ctx, int flfrst);                ///< W3UTAU(FLFRST), :1271
  void (*w3urho)(void* ctx, int flfrst);                ///< W3URHO(FLFRST), :1278
  /// W3IOBC('READ', NDS(9), TBPI0, TBPIN, ITEST, IMOD), :1306; returns ITEST.
  /// May change TBPI0, TBPIN and FLBPI (read back through Module).
  int (*w3iobc_read)(void* ctx, int imod);
  void (*w3ubpt)(void* ctx);                            ///< W3UBPT, :1307
  void (*w3uice)(void* ctx);                            ///< W3UICE(VA), :1350
  void (*w3uic1)(void* ctx, int flfrst);                ///< W3UIC1(FLFRST), :1381
  void (*w3ulev)(void* ctx);                            ///< W3ULEV(VA, VA), :1434

  // W3PROFSMD, unstructured grids only [arrays].
  /// W3CFLUG(ISEA, NKCFL, FACX, FACX, DTG, MAPFS, CFLXYMAX(JSEA), VGX, VGY), :1713.
  void (*w3cflug)(void* ctx, int isea, int jsea, int nkcfl, float facx, float dtg, float vgx,
                  float vgy);
  /// W3XYPUG(ISPEC, FACX, FACX, DTG, FIELD, VGX, VGY, UGDTUPDATE), :1976.
  /// field is a host copy of W3WAVE's local FIELD(1-NY:NY*(NX+2)), NY*(NX+3)
  /// entries, copied in and back by the driver; ugdtupdate is INOUT.
  void (*w3xypug)(void* ctx, int ispec, float facx, float dtg, float* field, float vgx,
                  float vgy, int* ugdtupdate);

  // W3SRCEMD on the Fortran side [arrays].
  /// Whether this step's source terms run on Kokkos: the fork's
  /// W3KOKKOS_SRCE_ON(IT) (IQTPE > 0, not INFLAGS2(4) with IICEDISP, and IT = 0
  /// or SINTAILPAR(4) > 0.5), the branches srce_kernel.hpp leaves to the
  /// Fortran. Not a call of W3WAVE: the seam of the fork's caller hunk, which
  /// W3WAVE under switch_lab_shrd does not have. Required: srce_kernel.hpp
  /// relies on it to refuse those branches.
  int (*srce_on_kokkos)(void* ctx, int it);
  /// The body of the JSEA loop for one point, :2249-2348, when srce_on_kokkos
  /// says no: IX, IY, DELA/DELX/DELY = 1., the MAPSTA and FLAGST test, the
  /// TMP1..TMP4 copies, W3SRCE(srce_direct, ...) with D50 = 0.0002,
  /// REFLEC = 0., REFLED = 0, PSIC = 0. (:2221-2224), or the UNDEF branch.
  /// tauwx, tauwy are W3WAVE's locals TAUWX(JSEA), TAUWY(JSEA), INOUT, on the
  /// host (the driver copies them around the loop).
  void (*w3srce_fortran)(void* ctx, int it, int isea, int jsea, int imod, float dtg,
                         float* tauwx, float* tauwy);

  // Output, :2488-2795 [arrays].
  void (*w3cprt)(void* ctx, int imod);                   ///< W3CPRT(IMOD), :2488
  /// W3OUTG(VA, FLPFLD, FLOUTG, FLOUTG2), :2490.
  void (*w3outg)(void* ctx, int flpfld, int floutg, int floutg2);
  void (*w3iogo)(void* ctx, int imod);                   ///< W3IOGO('WRITE', NDS(7), ITEST, IMOD)
  void (*w3iope)(void* ctx);                             ///< W3IOPE(VA), :2690
  void (*w3iopo)(void* ctx, int imod);                   ///< W3IOPO('WRITE', NDS(8), ITEST, IMOD)
  void (*w3iotr)(void* ctx, int imod);                   ///< W3IOTR(NDS(11), NDS(12), VA, IMOD)
  /// W3IORS('HOT', NDS(6), XXX, IMOD, FLOUT(8)), :2708 and :2795; xxx INOUT.
  void (*w3iors_hot)(void* ctx, float* xxx, int imod);
  void (*w3iobc_write)(void* ctx, int imod);             ///< W3IOBC('WRITE', NDS(10), TIME, TIME, ITEST, IMOD)
  void (*w3iosf)(void* ctx, int imod);                   ///< W3IOSF(NDS(13), IMOD), :2719
};

}  // extern "C"
