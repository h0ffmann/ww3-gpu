#!/usr/bin/env bash
# Host prerequisites for building WW3 (Debian / Ubuntu / WSL2).
set -euo pipefail

echo ">> WW3 build prerequisites"
echo "   Needs: Fortran 90 compiler, CMake >= 3.19, NetCDF4 (Fortran bindings), MPI."

sudo apt-get update
sudo apt-get install -y \
  build-essential gfortran g++ \
  cmake git curl wget \
  libnetcdf-dev libnetcdff-dev netcdf-bin \
  libhdf5-dev \
  libopenmpi-dev openmpi-bin \
  python3 python3-pip python3-venv

echo
echo ">> Versions:"
gfortran --version | head -1
cmake --version | head -1
nf-config --version 2>/dev/null || echo "   nf-config not found (netcdf-fortran dev package?)"
mpirun --version | head -1

cat <<'NOTE'

Notes
-----
* WW3 wants NetCDF >= 4.1.1 with the *Fortran* bindings (libnetcdff). The C library
  alone is not enough -- ww3_ounf/ww3_ounp/ww3_prnc link against netcdf.mod.
* CMake finds NetCDF via nf-config / nc-config on PATH, or via $NetCDF_ROOT.
  If detection fails:   export NetCDF_ROOT=$(nf-config --prefix)
* Fortran .mod files are compiler-specific. If you later switch to nvfortran
  (see ../gpu/), you must rebuild HDF5 and NetCDF with nvfortran too. The distro
  packages are gfortran-built and will NOT work with nvfortran.
NOTE
