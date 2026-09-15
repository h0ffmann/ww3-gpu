// kokkos/src/ww_kokkos/fixture_io.hpp
// Reader for the binary W3SNL1 parity fixture written by
// kokkos/tests/fixtures/gen_snl1_fixture.F90.
//
// The file is a raw little-endian stream (Fortran ACCESS='STREAM'), so it has no
// record markers and no padding; kokkos/tests/fixtures/README.md documents the
// layout field by field. This reader is the only place that knows it.
//
// Index convention: the file stores the INSNL1 tables exactly as Fortran holds
// them, 1-based and possibly <= 0. load() subtracts one from every index so the
// rest of the C++ code only ever sees the 0-based convention of snl1_tables.hpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "real.hpp"

namespace ww::fixture {

/// One sea point: the W3SNL1 inputs and the answers the Fortran produced.
struct Snl1Point {
  Real kdmean = 0;
  std::vector<Real> cg;  ///< CG(1:NK)
  std::vector<Real> a;   ///< A(1:NSPEC), action density
  std::vector<Real> s;   ///< S(1:NSPEC), the reference source term
  std::vector<Real> d;   ///< D(1:NSPEC), the reference diagonal
};

/// The whole fixture: the W3GDATMD parameters, the INSNL1 tables and the points.
struct Snl1Fixture {
  // Header / W3GDATMD side.
  int nk = 0, nth = 0, nspec = 0, npts = 0;
  Real xfr = 0, dth = 0, lam = 0, snlc1 = 0, kdcon = 0, kdmn = 0;
  Real snls1 = 0, snls2 = 0, snls3 = 0, fachfe = 0;
  std::vector<Real> sig;  ///< SIG(1:NK)

  // INSNL1 side. Same shape and ordering as ww::snl1::Tables, 0-based.
  int nfr = 0, nfrhgh = 0, nfrchg = 0, nspecx = 0, nspecy = 0;
  Real dal1 = 0, dal2 = 0, dal3 = 0;
  Real awg[8] = {};
  Real swg[8] = {};
  std::vector<int> ip[2][4];  ///< IP11..IP14 / IP21..IP24, size nspecx
  std::vector<int> im[2][4];  ///< IM11..IM14 / IM21..IM24, size nspecx
  std::vector<int> ic[8][2];  ///< IC11,IC12 .. IC81,IC82,  size nspec
  std::vector<Real> af11;     ///< AF11, size nspecx

  std::vector<Snl1Point> points;
};

/// Read `path`, or throw std::runtime_error if it is missing, truncated or does
/// not start with the 'SNL1' magic.
Snl1Fixture load(const std::string& path);

}  // namespace ww::fixture
