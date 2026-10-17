// kokkos/src/ww_kokkos/iopo_w3iopp.hpp
// W3IOPP, ported: the preprocessing of the point output. For each requested output point it
// asks the grid search (W3GRMP) or, on an unstructured grid, the triangle search
// (IS_IN_UNGRID) for the four corner points of the enclosing cell and their bilinear weights,
// skips a point outside the grid or with four land corners (with a warning on NDSE), and stores
// the location, corner indices, weights and name of every accepted point in OUTPTS(IMOD)%OUT2.
// On an unstructured grid it reads those tables from pnt_wght.<ext>.nc when that file exists,
// and otherwise writes them to out.pnt_wght.<ext>.nc. Host only, no kernel. Phase 1,
// translated, not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3IOPP, WW3-noaa/model/src/w3iopomd.F90:242-859 at NOAA-EMC/WW3
// 03adda91 (identical in the h0ffmann/WW3 fork at the WW3/ pin: `git diff 03adda91 HEAD --
// model/src/w3iopomd.F90` is empty), author H. L. Tolman; and its NetCDF error handler
// NF90_ERR_CHECK, :1301-1315, author E. Hartnett. Only what switches/switch_lab_shrd compiles is
// ported (read from `gfortran -E` with the defines of WW3's flags.make). Not compiled and not
// ported: W3_RTD (the rotated-pole conversion, :443-457, :481-485, :535-540, :684-686), W3_O7a
// (the diagnostic map of each point, :690-812, and its formats :818-830), W3_MPI (the broadcast
// of the weights, :614-631), W3_T test output, W3_S STRACE. FACTOR (:432-436) is read only by
// O7a; the -O3 object computes nothing for it and neither does this file.
//
// There is no floating-point arithmetic on the compiled path: XPT, YPT and the weights RD are
// copied, and XPT, YPT are widened to REAL(8) for IS_IN_UNGRID (cvtss2sd, exact). The output
// bytes are the gfortran edits of iopo_state.hpp.
//
// Routines W3IOPP calls and this file does not port are hooks (IoppHooks): W3DMO2 (allocation;
// iopo_shape_w3dmo2 reproduces its arrays), W3GRMP_R4 (w3gsrumd.F90:2521-2566, which closes over
// the caller's grid search object GSU), IS_IN_UNGRID (w3triamd.F90:1582-1762), the
// NetCDF-Fortran calls (IoppNetcdf, with the Fortran arguments) and the formatted output unit.
// A path that reaches an empty hook ends with kExitMissingHook.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <unistd.h>

#include "iogr_state.hpp"
#include "iogr_w3iogr.hpp"
#include "iopo_state.hpp"

namespace ww::iopo {

inline constexpr int kExitMissingHook = ww::iogr::kExitMissingHook;  // -101
inline constexpr int kExitShape = ww::iogr::kExitShape;              // -102
/// A corner index a hook returned lies outside MAPSTA(NY,NX), or a NetCDF read asks for more
/// elements than the array holds: the Fortran reads or writes out of bounds there (no bounds
/// checks in WW3's Release flags), which this port refuses rather than reproduce.
inline constexpr int kExitIndexBounds = -120;

/// The W3ODATMD scalars W3IOPP reads (w3iopomd.F90:355-356).
struct IoppUnits {
  std::int32_t ndse = 6, ndst = 6;
  std::int32_t iaproc = 1, naperr = 1;
};

/// The NetCDF-Fortran calls of W3IOPP, with the arguments the -O3 GIMPLE passes (the specific
/// procedures it resolves to in parentheses). Arrays are the Fortran arrays in element order;
/// start and count are in Fortran order, of the sizes the source writes; a get fills the
/// count elements contiguously from values[0], as the library writes the actual argument.
/// Each returns the NetCDF status (NF90_NOERR = 0).
struct IoppNetcdf {
  /// nf90_open(path, NF90_NOWRITE, ncid), the 124-character FILENAME blank padded.
  std::function<int(const FChar<124>& path, int mode, int& ncid)> open;
  std::function<int(int ncid, const std::string& name, int& dimid)> inq_dimid;
  /// nf90_inquire_dimension(ncid, dimid, len = len), NAME absent.
  std::function<int(int ncid, int dimid, int& len)> inquire_dimension;
  std::function<int(int ncid, const std::string& name, int& varid)> inq_varid;
  /// nf90_get_var (nf90_get_var_2d_fourbytereal) on PTLOC(2,NPT) or PTIFAC(4,NPT).
  std::function<int(int ncid, int varid, float* values, std::array<int, 2> shape,
                    std::array<int, 2> start, std::array<int, 2> count)>
      get_var_real_2d;
  /// nf90_get_var (nf90_get_var_1d_text) on PTNME(NPT), CHARACTER(LEN=len).
  std::function<int(int ncid, int varid, char* values, int len, int shape,
                    std::array<int, 2> start, std::array<int, 2> count)>
      get_var_text_1d;
  /// nf90_get_var (nf90_get_var_3d_fourbyteint) on IPTINT(2,4,NPT): START has two elements.
  std::function<int(int ncid, int varid, std::int32_t* values, std::array<int, 3> shape,
                    std::array<int, 2> start, std::array<int, 3> count)>
      get_var_int_3d;
  std::function<int(int ncid)> close;
  /// nf90_create(path, NF90_NETCDF4, ncid).
  std::function<int(const FChar<124>& path, int cmode, int& ncid)> create;
  std::function<int(int ncid, const std::string& name, int len, int& dimid)> def_dim;
  /// nf90_def_var (nf90_def_var_manydims)(ncid, name, xtype, dimids, varid).
  std::function<int(int ncid, const std::string& name, int xtype, const std::vector<int>& dimids,
                    int& varid)>
      def_var;
  std::function<int(int ncid)> enddef;
  /// nf90_put_var (nf90_put_var_2d_fourbytereal) of a contiguous section (n1,n2).
  std::function<int(int ncid, int varid, const float* values, std::array<int, 2> shape)>
      put_var_real_2d;
  /// nf90_put_var (nf90_put_var_1d_text) of PTNME(1:n), CHARACTER(LEN=len).
  std::function<int(int ncid, int varid, const char* values, int len, int n)> put_var_text_1d;
  /// nf90_put_var (nf90_put_var_3d_fourbyteint) of IPTINT(:,:,1:n), a contiguous section.
  std::function<int(int ncid, int varid, const std::int32_t* values, std::array<int, 3> shape)>
      put_var_int_3d;
  /// nf90_strerror(ncerr), CHARACTER(LEN=80); NF90_ERR_CHECK trims it.
  std::function<std::string(int ncerr)> strerror;
};

/// The calls of W3IOPP, in its order.
struct IoppHooks {
  /// W3DMO2(IMOD, NDSE, NDST, NPT), w3iopomd.F90:438: allocates OUTPTS(IMOD)%OUT2 with NPT
  /// points and selects it (W3SETO). Returns 0, or the EXTCDE code it ends with.
  std::function<int(IopoState& o, int imod, int ndse, int ndst, int npt)> w3dmo2;
  /// W3GRMP(GSU, XPT(IPT), YPT(IPT), IX, IY, RD), :490, the REAL(4) specific W3GRMP_R4 with
  /// every optional argument absent. Returns INGRID; is, js, rw have 4 elements each.
  std::function<FLogical(float xt, float yt, std::int32_t* is, std::int32_t* js, float* rw)>
      w3grmp_r4;
  /// IS_IN_UNGRID(IMOD, DBLE(XPT(IPT)), DBLE(YPT(IPT)), ITOUT, IX, IY, RD), :492-493.
  std::function<void(int imod, double xt, double yt, std::int32_t& itout, std::int32_t* is,
                     std::int32_t* js, float* rw)>
      is_in_ungrid;
  /// The bytes of one formatted WRITE statement on a unit (records ended by '\n').
  std::function<void(int unit, const std::string& bytes)> write_formatted;
  /// INQUIRE(FILE=path, EXIST=...), :468. Empty: libgfortran's file_exists, access(F_OK) on
  /// the name without its trailing blanks (io/unix.c:1912-1918, runtime/string.c:35-42,128-135).
  std::function<bool(const std::string& trimmed_path)> inquire_exist;
  IoppNetcdf nc;
};

/// What W3IOPP ends with. exit_code 0: it returned, possibly early from a NetCDF error, in which
/// case nc_line is the source line of the failing check (as NF90_ERR_CHECK prints it) and
/// ncerr its status. Otherwise exit_code is the code EXTCDE of a hook exits with, or one of the
/// kExit* codes above.
struct IoppStatus {
  int exit_code = 0;
  int nc_line = 0;
  int ncerr = 0;
  const char* missing = nullptr;  // the empty hook, with kExitMissingHook
};

namespace detail {

/// FORMATs 1000-1003, w3iopomd.F90:832-844: an empty record, the warning, the point, the
/// skip line and an empty record. what is "OUT OF GRID" or "ON LAND"; flagll picks F10.3
/// (1000, 1002) or E10.3 (1001, 1003). The literals are the GIMPLE's format strings (v).
inline std::string warning_point(bool on_land, bool flagll, float x, float y,
                                 const FChar<40>& name) {
  std::string s = "\n *** WAVEWATCH-III WARNING :\n";
  s += on_land ? "     OUTPUT POINT ON LAND : " : "     OUTPUT POINT OUT OF GRID : ";
  if (flagll) {
    s += fmt_f(x, 10, 3);
    s += fmt_f(y, 10, 3);
  } else {
    s += fmt_e(x, 10, 3);
    s += fmt_e(y, 10, 3);
  }
  s += "  ";
  s.append(name.c.data(), 40);
  s += "\n     POINT SKIPPPED \n\n";
  return s;
}

/// FILENAME = prefix//FILEXT(:LEN_TRIM(FILEXT))//'.nc' into CHARACTER(LEN=124) (:460, :638):
/// at most 13 + 13 + 3 characters, so never truncated.
inline FChar<124> weight_file(const char* prefix, const FChar<13>& filext) {
  return FChar<124>(std::string(prefix) + filext.trimmed() + ".nc");
}

}  // namespace detail

/// W3IOPP(NPT, XPT, YPT, PNAMES, IMOD, MPI_COMM_IOPP), w3iopomd.F90:242-859 under
/// switch_lab_shrd. g is the grid W3SETG last selected (W3IOPP reads GTYPE, FLAGLL, MAPSTA,
/// FILEXT and, through the W3GRMP hook, GSU of that grid, not of IMOD; W3DMO2 switches to IMOD
/// and back, w3odatmd.F90:1081, :1144); o is OUTPTS(IMOD)%OUT2. XPT and YPT are INTENT(INOUT)
/// but only W3_RTD writes them. mpi_comm_iopp is read only under W3_MPI.
inline IoppStatus w3iopp(int npt, float* xpt, float* ypt, const FChar<40>* pnames, int imod,
                         int mpi_comm_iopp, const ww::iogr::IogrState& g, IopoState& o,
                         const IoppUnits& u, const IoppHooks& h) {
  (void)mpi_comm_iopp;
  IoppStatus st;
  auto missing = [&](const char* name) {
    st.exit_code = kExitMissingHook;
    st.missing = name;
    return st;
  };

  // NF90_ERR_CHECK(errcode, ILINE), :1301-1315: four list-directed WRITEs to NDSE on an error.
  auto nf90_err = [&](int ncerr, int line) -> int {
    if (ncerr == kNf90Noerr) return 0;
    if (!h.write_formatted || !h.nc.strerror) {
      missing(!h.write_formatted ? "write_formatted" : "nc.strerror");
      return ncerr;
    }
    h.write_formatted(u.ndse, list_chars(" *** WAVEWATCH III ERROR IN W3IOPO :"));
    h.write_formatted(u.ndse, list_char_int(" LINE NUMBER ", line));
    h.write_formatted(u.ndse, list_chars(" NETCDF ERROR MESSAGE: "));
    std::string msg = h.nc.strerror(ncerr);
    while (!msg.empty() && msg.back() == ' ') msg.pop_back();  // TRIM
    h.write_formatted(u.ndse, list_chars("Error: ", msg));
    st.nc_line = line;
    st.ncerr = ncerr;
    return ncerr;
  };

  // :438 W3DMO2, then GRDID = FILEXT over every element W3DMO2 allocated (:439).
  if (!h.w3dmo2) return missing("w3dmo2");
  if (const int e = h.w3dmo2(o, imod, u.ndse, u.ndst, npt)) {
    st.exit_code = e;
    return st;
  }
  const auto n = static_cast<std::size_t>(npt < 0 ? 0 : npt);
  if (o.ptloc.size() < 2 * n || o.iptint.size() < 8 * n || o.ptifac.size() < 4 * n ||
      o.ptnme.size() < n) {
    st.exit_code = kExitShape;
    return st;
  }
  for (auto& id : o.grdid) id = g.filext;
  o.nopts = 0;  // :441

  // :460-470: the weight file is used on an unstructured grid only.
  const FChar<124> filename = detail::weight_file("pnt_wght.", g.filext);
  bool pnt_wght_exists = false, pnt_wght_write = false;
  if (g.gtype != ww::iogr::kUngType) {
    pnt_wght_exists = false;
    pnt_wght_write = false;
  } else {
    const std::string path = filename.trimmed();
    pnt_wght_exists = h.inquire_exist ? h.inquire_exist(path) : ::access(path.c_str(), F_OK) == 0;
    pnt_wght_write = !pnt_wght_exists;
  }

  if (!pnt_wght_exists) {
    // :474-550. IX, IY, RD keep their values from one point to the next, as the locals do.
    std::array<std::int32_t, 4> ix{}, iy{};
    std::array<float, 4> rd{};
    const auto nx = static_cast<std::int64_t>(g.nx), ny = static_cast<std::int64_t>(g.ny);
    for (int ipt = 1; ipt <= npt; ++ipt) {
      const auto i = static_cast<std::size_t>(ipt - 1);
      bool ingrid = false;
      if (g.gtype != ww::iogr::kUngType) {  // :489-494
        if (!h.w3grmp_r4) return missing("w3grmp_r4");
        ingrid = h.w3grmp_r4(xpt[i], ypt[i], ix.data(), iy.data(), rd.data()) != 0;
      } else {
        if (!h.is_in_ungrid) return missing("is_in_ungrid");
        std::int32_t itout = 0;
        h.is_in_ungrid(imod, static_cast<double>(xpt[i]), static_cast<double>(ypt[i]), itout,
                       ix.data(), iy.data(), rd.data());
        ingrid = itout > 0;
      }

      if (!ingrid) {  // :496-505
        if (u.iaproc == u.naperr) {
          if (!h.write_formatted) return missing("write_formatted");
          h.write_formatted(u.ndse,
                            detail::warning_point(false, g.flagll != 0, xpt[i], ypt[i], pnames[i]));
        }
        continue;
      }

      // :515-527, the four tests in source order, each read only when the previous one held.
      bool all_land = true;
      for (int k = 0; k < 4 && all_land; ++k) {
        if (iy[k] < 1 || iy[k] > ny || ix[k] < 1 || ix[k] > nx ||
            g.mapsta.size() < static_cast<std::size_t>(nx * ny)) {
          st.exit_code = kExitIndexBounds;
          return st;
        }
        const auto m = static_cast<std::size_t>((iy[k] - 1) + ny * (ix[k] - 1));
        all_land = g.mapsta[m] == 0;
      }
      if (all_land) {
        if (u.iaproc == u.naperr) {
          if (!h.write_formatted) return missing("write_formatted");
          h.write_formatted(u.ndse,
                            detail::warning_point(true, g.flagll != 0, xpt[i], ypt[i], pnames[i]));
        }
        continue;
      }

      // :531-548 Store interpolation data.
      o.nopts = o.nopts + 1;
      const auto p = static_cast<std::size_t>(o.nopts - 1);
      o.ptloc[2 * p] = xpt[i];
      o.ptloc[2 * p + 1] = ypt[i];
      for (std::size_t k = 0; k < 4; ++k) {
        o.iptint[0 + 2 * (k + 4 * p)] = ix[k];
        o.iptint[1 + 2 * (k + 4 * p)] = iy[k];
        o.ptifac[k + 4 * p] = rd[k];
      }
      o.ptnme[p] = pnames[i];
    }
  } else if (u.iaproc == 1) {
    // :551-612 Read the saved weights. A failed check returns from W3IOPP (:556 and on).
    const IoppNetcdf& nc = h.nc;
    if (!nc.open || !nc.inq_dimid || !nc.inquire_dimension || !nc.inq_varid ||
        !nc.get_var_real_2d || !nc.get_var_text_1d || !nc.get_var_int_3d || !nc.close)
      return missing("nc (read)");
    int fh = 0, d_nopts = 0, d_vsize = 0, d_namelen = 0, d_wghtlen = 0;
    int d_nopts_len = 0, d_vsize_len = 0, d_namelen_len = 0, d_wghtlen_len = 0;
    int v_ptloc = 0, v_ptnme = 0, v_iptint = 0, v_ptifac = 0;
    auto fits = [&](std::size_t need, std::size_t have) {
      if (need <= have) return true;
      st.exit_code = kExitIndexBounds;
      return false;
    };
    auto cnt = [](int a) { return static_cast<std::size_t>(a < 0 ? 0 : a); };
    const int npts = static_cast<int>(n);

    if (nf90_err(nc.open(filename, kNf90Nowrite, fh), 556)) return st;
    if (nf90_err(nc.inq_dimid(fh, kDnameNopts, d_nopts), 560)) return st;
    if (nf90_err(nc.inquire_dimension(fh, d_nopts, d_nopts_len), 562)) return st;
    o.nopts = d_nopts_len;  // :563
    if (nf90_err(nc.inq_dimid(fh, kDnameVsize, d_vsize), 567)) return st;
    if (nf90_err(nc.inquire_dimension(fh, d_vsize, d_vsize_len), 569)) return st;
    if (nf90_err(nc.inq_dimid(fh, kDnameNamelen, d_namelen), 573)) return st;
    if (nf90_err(nc.inquire_dimension(fh, d_namelen, d_namelen_len), 575)) return st;
    if (nf90_err(nc.inq_dimid(fh, kDnameWghtlen, d_wghtlen), 579)) return st;
    if (nf90_err(nc.inquire_dimension(fh, d_wghtlen, d_wghtlen_len), 581)) return st;

    if (nf90_err(nc.inq_varid(fh, kVnamePtloc, v_ptloc), 585)) return st;
    if (!fits(cnt(d_vsize_len) * cnt(d_nopts_len), o.ptloc.size())) return st;
    if (nf90_err(nc.get_var_real_2d(fh, v_ptloc, o.ptloc.data(), {2, npts}, {1, 1},
                                    {d_vsize_len, d_nopts_len}),
                 588))
      return st;

    if (nf90_err(nc.inq_varid(fh, kVnamePtnme, v_ptnme), 591)) return st;
    if (!fits(cnt(d_namelen_len) * cnt(d_nopts_len), 40 * o.ptnme.size())) return st;
    if (nf90_err(nc.get_var_text_1d(fh, v_ptnme, o.ptnme.empty() ? nullptr : o.ptnme[0].c.data(),
                                    40, npts, {1, 1}, {d_namelen_len, d_nopts_len}),
                 594))
      return st;

    if (nf90_err(nc.inq_varid(fh, kVnameIptint, v_iptint), 597)) return st;
    if (!fits(cnt(d_vsize_len) * cnt(d_wghtlen_len) * cnt(d_nopts_len), o.iptint.size()))
      return st;
    if (nf90_err(nc.get_var_int_3d(fh, v_iptint, o.iptint.data(), {2, 4, npts}, {1, 1},
                                   {d_vsize_len, d_wghtlen_len, d_nopts_len}),
                 600))
      return st;

    if (nf90_err(nc.inq_varid(fh, kVnamePtifac, v_ptifac), 603)) return st;
    if (!fits(cnt(d_wghtlen_len) * cnt(d_nopts_len), o.ptifac.size())) return st;
    if (nf90_err(nc.get_var_real_2d(fh, v_ptifac, o.ptifac.data(), {4, npts}, {1, 1},
                                    {d_wghtlen_len, d_nopts_len}),
                 606))
      return st;

    if (nf90_err(nc.close(fh), 610)) return st;
  }

  // :635-682 Write the weights of an unstructured grid that had no weight file.
  if (pnt_wght_write && o.nopts > 0 && u.iaproc == 1) {
    const IoppNetcdf& nc = h.nc;
    if (!nc.create || !nc.def_dim || !nc.def_var || !nc.enddef || !nc.put_var_real_2d ||
        !nc.put_var_text_1d || !nc.put_var_int_3d || !nc.close)
      return missing("nc (write)");
    const FChar<124> filenameout = detail::weight_file("out.pnt_wght.", g.filext);
    int fh = 0, d_nopts = 0, d_namelen = 0, d_vsize = 0, d_wghtlen = 0;
    int v_ptloc = 0, v_ptnme = 0, v_iptint = 0, v_ptifac = 0;
    const int np = o.nopts;
    if (nf90_err(nc.create(filenameout, kNf90Netcdf4, fh), 640)) return st;
    if (nf90_err(nc.def_dim(fh, kDnameNopts, np, d_nopts), 644)) return st;
    if (nf90_err(nc.def_dim(fh, kDnameNamelen, 40, d_namelen), 646)) return st;
    if (nf90_err(nc.def_dim(fh, kDnameVsize, 2, d_vsize), 648)) return st;
    if (nf90_err(nc.def_dim(fh, kDnameWghtlen, 4, d_wghtlen), 650)) return st;
    if (nf90_err(nc.def_var(fh, kVnamePtloc, kNf90Float, {d_vsize, d_nopts}, v_ptloc), 654))
      return st;
    if (nf90_err(nc.def_var(fh, kVnamePtnme, kNf90Char, {d_namelen, d_nopts}, v_ptnme), 656))
      return st;
    // IPTINT, an INTEGER array, is defined as NF90_FLOAT (:657); the library converts.
    if (nf90_err(nc.def_var(fh, kVnameIptint, kNf90Float, {d_vsize, d_wghtlen, d_nopts},
                            v_iptint),
                 658))
      return st;
    if (nf90_err(nc.def_var(fh, kVnamePtifac, kNf90Float, {d_wghtlen, d_nopts}, v_ptifac), 660))
      return st;
    if (nf90_err(nc.enddef(fh), 664)) return st;
    if (nf90_err(nc.put_var_real_2d(fh, v_ptloc, o.ptloc.data(), {2, np}), 668)) return st;
    if (nf90_err(nc.put_var_text_1d(fh, v_ptnme, o.ptnme[0].c.data(), 40, np), 670)) return st;
    if (nf90_err(nc.put_var_int_3d(fh, v_iptint, o.iptint.data(), {2, 4, np}), 673)) return st;
    if (nf90_err(nc.put_var_real_2d(fh, v_ptifac, o.ptifac.data(), {4, np}), 675)) return st;
    if (nf90_err(nc.close(fh), 679)) return st;
  }
  return st;
}

}  // namespace ww::iopo
