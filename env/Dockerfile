# A WW3 build environment. Deliberately CPU-only with gfortran: WW3 upstream has no
# GPU path, so a GPU image isn't the right first step.
#
#   docker build -t ww3-lab env/
#   docker run --rm -it -v "$PWD:/work" -w /work ww3-lab
#
# ⚠ Not built or tested. Treat the package list as a starting point.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential gfortran g++ \
      cmake git curl wget ca-certificates \
      libnetcdf-dev libnetcdff-dev netcdf-bin \
      libhdf5-dev \
      libopenmpi-dev openmpi-bin \
      python3 python3-pip python3-venv \
 && rm -rf /var/lib/apt/lists/*

# Build WW3 at image build time so the container is ready to go.
# Note: ww3_from_ftp.sh pulls a few hundred MB; drop it for a lean image
# and fetch at runtime instead.
ARG WW3_BRANCH=develop
ENV WW3=/opt/WW3
RUN git clone --depth 1 --branch ${WW3_BRANCH} \
      https://github.com/NOAA-EMC/WW3.git ${WW3}

COPY switches/switch_lab_shrd /opt/switch_lab_shrd

RUN cd ${WW3} && mkdir build && cd build \
 && cmake .. -DSWITCH=/opt/switch_lab_shrd -DCMAKE_BUILD_TYPE=Release \
 && make -j"$(nproc)"

ENV PATH="${WW3}/build/bin:${PATH}"

RUN python3 -m pip install --break-system-packages --no-cache-dir \
      numpy xarray netcdf4 matplotlib scipy dask wavespectra pyww3

WORKDIR /work
CMD ["/bin/bash"]
