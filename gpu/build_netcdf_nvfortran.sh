#!/usr/bin/env bash
# Build HDF5 and NetCDF (C + Fortran) with nvfortran, so WW3 can be built
# with nvfortran too.
#
# WHY THIS IS NECESSARY: Fortran .mod files are compiler-specific and not
# interchangeable. Your distro's libnetcdff-dev was compiled with gfortran,
# and nvfortran cannot read its modules. There is no way around rebuilding.
#
# ⚠ Not executed. Versions and configure flags drift; check the current
# release numbers before running. Budget an hour, and expect to debug it.
set -euo pipefail

PREFIX="${PREFIX:-$HOME/opt/netcdf-nvhpc}"
BUILD="${BUILD:-$HOME/build-netcdf-nvhpc}"
JOBS="${JOBS:-$(nproc)}"

command -v nvfortran >/dev/null || {
  echo "!! nvfortran not on PATH. Install the NVIDIA HPC SDK:"
  echo "   https://developer.nvidia.com/hpc-sdk"
  echo "   then source its environment, e.g."
  echo "   export PATH=/opt/nvidia/hpc_sdk/Linux_x86_64/<ver>/compilers/bin:\$PATH"
  exit 1
}

export CC=nvc CXX=nvc++ FC=nvfortran F77=nvfortran
export CFLAGS="-fPIC -O2" FCFLAGS="-fPIC -O2" FFLAGS="-fPIC -O2"
export LD_LIBRARY_PATH="$PREFIX/lib:${LD_LIBRARY_PATH:-}"

mkdir -p "$BUILD" "$PREFIX"
cd "$BUILD"

# ---- zlib (skip if your system zlib is fine; it's C-only so usually is) ----

# ---- HDF5 -----------------------------------------------------------------
HDF5_VER=1.14.4
if [ ! -d "hdf5-$HDF5_VER" ]; then
  wget -q "https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.14/hdf5-$HDF5_VER/src/hdf5-$HDF5_VER.tar.gz"
  tar xf "hdf5-$HDF5_VER.tar.gz"
fi
cd "hdf5-$HDF5_VER"
./configure --prefix="$PREFIX" --enable-fortran --enable-hl --with-zlib
make -j"$JOBS" && make install
cd "$BUILD"

# ---- netCDF-C -------------------------------------------------------------
NCC_VER=4.9.2
if [ ! -d "netcdf-c-$NCC_VER" ]; then
  wget -q -O "netcdf-c-$NCC_VER.tar.gz" \
    "https://github.com/Unidata/netcdf-c/archive/refs/tags/v$NCC_VER.tar.gz"
  tar xf "netcdf-c-$NCC_VER.tar.gz"
fi
cd "netcdf-c-$NCC_VER"
CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" \
  ./configure --prefix="$PREFIX" --disable-dap
make -j"$JOBS" && make install
cd "$BUILD"

# ---- netCDF-Fortran -------------------------------------------------------
NCF_VER=4.6.1
if [ ! -d "netcdf-fortran-$NCF_VER" ]; then
  wget -q -O "netcdf-fortran-$NCF_VER.tar.gz" \
    "https://github.com/Unidata/netcdf-fortran/archive/refs/tags/v$NCF_VER.tar.gz"
  tar xf "netcdf-fortran-$NCF_VER.tar.gz"
fi
cd "netcdf-fortran-$NCF_VER"
CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" \
  ./configure --prefix="$PREFIX"
make -j"$JOBS" && make install

cat <<NOTE

Done. Now:

    export NetCDF_ROOT=$PREFIX
    export PATH=$PREFIX/bin:\$PATH
    export LD_LIBRARY_PATH=$PREFIX/lib:\$LD_LIBRARY_PATH
    export CC=nvc CXX=nvc++ FC=nvfortran

    cd \$WW3 && rm -rf build && mkdir build && cd build
    cmake .. -DSWITCH=/path/to/switch_lab_shrd
    make -j

⚠ Expect friction. WW3 is routinely built with gfortran and Intel;
nvfortran is less travelled and may object to things the others accept.
Read the errors carefully -- they are usually genuine standards pedantry
rather than compiler bugs.

And remember: this gets you a CPU build. It is a prerequisite for GPU work,
not GPU work itself.
NOTE
