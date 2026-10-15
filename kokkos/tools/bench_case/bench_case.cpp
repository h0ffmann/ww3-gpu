// kokkos/tools/bench_case/bench_case.cpp
// The benchmark-case generator. See bench_case.hpp for the contract; the short
// version is "what bench/make_bench_case.py wrote, byte for byte".
//
// SPDX-License-Identifier: MIT
#include "bench_case.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace ww::bench {

namespace fs = std::filesystem;

namespace {

/// The run starts at 2020-01-01 00:00 and lasts `hours`. The calendar date is
/// arbitrary because the forcing is homogeneous and constant.
std::string ww3_date(int hours_after_start) {
  using namespace std::chrono;
  const sys_days start = year{2020} / January / 1;
  const sys_seconds tp = start + std::chrono::hours{hours_after_start};
  const sys_days day = floor<days>(tp);
  const year_month_day ymd{day};
  const auto tod = hh_mm_ss{tp - day};
  char buf[32];
  std::snprintf(buf, sizeof buf, "%04d%02d%02d %02d%02d%02d", static_cast<int>(ymd.year()),
                static_cast<int>(static_cast<unsigned>(ymd.month())),
                static_cast<int>(static_cast<unsigned>(ymd.day())),
                static_cast<int>(tod.hours().count()), static_cast<int>(tod.minutes().count()),
                static_cast<int>(tod.seconds().count()));
  return buf;
}

/// printf("%.0f") -- what Python's f"{x:.0f}" produces for the same double.
std::string fixed0(double x) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.0f", x);
  return buf;
}

void write_file(const fs::path& p, const std::string& text) {
  std::ofstream fh(p, std::ios::binary);
  fh << text;
  if (!fh) throw std::runtime_error("cannot write " + p.string());
}

std::string grid_nml(const Params& p, const Timesteps& ts) {
  std::ostringstream s;
  s << "! ww3-lab benchmark case -- generated, do not hand-edit\n"
       "&SPECTRUM_NML\n"
       "  SPECTRUM%XFR   = 1.1\n"
       "  SPECTRUM%FREQ1 = " << python_float_repr(kFreq1) << "\n"
       "  SPECTRUM%NK    = " << p.nk << "\n"
       "  SPECTRUM%NTH   = " << p.nth << "\n"
       "/\n"
       "&RUN_NML\n"
       "  RUN%FLCX  = T\n"
       "  RUN%FLCY  = T\n"
       "  RUN%FLCTH = T\n"
       "  RUN%FLCK  = F\n"
       "  RUN%FLSOU = T\n"
       "/\n"
       "&TIMESTEPS_NML\n"
       "  TIMESTEPS%DTMAX = " << python_float_repr(ts.dtmax) << "\n"
       "  TIMESTEPS%DTXY  = " << python_float_repr(ts.dtxy) << "\n"
       "  TIMESTEPS%DTKTH = " << python_float_repr(ts.dtkth) << "\n"
       "  TIMESTEPS%DTMIN = " << python_float_repr(ts.dtmin) << "\n"
       "/\n"
       "&GRID_NML\n"
       "  GRID%NAME  = 'WW3LAB BENCH " << p.nx << "x" << p.ny << "'\n"
       "  GRID%NML   = 'namelists.nml'\n"
       "  GRID%TYPE  = 'RECT'\n"
       "  GRID%COORD = 'CART'\n"
       "  GRID%CLOS  = 'NONE'\n"
       "  GRID%ZLIM  = -0.10\n"
       "  GRID%DMIN  = 2.50\n"
       "/\n"
       "&RECT_NML\n"
       "  RECT%NX  = " << p.nx << "\n"
       "  RECT%NY  = " << p.ny << "\n"
       "  RECT%SX  = " << python_float_repr(p.dx_m) << "\n"
       "  RECT%SY  = " << python_float_repr(p.dx_m) << "\n"
       "  RECT%SF  = 1.\n"
       "  RECT%X0  = 0.\n"
       "  RECT%Y0  = 0.\n"
       "  RECT%SF0 = 1.\n"
       "/\n"
       "&DEPTH_NML\n"
       "  DEPTH%SF       = -1.\n"
       "  DEPTH%FILENAME = 'depth.inp'\n"
       "  DEPTH%IDLA     = 1\n"
       "/\n"
       "&MASK_NML\n"
       "  MASK%FILENAME = 'mask.inp'\n"
       "  MASK%IDLA     = 1\n"
       "/\n";
  return s.str();
}

std::string shel_nml(const Params& p) {
  // Field output stride is '0' on purpose: we are timing COMPUTE, not I/O.
  // Writing hourly fields on a large grid can dominate the measurement and
  // turn a compute benchmark into a disk benchmark.
  std::ostringstream s;
  s << "&DOMAIN_NML\n"
       "  DOMAIN%START = '" << ww3_date(0) << "'\n"
       "  DOMAIN%STOP  = '" << ww3_date(p.hours) << "'\n"
       "/\n"
       "&INPUT_NML\n"
       "  INPUT%FORCING%WINDS = 'H'\n"
       "/\n"
       "&OUTPUT_TYPE_NML\n"
       "  TYPE%FIELD%LIST = 'HS'\n"
       "/\n"
       "&OUTPUT_DATE_NML\n"
       "  DATE%FIELD = '20200101 000000' '0' '20200101 000000'\n"
       "/\n"
       "&HOMOG_COUNT_NML\n"
       "  HOMOG_COUNT%N_WND = 1\n"
       "/\n"
       "&HOMOG_INPUT_NML\n"
       "  HOMOG_INPUT(1)%NAME   = 'WND'\n"
       "  HOMOG_INPUT(1)%DATE   = '20200101 000000'\n"
       "  HOMOG_INPUT(1)%VALUE1 = 12.\n"
       "  HOMOG_INPUT(1)%VALUE2 = 270.\n"
       "  HOMOG_INPUT(1)%VALUE3 = 0.\n"
       "/\n";
  return s.str();
}

/// Not in the Python original. Field output is disabled in ww3_shel.nml, so this
/// namelist only does something once a run sets DATE%FIELD's stride; it is here so
/// two benchmark runs can be compared field by field with nccmp-tol.
std::string ounf_nml(const Params& p) {
  std::ostringstream s;
  s << "! ww3-lab benchmark case -- generated, do not hand-edit\n"
       "! Field output is off in ww3_shel.nml (DATE%FIELD stride '0': the case times\n"
       "! compute, not disk). To compare two runs with nccmp-tol, set that stride to\n"
       "! '3600', rerun ww3_shel, then run ww3_ounf with this namelist.\n"
       "&FIELD_NML\n"
       "  FIELD%TIMESTART  = '" << ww3_date(0) << "'\n"
       "  FIELD%TIMESTRIDE = '3600'\n"
       // WW3's FIELD_NML counts output times (TIMECOUNT); it has no TIMESTOP key, and
       // ww3_ounf stops on a namelist read error if one is given. Hours 0..p.hours, hourly.
       "  FIELD%TIMECOUNT  = '" << (p.hours + 1) << "'\n"
       "  FIELD%TIMESPLIT  = 0\n"
       "  FIELD%LIST       = 'HS'\n"
       "  FIELD%PARTITION  = '0'\n"
       "  FIELD%SAMEFILE   = T\n"
       "  FIELD%TYPE       = 4\n"
       "/\n"
       "&FILE_NML\n"
       "  FILE%PREFIX = 'ww3.'\n"
       "  FILE%NETCDF = 4\n"
       "/\n";
  return s.str();
}

/// json.dumps(meta, indent=2): insertion order, two-space indent, no trailing newline.
std::string case_json(const Summary& m) {
  const Params& p = m.params;
  std::ostringstream s;
  s << "{\n"
       "  \"nx\": " << p.nx << ",\n"
       "  \"ny\": " << p.ny << ",\n"
       "  \"nk\": " << p.nk << ",\n"
       "  \"nth\": " << p.nth << ",\n"
       "  \"hours\": " << p.hours << ",\n"
       "  \"dx_m\": " << python_float_repr(p.dx_m) << ",\n"
       "  \"timesteps\": {\n"
       "    \"dtmax\": " << python_float_repr(m.ts.dtmax) << ",\n"
       "    \"dtxy\": " << python_float_repr(m.ts.dtxy) << ",\n"
       "    \"dtkth\": " << python_float_repr(m.ts.dtkth) << ",\n"
       "    \"dtmin\": " << python_float_repr(m.ts.dtmin) << "\n"
       "  },\n"
       "  \"sea_points\": " << m.sea_points << ",\n"
       "  \"spectral_bins\": " << m.spectral_bins << ",\n"
       "  \"state_values\": " << m.state_values << ",\n"
       "  \"state_mb\": " << python_float_repr(m.state_mb) << "\n"
       "}";
  return s.str();
}

int parse_int(std::string_view flag, std::string_view text) {
  int v = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || v < 0)
    throw std::invalid_argument(std::string(flag) + ": expected a non-negative integer, got '" +
                                std::string(text) + "'");
  return v;
}

double parse_double(std::string_view flag, std::string_view text) {
  double v = 0.0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || !(v > 0.0))
    throw std::invalid_argument(std::string(flag) + ": expected a positive number, got '" +
                                std::string(text) + "'");
  return v;
}

}  // namespace

std::optional<Params> size_preset(std::string_view name) {
  // nx, ny, nk, nth, hours -- chosen so serial runtimes land roughly at
  // 10 s / 90 s / 15 min on a modern desktop core. Very rough.
  constexpr double dx = 20.0 * 1000.0;
  if (name == "small") return Params{120, 80, 24, 24, 12, dx};
  if (name == "medium") return Params{300, 200, 32, 36, 24, dx};
  if (name == "large") return Params{700, 450, 32, 36, 48, dx};
  return std::nullopt;
}

Timesteps timesteps(double dx_m) {
  const double cg_max = kG / (4.0 * std::numbers::pi * kFreq1);
  const double t_cfl = dx_m / cg_max;
  const double dtxy = std::max(10.0, std::floor(0.9 * t_cfl / 10.0) * 10.0);
  const double dtmax = 3.0 * dtxy;
  return Timesteps{dtmax, dtxy, dtmax / 2.0, 10.0};
}

Summary write_case(const fs::path& out, const Params& p) {
  if (p.nx < 1 || p.ny < 1 || p.nk < 1 || p.nth < 1 || p.hours < 0 || !(p.dx_m > 0.0))
    throw std::invalid_argument("nx, ny, nk, nth must be >= 1, hours >= 0, dx > 0");
  std::error_code ec;
  fs::create_directories(out, ec);
  if (ec) throw std::runtime_error("cannot create " + out.string() + ": " + ec.message());

  Summary m;
  m.params = p;
  m.ts = timesteps(p.dx_m);
  m.sea_points = static_cast<long long>(p.nx - 1) * p.ny;
  m.spectral_bins = static_cast<long long>(p.nk) * p.nth;
  m.state_values = m.sea_points * m.spectral_bins;
  m.state_mb = static_cast<double>(m.state_values * 4) / 1e6;
  m.global_steps = static_cast<int>(static_cast<double>(p.hours * 3600) / m.ts.dtmax);

  // Flat deep basin with land along the west edge. Same physics as
  // examples/01, just big enough to be worth parallelising.
  {
    std::string row;
    for (int i = 0; i < p.nx; ++i) row += (i == 0 ? "500.0" : " 500.0");
    row += '\n';
    std::string depth;
    depth.reserve(row.size() * static_cast<std::size_t>(p.ny));
    for (int j = 0; j < p.ny; ++j) depth += row;
    write_file(out / "depth.inp", depth);
  }
  {
    std::string row = "0";
    for (int i = 1; i < p.nx; ++i) row += " 1";
    row += '\n';
    std::string mask;
    mask.reserve(row.size() * static_cast<std::size_t>(p.ny));
    for (int j = 0; j < p.ny; ++j) mask += row;
    write_file(out / "mask.inp", mask);
  }
  write_file(out / "namelists.nml", "&MISC\n  FLAGTR = 0\n/\n");
  write_file(out / "ww3_grid.nml", grid_nml(p, m.ts));
  write_file(out / "ww3_shel.nml", shel_nml(p));
  write_file(out / "ww3_ounf.nml", ounf_nml(p));
  write_file(out / "case.json", case_json(m));
  return m;
}

std::string summary_text(const fs::path& out, const Summary& m) {
  const Params& p = m.params;
  std::ostringstream s;
  s << "wrote " << out.string() << "\n"
    << "  grid          : " << p.nx << " x " << p.ny << " at " << fixed0(p.dx_m / 1000) << " km\n"
    << "  spectrum      : " << p.nk << " freq x " << p.nth << " dir = " << m.spectral_bins
    << " bins\n"
    << "  sea points    : " << with_thousands(m.sea_points) << "\n"
    << "  state array   : " << fixed0(m.state_mb) << " MB (single precision)\n"
    << "  timesteps     : DTMAX=" << fixed0(m.ts.dtmax) << " DTXY=" << fixed0(m.ts.dtxy)
    << " DTKTH=" << fixed0(m.ts.dtkth) << " DTMIN=" << fixed0(m.ts.dtmin) << "\n"
    << "  global steps  : " << m.global_steps << "\n"
    << "  field output  : disabled (timing compute, not disk)\n";
  return s.str();
}

std::string usage() {
  return "usage: ww_bench_case [--size small|medium|large] [--nx N] [--ny N] [--nk N] [--nth N]\n"
         "                     [--hours H] [--dx-km X] [-o|--out DIR]\n"
         "\n"
         "Write a self-contained WW3 benchmark run directory (default: case_<size>).\n"
         "  --size    preset (default medium): small 120x80x24x24 12h, medium 300x200x32x36 24h,\n"
         "            large 700x450x32x36 48h; --nx/--ny/--nk/--nth/--hours override it\n"
         "  --dx-km   grid spacing in km (default 20); the timesteps follow from it via CFL\n";
}

Options parse_args(int argc, char** argv) {
  Options o;
  std::string size = "medium";
  std::optional<int> nx, ny, nk, nth, hours;
  double dx_km = 20.0;
  std::optional<std::string> out;

  for (int i = 1; i < argc; ++i) {
    std::string flag = argv[i];
    std::optional<std::string> inline_value;
    if (const auto eq = flag.find('='); flag.rfind("--", 0) == 0 && eq != std::string::npos) {
      inline_value = flag.substr(eq + 1);
      flag.erase(eq);
    }
    if (flag == "-h" || flag == "--help") {
      o.help = true;
      return o;
    }
    auto value = [&]() -> std::string {
      if (inline_value) return *inline_value;
      if (i + 1 >= argc) throw std::invalid_argument(flag + ": missing value");
      return argv[++i];
    };
    if (flag == "--size") size = value();
    else if (flag == "--nx") nx = parse_int(flag, value());
    else if (flag == "--ny") ny = parse_int(flag, value());
    else if (flag == "--nk") nk = parse_int(flag, value());
    else if (flag == "--nth") nth = parse_int(flag, value());
    else if (flag == "--hours") hours = parse_int(flag, value());
    else if (flag == "--dx-km") dx_km = parse_double(flag, value());
    else if (flag == "-o" || flag == "--out") out = value();
    else throw std::invalid_argument("unknown argument '" + flag + "'");
  }

  const auto preset = size_preset(size);
  if (!preset) throw std::invalid_argument("--size: expected small, medium or large, got '" + size + "'");
  o.params = *preset;
  if (nx) o.params.nx = *nx;
  if (ny) o.params.ny = *ny;
  if (nk) o.params.nk = *nk;
  if (nth) o.params.nth = *nth;
  if (hours) o.params.hours = *hours;
  o.params.dx_m = dx_km * 1000.0;
  o.out = out ? fs::path(*out) : fs::path("case_" + size);
  return o;
}

std::string python_float_repr(double x) {
  char buf[64];
  const auto r = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::fixed);
  std::string s(buf, r.ptr);
  if (s.find('.') == std::string::npos) s += ".0";
  return s;
}

std::string with_thousands(long long n) {
  std::string digits = std::to_string(n < 0 ? -n : n);
  std::string grouped;
  const std::size_t len = digits.size();
  for (std::size_t i = 0; i < len; ++i) {
    if (i != 0 && (len - i) % 3 == 0) grouped += ',';
    grouped += digits[i];
  }
  return n < 0 ? "-" + grouped : grouped;
}

}  // namespace ww::bench
