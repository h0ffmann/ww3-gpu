// kokkos/tools/fetch_analyse/fetch_analyse.cpp
// The fetch-growth analyser: netcdf-c reader and the empirical-law report.
//
// SPDX-License-Identifier: MIT
#include "fetch_analyse.hpp"

#include <netcdf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ww::fetch {

namespace {

/// Throw with the NetCDF library's own message for a failed call.
void check(int status, const std::string& what) {
  if (status != NC_NOERR) throw std::runtime_error(what + ": " + nc_strerror(status));
}

/// RAII around an open NetCDF file.
class File {
 public:
  explicit File(const std::string& path) {
    if (nc_open(path.c_str(), NC_NOWRITE, &id_) != NC_NOERR)
      throw std::runtime_error("could not open '" + path + "' -- run ./run.sh first");
  }
  ~File() { nc_close(id_); }
  File(const File&) = delete;
  File& operator=(const File&) = delete;
  int id() const { return id_; }

 private:
  int id_ = -1;
};

/// The dimension id for the first of `names` that exists, or -1.
int find_dim(int ncid, std::initializer_list<const char*> names, std::string& found) {
  for (const char* n : names) {
    int id = -1;
    if (nc_inq_dimid(ncid, n, &id) == NC_NOERR) {
      found = n;
      return id;
    }
  }
  return -1;
}

/// An optional numeric attribute (_FillValue, scale_factor, add_offset), or `dflt`.
double att_or(int ncid, int varid, const char* name, double dflt) {
  double v = dflt;
  if (nc_get_att_double(ncid, varid, name, &v) != NC_NOERR) return dflt;
  return v;
}

std::string fmt(const char* spec, double v) {
  char buf[64];
  std::snprintf(buf, sizeof buf, spec, v);
  return buf;
}

}  // namespace

double kahma_calkoen(double x_hat) { return 5.2e-7 * std::pow(x_hat, 0.9); }

double hs_from_ehat(double e_hat, double u10) {
  const double energy = e_hat * std::pow(u10, 4) / (kG * kG);
  return 4.0 * std::sqrt(energy);
}

double pierson_moskowitz_hs(double u10) { return 0.0246 * u10 * u10; }

Profile read_profile(const std::string& path) {
  const File f(path);
  const int ncid = f.id();
  Profile p;

  // ww3_ounf names the horizontal coords 'x'/'y' for Cartesian grids and
  // 'longitude'/'latitude' for spherical ones. Handle both.
  const int xdim = find_dim(ncid, {"x", "longitude"}, p.xname);
  const int ydim = find_dim(ncid, {"y", "latitude"}, p.yname);
  std::string tname;
  const int tdim = find_dim(ncid, {"time"}, tname);

  // A one-line stand-in for xarray's dataset printout.
  {
    int ndims = 0, nvars = 0;
    check(nc_inq(ncid, &ndims, &nvars, nullptr, nullptr), "nc_inq");
    std::ostringstream h;
    h << path << ": dims";
    for (int d = 0; d < ndims; ++d) {
      char name[NC_MAX_NAME + 1];
      std::size_t len = 0;
      check(nc_inq_dim(ncid, d, name, &len), "nc_inq_dim");
      h << ' ' << name << '=' << len;
    }
    h << "; variables";
    for (int v = 0; v < nvars; ++v) {
      char name[NC_MAX_NAME + 1];
      check(nc_inq_varname(ncid, v, name), "nc_inq_varname");
      h << ' ' << name;
    }
    p.header = h.str();
  }

  if (xdim < 0) {
    throw std::runtime_error("unexpected dims in '" + path +
                             "' (no x/longitude) -- inspect the file by hand: " + p.header);
  }

  int hsid = -1;
  if (nc_inq_varid(ncid, "hs", &hsid) != NC_NOERR)
    throw std::runtime_error("no 'hs' variable in '" + path + "' -- is HS in FIELD%LIST?");

  int ndims = 0;
  int dimids[NC_MAX_VAR_DIMS];
  check(nc_inq_var(ncid, hsid, nullptr, nullptr, &ndims, dimids, nullptr), "nc_inq_var(hs)");

  // Final time (steady state), centre row (away from the edges), the whole x range.
  std::vector<std::size_t> start(static_cast<std::size_t>(ndims), 0);
  std::vector<std::size_t> count(static_cast<std::size_t>(ndims), 1);
  std::size_t nx = 0;
  for (int k = 0; k < ndims; ++k) {
    std::size_t len = 0;
    check(nc_inq_dimlen(ncid, dimids[k], &len), "nc_inq_dimlen");
    const auto kk = static_cast<std::size_t>(k);
    if (dimids[k] == xdim) {
      count[kk] = len;
      nx = len;
    } else if (dimids[k] == tdim && len > 0) {
      start[kk] = len - 1;
    } else if (dimids[k] == ydim) {
      start[kk] = len / 2;
    }
  }
  if (nx == 0) throw std::runtime_error("'hs' does not vary along " + p.xname);

  p.hs.assign(nx, 0.0);
  check(nc_get_vara_double(ncid, hsid, start.data(), count.data(), p.hs.data()),
        "nc_get_vara_double(hs)");
  const double fill = att_or(ncid, hsid, "_FillValue", std::numeric_limits<double>::quiet_NaN());
  const double scale = att_or(ncid, hsid, "scale_factor", 1.0);
  const double offset = att_or(ncid, hsid, "add_offset", 0.0);
  for (double& v : p.hs) {
    if (!std::isnan(fill) && v == fill) v = std::numeric_limits<double>::quiet_NaN();
    else v = v * scale + offset;
  }

  int xid = -1;
  check(nc_inq_varid(ncid, p.xname.c_str(), &xid), "coordinate variable " + p.xname);
  std::vector<double> x(nx, 0.0);
  check(nc_get_var_double(ncid, xid, x.data()), "nc_get_var_double(" + p.xname + ")");
  const double xmin = *std::min_element(x.begin(), x.end());
  const double xmax = *std::max_element(x.begin(), x.end());
  p.x_looks_small = xmax < 1e4;  // some builds report km or degrees; assume metres otherwise
  p.fetch.resize(nx);
  for (std::size_t i = 0; i < nx; ++i) p.fetch[i] = x[i] - xmin;
  return p;
}

std::string report(const Profile& p, double u10) {
  std::ostringstream out;
  out << p.header << "\n\n";
  if (p.x_looks_small)
    out << "note: x looks small -- check units before trusting the fetch axis\n";

  const double hs_pm = pierson_moskowitz_hs(u10);
  out << "U10 = " << fmt("%.1f", u10) << " m/s      Pierson-Moskowitz fully-developed Hs = "
      << fmt("%.2f", hs_pm) << " m\n\n";

  char line[128];
  std::snprintf(line, sizeof line, "%11s %11s %13s %7s\n", "fetch [km]", "WW3 Hs [m]",
                "K&C92 Hs [m]", "ratio");
  out << line;
  const std::size_t n = p.fetch.size();
  const std::size_t step = std::max<std::size_t>(1, n / 12);
  for (std::size_t i = 1; i < n; i += step) {
    const double x_hat = kG * p.fetch[i] / (u10 * u10);
    const double hs_emp = hs_from_ehat(kahma_calkoen(x_hat), u10);
    const double ratio =
        hs_emp > 0 ? p.hs[i] / hs_emp : std::numeric_limits<double>::quiet_NaN();
    std::snprintf(line, sizeof line, "%11.0f %11.3f %13.3f %7.2f\n", p.fetch[i] / 1000.0,
                  p.hs[i], hs_emp, ratio);
    out << line;
  }

  out << "\nWhat to look for:\n"
         "  * Hs should increase monotonically with fetch. If it decreases, your\n"
         "    wind direction convention is flipped -- see the note in ww3_shel.nml.\n"
         "  * Ratio should sit near 1 over most of the fetch, drifting as the sea\n"
         "    approaches full development.\n"
      << "  * Nothing should meaningfully exceed " << fmt("%.2f", hs_pm)
      << " m at steady state.\n";
  return out.str();
}

}  // namespace ww::fetch
