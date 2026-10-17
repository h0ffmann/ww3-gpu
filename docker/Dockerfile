# syntax=docker/dockerfile:1
# A runtime image for WAVEWATCH III three ways (scripts/ww3-gpu.sh): --ww3 (pure NOAA WW3, from
# WW3-noaa/), --ww3-gpu (the fork WW3/ with the Kokkos port, W3SNL1 and ST4, OpenMP backend) and --ww4 (WAVEWATCH IV). Ubuntu packages only, no
# Nix: the build stage compiles everything, and the final image keeps the programs, their
# runtime libraries and the case inputs, not the sources. CPU only; the GPU image is Dockerfile.cuda.
#
#   just dbuild                        # docker build -f docker/Dockerfile -t ww3-gpu:local .
#   just dsmoke-ww3-gpu                   # scripts/ww3-gpu.sh --ww3-gpu smoke inside the image
#
# Bit for bit is judged inside one executable (AGENTS.md): --ww3-gpu runs the Fortran and the Kokkos
# routines of the same program here, so the verdict holds for this image's compiler too. Its numbers
# are not the Nix toolchain's numbers: gfortran differs (Ubuntu's 15.2 against Nix's 16.2).

# Ubuntu 26.04 LTS, by digest (2026-10-15): the same base for every stage, so the runtime has
# exactly the libraries the programs were linked against.
ARG UBUNTU=ubuntu:26.04@sha256:f144425ff09be612d6d9ad965196e9cdc23dae1f42110a8a11a3e9a8198759f7
# Kokkos 5.2.2, the version picasso's labs/pratico pins. Ubuntu packages 5.0.2, so it is built
# from the release tarball, checked by SHA-256.
ARG KOKKOS_VERSION=5.2.2
ARG KOKKOS_SHA256=d6557aaef39302282a15f9c770433d1fcdf4e961dfd6d9ed726b9d0d0f546b9f

# Stages: toolchain (compilers, netCDF, Kokkos), build (compiles the programs from it), dev (the
# toolchain plus the gate tools and Nix, for .devcontainer/), and the runtime image, which stays
# last so a plain `docker build` still produces it.
FROM ${UBUNTU} AS toolchain
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      gfortran g++ cmake ninja-build make pkg-config perl python3 ca-certificates curl \
      libnetcdf-dev libnetcdff-dev libgtest-dev \
 && rm -rf /var/lib/apt/lists/*

# Kokkos Serial + OpenMP, as pratico builds it.
ARG KOKKOS_VERSION
ARG KOKKOS_SHA256
RUN curl -fsSL -o /tmp/kokkos.tgz \
      "https://github.com/kokkos/kokkos/releases/download/${KOKKOS_VERSION}/kokkos-${KOKKOS_VERSION}.tar.gz" \
 && echo "${KOKKOS_SHA256}  /tmp/kokkos.tgz" | sha256sum -c - \
 && tar xzf /tmp/kokkos.tgz -C /tmp \
 && cmake -S "/tmp/kokkos-${KOKKOS_VERSION}" -B /tmp/kokkos-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/kokkos -DKokkos_ENABLE_SERIAL=ON -DKokkos_ENABLE_OPENMP=ON \
      -DKokkos_ENABLE_TESTS=OFF -DKokkos_ENABLE_DEPRECATED_CODE_4=OFF -DCMAKE_CXX_STANDARD=20 \
 && cmake --build /tmp/kokkos-build && cmake --install /tmp/kokkos-build \
 && rm -rf /tmp/kokkos*

FROM toolchain AS build
ARG DEBIAN_FRONTEND=noninteractive
ARG KOKKOS_VERSION
ARG KOKKOS_SHA256
# The sources, in the repository's layout (kokkos/ finds WW3's w3kokkosmd.F90 at ../WW3).
WORKDIR /src
# Each source is copied just before the step that needs it, so a change to a later one does not
# rebuild the earlier ones (ww3-gpu.sh, the entry point, is copied into the runtime stage only).
COPY kokkos/ kokkos/
COPY WW3/ WW3/
COPY WW3-noaa/ WW3-noaa/
COPY switches/switch_lab_ompg switches/

ENV P=/opt/ww3-gpu/prebuilt
# The Kokkos port and nccmp-tol: the openmp-release preset (-O3, contraction off in the kernel).
RUN cmake -S kokkos --preset openmp-release -B /build/kokkos -DCMAKE_PREFIX_PATH=/opt/kokkos \
 && cmake --build /build/kokkos \
 && cmake --install /build/kokkos --prefix /opt/ww_kokkos \
 && install -D -m 755 /build/kokkos/tools/nccmp-tol/nccmp-tol "$P/bin/nccmp-tol" \
 && install -D -m 644 kokkos/tools/nccmp-tol/tolerances.txt "$P/share/tolerances.txt"

# WW3 twice, as scripts/ww3-gpu.sh builds it: pure NOAA WW3 from WW3-noaa/ with the lab switch
# (--ww3), and the fork WW3/ with the same switch plus KOKKOS linked to the port (--ww3-gpu).
# WW3's own Release flags.
RUN cat switches/switch_lab_ompg > /tmp/switch_ww3 \
 && printf '%s KOKKOS\n' "$(cat switches/switch_lab_ompg)" > /tmp/switch_ww3gpu \
 && cmake -S WW3-noaa -B /build/ww3 -DSWITCH=/tmp/switch_ww3 -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /build/ww3 -j"$(nproc)" \
 && cmake -S WW3 -B /build/ww3-gpu -DSWITCH=/tmp/switch_ww3gpu -DCMAKE_BUILD_TYPE=Release \
      -DWW_KOKKOS=ON "-DCMAKE_PREFIX_PATH=/opt/ww_kokkos;/opt/kokkos" \
 && cmake --build /build/ww3-gpu -j"$(nproc)" \
 && mkdir -p "$P/ww3" "$P/ww3-gpu-openmp" \
 && cp -r /build/ww3/bin "$P/ww3/bin" && cp -r /build/ww3-gpu/bin "$P/ww3-gpu-openmp/bin"

# WW4: its parts report for each case is recorded now, since the image has no WW4 source;
# ww4_standalone is kept when WW4 builds (scripts/ww3-gpu.sh --ww4 exits 5 before running it while a
# part the case needs is not ported).
COPY WW4/ WW4/
COPY scripts/ww4_parts.py scripts/ww4_status.py scripts/
ARG WW4_REV=?
RUN mkdir -p "$P/ww4" \
 && for c in smoke full; do \
      python3 scripts/ww4_parts.py "$c" > "$P/ww4/parts-$c.txt" 2>&1; echo $? > "$P/ww4/parts-$c.rc"; \
      sed -i "s/WW4@?/WW4@${WW4_REV}/" "$P/ww4/parts-$c.txt"; \
    done \
 && if cmake -S WW4 -B /build/ww4 -DCMAKE_BUILD_TYPE=Release -DWW4_ENABLE_TESTING=OFF \
         -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > /build/ww4.log 2>&1 \
       && cmake --build /build/ww4 -j"$(nproc)" >> /build/ww4.log 2>&1; then \
      install -m 755 WW4/exe/ww4_standalone "$P/ww4/ww4_standalone"; \
    else echo "WW4 did not build:" > "$P/ww4/BUILD_FAILED"; tail -20 /build/ww4.log >> "$P/ww4/BUILD_FAILED"; fi

# The case inputs: data, not sources.
RUN mkdir -p "$P/cases/ww3_ts1" "$P/cases/ww4" \
 && cp -r WW3/regtests/ww3_ts1/input "$P/cases/ww3_ts1/input" \
 && cp -r kokkos/tests/fixtures/bench_small "$P/cases/bench_small"

# The runtime packages: the Ubuntu packages that own every library the programs load.
RUN find "$P" -type f -perm -u+x -exec ldd {} + 2>/dev/null \
      | awk '/=> \// { print $3 }' | sort -u | xargs -r realpath \
      | while read -r f; do dpkg -S "$f" 2>/dev/null || dpkg -S "/usr$f" 2>/dev/null || dpkg -S "${f#/usr}" 2>/dev/null; done \
      | cut -d: -f1 | sort -u > /opt/ww3-gpu/runtime-packages.txt \
 && cat /opt/ww3-gpu/runtime-packages.txt

ARG WW3_GPU_REV=?
ARG WW3_REV=?
ARG WW3_NOAA_REV=?
RUN { echo "ww3-gpu ${WW3_GPU_REV}"; echo "WW3 ${WW3_REV} (h0ffmann/WW3 ww3-gpu/kokkos-st4)"; \
      echo "WW3-noaa ${WW3_NOAA_REV} (NOAA-EMC/WW3)"; \
      echo "WW4 ${WW4_REV}"; echo "kokkos ${KOKKOS_VERSION}"; gfortran --version | head -1; \
      cmake --version | head -1; } > /opt/ww3-gpu/BUILD_INFO

# The dev container (.devcontainer/devcontainer.json): the toolchain stage, no sources (the
# workspace is mounted), plus the gate tools of AGENTS.md. Nix is Ubuntu's nix-bin, single-user,
# /nix owned by the dev user, for the reference toolchain (`just ww3`, `just dev`), whose gfortran
# is not this stage's. Without /nix/var/nix, Nix falls back to a chroot store that needs a mount
# namespace an unprivileged container does not get; no build users and no sandbox for the same
# reason. CMAKE_PREFIX_PATH is left unset so this Kokkos (/opt/kokkos, Ubuntu's gfortran) cannot
# leak into a `nix develop` build; pass -DCMAKE_PREFIX_PATH=/opt/kokkos outside Nix, as the build
# stage does. User ubuntu (uid 1000, already in Ubuntu's image) gets sudo without a password, as a
# dev container's user does; the runtime image has no sudo.
FROM toolchain AS dev
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      git openssh-client sudo just shellcheck python3-yaml nix-bin xz-utils less \
 && rm -rf /var/lib/apt/lists/* \
 && echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/ubuntu && chmod 440 /etc/sudoers.d/ubuntu \
 && mkdir -p /etc/nix /nix/store /nix/var/nix && chown -R ubuntu:ubuntu /nix \
 && printf '%s\n' 'experimental-features = nix-command flakes' 'build-users-group =' 'sandbox = false' \
      > /etc/nix/nix.conf
USER ubuntu

FROM ${UBUNTU}
ARG DEBIAN_FRONTEND=noninteractive
COPY --from=build /opt/ww3-gpu /opt/ww3-gpu
# Security fixes released since the base image, then the runtime packages. /usr/bin/pebble ships
# in Ubuntu's base image outside any package, is not used here, and its Go runtime is what the
# CI scan (.github/workflows/docker-scan.yml) flags, so it goes.
RUN apt-get update && apt-get upgrade -y \
 && xargs -a /opt/ww3-gpu/runtime-packages.txt apt-get install -y --no-install-recommends \
 && rm -rf /var/lib/apt/lists/* /usr/bin/pebble
# Not root. Ubuntu's image already has uid 1000 (ubuntu), so the user takes the next free one.
RUN useradd --create-home --shell /bin/bash wave
# The entry point: the same scripts/ww3-gpu.sh the host runs, with WW3GPU_PREBUILT set.
COPY --chmod=755 scripts/ww3-gpu.sh /opt/ww3-gpu/scripts/ww3-gpu.sh
# Its list of Kokkos parts, one file per wiring unit.
COPY kokkos/src/fortran_iface/parts.d/ /opt/ww3-gpu/kokkos/src/fortran_iface/parts.d/
ENV WW3GPU_PREBUILT=/opt/ww3-gpu/prebuilt \
    WW3GPU_RUNS=/work/runs \
    OMP_PROC_BIND=false
WORKDIR /work
RUN chown wave:wave /work
USER wave
ENTRYPOINT ["/opt/ww3-gpu/scripts/ww3-gpu.sh"]
CMD ["--ww3-gpu", "smoke"]
