// kokkos/tools/nccmp-tol/compare.cpp
// The comparison proper, on netcdf-c. Every numeric variable is read through
// nc_get_var_double whatever its stored type, so one code path serves float32
// fields (what ww3_ounf writes), doubles and integer masks alike. The fill value
// comes from nc_inq_var_fill, which answers for every format and also yields the
// type's default fill when a variable carries no _FillValue attribute -- netCDF's
// own meaning of "this cell was never written".
//
// SPDX-License-Identifier: MIT
#include "nccmp-tol/compare.hpp"

#include <netcdf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ww::nccmp {

namespace {

void check(int status, const std::string& what) {
  if (status != NC_NOERR) throw std::runtime_error(what + ": " + nc_strerror(status));
}

/// An open dataset that closes itself, whichever way the scope is left.
class NcFile {
 public:
  explicit NcFile(const std::string& path) : path_(path) {
    check(nc_open(path.c_str(), NC_NOWRITE, &id_), "open " + path);
  }
  ~NcFile() {
    if (id_ >= 0) nc_close(id_);
  }
  NcFile(const NcFile&) = delete;
  NcFile& operator=(const NcFile&) = delete;
  NcFile(NcFile&&) = delete;
  NcFile& operator=(NcFile&&) = delete;

  int id() const { return id_; }
  const std::string& path() const { return path_; }

 private:
  int id_ = -1;
  std::string path_;
};

/// The header of one variable.
struct VarInfo {
  std::string name;
  int id = -1;
  nc_type type = NC_NAT;
  std::vector<std::size_t> shape;
};

/// One variable's cells, as doubles, plus how to recognise an unwritten cell.
struct Field {
  std::vector<double> values;
  std::vector<std::size_t> shape;
  bool has_fill = false;
  double fill = 0.0;
};

bool is_numeric(nc_type t) {
  switch (t) {
    case NC_BYTE:
    case NC_UBYTE:
    case NC_SHORT:
    case NC_USHORT:
    case NC_INT:
    case NC_UINT:
    case NC_INT64:
    case NC_UINT64:
    case NC_FLOAT:
    case NC_DOUBLE:
      return true;
    default:
      return false;
  }
}

/// Widen a fill value stored in its native type to double, the way
/// nc_get_var_double widens the data, so equality is exact.
double fill_as_double(nc_type t, const void* buf) {
  switch (t) {
    case NC_BYTE:
      return static_cast<double>(*static_cast<const std::int8_t*>(buf));
    case NC_UBYTE:
      return static_cast<double>(*static_cast<const std::uint8_t*>(buf));
    case NC_SHORT:
      return static_cast<double>(*static_cast<const std::int16_t*>(buf));
    case NC_USHORT:
      return static_cast<double>(*static_cast<const std::uint16_t*>(buf));
    case NC_INT:
      return static_cast<double>(*static_cast<const std::int32_t*>(buf));
    case NC_UINT:
      return static_cast<double>(*static_cast<const std::uint32_t*>(buf));
    case NC_INT64:
      return static_cast<double>(*static_cast<const long long*>(buf));
    case NC_UINT64:
      return static_cast<double>(*static_cast<const unsigned long long*>(buf));
    case NC_FLOAT:
      return static_cast<double>(*static_cast<const float*>(buf));
    case NC_DOUBLE:
      return *static_cast<const double*>(buf);
    default:
      throw std::runtime_error("fill_as_double: non-numeric type");
  }
}

VarInfo inquire(const NcFile& f, int varid) {
  VarInfo v;
  char name[NC_MAX_NAME + 1] = {};
  int ndims = 0;
  int dimids[NC_MAX_VAR_DIMS] = {};
  check(nc_inq_var(f.id(), varid, name, &v.type, &ndims, dimids, nullptr),
        f.path() + ": inquire variable " + std::to_string(varid));
  v.name = name;
  v.id = varid;
  for (int d = 0; d < ndims; ++d) {
    std::size_t len = 0;
    check(nc_inq_dimlen(f.id(), dimids[d], &len), f.path() + ": dimlen of " + v.name);
    v.shape.push_back(len);
  }
  return v;
}

/// The variable named `name` in `f`, or nothing if the file has none.
std::optional<VarInfo> find_var(const NcFile& f, const std::string& name) {
  int varid = -1;
  const int status = nc_inq_varid(f.id(), name.c_str(), &varid);
  if (status == NC_ENOTVAR) return std::nullopt;
  check(status, f.path() + ": look up " + name);
  return inquire(f, varid);
}

Field read_field(const NcFile& f, const VarInfo& v) {
  Field out;
  out.shape = v.shape;
  std::size_t n = 1;
  for (std::size_t len : v.shape) n *= len;
  out.values.resize(n);
  if (n > 0) {
    check(nc_get_var_double(f.id(), v.id, out.values.data()), f.path() + ": read " + v.name);
  }
  int no_fill = 0;
  alignas(8) unsigned char buf[8] = {};
  check(nc_inq_var_fill(f.id(), v.id, &no_fill, buf), f.path() + ": fill of " + v.name);
  if (no_fill == 0) {
    out.has_fill = true;
    out.fill = fill_as_double(v.type, buf);
  }
  return out;
}

std::string shape_str(const std::vector<std::size_t>& shape) {
  std::string s = "(";
  for (std::size_t i = 0; i < shape.size(); ++i) {
    if (i > 0) s += "x";
    s += std::to_string(shape[i]);
  }
  return s + ")";
}

Stats compare_field(const std::string& name, const Field& ref, const Field& test,
                    const Tolerance* tol) {
  if (ref.shape != test.shape) {
    throw std::runtime_error(name + ": shape mismatch, ref " + shape_str(ref.shape) +
                             " vs test " + shape_str(test.shape));
  }
  constexpr double eps = std::numeric_limits<double>::epsilon();
  Stats s{name, 0, 0, 0.0, 0.0, 0.0, tol != nullptr, tol != nullptr};
  double sum_sq = 0.0;
  for (std::size_t i = 0; i < ref.values.size(); ++i) {
    const double r = ref.values[i];
    const double t = test.values[i];
    // A cell the reference does not have is nobody's business (land, mask);
    // a cell the reference has and the test lost is counted, and reported.
    if (std::isnan(r) || (ref.has_fill && r == ref.fill)) continue;
    if (std::isnan(t) || (test.has_fill && t == test.fill)) {
      ++s.dropped;
      continue;
    }
    const double d = std::fabs(t - r);
    const double rel = d / std::max(std::fabs(r), eps);
    ++s.n;
    sum_sq += d * d;
    s.max_abs = std::max(s.max_abs, d);
    s.max_rel = std::max(s.max_rel, rel);
    if (tol != nullptr && !(d <= tol->abs || rel <= tol->rel)) s.pass = false;
  }
  if (s.n > 0) s.rms = std::sqrt(sum_sq / static_cast<double>(s.n));
  // Nothing to compare is not agreement: an all-NaN or all-fill test field
  // must fail, not pass vacuously.
  if (s.n == 0) s.pass = false;
  return s;
}

}  // namespace

std::vector<Stats> compare_files(const std::string& ref_path, const std::string& test_path,
                                 const std::vector<Tolerance>& tolerances) {
  const NcFile ref(ref_path);
  const NcFile test(test_path);
  int nvars = 0;
  check(nc_inq_nvars(ref.id(), &nvars), ref.path() + ": count variables");

  std::vector<Stats> out;
  for (int varid = 0; varid < nvars; ++varid) {
    const VarInfo rv = inquire(ref, varid);
    if (!is_numeric(rv.type)) continue;
    const std::optional<VarInfo> tv = find_var(test, rv.name);
    if (!tv || !is_numeric(tv->type)) continue;
    const auto tol = std::find_if(tolerances.begin(), tolerances.end(),
                                  [&](const Tolerance& t) { return t.name == rv.name; });
    out.push_back(compare_field(rv.name, read_field(ref, rv), read_field(test, *tv),
                                tol == tolerances.end() ? nullptr : &*tol));
  }
  return out;
}

bool all_pass(const std::vector<Stats>& stats) {
  return std::all_of(stats.begin(), stats.end(),
                     [](const Stats& s) { return !s.judged || s.pass; });
}

std::string format_table(const std::vector<Stats>& stats) {
  std::size_t width = 8;
  for (const Stats& s : stats) width = std::max(width, s.name.size());
  std::ostringstream os;
  os << std::left << std::setw(static_cast<int>(width)) << "variable" << std::right
     << std::setw(10) << "n" << std::setw(9) << "dropped" << std::setw(12) << "max|d|"
     << std::setw(12) << "rms" << std::setw(12) << "max rel"
     << "  verdict\n";
  os << std::string(width + 10 + 9 + 12 + 12 + 12 + 2 + 11, '-') << '\n';
  os << std::scientific << std::setprecision(3);
  for (const Stats& s : stats) {
    const char* verdict = !s.judged ? "unlisted" : s.pass ? "PASS" : s.n == 0 ? "FAIL (n=0)" : "FAIL";
    os << std::left << std::setw(static_cast<int>(width)) << s.name << std::right
       << std::setw(10) << s.n << std::setw(9) << s.dropped << std::setw(12) << s.max_abs
       << std::setw(12) << s.rms << std::setw(12) << s.max_rel << "  " << verdict << '\n';
  }
  return os.str();
}

}  // namespace ww::nccmp
