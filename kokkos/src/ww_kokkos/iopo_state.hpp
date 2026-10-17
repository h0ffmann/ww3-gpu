// kokkos/src/ww_kokkos/iopo_state.hpp
// Host state shared by the three routines of W3IOPOMD (W3IOPP, W3IOPE, W3IOPO): the point-output
// arrays of OUTPTS(IMOD)%OUT2, their allocation by W3DMO2 as a shape helper, the NetCDF names
// of the point weight file, and the gfortran F, E and list-directed edits the routines' messages
// use. Phase 1, translated, not regtested.
//
// WW3 heritage: TYPE OTYPE2, WW3-noaa/model/src/w3odatmd.F90:364-388 at NOAA-EMC/WW3 03adda91
// (identical in the h0ffmann/WW3 fork at the WW3/ pin), its initial values in W3NOUT
// (:711-713) and its allocation in W3DMO2 (:965-1167); the module constants of W3IOPOMD,
// w3iopomd.F90:122-223. Under switches/switch_lab_shrd neither W3_MPI nor W3_FLX5 is defined,
// so NRQPO, NRQPO2, IRQPO1, IRQPO2, O2IRQI (MPI) and TAUAO, TAUDO, DAIRO (FLX5) do not exist.
//
// Arrays are std::vector in Fortran element order (column-major, first index fastest), with the
// extents W3DMO2 gives; element (i,j,k) of an array (n1,n2,n3) with lower bounds 1 is
// v[(i-1) + n1*((j-1) + n2*(k-1))]. W3DMO2 allocates with NPT, the number of points requested,
// and W3IOPP then counts the accepted points in NOPTS <= NPT, so the arrays are longer than
// NOPTS whenever a point is skipped.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "iogr_state.hpp"

namespace ww::iopo {

using ww::iogr::FChar;
using ww::iogr::FLogical;

// PTNME and GRDID are handed to NetCDF as contiguous CHARACTER storage.
static_assert(sizeof(FChar<40>) == 40 && sizeof(FChar<13>) == 13);

/// OUTPTS(IMOD)%OUT2 (w3odatmd.F90:364-388) under switch_lab_shrd. After W3DMO2 has run W3SETO
/// (w3odatmd.F90:1126), the module pointers NOPTS, PTLOC, ... of W3ODATMD alias these fields;
/// the caller keeps one IopoState per output grid IMOD.
struct IopoState {
  std::int32_t ipass2 = 0;  // IPASS2 = 0, W3NOUT (w3odatmd.F90:711)
  std::int32_t nopts = 0;   // NOPTS = 0, W3NOUT (:712)
  FLogical o2init = 0;      // O2INIT = .FALSE., W3NOUT (:713)
  std::int32_t npt = 0;     // the NPT the arrays were allocated with (W3DMO2, :1086-1109)
  std::vector<std::int32_t> iptint;  // IPTINT(2,4,NPT)
  std::vector<std::int32_t> il, iw, ii;  // IL(NPT), IW(NPT), II(NPT)
  std::vector<float> ptloc;   // PTLOC(2,NPT)
  std::vector<float> ptifac;  // PTIFAC(4,NPT)
  std::vector<float> dpo, wao, wdo, aso, cao, cdo, iceo, iceho, icefo;  // (NPT) each
  std::vector<float> zet_seto;  // ZET_SETO(NPT)
  std::vector<float> spco;      // SPCO(NSPEC,NPT)
  std::vector<FChar<40>> ptnme;  // PTNME(NPT), CHARACTER(LEN=40)
  std::vector<FChar<13>> grdid;  // GRDID(NPT), CHARACTER(LEN=13)
};

/// The allocation and the four fills of W3DMO2 (w3odatmd.F90:1086-1118) and its NOPTS = NPT
/// (:1135), nothing else: not its checks of NGRIDS, IMOD and O2INIT (:1060-1074, EXTCDE 1, 2,
/// 3) and not its W3SETG/W3SETO calls, which a caller's W3DMO2 hook does or reproduces. Fortran
/// leaves the arrays outside the fills undefined; here they are zero, and GRDID blank.
inline void iopo_shape_w3dmo2(IopoState& o, int npt, int nspec) {
  const auto n = static_cast<std::size_t>(npt < 0 ? 0 : npt);
  o.npt = npt;
  o.iptint.assign(2 * 4 * n, 0);  // IPTINT = 0 (:1114)
  o.il.assign(n, 0);
  o.iw.assign(n, 0);
  o.ii.assign(n, 0);
  o.ptifac.assign(4 * n, 0.0f);  // PTIFAC = 0. (:1117)
  o.ptnme.assign(n, FChar<40>{});  // PTNME = '' (:1115), blank padded
  o.grdid.assign(n, FChar<13>{});
  o.dpo.assign(n, 0.0f);
  o.wao.assign(n, 0.0f);
  o.zet_seto.assign(n, 0.0f);
  o.wdo.assign(n, 0.0f);
  o.aso.assign(n, 0.0f);
  o.cao.assign(n, 0.0f);
  o.cdo.assign(n, 0.0f);
  o.iceo.assign(n, 0.0f);
  o.iceho.assign(n, 0.0f);
  o.icefo.assign(n, 0.0f);
  o.spco.assign(static_cast<std::size_t>(nspec < 0 ? 0 : nspec) * n, 0.0f);
  o.ptloc.assign(2 * n, 0.0f);  // PTLOC = 0. (:1116)
  o.o2init = 1;                 // O2INIT = .TRUE. (:1112)
  o.nopts = npt;                // NOPTS = NPT (:1135), overwritten by W3IOPP
}

// ---- Point output file: constants of W3IOPOMD (w3iopomd.F90:122-223) ---------------------

inline constexpr const char* kVeropt = "2021-04-06";                       // VEROPT, :122
inline constexpr const char* kIdstr = "WAVEWATCH III POINT OUTPUT FILE";   // IDSTR, :123-124
inline constexpr const char* kDnameNopts = "NOPTS";      // DNAME_NOPTS, :128
inline constexpr const char* kDnameNspec = "NSPEC";      // DNAME_NSPEC, :131
inline constexpr const char* kDnameVsize = "VSIZE";      // DNAME_VSIZE, :135
inline constexpr const char* kDnameNamelen = "NAMELEN";  // DNAME_NAMELEN, :140
inline constexpr const char* kDnameGrdidlen = "GRDIDLEN";  // DNAME_GRDIDLEN, :144
inline constexpr const char* kDnameTime = "TIME";        // DNAME_TIME, :147
inline constexpr const char* kDnameWw3time = "WW3TIME";  // DNAME_WW3TIME, :150
inline constexpr const char* kDnameWghtlen = "WGHTLEN";  // DNAME_WGHTLEN, :154
inline constexpr const char* kVnamePtloc = "PTLOC";      // VNAME_PTLOC, :163
inline constexpr const char* kVnamePtnme = "PTNME";      // VNAME_PTNME, :166
inline constexpr const char* kVnameIptint = "IPTINT";    // VNAME_IPTINT, :169
inline constexpr const char* kVnamePtifac = "PTIFAC";    // VNAME_PTIFAC, :172

// NetCDF-Fortran constants as the -O3 GIMPLE of w3iopomd.F90 passes them (gfortran 16.2.0,
// netcdf-fortran of the pinned ww3 shell): the static C.* operands of nf90_open, nf90_create
// and nf90_def_var.
inline constexpr int kNf90Nowrite = 0;    // NF90_NOWRITE
inline constexpr int kNf90Netcdf4 = 4096;  // NF90_NETCDF4
inline constexpr int kNf90Char = 2;       // NF90_CHAR
inline constexpr int kNf90Float = 5;      // NF90_FLOAT
inline constexpr int kNf90Noerr = 0;      // NF90_NOERR

// ---- gfortran edits used by the messages of W3IOPOMD -------------------------------------
// Bytes as libgfortran 16.2.0 writes them (io/write_float.def, io/write.c), checked against
// gfortran 16.2.0 output in the pinned shell (kokkos/port_report/W3IOPP_KOKKOS.md). The value
// is a REAL(4); snprintf runs on its exact double promotion, as libgfortran's DTOA/FDTOA do.

namespace detail {
/// build_infnan_string (io/write_float.def:754-833) for a width w >= 9, default sign mode.
inline std::string infnan(float x, int w) {
  std::string p(static_cast<std::size_t>(w), ' ');
  if (std::isnan(x)) {
    p.replace(static_cast<std::size_t>(w - 3), 3, "NaN");
  } else {
    p.replace(static_cast<std::size_t>(w - 8), 8, "Infinity");
    if (std::signbit(x)) p[static_cast<std::size_t>(w - 9)] = '-';
  }
  return p;
}
inline std::string right(const std::string& s, int w) {
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}
}  // namespace detail

/// Fw.d, scale factor 0, default rounding (w >= 9 for the Inf/NaN forms). The digits are
/// snprintf "%+-#.*f" with precision d (FDTOA2); a negative value keeps its '-' when it rounds
/// to zero, and -0.0 prints -0.000, as gfortran does. The leading zero of |x| < 1 is dropped
/// only when the field is one character short.
inline std::string fmt_f(float x, int w, int d) {
  if (!std::isfinite(x)) return detail::infnan(x, w);
  char buf[512];
  std::snprintf(buf, sizeof buf, "%+-#.*f", d, static_cast<double>(x));
  std::string digits(buf + 1);  // without the sign snprintf always writes
  const bool neg = buf[0] == '-';
  std::string s = (neg ? "-" : "") + digits;
  if (static_cast<int>(s.size()) == w + 1 && digits.size() > 1 && digits[0] == '0' &&
      digits[1] == '.')
    s = (neg ? "-" : "") + digits.substr(1);
  return detail::right(s, w);
}

/// Ew.d, scale factor 0, default rounding, exponent width chosen by the value (w >= 9 for the
/// Inf/NaN forms): 0.d1..dd followed by E+nn, or +nnn without the E when |exponent| > 99. The
/// d significant digits are snprintf "%+-#.*e" with precision d-1 (DTOA2); zero has exponent
/// +00.
inline std::string fmt_e(float x, int w, int d) {
  if (!std::isfinite(x)) return detail::infnan(x, w);
  char buf[512];
  std::snprintf(buf, sizeof buf, "%+-#.*e", d - 1, static_cast<double>(x));
  const bool neg = buf[0] == '-';
  const std::string b(buf);
  const std::size_t epos = b.find('e');
  std::string mant;  // the d digits, without the point
  for (std::size_t i = 1; i < epos; ++i)
    if (b[i] != '.') mant += b[i];
  int e = std::atoi(b.c_str() + epos + 1);
  bool zero = true;
  for (const char c : mant) zero = zero && c == '0';
  if (!zero) ++e;  // d1.d2.. x 10^e is 0.d1d2.. x 10^(e+1)
  char ex[16];
  const int ae = e < 0 ? -e : e;
  if (ae <= 99)
    std::snprintf(ex, sizeof ex, "E%c%02d", e < 0 ? '-' : '+', ae);
  else
    std::snprintf(ex, sizeof ex, "%c%03d", e < 0 ? '-' : '+', ae);
  std::string s = std::string(neg ? "-" : "") + "0." + mant + ex;
  if (static_cast<int>(s.size()) == w + 1) s = std::string(neg ? "-" : "") + "." + mant + ex;
  return detail::right(s, w);
}

/// One record of a list-directed WRITE(unit,*) of CHARACTER items only, with the default
/// DELIM='NONE': a leading blank, then the items with no separator, then a newline.
inline std::string list_chars(const std::string& a, const std::string& b = std::string()) {
  return " " + a + b + "\n";
}

/// One record of a list-directed WRITE(unit,*) of a CHARACTER item followed by a default
/// INTEGER: the leading blank, the string, and the integer right-justified in 12 characters.
inline std::string list_char_int(const std::string& a, std::int32_t i) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%12d", i);
  return " " + a + buf + "\n";
}

}  // namespace ww::iopo
