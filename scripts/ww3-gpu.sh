#!/usr/bin/env bash
# ww3-gpu — one command, six runtimes of WAVEWATCH (`just run` is this script):
#
#   --ww3                 the reference: MetOcean's WW3 7.14, built from WW3-metocean-7.14/ (metocean/WW3
#                         tag 7.14.mos-92e2cf6), the lab switch, Fortran only. Always this source.
#   --ww3-gpu --cpu       WW3 from the fork WW3/ (h0ffmann/WW3), built with the KOKKOS switch and
#   --ww3-gpu --gpu       linked to this repository's Kokkos port (kokkos/, the ww_kokkos package), on
#                         Kokkos' OpenMP backend or its CUDA backend.
#   --chaceon --cpu       chaceon (h0ffmann/chaceon, the chaceon/ submodule): WW3 rewritten in Rust,
#   --chaceon --gpu       no Fortran in the run; --cceon is the same flag. Compared with --ww3 built
#                         with chaceon's own switch. At chaceon's pin nothing of it has run on a real
#                         GPU, so --gpu exits 5 and says so, as --ww4 does for a part not ported.
#   --ww4                 WAVEWATCH IV, built from the WW4/ submodule, when it is released: it runs
#                         only when every part the case needs is ported to WW4
#                         (scripts/ww4_parts.py); if one is not, it exits 5 and says which, before
#                         building anything.
#
# A seventh mode is not a runtime: --ww3-noaa builds pure NOAA-EMC/WW3 at the commit the fork
# branched from (kokkos/src/fortran_iface/WW3_BASE). It has no submodule: scripts/ww3_base.sh
# writes its tree from WW3/'s own history to WW3-noaa/, which git ignores. It is what --ww3-gpu is
# compared with pair to pair, because the fork is that commit plus the KOKKOS patch and is not
# rebased onto MetOcean's WW3 yet
# (docs/plans/2026-10-29-001-metocean-upstream-plan.md, U1); MetOcean's ST4 arithmetic is not
# NOAA's, so --ww3 cannot be the bit-for-bit partner of --ww3-gpu until then.
# --ww3-noaa and --ww3-gpu build the lab switch with OpenMP, switches/switch_lab_ompg, so that
# comparison is OpenMP against OpenMP; WW3GPU_SWITCH=switch_lab_shrd builds the serial lab switch.
# --ww3 and --chaceon build the serial one: MetOcean's WW3 at 92e2cf66 does not compile with OMPG
# (model/src/w3iogomd.F90:2374 opens a parallel loop that :2380 opens again), and --ww3 with an
# OMPG switch is refused (exit 64).
#
#   scripts/ww3-gpu.sh --ww3      <case> [--cpu]
#   scripts/ww3-gpu.sh --ww3-gpu  <case> [--cpu|--gpu] [--kokkos all|none|<PART,...>]
#   scripts/ww3-gpu.sh --chaceon  <case> [--cpu|--gpu]
#   scripts/ww3-gpu.sh --ww4      smoke|full [--cpu]
#   <case> is smoke, full, coast, or --case <dir> [--wind ifs|<file.nc>] [--temp cmems|<file.nc>]
#
# --case <dir> runs a case of the caller's: a directory with ww3_grid.nml and ww3_shel.nml, the
# files they name (depth and mask of a regular grid, or the Gmsh .msh of a triangular one,
# GRID%TYPE = 'UNST'), and optionally ww3_strt.inp or .nml, ww3_ounf.nml, ww3_ounp.nml,
# points.list and ww3_prnc_<field>.nml. Its name is the directory's. Regular and curvilinear grids
# run in every runtime; a triangular grid runs in --ww3 and --ww3-gpu (the resident preset refuses
# it, kokkos/src/fortran_iface/PATCH.md section 30) and is refused by --chaceon (exit 5), which
# has no unstructured propagation at its pin.
# --wind ifs fetches the newest ECMWF IFS cycle for the case's box and length with
# scripts/ifs_get.sh (`just wind`; what is already in data/ifs is not fetched again), links it as
# wind.nc, writes ww3_prnc_wind.nml when the case has none, and moves every date of the case's
# namelists so the run starts at the cycle. --wind <file.nc> takes a file instead: one named as
# ifs_get names it (ifs_<YYYYMMDDHH>_f<HHH>[_box_<W>_<E>_<S>_<N>].nc) retimes the case the same
# way, any other leaves the case's dates alone. So `--wind "$(just wind --print-path -b W,E,S,N)"` and `--wind ifs` are
# the same run. The case's ww3_shel.nml must say INPUT%FORCING%WINDS = 'T'.
# --temp cmems fetches the sea surface temperature of the Copernicus Marine global forecast for
# the same box and time span with scripts/cmems_get.sh (`just temp`; it needs a Copernicus Marine
# account) and links it as temperature.nc in the run directory; --temp <file.nc> links a file.
# WW3 does not read it: the air-sea temperature difference enters ST4 only under the STAB3 switch
# (model/src/w3src4md.F90:668 at 92e2cf66), which no lab switch has. It is staged for whoever
# reads the run directory.
# WW3GPU_IFS_DATE and WW3GPU_IFS_CYCLE choose the cycle of --wind ifs (latest, 00).
#
# --cpu is the default everywhere. --gpu exists for --ww3-gpu (Kokkos on CUDA) and --chaceon:
# WW3's Fortran and WW4 have no GPU path here, so --ww3 --gpu and --ww4 --gpu are refused (exit 64).
# Hard rule (AGENTS.md): --ww3-gpu --gpu runs only under a human's supervision. It asks for a
# confirmation typed at a terminal and refuses without one (exit 7); --build-only compiles for
# the GPU without running and needs none. --ww3-gpu --cpu is the agents' regtest: its Kokkos run is
# compared pair to pair with --ww3-noaa, pure NOAA WW3 at the fork's base; --ww3-gpu --gpu runs
# --ww3-noaa on the CPU too and ends with the three ww3_shel wall times (--ww3-noaa, the Kokkos
# run, the Fortran run of the same executable) and their ratio, one run each; --repeat N then runs
# --ww3-noaa and the Kokkos case N times each, alternated, checks every output against run 1's and
# prints the medians.
# --timers sets WW_KOKKOS_TIMERS=1 in the Kokkos run and prints where its time went: per part, the
# copies to the device, the kernel and the copies back, and the time outside Kokkos (never with
# --repeat: the timers fence the device, so their run is a breakdown, not a timing).
# Before it builds --ww3-noaa, the script checks that the fork differs from WW3-noaa/ only in the
# files of the KOKKOS patch (kokkos_patch below) and exits 3 when it does not.
#   add --build-only to build and stop before the run (a machine without the GPU can still compile)
#   scripts/ww3-gpu.sh --self-test
# WW3GPU_MPI_RANKS=<n> with an MPI switch (WW3GPU_SWITCH=switch_lab_mpi) runs ww3_shel under mpirun
# in the --ww3, --ww3-noaa, Fortran and Kokkos runs alike, CPU only (ADR-0020, Proposed). An MPI switch without
# it runs one rank and says so, so a singleton never reads as an MPI result. Ranks are not bound, and
# OMP_NUM_THREADS defaults to the cores per rank: Kokkos' OpenMP pool otherwise takes every core
# in every rank.
# WW3GPU_CASE_HOOK=<file> runs `bash <file> <case dir>` after each copy of the case inputs: a
# diagnostic variant of the grid or its namelists, in the Fortran run and the Kokkos run alike. It is
# never a regtest verdict.
#
# Exit codes: 0 pass; 1 a run or a comparison failed; 3 a submodule is missing, on the wrong
# branch, or the fork is more than its base (WW3-noaa/) plus the KOKKOS patch; 4 an NVIDIA GPU whose driver does not answer (--ww3-gpu --gpu); 5 a part the case needs is not
# ported to WW4 (--ww4), or chaceon cannot run what was asked (--chaceon --gpu, a triangular grid);
# 6 WW4 has every part but this repository has no WW4 input for the case;
# 7 --ww3-gpu --gpu without a supervising human; 64 a bad argument or combination.
#
# Cases:
#   smoke  regtest ww3_ts1 (one point, ST4 + NL1, 10 model days; seconds on a CPU), from the tree
#          the mode builds: the fork's ww3_grid_ST4_T475.nml (NK=36 NTH=24) for --ww3-gpu and
#          --ww3-noaa, MetOcean's own ww3_grid.nml for --ww3 and --chaceon (its tree has no T475,
#          and the fork's ww3_shel.nml asks for output fields the 2023 code does not have)
#   full   kokkos/tests/fixtures/bench_small (120 x 80, 9 520 sea points, NK=24 NTH=24, 12 hours)
#   coast  kokkos/tests/fixtures/coast (80 x 50 cells of 5 km, 3 791 sea and 50 boundary points,
#          NK=24 NTH=24, 12 hours): FLCK on, and hourly currents, water levels and ice
#          concentration through ww3_prnc and boundary spectra through ww3_bounc, all written by
#          scripts/coast_case.sh; the paths full and smoke never take (W3QCK2, W3UCUR, W3ULEV
#          with drying, W3UICE, W3UBPT). No --ww4 and no prebuilt image for it yet. --ww3 fails on
#          it: MetOcean's ww3_bounc stops with a segmentation fault at model/src/ww3_bounc.F90:556
#          on the fixture's boundary spectra (seen 2026-11-11, cause not found).
#
# --ww3-gpu runs the case twice with the same executable, every WW_KOKKOS_<R>=0 (the Fortran) and
# WW_KOKKOS_<R>=1 for each routine R of WW3GPU_ROUTINES (the Kokkos), and compares the gridded
# output with nccmp-tol: the L2 replay of kokkos/PORT_STATUS.md. WW3GPU_ROUTINES is a space- or
# comma-separated subset of SNL1 SDS4 SIN4 SPR4, SNL1 by default; ADR-0011's ST4 checkpoint turns
# one routine on at a time, then all. It fails when a judged field fails or the second run's log
# lacks the line WW3's W3INIT writes for a selected routine ("Kokkos DIA (W3SNL1) enabled.",
# "Kokkos ST4 (W3SDS4 W3SIN4 W3SPR4) enabled: SDS4=T ..."). --ww3 and --ww3-noaa run the case once
# and fail when ww3_shel does not reach "End of program".
#
# --chaceon builds chaceon's Rust programs with cargo (build/chaceon; cargo must be on PATH) and
# runs ww3_grid, ww3_strt, ww3_prnc and ww3_shel --threads N in sequence (N is every core,
# WW3GPU_CHACEON_THREADS sets it). Then it runs --ww3 on the same inputs, built with chaceon's own
# switch and -ffp-contract=off (chaceon/switches/switch_chaceon_shrd, as chaceon's reference
# image; build/ww3-chaceon-ref), and compares mod_def.ww3, the gridded and point output and the
# restarts byte for byte. chaceon's bit-for-bit claim is against its reference on linux/arm64, and
# its libm is that platform's: on aarch64 a file that differs fails the run, on any other machine
# the difference is counted and reported, not judged.
#
# --kokkos replaces WW3GPU_ROUTINES with the master flag of the fork (PATCH.md, "Run-time
# flags"): "all" runs the Kokkos case with WW_KOKKOS_ALL=1 (every part the build covers),
# "none" with WW_KOKKOS_ALL=0, and a comma list of parts (SNL1, GATH, ...) with
# WW_KOKKOS_ALL=0 and WW_KOKKOS_<PART>=1 for each. The Fortran run always has WW_KOKKOS_ALL=0,
# every WW_KOKKOS_<PART> is unset in both runs, and the Kokkos run sets every *_COUNT=1, so its
# ww3_shel.out says how many calls each part made. It fails when the log's "Kokkos parts" line
# (W3INIT) does not show a listed part as K.
#
# Every Kokkos run, with --kokkos or WW3GPU_ROUTINES, has WW_KOKKOS_VA_POISON=1: after each kernel
# write of the resident VA the stale host copy is filled with NaN, so a host reader that misses
# its sync changes an output file and the byte comparison fails (PATCH.md section 21). It fills
# only a copy no correct reader reads, so it cannot change the bits of a correct run; the line
# "VA host copies poisoned" of ww3_shel.out counts the fills. WW_KOKKOS_VA_POISON=0 in the
# caller's environment turns it off (a --repeat timing without the fills, for instance). The
# Fortran runs never have it.
#
# --kokkos gpu is the part list for a run on Kokkos' CUDA backend, SRCE alone (ADR-0013): phase 1
# copies one sea point per call, so W3SRCE routed whole makes one round trip per point and time
# step with the DIA and ST4 inside it, where SNL1, SDS4, SIN4 and SPR4 routed one by one make one
# each; GATH and SCAT are copies of the whole spectrum with no arithmetic to pay for the transfer,
# and the other support parts run once or once per input field. W3INIT builds the DIA and ST4
# tables for SRCE alone (WW3/model/src/w3initmd.F90:759,771). --kokkos resident adds the PR3/UQ
# propagation (XYP3, KTP3, opt-in, kokkos/src/fortran_iface/PATCH.md §11 and §13) and W3OUTG (OUTG,
# opt-in, §15), the partial read before the point output (IOPE, opt-in, §16) and W3UICE's zeroing
# of VA repeated on the device (UPDV, opt-in, §17) and W3CPRT's partitioning (CPRT, opt-in, §27):
# with SRCE on the resident VA, the spectrum then
# crosses only for the host readers of VA at output times, not for the gridded fields, the point
# output copies back only the spectra it interpolates, and an ice update copies none (WFIP-0005
# units 1 to 5). It is not a default: XYP3, KTP3, OUTG, IOPE, UPDV and CPRT are not regtested. --kokkos gpu is the default of --gpu when
# neither --kokkos nor WW3GPU_ROUTINES is given. The Fortran run of the pair is unchanged.
#
# --cpu is Kokkos' OpenMP backend, --gpu its CUDA backend. The script enters the pinned toolchain
# itself (picasso/labs/pratico: #ww3, or #cuda for --gpu).
# Builds go to build/<ww3|ww3-noaa|ww3-gpu-BACKEND|chaceon>/, runs to
# build/runs/<mode>-<case>[-BACKEND]/ ($WW3GPU_RUNS).
#
# WW3GPU_KOKKOS_PREFIX=<dir> (--ww3-gpu) builds WW3 against the ww_kokkos package installed in
# <dir>, for instance by an install-only configure (kokkos/README.md), in place of the one this
# script installs from kokkos/build/<backend>-release; that build still runs, for nccmp-tol.
#
# WW3GPU_PREBUILT=<prefix> runs programs built beforehand and builds nothing, with no Nix: the
# Docker image (docker/Dockerfile) sets it. <prefix> holds ww3/bin (--ww3-noaa) and
# ww3-gpu-openmp/bin (WW3's programs), bin/nccmp-tol, cases/ (ww3_ts1, bench_small, ww4/<case>),
# and ww4/ (ww4_standalone and the parts report of each case, recorded when the image was built).
# CPU only. The images do not hold MetOcean's WW3 or chaceon yet: there --ww3 and --chaceon exit
# 64 (a prefix with ww3-metocean/bin and cases/ww3_ts1-metocean would run --ww3).
#
# WW3 is built with its own Release flags in both modes. The two runs of --ww3-gpu agree to the
# tolerances of kokkos/tools/nccmp-tol/tolerances.txt; since 2026-10-15 they are byte-identical
# (docs/log/2026-10-14-snl1-bitwise-af11.md). On the CPU every model file (gridded and point
# output, restarts, NetCDF) must be byte-identical to the Fortran run's, and the binary ones to
# --ww3-noaa's, or the run fails; on a GPU the differences are reported (docs/log/2026-10-17-mutation-audit.md).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pratico="$root/picasso/labs/pratico"
pre="${WW3GPU_PREBUILT:-}"
tols=()

usage() { sed -n '2,/^set -euo/{/^set -euo/!p}' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 64; }

mode="" case_name="" device="" build_only="" kokkos="" repeat=1 timers="" case_dir="" wind="" temp=""
# The parts, their counters and the fork files their hunks touch, one file per wiring unit
# (kokkos/src/fortran_iface/parts.d/<unit>.txt, PATCH.md section 9), so a new unit adds a file and
# edits nothing here. Lines: "PART <name>", "COUNT <variable>", "FILE <path in the fork>"; # comments.
all_parts=() count_flags=() unit_files=()
parts_d="$root/kokkos/src/fortran_iface/parts.d"
parts_files=("$parts_d"/*.txt)
[ -f "${parts_files[0]}" ] || { echo "ww3-gpu: no $parts_d/*.txt" >&2; exit 3; }
for f in "${parts_files[@]}"; do
  while read -r key val rest; do
    case "$key" in
      ''|'#'*) ;;
      # Shapes checked so a data file cannot set an arbitrary variable (PART only ever becomes
      # WW_KOKKOS_<PART>, COUNT is exported as <COUNT>=1) or exempt anything but a Fortran file of
      # model/src from the fork check below.
      PART) [[ $val =~ ^[A-Z][A-Z0-9_]*$ ]] || { echo "ww3-gpu: $f: bad part name '$val'" >&2; exit 3; }
            [[ " ${all_parts[*]} " != *" $val "* ]] || { echo "ww3-gpu: $f: part $val is already in another parts.d file" >&2; exit 3; }
            all_parts+=("$val") ;;
      COUNT) [[ $val =~ ^WW_KOKKOS_[A-Z0-9_]+_COUNT$ ]] || { echo "ww3-gpu: $f: bad counter '$val'" >&2; exit 3; }
             count_flags+=("$val=1") ;;
      FILE) [[ $val =~ ^model/src/[a-z0-9_]+\.F90$ ]] || { echo "ww3-gpu: $f: bad fork file '$val'" >&2; exit 3; }
            unit_files+=("$val") ;;
      *) echo "ww3-gpu: $f: unknown line '$key $val $rest'" >&2; exit 3 ;;
    esac
  done < "$f"
done
while [ $# -gt 0 ]; do
  case "$1" in
    --ww3|--ww3-noaa|--ww3-gpu|--ww4|--chaceon) mode="${1#--}" ;;
    --cceon) mode="chaceon" ;;
    --hoff) mode="ww3-gpu" ;;  # the old name, as the dated records in docs/log and docs/data cite it
    smoke|full|coast) case_name="$1" ;;
    --case|--wind|--temp)
      [ $# -ge 2 ] && [ -n "$2" ] || { echo "ww3-gpu: $1 needs a value" >&2; exit 64; }
      case "$1" in --case) case_dir="$2" ;; --wind) wind="$2" ;; --temp) temp="$2" ;; esac
      shift ;;
    --cpu|--gpu) [ -z "$device" ] || [ "$device" = "${1#--}" ] || { echo "ww3-gpu: --cpu or --gpu, not both" >&2; exit 64; }
                 device="${1#--}" ;;
    --build-only) build_only=--build-only ;;
    --kokkos) [ $# -ge 2 ] || { echo "ww3-gpu: --kokkos needs all, none, gpu, resident or a list of parts" >&2; exit 64; }
              kokkos="$2"; shift ;;
    --kokkos=*) kokkos="${1#--kokkos=}"
                [ -n "$kokkos" ] || { echo "ww3-gpu: --kokkos needs all, none, gpu, resident or a list of parts" >&2; exit 64; } ;;
    --repeat) [ $# -ge 2 ] && [[ "$2" =~ ^[1-9][0-9]?$ ]] || { echo "ww3-gpu: --repeat needs a count, 1 to 99" >&2; exit 64; }
              repeat="$2"; shift ;;
    --timers) timers=1 ;;
    --self-test) mode=self-test ;;
    -h|--help) usage ;;
    *) echo "ww3-gpu: unknown argument: $1" >&2; usage ;;
  esac
  shift
done

# --- a case directory and its forcing (--case, --wind, --temp) ---------------------------------
# nml_get <file> <GROUP%NAME>: the value of the first assignment to it, comments and quotes dropped.
nml_get() {
  [ -f "$1" ] || return 0
  awk -v key="$2" '
    { line = $0; sub(/!.*/, "", line)
      n = index(toupper(line), toupper(key)); if (!n) next
      rest = substr(line, n + length(key)); if (rest !~ /^[ \t]*=/) next
      sub(/^[ \t]*=[ \t]*/, "", rest); sub(/[ \t,]+$/, "", rest)
      gsub(/^\047|\047$/, "", rest); print rest; exit }' "$1"
}
# nml_epoch <"YYYYMMDD HHMMSS">: seconds since 1970, UTC; nothing when it is not a date.
nml_epoch() {
  [[ "$1" =~ ^([0-9]{4})([0-9]{2})([0-9]{2})\ ([0-9]{2})([0-9]{2})([0-9]{2})$ ]] || return 0
  date -u -d "${BASH_REMATCH[1]}-${BASH_REMATCH[2]}-${BASH_REMATCH[3]} ${BASH_REMATCH[4]}:${BASH_REMATCH[5]}:${BASH_REMATCH[6]}" +%s
}
# case_box <case dir>: "W,E,S,N" in degrees around the grid, one degree wider on each side, for a
# regular grid (RECT_NML) or a triangular one (the nodes of its Gmsh file) in spherical
# coordinates; nothing for any other grid or one that leaves -180..180, whose forcing stays global.
case_box() {
  local g="$1/ww3_grid.nml" type coord msh
  type="$(nml_get "$g" GRID%TYPE)" coord="$(nml_get "$g" GRID%COORD)"
  [ "${coord^^}" = SPHE ] || return 0
  case "${type^^}" in
    RECT)
      awk -v nx="$(nml_get "$g" RECT%NX)" -v ny="$(nml_get "$g" RECT%NY)" \
          -v sx="$(nml_get "$g" RECT%SX)" -v sy="$(nml_get "$g" RECT%SY)" -v sf="$(nml_get "$g" RECT%SF)" \
          -v x0="$(nml_get "$g" RECT%X0)" -v y0="$(nml_get "$g" RECT%Y0)" -v sf0="$(nml_get "$g" RECT%SF0)" '
        BEGIN { if (sf == "") sf = 1; if (sf0 == "") sf0 = 1
                if (nx < 1 || ny < 1 || sx == "" || sy == "" || x0 == "" || y0 == "") exit
                w = x0 / sf0; s = y0 / sf0; e = w + (nx - 1) * sx / sf; n = s + (ny - 1) * sy / sf
                box(w, e, s, n) }
        function box(w, e, s, n) { w -= 1; e += 1; s -= 1; n += 1; if (s < -90) s = -90; if (n > 90) n = 90
                if (w < -180 || e > 180) exit; printf "%.4f,%.4f,%.4f,%.4f\n", w, e, s, n }' ;;
    UNST)
      msh="$1/$(nml_get "$g" UNST%FILENAME)"
      [ -f "$msh" ] || return 0
      # Gmsh 2 ASCII, what ww3_grid reads: "$Nodes", the count, then "id x y z" per node.
      awk '
        /^\$EndNodes/ { inn = 0 }
        inn == 1 { inn = 2; next }
        inn == 2 && NF >= 3 { x = $2 + 0; y = $3 + 0
                 if (!seen++) { w = e = x; s = n = y }
                 if (x < w) w = x; if (x > e) e = x; if (y < s) s = y; if (y > n) n = y }
        /^\$Nodes/ { inn = 1 }
        END { if (!seen) exit; w -= 1; e += 1; s -= 1; n += 1; if (s < -90) s = -90; if (n > 90) n = 90
              if (w < -180 || e > 180) exit; printf "%.4f,%.4f,%.4f,%.4f\n", w, e, s, n }' "$msh" ;;
  esac
}
# ifs_hours <hours>: the first IFS step at or after it (3 h apart to 144 h, 6 h apart after).
ifs_hours() { if [ "$1" -le 144 ]; then echo $(( ($1 + 2) / 3 * 3 )); else echo $(( ($1 + 5) / 6 * 6 )); fi; }
# shift_dates <seconds> <namelist>...: move every quoted 'YYYYMMDD HHMMSS' of each file by that
# much. Two passes through placeholders, so a date moved onto another date of the file is not moved twice.
shift_dates() {
  local delta="$1" f lit e i j
  local -a moved
  shift
  for f in "$@"; do
    [ -f "$f" ] || continue
    i=0 moved=()
    while IFS= read -r lit; do
      e="$(nml_epoch "$lit")"
      [ -n "$e" ] || continue
      moved[i]="$(date -u -d "@$((e + delta))" '+%Y%m%d %H%M%S')"
      sed -i "s/'$lit'/'@WW3GPU_DATE_$i@'/g" "$f"
      i=$((i + 1))
    done < <(grep -o "'[0-9]\{8\} [0-9]\{6\}'" "$f" | tr -d "'" | sort -u)
    for ((j = 0; j < i; j++)); do sed -i "s/'@WW3GPU_DATE_$j@'/'${moved[j]}'/g" "$f"; done
  done
}

# kokkos_env <0|1> <all|none|PART,...>: the run-time flags of the Fortran (0) or Kokkos (1) run.
kokkos_env() {
  local -a parts=()
  local p
  if [ "$1" = 0 ] || [ "$2" = none ]; then echo "WW_KOKKOS_ALL=0"; return; fi
  if [ "$2" = all ]; then echo "WW_KOKKOS_ALL=1"; return; fi
  IFS=', ' read -r -a parts <<<"$2"
  printf '%s' "WW_KOKKOS_ALL=0"
  for p in "${parts[@]}"; do printf ' %s' "WW_KOKKOS_$p=1"; done
  echo
}
# va_poison: the WW_KOKKOS_VA_POISON of the Kokkos run, 1 unless the caller set it.
va_poison() { echo "${WW_KOKKOS_VA_POISON:-1}"; }
# model_files <dir>: the files ww3_shel and ww3_ounf write in a run directory, the gridded and
# point output, the restarts and the NetCDF fields, one name per line. Each is compared byte for
# byte: a defect can reach the point output or a restart and leave the field file alone
# (docs/log/2026-10-17-mutation-audit.md).
model_files() {
  (cd "$1" && find . -maxdepth 1 -type f \( -name 'out_grd.*' -o -name 'out_pnt.*' -o -name 'restart*.ww3' \
     -o -name 'ww3.*nc' \) -printf '%f\n' | sort)
}
# cmp_files <reference dir> <dir> <what the reference is> [nc]: every model file of either dir, cmp
# against the other; the NetCDF files only with "nc" (--ww3's carry another switch attribute).
# Prints one line per file that differs or is missing, and returns 1 when there is one.
cmp_files() {
  local f bad=0
  while read -r f; do
    [ -n "$f" ] || continue
    [[ "$f" != ww3.*nc ]] || [ "${4:-}" = nc ] || continue
    if [ ! -f "$1/$f" ] || [ ! -f "$2/$f" ]; then echo ">> $f: written in one run only ($3)"; bad=1
    elif ! cmp -s "$1/$f" "$2/$f"; then echo ">> $f: differs from $3"; bad=1; fi
  done < <({ model_files "$1"; model_files "$2"; } | sort -u)
  return "$bad"
}
# The presets of WW_KOKKOS_PRESET, "PRESET <name> <PART,...>" lines (PATCH.md section 25): the shim
# compiles the same file, so --kokkos resident and WW_KOKKOS_PRESET=resident cannot drift.
presets_txt="$root/kokkos/src/fortran_iface/presets.txt"
kokkos_preset() { # <all|none|gpu|resident|PART,...>: a preset's part list, anything else unchanged
  local key name list
  case "$1" in
    gpu) echo SRCE; return ;;
    all|none|*,*) echo "$1"; return ;;
  esac
  [ -f "$presets_txt" ] || { echo "ww3-gpu: no $presets_txt" >&2; exit 3; }
  while read -r key name list; do
    if [ "$key" = PRESET ] && [ "$name" = "$1" ]; then
      [[ $list =~ ^[A-Z][A-Z0-9_]*(,[A-Z][A-Z0-9_]*)*$ ]] || { echo "ww3-gpu: $presets_txt: bad part list of $name" >&2; exit 3; }
      echo "$list"; return
    fi
  done < "$presets_txt"
  echo "$1"
}
if [ -n "$kokkos" ]; then
  [ "$mode" = ww3-gpu ] || [ "$mode" = self-test ] || { echo "ww3-gpu: --kokkos is for --ww3-gpu only" >&2; exit 64; }
  kokkos="$(kokkos_preset "$kokkos")"
  if [ "$kokkos" != all ] && [ "$kokkos" != none ]; then
    IFS=', ' read -r -a kparts <<<"$kokkos"
    [ "${#kparts[@]}" -gt 0 ] || { echo "ww3-gpu: --kokkos is empty" >&2; exit 64; }
    for p in "${kparts[@]}"; do
      [[ " ${all_parts[*]} " == *" $p "* ]] || { echo "ww3-gpu: --kokkos: '$p' is not all, none, gpu, resident or one of ${all_parts[*]}" >&2; exit 64; }
    done
  fi
fi
if [ -n "$timers" ]; then
  [ "$mode" = ww3-gpu ] || [ "$mode" = self-test ] || { echo "ww3-gpu: --timers is for --ww3-gpu only" >&2; exit 64; }
  # The timers fence the device around every phase: a run with them is a breakdown, never a median.
  [ "$repeat" = 1 ] || { echo "ww3-gpu: --timers and --repeat do not go together: time with --repeat, break down with --timers" >&2; exit 64; }
fi
# KOKKOS_NUM_THREADS sizes Kokkos' pool alone, so it would time the two programs on different thread
# counts; OMP_* settings reach both runs alike.
if [ "$mode" = ww3-gpu ] && [ -n "${KOKKOS_NUM_THREADS:-}" ]; then
  echo "ww3-gpu: KOKKOS_NUM_THREADS acts on the Kokkos run only; unset it (OMP_NUM_THREADS sets both)" >&2; exit 64
fi
if [ "$mode" = self-test ]; then
  unset WW3GPU_MPI_RANKS WW3GPU_SWITCH # the cases below set their own
  out="$("${BASH_SOURCE[0]}" --help 2>&1 || true)"
  if ! grep -q -- "--ww3" <<<"$out" || ! grep -q -- "--ww3-gpu" <<<"$out" || ! grep -q -- "--ww4" <<<"$out"; then echo "ww3-gpu: help text lost" >&2; exit 1; fi
  if "${BASH_SOURCE[0]}" --bogus >/dev/null 2>&1; then echo "ww3-gpu: accepted a bad argument" >&2; exit 1; fi
  for bad in "--ww3 smoke --gpu" "--ww4 smoke --gpu" "--ww3-gpu smoke --cpu --gpu"; do
    # shellcheck disable=SC2086
    "${BASH_SOURCE[0]}" $bad </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "ww3-gpu: '$bad' gave exit $rc, not 64" >&2; exit 1; }
  done
  KOKKOS_NUM_THREADS=16 "${BASH_SOURCE[0]}" --ww3-gpu smoke </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "ww3-gpu: KOKKOS_NUM_THREADS=16 gave exit $rc, not 64" >&2; exit 1; }
  WW3GPU_CASE_HOOK=/nonexistent/hook.sh "${BASH_SOURCE[0]}" --ww3-gpu smoke </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "ww3-gpu: a missing WW3GPU_CASE_HOOK gave exit $rc, not 64" >&2; exit 1; }
  WW3GPU_ROUTINES=SDS4,BOGUS "${BASH_SOURCE[0]}" --ww3-gpu smoke </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "ww3-gpu: WW3GPU_ROUTINES=SDS4,BOGUS gave exit $rc, not 64" >&2; exit 1; }
  for bad in "--ww3-gpu smoke --kokkos" "--ww3-gpu smoke --kokkos SNL1,BOGUS" "--ww3-gpu smoke --kokkos all,SNL1" \
             "--ww3-gpu smoke --kokkos=" "--ww3 smoke --kokkos all" "--ww3-gpu smoke --repeat 0" \
             "--ww3-gpu smoke --repeat x" "--ww3-gpu smoke --repeat" \
             "--ww3-gpu smoke --timers --repeat 3" "--ww3 smoke --timers"; do
    # shellcheck disable=SC2086
    "${BASH_SOURCE[0]}" $bad </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "ww3-gpu: '$bad' gave exit $rc, not 64" >&2; exit 1; }
  done
  # WW3GPU_MPI_RANKS: a bad or absurd count, no MPI in the switch, a device, WW4.
  for bad in "WW3GPU_MPI_RANKS=0 WW3GPU_SWITCH=switch_lab_mpi --ww3-gpu smoke" \
             "WW3GPU_MPI_RANKS=x WW3GPU_SWITCH=switch_lab_mpi --ww3-gpu smoke" \
             "WW3GPU_MPI_RANKS=18446744073709551615 WW3GPU_SWITCH=switch_lab_mpi --ww3-gpu smoke" \
             "WW3GPU_MPI_RANKS=4 --ww3-gpu smoke" \
             "WW3GPU_MPI_RANKS=4 WW3GPU_SWITCH=switch_lab_mpi --ww3-gpu smoke --gpu" \
             "WW3GPU_MPI_RANKS=4 WW3GPU_SWITCH=switch_lab_mpi --ww4 smoke"; do
    # shellcheck disable=SC2086
    env ${bad%%--*} "${BASH_SOURCE[0]}" --${bad#*--} </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "ww3-gpu: '$bad' gave exit $rc, not 64" >&2; exit 1; }
  done
  # The launcher of run_model, read as text, as below: every run of a pair takes the same one.
  # shellcheck disable=SC2016
  grep -qF '"${flags[@]}" "${launch[@]}" "$bin/ww3_shel"' "${BASH_SOURCE[0]}" \
    || { echo "ww3-gpu: run_model lost the mpirun launcher" >&2; exit 1; }
  for p in SNL1 SRCE GATH DISTAB FLX_TABLES; do
    [[ " ${all_parts[*]} " == *" $p "* ]] || { echo "ww3-gpu: parts.d lost part $p" >&2; exit 1; }
  done
  [[ " ${count_flags[*]} " == *" WW_KOKKOS_WAVE_COUNT=1 "* ]] || { echo "ww3-gpu: parts.d lost WW_KOKKOS_WAVE_COUNT" >&2; exit 1; }
  [ "$(kokkos_env 1 all)" = "WW_KOKKOS_ALL=1" ] || { echo "ww3-gpu: --kokkos all: $(kokkos_env 1 all)" >&2; exit 1; }
  [ "$(kokkos_env 1 none)" = "WW_KOKKOS_ALL=0" ] || { echo "ww3-gpu: --kokkos none: $(kokkos_env 1 none)" >&2; exit 1; }
  [ "$(kokkos_env 1 GATH,SNL1)" = "WW_KOKKOS_ALL=0 WW_KOKKOS_GATH=1 WW_KOKKOS_SNL1=1" ] \
    || { echo "ww3-gpu: --kokkos GATH,SNL1: $(kokkos_env 1 GATH,SNL1)" >&2; exit 1; }
  [ "$(kokkos_env 0 all)" = "WW_KOKKOS_ALL=0" ] || { echo "ww3-gpu: the Fortran run of --kokkos all: $(kokkos_env 0 all)" >&2; exit 1; }
  [ "$(kokkos_env 1 "$(kokkos_preset gpu)")" = "WW_KOKKOS_ALL=0 WW_KOKKOS_SRCE=1" ] \
    || { echo "ww3-gpu: --kokkos gpu: $(kokkos_env 1 "$(kokkos_preset gpu)")" >&2; exit 1; }
  [ "$(kokkos_env 1 "$(kokkos_preset resident)")" = "WW_KOKKOS_ALL=0 WW_KOKKOS_SRCE=1 WW_KOKKOS_XYP3=1 WW_KOKKOS_KTP3=1 WW_KOKKOS_OUTG=1 WW_KOKKOS_IOPE=1 WW_KOKKOS_UPDV=1 WW_KOKKOS_CPRT=1" ] \
    || { echo "ww3-gpu: --kokkos resident: $(kokkos_env 1 "$(kokkos_preset resident)")" >&2; exit 1; }
  [ "$(unset WW_KOKKOS_VA_POISON; va_poison)" = 1 ] || { echo "ww3-gpu: WW_KOKKOS_VA_POISON is not on by default" >&2; exit 1; }
  [ "$(WW_KOKKOS_VA_POISON=0 va_poison)" = 0 ] || { echo "ww3-gpu: WW_KOKKOS_VA_POISON=0 did not turn it off" >&2; exit 1; }
  # The two lines of run_model, read as text: run_model itself runs the model.
  # shellcheck disable=SC2016
  grep -qxF '  [ "$k" = 0 ] || flags+=("WW_KOKKOS_VA_POISON=$(va_poison)")' "${BASH_SOURCE[0]}" \
    || { echo "ww3-gpu: run_model lost the Kokkos run's WW_KOKKOS_VA_POISON" >&2; exit 1; }
  # shellcheck disable=SC2016
  grep -qxF '  [ "$k" = 1 ] || unset+=(-u WW_KOKKOS_STRICT -u WW_KOKKOS_VA_POISON)' "${BASH_SOURCE[0]}" \
    || { echo "ww3-gpu: run_model no longer unsets WW_KOKKOS_VA_POISON in the Fortran run" >&2; exit 1; }
  [ "$(kokkos_preset SNL1,GATH)" = "SNL1,GATH" ] || { echo "ww3-gpu: kokkos_preset changed a part list" >&2; exit 1; }
  # cmp_files: a point output or restart that differs, or exists on one side only, fails; the
  # NetCDF is compared only when asked; logs are never compared.
  t="$(mktemp -d)"
  mkdir "$t/a" "$t/b"
  for f in out_grd.ww3 out_pnt.ww3 restart001.ww3 ww3.field.nc log.ww3; do echo "$f" > "$t/a/$f"; cp "$t/a/$f" "$t/b/$f"; done
  echo other > "$t/b/log.ww3"
  cmp_files "$t/a" "$t/b" ref nc >/dev/null || { rm -rf -- "$t"; echo "ww3-gpu: cmp_files failed equal model files" >&2; exit 1; }
  echo other > "$t/b/out_pnt.ww3"
  if cmp_files "$t/a" "$t/b" ref >/dev/null; then rm -rf -- "$t"; echo "ww3-gpu: cmp_files passed a different out_pnt.ww3" >&2; exit 1; fi
  cp "$t/a/out_pnt.ww3" "$t/b/" && rm "$t/b/restart001.ww3"
  if cmp_files "$t/a" "$t/b" ref >/dev/null; then rm -rf -- "$t"; echo "ww3-gpu: cmp_files passed a missing restart" >&2; exit 1; fi
  cp "$t/a/restart001.ww3" "$t/b/" && echo other > "$t/b/ww3.field.nc"
  cmp_files "$t/a" "$t/b" ref >/dev/null || { rm -rf -- "$t"; echo "ww3-gpu: cmp_files compared a NetCDF without nc" >&2; exit 1; }
  if cmp_files "$t/a" "$t/b" ref nc >/dev/null; then rm -rf -- "$t"; echo "ww3-gpu: cmp_files passed a different NetCDF" >&2; exit 1; fi
  rm -rf -- "$t"
  # coast: no WW4 parts model for it, and every input the case reads is in the fixture or written
  # by scripts/coast_case.sh (whose own self-test needs ncgen, so it runs only where ncgen is).
  "${BASH_SOURCE[0]}" --ww4 coast </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "ww3-gpu: '--ww4 coast' gave exit $rc, not 64" >&2; exit 1; }
  for f in ww3_grid.nml namelists.nml ww3_shel.nml ww3_ounf.nml ww3_bounc.nml spec.list points.list \
           ww3_prnc_current.nml ww3_prnc_level.nml ww3_prnc_ice.nml; do
    [ -f "$root/kokkos/tests/fixtures/coast/$f" ] || { echo "ww3-gpu: the coast fixture lost $f" >&2; exit 1; }
  done
  grep -q "RUN%FLCK  = T" "$root/kokkos/tests/fixtures/coast/ww3_grid.nml" || { echo "ww3-gpu: the coast case lost FLCK" >&2; exit 1; }
  if command -v ncgen >/dev/null; then bash "$root/scripts/coast_case.sh" --self-test >/dev/null || exit 1; fi
  # Without a terminal --ww3-gpu --gpu never runs: 7 (no supervisor) or 4 (no driver), before any
  # build; with prebuilt programs (the Docker image, CPU only) it is refused outright, 64.
  "${BASH_SOURCE[0]}" --ww3-gpu smoke --gpu </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  ok="4|7"; [ -z "${WW3GPU_PREBUILT:-}" ] || ok="64"
  [[ "$rc" =~ ^($ok)$ ]] || { echo "ww3-gpu: --ww3-gpu --gpu without a terminal gave exit $rc" >&2; exit 1; }
  # The six runtimes' flags: --cceon is --chaceon, whose --gpu and triangular grids stop with 5
  # before anything is built; --ww3 never takes an OMPG switch; a case is named or a directory.
  grep -q -- "--chaceon" <<<"$out" && grep -q -- "--ww3-noaa" <<<"$out" || { echo "ww3-gpu: help text lost --chaceon or --ww3-noaa" >&2; exit 1; }
  t="$(mktemp -d)"
  mkdir "$t/rect" "$t/unst" "$t/smoke"
  printf "&GRID_NML\n  GRID%%TYPE = 'RECT'  ! regular\n  GRID%%COORD = 'SPHE'\n/\n&RECT_NML\n  RECT%%NX = 81\n  RECT%%NY = 81\n  RECT%%SX = 0.1\n  RECT%%SY = 0.1\n  RECT%%X0 = -52.0\n  RECT%%Y0 = -32.0\n/\n" > "$t/rect/ww3_grid.nml"
  printf "&GRID_NML\n  GRID%%TYPE = 'UNST'\n  GRID%%COORD = 'SPHE'\n/\n&UNST_NML\n  UNST%%FILENAME = './m.msh'\n/\n" > "$t/unst/ww3_grid.nml"
  printf '$MeshFormat\n2 0 8\n$EndMeshFormat\n$Nodes\n3\n1 -83.5 9.5 2.5\n2 -83.0 10.0 3.0\n3 -83.25 9.75 1.0\n$EndNodes\n$Elements\n1\n1 2 3 0 1 0 1 2 3\n$EndElements\n' > "$t/unst/m.msh"
  printf "&DOMAIN_NML\n  DOMAIN%%START = '20240701 000000'\n  DOMAIN%%STOP  = '20240702 000000'\n/\n&INPUT_NML\n  INPUT%%FORCING%%WINDS = 'T'\n/\n&OUTPUT_DATE_NML\n  DATE%%FIELD = '20240701 000000' '3600' '20240702 000000'\n/\n" > "$t/rect/ww3_shel.nml"
  cp "$t/rect/ww3_shel.nml" "$t/unst/"; cp "$t/rect/ww3_grid.nml" "$t/rect/ww3_shel.nml" "$t/smoke/"
  clean() { rm -rf -- "$t"; }
  [ "$(case_box "$t/rect")" = "-53.0000,-43.0000,-33.0000,-23.0000" ] || { echo "ww3-gpu: case_box of a regular grid: $(case_box "$t/rect")" >&2; clean; exit 1; }
  [ "$(case_box "$t/unst")" = "-84.5000,-82.0000,8.5000,11.0000" ] || { echo "ww3-gpu: case_box of a triangular grid: $(case_box "$t/unst")" >&2; clean; exit 1; }
  sed -i "s/'SPHE'/'CART'/" "$t/smoke/ww3_grid.nml"
  [ -z "$(case_box "$t/smoke")" ] || { echo "ww3-gpu: case_box gave a box to a Cartesian grid" >&2; clean; exit 1; }
  [ "$(nml_get "$t/rect/ww3_grid.nml" GRID%TYPE)" = RECT ] && [ "$(nml_get "$t/rect/ww3_shel.nml" DOMAIN%STOP)" = "20240702 000000" ] \
    || { echo "ww3-gpu: nml_get lost a value" >&2; clean; exit 1; }
  [ "$(nml_epoch "20240701 000000")" = 1719792000 ] && [ -z "$(nml_epoch "2024-07-01")" ] || { echo "ww3-gpu: nml_epoch" >&2; clean; exit 1; }
  [ "$(ifs_hours 1) $(ifs_hours 24) $(ifs_hours 144) $(ifs_hours 145)" = "3 24 144 150" ] || { echo "ww3-gpu: ifs_hours" >&2; clean; exit 1; }
  # One day later: the start becomes the old stop, and must not be moved a second time.
  shift_dates 86400 "$t/rect/ww3_shel.nml" "$t/rect/absent.nml"
  [ "$(nml_get "$t/rect/ww3_shel.nml" DOMAIN%START)|$(nml_get "$t/rect/ww3_shel.nml" DOMAIN%STOP)" = "20240702 000000|20240703 000000" ] \
    && grep -q "DATE%FIELD = '20240702 000000' '3600' '20240703 000000'" "$t/rect/ww3_shel.nml" \
    || { echo "ww3-gpu: shift_dates: $(cat "$t/rect/ww3_shel.nml")" >&2; clean; exit 1; }
  for bad in "--ww3 smoke --case $t/rect" "--ww3 smoke --wind ifs" "--ww3 --case $t/absent" "--ww3 --case $t/smoke" \
             "--ww4 --case $t/rect" "--ww3 --case $t/rect --wind /nonexistent.nc" "--ww3 --case $t/rect --wind cmems" \
             "--ww3 --case $t/rect --temp ifs" "--ww3 --case" "--ww3-noaa smoke --gpu"; do
    # shellcheck disable=SC2086
    "${BASH_SOURCE[0]}" $bad </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
    [ "$rc" = 64 ] || { echo "ww3-gpu: '$bad' gave exit $rc, not 64" >&2; clean; exit 1; }
  done
  WW3GPU_SWITCH=switch_lab_ompg "${BASH_SOURCE[0]}" --ww3 smoke </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
  [ "$rc" = 64 ] || { echo "ww3-gpu: --ww3 with an OMPG switch gave exit $rc, not 64" >&2; clean; exit 1; }
  if [ -z "${WW3GPU_PREBUILT:-}" ]; then
    for five in "--chaceon smoke --gpu" "--cceon full --gpu" "--chaceon --case $t/unst"; do
      # shellcheck disable=SC2086
      "${BASH_SOURCE[0]}" $five </dev/null >/dev/null 2>&1 && rc=0 || rc=$?
      [ "$rc" = 5 ] || { echo "ww3-gpu: '$five' gave exit $rc, not 5" >&2; clean; exit 1; }
    done
  fi
  clean
  echo "ww3-gpu self-test: ok"
  exit 0
fi
case_args=("$case_name")
if [ -n "$case_dir" ]; then
  [ -z "$case_name" ] || { echo "ww3-gpu: a named case ($case_name) or --case <dir>, not both" >&2; exit 64; }
  [ -f "$case_dir/ww3_grid.nml" ] && [ -f "$case_dir/ww3_shel.nml" ] \
    || { echo "ww3-gpu: --case needs a directory with ww3_grid.nml and ww3_shel.nml, not '$case_dir'" >&2; exit 64; }
  case_dir="$(cd "$case_dir" && pwd)"
  case_name="$(basename "$case_dir")"
  [[ "$case_name" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] && [[ ! "$case_name" =~ ^(smoke|full|coast)$ ]] \
    || { echo "ww3-gpu: --case: the directory's name is the case's name; '$case_name' is taken or has a character outside A-Z a-z 0-9 . _ -" >&2; exit 64; }
  [ "$mode" != ww4 ] || { echo "ww3-gpu: --ww4 takes smoke or full (scripts/ww4_parts.py knows no other case)" >&2; exit 64; }
  case_args=(--case "$case_dir")
  [ -z "$wind" ] || case_args+=(--wind "$wind")
  [ -z "$temp" ] || case_args+=(--temp "$temp")
elif [ -n "$wind$temp" ]; then
  echo "ww3-gpu: --wind and --temp force a case of yours: give it with --case <dir>" >&2; exit 64
fi
[ -n "$mode" ] && [ -n "$case_name" ] || usage
for f in "$wind" "$temp"; do
  [ -z "$f" ] || [ "$f" = ifs ] || [ "$f" = cmems ] || [ -f "$f" ] \
    || { echo "ww3-gpu: --wind takes ifs or a file, --temp cmems or a file; '$f' is neither" >&2; exit 64; }
done
[ "$wind" != cmems ] && [ "$temp" != ifs ] \
  || { echo "ww3-gpu: the wind comes from ifs and the temperature from cmems, not the reverse" >&2; exit 64; }
if [ "$case_name" = coast ] && [ "$mode" = ww4 ]; then
  echo "ww3-gpu: --ww4 has no coast case (scripts/ww4_parts.py knows smoke and full)" >&2; exit 64
fi
if [ "$case_name" = coast ] && [ -n "$pre" ]; then
  echo "ww3-gpu: the prebuilt image has no coast case yet; run it from the source tree" >&2; exit 64
fi
# A GPU run with no part list takes --kokkos gpu: the DIA alone, WW3GPU_ROUTINES' default, is the
# part with the most round trips per unit of work (ADR-0013).
if [ "$mode" = ww3-gpu ] && [ "$device" = gpu ] && [ -z "$kokkos" ] && [ -z "${WW3GPU_ROUTINES:-}" ]; then
  kokkos="$(kokkos_preset gpu)"
fi
all_routines=(SNL1 SDS4 SIN4 SPR4)
IFS=', ' read -r -a routines <<<"${WW3GPU_ROUTINES:-SNL1}"
[ "${#routines[@]}" -gt 0 ] || { echo "ww3-gpu: WW3GPU_ROUTINES is empty" >&2; exit 64; }
for r in "${routines[@]}"; do
  [[ " ${all_routines[*]} " == *" $r "* ]] || { echo "ww3-gpu: WW3GPU_ROUTINES: '$r' is not one of ${all_routines[*]}" >&2; exit 64; }
done

# gpu_doctor: a machine with an NVIDIA GPU whose driver does not answer is a fault to fix, not a
# reason to fall back to the CPU. The usual cause on Ubuntu: a new kernel was booted and the
# driver's module package for it was not installed (2026-10-14: kernel 7.0.0-38, the 595 modules
# installed for 7.0.0-31 only).
gpu_doctor() {
  local k pkg
  k="$(uname -r)"
  echo "ww3-gpu: this machine has an NVIDIA GPU, and nvidia-smi cannot reach its driver." >&2
  if ! find "/lib/modules/$k" -name 'nvidia.ko*' 2>/dev/null | grep -q .; then
    echo "      There is no nvidia kernel module for the running kernel, $k." >&2
    pkg="$(dpkg-query -W -f '${Package}\n' 'linux-modules-nvidia-*-generic-hwe-*' 2>/dev/null | head -1 || true)"
    if [ -n "$pkg" ]; then
      echo "      Fix: sudo apt install $pkg   (brings the module for $k), then sudo modprobe nvidia" >&2
    else
      echo "      Fix: install the NVIDIA module package for $k (ubuntu-drivers install), then sudo modprobe nvidia" >&2
    fi
  else
    echo "      The module for $k exists but is not loaded. Fix: sudo modprobe nvidia (or reboot)" >&2
  fi
  echo "      To run on the CPU instead: --cpu (just smoke-ww3-gpu)." >&2
}
has_nvidia_gpu() { lspci 2>/dev/null | grep -qiE 'VGA.*NVIDIA|3D controller.*NVIDIA'; }

device="${device:-cpu}"
if [ "$device" = gpu ] && [ "$mode" != ww3-gpu ] && [ "$mode" != chaceon ]; then
  echo "ww3-gpu: --$mode has no GPU path (--ww3-gpu runs Kokkos on CUDA, --chaceon its own kernels); use --cpu" >&2
  exit 64
fi
if [ "$mode" = chaceon ]; then
  [ -z "$pre" ] || { echo "ww3-gpu: the prebuilt programs ($pre) have no chaceon; run it from the source tree" >&2; exit 64; }
  # Checked before anything is built or asked. spirv_shel takes a CPU Vulkan implementation only
  # (chaceon/rust/tools/spirv_shel/src/main.rs:3 at the pin) and the PTX has never executed
  # (chaceon/README.md, "State"), so there is nothing to run on a device, supervised or not.
  if [ "$device" = gpu ]; then
    echo "ww3-gpu: --chaceon --gpu: chaceon at its pin has no run on a real GPU (its SPIR-V runs on a CPU Vulkan implementation only, its PTX has never executed); --chaceon --cpu runs" >&2
    exit 5
  fi
  if [ -n "$case_dir" ] && [ "$(nml_get "$case_dir/ww3_grid.nml" GRID%TYPE | tr '[:lower:]' '[:upper:]')" = UNST ]; then
    echo "ww3-gpu: --chaceon: $case_name has a triangular grid, and chaceon at its pin has no unstructured propagation (chaceon/README.md, \"Port status\"); --ww3 and --ww3-gpu run it" >&2
    exit 5
  fi
fi
if [ "$device" = gpu ]; then backend=cuda; else backend=openmp; fi
dev_name=""
if [ -n "${WW3GPU_CASE_HOOK:-}" ] && [ ! -f "$WW3GPU_CASE_HOOK" ]; then
  echo "ww3-gpu: WW3GPU_CASE_HOOK is not a file: $WW3GPU_CASE_HOOK" >&2; exit 64
fi
# MetOcean's WW3 does not compile with OMPG (the header), so --ww3 and --chaceon take the serial
# lab switch unless told otherwise.
default_switch=switch_lab_ompg
[ "$mode" != ww3 ] && [ "$mode" != chaceon ] || default_switch=switch_lab_shrd
lab_switch="${WW3GPU_SWITCH:-$default_switch}"
[[ "$lab_switch" =~ ^switch_lab_[a-z0-9]+$ ]] && [ -f "$root/switches/$lab_switch" ] \
  || { echo "ww3-gpu: WW3GPU_SWITCH is a switches/switch_lab_* file, not '$lab_switch'" >&2; exit 64; }
switch_path="$root/switches/$lab_switch"
# The reference of a --chaceon run (set by this script only): MetOcean's WW3 with chaceon's switch.
chaceon_ref="${WW3GPU_CHACEON_REF:-}"
if [ -n "$chaceon_ref" ]; then
  [ "$mode" = ww3 ] || { echo "ww3-gpu: WW3GPU_CHACEON_REF is for the --ww3 run of --chaceon" >&2; exit 64; }
  lab_switch=switch_chaceon_shrd switch_path="$root/chaceon/switches/switch_chaceon_shrd"
  [ -f "$switch_path" ] || { echo "ww3-gpu: chaceon/ is empty: git submodule update --init chaceon" >&2; exit 3; }
fi
if [ "$mode" = ww3 ] && grep -qw OMPG "$switch_path"; then
  echo "ww3-gpu: --ww3 is MetOcean's WW3, which does not compile with OMPG (model/src/w3iogomd.F90:2374 at 92e2cf66); $lab_switch has it. Use switch_lab_shrd or switch_lab_mpi" >&2
  exit 64
fi
if grep -qw OMPG "$switch_path"; then
  base_desc="CPU, OpenMP, ${OMP_NUM_THREADS:-$(nproc)} threads"
else
  base_desc="CPU, one thread"
fi
launch=()
if [ -n "${WW3GPU_MPI_RANKS:-}" ]; then
  [[ "$WW3GPU_MPI_RANKS" =~ ^[1-9][0-9]{0,5}$ ]] \
    || { echo "ww3-gpu: WW3GPU_MPI_RANKS is a positive integer, not '$WW3GPU_MPI_RANKS'" >&2; exit 64; }
  grep -qw MPI "$switch_path" \
    || { echo "ww3-gpu: WW3GPU_MPI_RANKS needs an MPI switch, and $lab_switch has none" >&2; exit 64; }
  [ "$mode" != ww4 ] && [ "$mode" != chaceon ] && [ "$device" = cpu ] && [ -z "$pre" ] \
    || { echo "ww3-gpu: WW3GPU_MPI_RANKS is for --ww3, --ww3-noaa and --ww3-gpu --cpu from source" >&2; exit 64; }
  launch=(mpirun -n "$WW3GPU_MPI_RANKS" --bind-to none)
  threads="$(( $(nproc) / WW3GPU_MPI_RANKS ))"
  export OMP_NUM_THREADS="${OMP_NUM_THREADS:-$(( threads > 0 ? threads : 1 ))}"
  base_desc="$base_desc, $WW3GPU_MPI_RANKS MPI ranks"
elif grep -qw MPI "$switch_path"; then
  echo "ww3-gpu: $lab_switch is an MPI switch and this run has one rank; WW3GPU_MPI_RANKS sets more" >&2
fi
if [ "$device" = gpu ]; then dev_name="$(nvidia-smi -L 2>/dev/null | head -1 || true)"; fi
if [ -n "$pre" ] && [ "$device" = gpu ] && [ ! -d "$pre/ww3-gpu-cuda/bin" ]; then
  echo "ww3-gpu: the prebuilt programs ($pre) are CPU only; the GPU image is docker/Dockerfile.cuda" >&2
  exit 64
fi

# --ww4: what WW4 has decides whether there is anything to build. Checked before the toolchain.
if [ "$mode" = ww4 ] && [ -n "$pre" ]; then
  cat "$pre/ww4/parts-$case_name.txt"
  rc="$(cat "$pre/ww4/parts-$case_name.rc")"
  [ "$rc" = 0 ] || exit "$rc"
elif [ "$mode" = ww4 ] && [ -z "${WW3GPU_IN_SHELL:-}" ] && [ -z "$build_only" ]; then
  python3 "$root/scripts/ww4_parts.py" "$case_name" || exit $?
fi

# Hard rule: --ww3-gpu --gpu runs only with a human watching. Asked once, before the toolchain.
if [ "$device" = gpu ] && [ -z "$build_only" ] && [ -z "${WW3GPU_IN_SHELL:-}" ]; then
  nvidia-smi -L >/dev/null 2>&1 || { gpu_doctor; exit 4; }
  if [ ! -t 0 ]; then
    echo "ww3-gpu: --ww3-gpu --gpu runs only under a human's supervision (AGENTS.md hard rule): run it at a terminal" >&2
    exit 7
  fi
  printf 'ww3-gpu: --ww3-gpu %s --gpu runs the Kokkos port on %s. Are you supervising this run? Type yes: ' \
    "$case_name" "$(nvidia-smi -L | head -1)" >&2
  read -r answer
  [ "$answer" = yes ] || { echo "ww3-gpu: not confirmed; nothing ran" >&2; exit 7; }
fi

# Enter the pinned toolchain once, then continue inside it (not with prebuilt programs).
if [ -z "${WW3GPU_IN_SHELL:-}" ] && [ -z "$pre" ]; then
  shell="ww3"; [ "$mode" = ww3-gpu ] && [ "$backend" = cuda ] && shell="cuda"
  exec env WW3GPU_IN_SHELL=1 \
    nix develop "$pratico#$shell" --command bash "${BASH_SOURCE[0]}" "--$mode" "${case_args[@]}" "--$device" $build_only \
    ${kokkos:+--kokkos "$kokkos"} --repeat "$repeat" ${timers:+--timers}
fi

runs="${WW3GPU_RUNS:-$root/build/runs}"
# ww3_ts1 comes from the tree the mode builds (the header, "Cases").
metocean_case=""; [ "$mode" != ww3 ] && [ "$mode" != chaceon ] || metocean_case=1
if [ -n "$pre" ]; then
  smoke_in="$pre/cases/ww3_ts1${metocean_case:+-metocean}/input" full_in="$pre/cases/bench_small" ww4_in="$pre/cases/ww4"
else
  smoke_in="$root/WW3/regtests/ww3_ts1/input" full_in="$root/kokkos/tests/fixtures/bench_small"
  [ -z "$metocean_case" ] || smoke_in="$root/WW3-metocean-7.14/regtests/ww3_ts1/input"
  coast_in="$root/kokkos/tests/fixtures/coast"
  ww4_in="$root/kokkos/tests/fixtures/ww4"
fi

# --- WW4 (--ww4) -----------------------------------------------------------------------------
if [ "$mode" = ww4 ] && [ -n "$pre" ]; then
  exe="$pre/ww4/ww4_standalone"
  [ -x "$exe" ] || { echo "ww3-gpu: this image has no ww4_standalone (WW4 did not build)" >&2; exit 1; }
  [ -d "$ww4_in/$case_name" ] || { echo "ww3-gpu: no WW4 input for --ww4 $case_name in $ww4_in" >&2; exit 6; }
  d="$runs/ww4-$case_name"
  rm -rf -- "$d" && mkdir -p "$d" && cp -r "$ww4_in/$case_name"/. "$d"/
  (cd "$d" && "$exe" > ww4_standalone.out 2>&1) || { tail -20 "$d/ww4_standalone.out" >&2; exit 1; }
  echo ">> --ww4 $case_name: ww4_standalone finished; output in $d"
  exit 0
fi
if [ "$mode" = ww4 ]; then
  ww4="$root/WW4"
  [ -d "$ww4/src/ww4_core" ] || { echo "ww3-gpu: WW4/ is empty: git submodule update --init WW4" >&2; exit 3; }
  # yaml-cpp and googletest are WW4's own submodules.
  [ -f "$ww4/externals/yaml-cpp/CMakeLists.txt" ] || git -C "$ww4" submodule update --init --depth 1 externals/yaml-cpp externals/googletest
  wb="$root/build/ww4"
  # yaml-cpp 0.8.0, which WW4 pins, asks for a CMake older than 3.5, which the toolchain's CMake 4
  # refuses; the policy floor lets it configure without a change to WW4's files.
  [ -f "$wb/Makefile" ] || [ -f "$wb/build.ninja" ] || cmake -S "$ww4" -B "$wb" -DCMAKE_BUILD_TYPE=Release \
    -DWW4_ENABLE_TESTING=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > "$root/build/ww4.configure.log" 2>&1 \
    || { tail -30 "$root/build/ww4.configure.log"; exit 1; }
  cmake --build "$wb" -j"${JOBS:-$(nproc)}" > "$root/build/ww4.build.log" 2>&1 || { grep -E -A8 "error" "$root/build/ww4.build.log" | head -60; exit 1; }
  # WW4's CMakeLists puts the program in its own exe/, which its .gitignore lists.
  exe="$ww4/exe/ww4_standalone"
  [ -x "$exe" ] || { echo "ww3-gpu: WW4 built, but $exe is not there" >&2; exit 1; }
  echo ">> WW4 ($(git -C "$ww4" rev-parse --short HEAD)) built: $exe"
  [ -z "$build_only" ] || exit 0
  # Reached only when scripts/ww4_parts.py found every part ported.
  if [ ! -d "$ww4_in/$case_name" ]; then
    echo "ww3-gpu: WW4 has every part --ww4 $case_name needs, but there is no WW4 input for it yet (kokkos/tests/fixtures/ww4/$case_name/)" >&2
    exit 6
  fi
  d="$runs/ww4-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  mkdir -p "$d" && cp -r "$ww4_in/$case_name"/. "$d"/
  (cd "$d" && "$exe" > ww4_standalone.out 2>&1) || { tail -20 "$d/ww4_standalone.out" >&2; exit 1; }
  echo ">> --ww4 $case_name: ww4_standalone finished; output in $d"
  exit 0
fi

if [ "$mode" = chaceon ]; then
  # --- chaceon (--chaceon): its Rust programs, built with cargo -------------------------------
  cz="$root/chaceon"
  [ -f "$cz/rust/Cargo.toml" ] || { echo "ww3-gpu: chaceon/ is empty: git submodule update --init chaceon" >&2; exit 3; }
  command -v cargo >/dev/null || { echo "ww3-gpu: --chaceon needs cargo on PATH (rustup.rs); chaceon's VERSIONS.md names the Rust it was built with" >&2; exit 3; }
  # chaceon is compared with --ww3, so the WW3 it was ported from (its own WW3 gitlink) must be
  # the commit WW3-metocean-7.14/ holds.
  cz_ref="$(git -C "$cz" ls-tree HEAD WW3 2>/dev/null | awk '{print $3}')"
  mo_ref="$(git -C "$root/WW3-metocean-7.14" rev-parse HEAD 2>/dev/null || true)"
  if [ -n "$cz_ref" ] && [ -n "$mo_ref" ] && [ "$cz_ref" != "$mo_ref" ]; then
    echo "ww3-gpu: chaceon's reference is WW3 ${cz_ref:0:8} and WW3-metocean-7.14/ is ${mo_ref:0:8}: move one pin to the other before comparing them" >&2; exit 3
  fi
  if [ "$case_name" = coast ]; then
    echo "ww3-gpu: --chaceon has no coast case: it needs ww3_bounc, which chaceon has not ported (chaceon/README.md, \"State\")" >&2; exit 5
  fi
  mkdir -p "$root/build/chaceon"
  # ww3_prnc reads NetCDF through the system libnetcdf, behind its feature (chaceon/VERSIONS.md),
  # so it is built only for a case that has forcing.
  pkgs=(ww3_grid ww3_strt ww3_shel)
  if [ -n "$wind" ] || { [ -n "$case_dir" ] && compgen -G "$case_dir/ww3_prnc_*.nml" >/dev/null; }; then
    pkgs+=("ww3_prnc --features netcdf")
  fi
  for pkg in "${pkgs[@]}"; do
    # shellcheck disable=SC2086
    CARGO_TARGET_DIR="$root/build/chaceon" cargo build --release --locked -q --manifest-path "$cz/rust/Cargo.toml" -p $pkg \
      > "$root/build/chaceon/build.log" 2>&1 \
      || { tail -30 "$root/build/chaceon/build.log" >&2; echo "ww3-gpu: chaceon's ${pkg%% *} did not build; build/chaceon/build.log" >&2; exit 1; }
  done
  cbin="$root/build/chaceon/release"
  echo ">> chaceon ($(git -C "$cz" rev-parse --short HEAD)) built in $cbin with $(cargo --version)"
  [ -z "$build_only" ] || exit 0
elif [ -n "$pre" ]; then
  case "$mode" in
    ww3) bin="$pre/ww3-metocean/bin"
         # The images of docker/ were last built with NOAA's WW3 alone.
         [ -x "$bin/ww3_shel" ] || { echo "ww3-gpu: the prebuilt programs ($pre) have no MetOcean WW3 for --ww3; --ww3-noaa runs NOAA's WW3, the fork's base" >&2; exit 64; } ;;
    ww3-noaa) bin="$pre/ww3/bin" ;;
    *) bin="$pre/ww3-gpu-$backend/bin" ;;
  esac
  nccmp="$pre/bin/nccmp-tol"
  # nccmp-tol's default tolerance file is a path in the source tree, which the image does not have.
  tols=("$pre/share/tolerances.txt")
  [ -x "$bin/ww3_shel" ] && [ -x "$nccmp" ] || { echo "ww3-gpu: $pre has no $bin/ww3_shel or bin/nccmp-tol" >&2; exit 3; }
  echo ">> WW3 --$mode prebuilt: $bin"
  [ -z "$build_only" ] || exit 0
else
  # NOAA's WW3 is not a submodule: WW3-noaa/ is the fork's base commit, written from WW3/'s history
  # by scripts/ww3_base.sh, and git ignores it.
  fork="$root/WW3" noaa="$root/WW3-noaa"
  case "$mode" in
    ww3) ww3="$root/WW3-metocean-7.14" ;;
    ww3-noaa) ww3="$noaa"
      [ -f "$fork/model/src/CMakeLists.txt" ] || { echo "ww3-gpu: WW3/ is empty: git submodule update --init WW3" >&2; exit 3; }
      bash "$root/scripts/ww3_base.sh" export >/dev/null ;;
    *) ww3="$fork" ;;
  esac
  sm="${ww3#"$root/"}"
  [ -f "$ww3/model/src/CMakeLists.txt" ] || { echo "ww3-gpu: $sm/ is empty: git submodule update --init $sm" >&2; exit 3; }
  # --ww3-noaa is the partner of --ww3-gpu, so the fork must be its base commit
  # (kokkos/src/fortran_iface/WW3_BASE) plus the KOKKOS patch and nothing else: a fork rebased onto
  # a newer NOAA commit, with WW3_BASE left behind, would compare two versions of WW3's physics.
  # Plus every wiring unit's own module (model/src/w3kokkos_<unit>md.F90, matched below) and the
  # fork files a unit's parent hook edits (its FILE lines in parts.d).
  kokkos_patch=(model/src/CMakeLists.txt model/src/cmake/switches.json model/src/w3initmd.F90
                model/src/w3kokkosmd.F90 model/src/w3srcemd.F90 model/src/w3wavemd.F90
                "${unit_files[@]}")
  if [ "$mode" = ww3-noaa ] && [ -f "$fork/model/src/w3kokkosmd.F90" ]; then
    # The trees, not the histories: CI clones the fork one commit deep. model/ holds every source
    # file WW3 compiles, and nothing a build or a regtest writes.
    # Relative paths, so the patterns below never contain the checkout's own path.
    extra="$(cd "$root" && diff -rq WW3-noaa/model WW3/model \
      | sed -E -e 's#^Files WW3-noaa/model/(.*) and WW3/model/.* differ$#\1#' \
               -e 's#^Only in (WW3-noaa|WW3)/model/?([^:]*): (.*)$#\2/\3#' -e 's#^/##' -e 's#^#model/#' \
      | grep -v -x -F -f <(printf '%s\n' "${kokkos_patch[@]}") \
      | grep -v -x -E 'model/src/w3kokkos_[a-z][a-z0-9_]*md\.F90' || true)"
    # diff -rq reports a directory present on one side as one line, so an exempted name must be a
    # regular file, or the directory's contents would pass unseen.
    for p in "${kokkos_patch[@]}" "$fork"/model/src/w3kokkos_*md.F90; do
      p="${p#"$fork"/}"
      [ ! -e "$fork/$p" ] || [ -f "$fork/$p" ] || extra+=$'\n'"$p (not a regular file)"
    done
    extra="${extra#$'\n'}"
    if [ -n "$extra" ]; then
      echo "ww3-gpu: the fork WW3/ differs from its base, NOAA WW3 $(bash "$root/scripts/ww3_base.sh" rev | cut -c1-8) in WW3-noaa/, outside the KOKKOS patch:" >&2
      printf '%s\n' "$extra" | sed 's/^/  /' >&2
      echo "      write the NOAA commit the fork branched from in kokkos/src/fortran_iface/WW3_BASE, or add a new patch file to kokkos_patch" >&2
      exit 3
    fi
  fi
  if [ "$mode" = ww3-gpu ] && [ ! -f "$ww3/model/src/w3kokkosmd.F90" ]; then
    echo "ww3-gpu: WW3/ is not the ww3-gpu/kokkos-pnt branch of h0ffmann/WW3 (no model/src/w3kokkosmd.F90):" >&2
    echo "      git submodule update --init WW3   (the pin; h0ffmann/WW3 ww3-gpu/kokkos-pnt follows it)" >&2
    exit 3
  fi
  jobs="${JOBS:-$(nproc)}"

  # A picasso bump changes the compilers and libraries a CMake cache has baked in, and a stale
  # cache links the new libraries against the old glibc. Each build directory keeps the toolchain
  # it was configured with and is rebuilt from scratch when that changes.
  toolchain="$(readlink -f "$(command -v gfortran)") $(readlink -f "$(command -v c++)") ${CMAKE_PREFIX_PATH:-}"
  fresh_for_toolchain() { # <dir under build/ or kokkos/build/>
    if [ -d "$1" ] && [ "$(cat "$1/toolchain" 2>/dev/null)" != "$toolchain" ]; then
      [[ "$1" == "$root/build/"* || "$1" == "$root/kokkos/build/"* ]] && rm -rf -- "$1"
    fi
    mkdir -p "$1"
    printf '%s\n' "$toolchain" > "$1/toolchain"
  }

  # --- the Kokkos package (--ww3-gpu), and nccmp-tol, which every --ww3-gpu verdict needs ----------
  preset="${backend}-release"
  kbuild="$root/kokkos/build/$preset"
  kprefix="$kbuild/install"
  if [ -n "${WW3GPU_KOKKOS_PREFIX:-}" ]; then
    kprefix="$(cd "$WW3GPU_KOKKOS_PREFIX" 2>/dev/null && pwd)" \
      || { echo "ww3-gpu: WW3GPU_KOKKOS_PREFIX=$WW3GPU_KOKKOS_PREFIX is not a directory" >&2; exit 64; }
  fi
  if [ "$mode" = ww3-gpu ]; then
    fresh_for_toolchain "$kbuild"
    [ -f "$kbuild/CMakeCache.txt" ] || cmake -S "$root/kokkos" --preset "$preset" >/dev/null
    # nvcc writes its errors to stdout: keep them in a log and show them, or the build fails silently.
    cmake --build "$kbuild" -j"$jobs" > "$kbuild/build.log" 2>&1 \
      || { grep -E -A8 "FAILED|error" "$kbuild/build.log" | head -60; echo "ww3-gpu: the Kokkos build ($preset) failed; $kbuild/build.log" >&2; exit 1; }
    cmake --install "$kbuild" --prefix "$kbuild/install" >/dev/null
    echo ">> ww_kokkos ($preset) installed in $kbuild/install"
    [ -z "${WW3GPU_KOKKOS_PREFIX:-}" ] || echo ">> WW3 links the ww_kokkos package in $kprefix (WW3GPU_KOKKOS_PREFIX)"
  fi

  # --- WW3 ------------------------------------------------------------------------------------
  case "$mode" in
    ww3) bdir="$root/build/ww3${chaceon_ref:+-chaceon-ref}" ;;
    ww3-noaa) bdir="$root/build/ww3-noaa" ;;
    *) bdir="$root/build/ww3-gpu-$backend" ;;
  esac
  fresh_for_toolchain "$bdir"
  switch_file="$bdir/switch"
  # build/ww3 was built from WW3-noaa/ before --ww3 moved to MetOcean's WW3: a new source needs a fresh configure.
  if [ "$(cat "$bdir/source" 2>/dev/null)" != "$ww3" ]; then
    [ -d "$bdir/ww3" ] && [[ "$bdir/ww3" == "$root/build/"* ]] && rm -rf -- "$bdir/ww3"
    printf '%s\n' "$ww3" > "$bdir/source"
  fi
  switch_now="$(cat "$switch_path")"
  [ "$mode" = ww3-gpu ] && switch_now="$switch_now KOKKOS"
  # A changed switch needs a fresh configure: WW3 bakes it into the preprocessed sources.
  if [ ! -f "$switch_file" ] || [ "$(cat "$switch_file")" != "$switch_now" ]; then
    printf '%s\n' "$switch_now" > "$switch_file"
    [ -d "$bdir/ww3" ] && [[ "$bdir/ww3" == "$root/build/"* ]] && rm -rf -- "$bdir/ww3"
  fi
  # Another ww_kokkos package needs a fresh configure: WW3's cache holds the ww_kokkos_DIR it found.
  if [ "$mode" = ww3-gpu ] && [ "$(cat "$bdir/kprefix" 2>/dev/null || echo "$kbuild/install")" != "$kprefix" ]; then
    [ -d "$bdir/ww3" ] && [[ "$bdir/ww3" == "$root/build/"* ]] && rm -rf -- "$bdir/ww3"
  fi
  [ "$mode" != ww3-gpu ] || printf '%s\n' "$kprefix" > "$bdir/kprefix"
  if [ ! -f "$bdir/ww3/CMakeCache.txt" ]; then
    args=(-S "$ww3" -B "$bdir/ww3" -DSWITCH="$switch_file" -DCMAKE_BUILD_TYPE=Release)
    if [ "$mode" = ww3-gpu ]; then
      args+=(-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH="$kprefix;${CMAKE_PREFIX_PATH:-}")
      # Kokkos' CUDA package wants the compiler it was built with.
      [ "$backend" = cuda ] && args+=(-DCMAKE_CXX_COMPILER=nvcc_wrapper)
    fi
    # chaceon's reference is built with contraction off (chaceon/docker/Dockerfile.reference).
    [ -z "$chaceon_ref" ] || args+=(-DCMAKE_Fortran_FLAGS=-ffp-contract=off)
    cmake "${args[@]}" > "$bdir/configure.log" 2>&1 || { tail -30 "$bdir/configure.log"; exit 1; }
  fi
  cmake --build "$bdir/ww3" -j"$jobs" > "$bdir/build.log" 2>&1 || { grep -E -A8 "Error|error:" "$bdir/build.log" | head -60; exit 1; }
  bin="$bdir/ww3/bin"
  nccmp="$kbuild/tools/nccmp-tol/nccmp-tol"
  echo ">> WW3 --$mode built in $bdir/ww3"
  [ -z "$build_only" ] || exit 0
fi

# --- the case ---------------------------------------------------------------------------------
# The forcing of a --case run, resolved once, so every run of a pair reads the same files:
# wind_nc and temp_nc are the files, retime the seconds every date of the case moves by.
wind_nc="" temp_nc="" retime=0
resolve_forcing() {
  local shel="$case_dir/ww3_shel.nml" start stop t0 t1 hours box name cyc span f
  [ -n "$wind$temp" ] || return 0
  start="$(nml_get "$shel" DOMAIN%START)" stop="$(nml_get "$shel" DOMAIN%STOP)"
  t0="$(nml_epoch "$start")" t1="$(nml_epoch "$stop")"
  [ -n "$t0" ] && [ -n "$t1" ] && [ "$t1" -gt "$t0" ] \
    || { echo "ww3-gpu: --wind and --temp need DOMAIN%START before DOMAIN%STOP in $shel" >&2; exit 64; }
  hours=$(( (t1 - t0 + 3599) / 3600 ))
  box="$(case_box "$case_dir")"
  if [ -n "$wind" ]; then
    [ "$(nml_get "$shel" INPUT%FORCING%WINDS)" = T ] \
      || { echo "ww3-gpu: --wind: $shel does not read a wind file; it needs INPUT%FORCING%WINDS = 'T'" >&2; exit 64; }
    if [ "$wind" = ifs ]; then
      [ -n "$box" ] || echo "ww3-gpu: --wind ifs: no box read from the grid of $case_name, so the wind is global" >&2
      f="$(cd "$root" && bash scripts/ifs_get.sh -d "${WW3GPU_IFS_DATE:-latest}" -c "${WW3GPU_IFS_CYCLE:-00}" \
             -H "$(ifs_hours "$hours")" ${box:+-b "$box"} --print-path)" || { echo "ww3-gpu: --wind ifs: scripts/ifs_get.sh failed" >&2; exit 1; }
      wind_nc="$(cd "$root" && readlink -f "$f")"
    else
      wind_nc="$(readlink -f "$wind")"
    fi
    name="$(basename "$wind_nc")"
    if [[ "$name" =~ ^ifs_([0-9]{8})([0-9]{2})_f([0-9]{3})(_box(_-?[0-9.]+){4})?\.nc$ ]]; then
      cyc="$(date -u -d "${BASH_REMATCH[1]} ${BASH_REMATCH[2]}:00:00" +%s)" span=$((10#${BASH_REMATCH[3]}))
      [ "$hours" -le "$span" ] \
        || { echo "ww3-gpu: --wind: $case_name runs $hours h and $name has $span h" >&2; exit 64; }
      retime=$((cyc - t0))
      echo ">> wind: $wind_nc; $case_name starts at its cycle, $(date -u -d "@$cyc" '+%Y-%m-%d %H:%M') UTC, for $hours h"
    else
      echo ">> wind: $wind_nc; the dates of $case_name stand"
    fi
  fi
  if [ -n "$temp" ]; then
    if [ "$temp" = cmems ]; then
      [ -n "$box" ] || { echo "ww3-gpu: --temp cmems: no box read from the grid of $case_name (a regular or triangular grid in spherical coordinates inside -180..180)" >&2; exit 64; }
      f="$(cd "$root" && bash scripts/cmems_get.sh -s "$(date -u -d "@$((t0 + retime))" +%Y%m%d%H)" -H "$hours" -b "$box" --print-path)" \
        || { echo "ww3-gpu: --temp cmems: scripts/cmems_get.sh failed" >&2; exit 1; }
      temp_nc="$(cd "$root" && readlink -f "$f")"
    else
      temp_nc="$(readlink -f "$temp")"
    fi
    echo ">> temperature: $temp_nc, linked as temperature.nc; WW3 does not read it (no STAB3 in $lab_switch)"
  fi
}
prepare() { # <dir>: copy the case's inputs into <dir> and make their paths local
  local d="$1"
  mkdir -p "$d"
  if [ -n "$case_dir" ]; then
    cp -r "$case_dir"/. "$d"/
    if [ -n "$wind_nc" ]; then
      ln -sf "$wind_nc" "$d/wind.nc"
      if [ -f "$d/ww3_prnc_wind.nml" ]; then
        sed -i "s#^\( *FILE%FILENAME *= *\).*#\1'wind.nc'#" "$d/ww3_prnc_wind.nml"
      else
        # The names scripts/ifs_get.sh writes. No FORCING%TIMESTART or TIMESTOP: every time of the
        # file, so the run's last hours lie between two wind fields and not after the last one.
        cat > "$d/ww3_prnc_wind.nml" <<NML
&FORCING_NML
  FORCING%FIELD%WINDS = T
  FORCING%GRID%LATLON = T
/
&FILE_NML
  FILE%FILENAME  = 'wind.nc'
  FILE%LONGITUDE = 'longitude'
  FILE%LATITUDE  = 'latitude'
  FILE%VAR(1)    = 'u10'
  FILE%VAR(2)    = 'v10'
/
NML
      fi
    fi
    [ -z "$temp_nc" ] || ln -sf "$temp_nc" "$d/temperature.nc"
    [ "$retime" = 0 ] || shift_dates "$retime" "$d"/ww3_shel.nml "$d"/ww3_ounf.nml "$d"/ww3_ounp.nml "$d"/ww3_prnc_*.nml
  elif [ "$case_name" = smoke ]; then
    cp "$smoke_in"/* "$d"/
    # The fork's tree: the setup docs/data's profile used. MetOcean's has its own ww3_grid.nml.
    [ -n "$metocean_case" ] || cp "$d/ww3_grid_ST4_T475.nml" "$d/ww3_grid.nml"
    sed -i 's#\.\./input/#./#g' "$d"/*.nml "$d"/*.inp
  elif [ "$case_name" = coast ]; then
    cp "$coast_in"/* "$d"/
    bash "$root/scripts/coast_case.sh" "$d" >/dev/null
  else
    cp "$full_in"/* "$d"/
    # The fixture writes no field (stride 0: it was made for timing); write HS every hour, so
    # ww3_ounf has a file to compare.
    sed -i "s#^  DATE%FIELD = .*#  DATE%FIELD = '20200101 000000' '3600' '20200101 120000'#" "$d/ww3_shel.nml"
  fi
  # A diagnostic variant of the inputs (a grid with NTH not a multiple of 4, a tail parameter): the
  # hook edits the copy in <dir>, in the Fortran run and the Kokkos run alike. Never a regtest verdict.
  if [ -n "${WW3GPU_CASE_HOOK:-}" ]; then bash "$WW3GPU_CASE_HOOK" "$d" || { echo "ww3-gpu: WW3GPU_CASE_HOOK failed" >&2; return 1; }; fi
}
run_model() { # <dir> <0|1>: grid, strt, shel, ounf; prints ww3_shel's wall time
  # Called inside $(...): every diagnostic goes to stderr, or it would vanish into the variable.
  # <1> sets WW_KOKKOS_<R>=1 for each routine of WW3GPU_ROUTINES; every other flag is 0.
  # With --kokkos, the master flag decides instead (kokkos_env).
  local d="$1" k="$2" t0 t1 r
  local -a flags=() unset=()
  for r in "${all_parts[@]}"; do unset+=(-u "WW_KOKKOS_$r"); done
  # No preset from the shell in either run (the Fortran run would turn its parts on); strict mode
  # reaches the Kokkos run only (PATCH.md sections 24 and 25), and so does the missed-sync poison.
  unset+=(-u WW_KOKKOS_PRESET)
  [ "$k" = 1 ] || unset+=(-u WW_KOKKOS_STRICT -u WW_KOKKOS_VA_POISON)
  if [ -n "$kokkos" ]; then
    read -r -a flags <<<"$(kokkos_env "$k" "$kokkos")"
    [ "$k" = 0 ] || flags+=("${count_flags[@]}")
  else
    flags+=(WW_KOKKOS_ALL=0)
    for r in "${all_routines[@]}"; do
      if [ "$k" = 1 ] && [[ " ${routines[*]} " == *" $r "* ]]; then flags+=("WW_KOKKOS_$r=1"); else flags+=("WW_KOKKOS_$r=0"); fi
    done
  fi
  [ "$k" = 0 ] || flags+=("WW_KOKKOS_VA_POISON=$(va_poison)")
  [ "$k" = 0 ] || [ -z "$timers" ] || flags+=(WW_KOKKOS_TIMERS=1)
  (cd "$d" && "$bin/ww3_grid" > ww3_grid.out 2>&1) || { tail -20 "$d/ww3_grid.out" >&2; return 1; }
  # The forcing and the boundary spectra (the coast case), made by the Fortran in every run, so
  # the two runs of a pair read the same files.
  local p
  for p in "$d"/ww3_prnc_*.nml; do
    [ -f "$p" ] || continue
    cp "$p" "$d/ww3_prnc.nml"
    (cd "$d" && env "${unset[@]}" WW_KOKKOS_ALL=0 "$bin/ww3_prnc" > "$(basename "$p" .nml).out" 2>&1) \
      || { tail -20 "$d/$(basename "$p" .nml).out" >&2; return 1; }
  done
  if [ -f "$d/ww3_bounc.nml" ]; then
    (cd "$d" && env "${unset[@]}" WW_KOKKOS_ALL=0 "$bin/ww3_bounc" > ww3_bounc.out 2>&1) || { tail -20 "$d/ww3_bounc.out" >&2; return 1; }
  fi
  if [ -f "$d/ww3_strt.inp" ] || [ -f "$d/ww3_strt.nml" ]; then
    (cd "$d" && "$bin/ww3_strt" > ww3_strt.out 2>&1) || { tail -20 "$d/ww3_strt.out" >&2; return 1; }
  fi
  t0="$(date +%s.%N)"
  (cd "$d" && env "${unset[@]}" OMP_PROC_BIND="${OMP_PROC_BIND:-false}" "${flags[@]}" "${launch[@]}" "$bin/ww3_shel" > ww3_shel.out 2>&1) \
    || { grep -h "ww_kokkos:\|Kokkos::" "$d/ww3_shel.out" >&2; tail -20 "$d/ww3_shel.out" >&2; return 1; }
  t1="$(date +%s.%N)"
  grep -q "End of program" "$d/ww3_shel.out" || { echo "ww3-gpu: ww3_shel did not finish in $d" >&2; return 1; }
  (cd "$d" && "$bin/ww3_ounf" > ww3_ounf.out 2>&1) || { echo "ww3-gpu: ww3_ounf failed in $d:" >&2; tail -20 "$d/ww3_ounf.out" >&2; return 1; }
  if [ -n "$case_dir" ] && [ -f "$d/ww3_ounp.nml" ]; then
    (cd "$d" && "$bin/ww3_ounp" > ww3_ounp.out 2>&1) || { echo "ww3-gpu: ww3_ounp failed in $d:" >&2; tail -20 "$d/ww3_ounp.out" >&2; return 1; }
  fi
  awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.2f\n", b - a }'
}

resolve_forcing
if [ "$mode" = ww3 ] || [ "$mode" = ww3-noaa ]; then
  d="$runs/$mode${chaceon_ref:+-chaceon-ref}-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  prepare "$d"
  wall="$(run_model "$d" 0)"
  echo "$wall" > "$d/ww3_shel.wall"
  # WW3-noaa/ is a tree without a repository: git there would answer with this checkout's commit.
  src_rev="$(git -C "${ww3:-$root}" rev-parse --short HEAD 2>/dev/null || echo prebuilt)"
  if [ "$mode" = ww3-noaa ] && [ -n "${ww3:-}" ]; then src_rev="$(bash "$root/scripts/ww3_base.sh" rev | cut -c1-7)"; fi
  echo ">> --$mode $case_name ($src_rev, $lab_switch): ww3_shel finished in ${wall} s; output: $(cd "$d" && ls ww3.*nc | tr '\n' ' ')"
  exit 0
fi

if [ "$mode" = chaceon ]; then
  d="$runs/chaceon-$case_name"
  [[ "$d" == "$runs/"* ]] && rm -rf -- "$d"
  prepare "$d"
  threads="${WW3GPU_CHACEON_THREADS:-$(nproc)}"
  [[ "$threads" =~ ^[1-9][0-9]{0,3}$ ]] || { echo "ww3-gpu: WW3GPU_CHACEON_THREADS is a positive integer, not '$threads'" >&2; exit 64; }
  step() { # <log> <program> [args]: one of chaceon's programs on the run directory
    local log="$1"; shift
    "$cbin/$1" "$d" "${@:2}" > "$d/$log" 2>&1 || { tail -12 "$d/$log" >&2; echo "ww3-gpu: chaceon's $1 stopped on $case_name; $d/$log" >&2; exit 1; }
  }
  step chaceon_grid.out ww3_grid
  if [ -f "$d/ww3_strt.inp" ] || [ -f "$d/ww3_strt.nml" ]; then step chaceon_strt.out ww3_strt; fi
  for p in "$d"/ww3_prnc_*.nml; do
    [ -f "$p" ] || continue
    cp "$p" "$d/ww3_prnc.nml"
    step "chaceon_$(basename "$p" .nml).out" ww3_prnc
  done
  t0="$(date +%s.%N)"
  step chaceon_shel.out ww3_shel --threads "$threads"
  w1="$(awk -v a="$t0" -v b="$(date +%s.%N)" 'BEGIN { printf "%.2f\n", b - a }')"
  echo ">> --chaceon $case_name: Rust's ww3_shel finished in ${w1} s on $threads threads; output in $d"
  # The reference: MetOcean's WW3 with chaceon's switch, on the same inputs and forcing files.
  ref_args=("${case_args[@]}")
  if [ -n "$case_dir" ]; then
    ref_args=(--case "$case_dir")
    [ -z "$wind_nc" ] || ref_args+=(--wind "$wind_nc")
    [ -z "$temp_nc" ] || ref_args+=(--temp "$temp_nc")
  fi
  WW3GPU_CHACEON_REF=1 bash "${BASH_SOURCE[0]}" --ww3 "${ref_args[@]}" --cpu
  ref="$runs/ww3-chaceon-ref-$case_name"
  bad=0
  for f in mod_def.ww3 $(model_files "$ref" | grep -v '^ww3\..*nc$'); do
    if [ ! -f "$d/$f" ]; then echo ">> $f: written by --ww3 only"; bad=1
    elif cmp -s "$ref/$f" "$d/$f"; then echo ">> $f: byte-identical to --ww3's, $(wc -c < "$d/$f") bytes"
    else echo ">> $f: differs from --ww3's in $(cmp -l "$ref/$f" "$d/$f" 2>/dev/null | wc -l) of $(wc -c < "$ref/$f") bytes"; bad=1; fi
  done
  w3="$(cat "$ref/ww3_shel.wall")"
  echo ">> ww3_shel wall time, $case_name, one run each: --ww3 ${w3} s (one thread); --chaceon ${w1} s ($threads threads). A check that the pieces work, not a benchmark"
  if [ "$bad" = 0 ]; then
    echo ">> --chaceon $case_name: every model file is byte-identical to --ww3's (MetOcean WW3, $lab_switch of chaceon)"
  elif [ "$(uname -m)" = aarch64 ]; then
    echo "ww3-gpu: chaceon's model files are not --ww3's" >&2; exit 1
  else
    echo ">> --chaceon $case_name: not judged on $(uname -m): chaceon's bit-for-bit claim is for linux/arm64, whose libm it reproduces (chaceon/docs/ADRs/ADR-0005-three-verdicts.md)"
  fi
  exit 0
fi

d0="$runs/ww3-gpu-$case_name-$backend/fortran"
d1="$runs/ww3-gpu-$case_name-$backend/kokkos"
[[ "$runs/ww3-gpu-$case_name-$backend" == "$runs/"* ]] && rm -rf -- "$runs/ww3-gpu-$case_name-$backend"
prepare "$d0"; prepare "$d1"
w0="$(run_model "$d0" 0)"
w1="$(run_model "$d1" 1)"
if [ -n "$timers" ]; then
  # kokkos/src/fortran_iface/timers.hpp: one line per region; .call is a whole entry point, so
  # the wall time minus the .call totals is what the run spent outside Kokkos (the Fortran).
  echo ">> time breakdown of the Kokkos run, $case_name ($backend), ww3_shel ${w1} s (WW_KOKKOS_TIMERS=1, fenced):"
  grep -h '^ww_kokkos: time ' "$d1/ww3_shel.out" | awk -v w="$w1" '
    { name = $3; sub(/:$/, "", name); calls = $4; s = $6 + 0
      printf ">>   %-22s %10d calls %10.3f s %6.1f %%\n", name, calls, s, (w > 0 ? 100 * s / w : 0)
      if (name ~ /\.call$/) in_kokkos += s }
    END { printf ">>   %-22s %21s %10.3f s %6.1f %%\n", "outside Kokkos", "", w - in_kokkos, (w > 0 ? 100 * (w - in_kokkos) / w : 0) }'
fi
# W3WAVE no longer reads VA back at its return (PATCH.md section 26): each host reader outside it
# syncs what it reads, and ww3_shel reads no VA after its last W3WAVE, so a run may end with the
# device copy newer. A host reader that misses its sync shows under WW_KOKKOS_VA_POISON=1, on here.
if grep -q "still newer on the device at finalize" "$d1/ww3_shel.out"; then
  echo ">> $(grep -h "still newer on the device at finalize" "$d1/ww3_shel.out" | head -1) (expected: no read at W3WAVE's return)"
fi
if [ -n "$kokkos" ]; then
  parts_line="$(grep -h "Kokkos parts" "$d1/log.ww3" | head -1 || true)"
  [ -n "$parts_line" ] || { echo "ww3-gpu: no 'Kokkos parts' line in $d1/log.ww3" >&2; exit 1; }
  echo ">> $parts_line"
  if [ "$kokkos" != all ] && [ "$kokkos" != none ]; then
    for p in "${kparts[@]}"; do
      [[ "$parts_line" == *" $p=K"* ]] || { echo "ww3-gpu: the Kokkos run did not put $p on Kokkos" >&2; exit 1; }
    done
  fi
  grep -h "^ww_kokkos: .*: [0-9]*$" "$d1/ww3_shel.out" | sed 's/^/>> /' || true
  # The case's expected calls per part (kokkos/tests/fixtures/<case>/counts_resident.txt): a part
  # that silently falls back to the Fortran writes the Fortran's bits, so only its count shows it.
  counts="$root/kokkos/tests/fixtures/$case_name/counts_resident.txt"
  if [ "$kokkos" = "$(kokkos_preset resident)" ] && [ -f "$counts" ] && [ -z "$pre" ] \
     && [ -z "${WW_KOKKOS_SRCE_BATCH_MIB:-}${WW_KOKKOS_PROP3_BATCH_MIB:-}" ]; then
    miss=0
    while IFS= read -r line; do
      case "$line" in ''|'#'*) continue ;; esac
      grep -qxF "ww_kokkos: $line" "$d1/ww3_shel.out" || { echo "ww3-gpu: expected 'ww_kokkos: $line' (${counts#"$root/"})" >&2; miss=1; }
    done < "$counts"
    [ "$miss" = 0 ] || { echo "ww3-gpu: the Kokkos run's calls per part are not the case's" >&2; exit 1; }
    echo ">> the calls per part match ${counts#"$root/"}"
  fi
  kname="Kokkos --kokkos $kokkos"
else
  for r in "${routines[@]}"; do
    if [ "$r" = SNL1 ]; then re="Kokkos DIA \(W3SNL1\) enabled"; else re="Kokkos ST4 \(W3SDS4 W3SIN4 W3SPR4\) enabled:.* $r=T"; fi
    grep -qE "$re" "$d1/log.ww3" \
      || { echo "ww3-gpu: the Kokkos run did not enable W3$r (no '$re' in $d1/log.ww3)" >&2; exit 1; }
  done
  kname="Kokkos ${routines[*]}"
fi
nc0="$(cd "$d0" && find . -maxdepth 1 -name 'ww3.*nc' -printf '%f\n' | sort | head -1)"
echo ">> --ww3-gpu $case_name ($backend): ww3_shel ${w0} s with the Fortran, ${w1} s with $kname"
if cmp -s "$d0/$nc0" "$d1/$nc0"; then echo ">> the two outputs are byte-identical"; fi
echo ">> $kname against the Fortran, same executable:"
"$nccmp" "$d0/$nc0" "$d1/$nc0" "${tols[@]}"
# Bit for bit (AGENTS.md): on the CPU every model file must be the Fortran's, not only the field
# file nccmp-tol judges within its tolerances. On a GPU the differences are reported, not judged.
if cmp_files "$d0" "$d1" "the Fortran of the same executable" nc; then
  echo ">> every model file is byte-identical to the Fortran's: $(model_files "$d1" | tr '\n' ' ')"
elif [ "$backend" != cuda ]; then
  echo "ww3-gpu: the Kokkos run's model files are not the Fortran's" >&2; exit 1
fi
# Pair to pair with --ww3-noaa, pure NOAA WW3 at the fork's base (WW3-noaa/), without the KOKKOS
# switch, on the CPU: the agents' regtest for --cpu, and for --gpu the reference whose wall time
# the GPU run is set against. A GPU image built before it carried the reference has no $pre/ww3.
if [ -n "$pre" ] && [ ! -x "$pre/ww3/bin/ww3_shel" ]; then
  echo ">> no --ww3-noaa programs in $pre: rebuild the image (just dbuild-cuda) to compare with pure WW3"
else
  # The partner reads the forcing files this run resolved, never a newer cycle.
  ref_args=("${case_args[@]}")
  if [ -n "$case_dir" ]; then
    ref_args=(--case "$case_dir")
    [ -z "$wind_nc" ] || ref_args+=(--wind "$wind_nc")
    [ -z "$temp_nc" ] || ref_args+=(--temp "$temp_nc")
  fi
  bash "${BASH_SOURCE[0]}" --ww3-noaa "${ref_args[@]}" --cpu
  ref="$runs/ww3-noaa-$case_name/$nc0"
  [ -f "$ref" ] || { echo "ww3-gpu: the --ww3-noaa run wrote no $nc0" >&2; exit 1; }
  echo ">> $kname against --ww3-noaa (pure NOAA WW3, WW3-noaa/), pair to pair:"
  "$nccmp" "$ref" "$d1/$nc0" "${tols[@]}"
  if cmp_files "$runs/ww3-noaa-$case_name" "$d1" "--ww3-noaa"; then
    echo ">> every model file but the NetCDF is byte-identical to --ww3-noaa's"
  elif [ "$backend" != cuda ]; then
    echo "ww3-gpu: the Kokkos run's model files are not --ww3-noaa's" >&2; exit 1
  fi
  # One run each, so a check that the pieces work, not a benchmark: a timing for a table is the
  # median of three runs after this parity passes (bench/README.md).
  w3="$(cat "$runs/ww3-noaa-$case_name/ww3_shel.wall")"
  echo ">> ww3_shel wall time, $case_name, one run each: --ww3-noaa ${w3} s ($base_desc); --ww3-gpu ($backend) ${w1} s with $kname, ${w0} s with the Fortran"
  awk -v a="$w3" -v b="$w1" -v c="$backend" -v t="$base_desc" 'BEGIN { if (b > 0) printf ">> --ww3-noaa (%s) / --ww3-gpu (%s): %.2fx (above 1: the Kokkos run is faster; NOAA runs WW3 on many cores under MPI)\n", t, c, a / b }'
  # --repeat N: after the parity above, N runs of each, alternated so a drift of the machine hits
  # both, every output byte-identical to run 1's, and the medians (bench/README.md: a timing for a
  # table is the median of three runs after a passed regtest replay on the same commit).
  if [ "$repeat" -gt 1 ]; then
    cp "$ref" "$runs/ww3-gpu-$case_name-$backend/ww3-ref.nc"
    w1s=("$w1") w3s=("$w3")
    load0="$(cut -d' ' -f1 /proc/loadavg)"
    for i in $(seq 2 "$repeat"); do
      bash "${BASH_SOURCE[0]}" --ww3-noaa "${ref_args[@]}" --cpu >/dev/null
      cmp -s "$runs/ww3-gpu-$case_name-$backend/ww3-ref.nc" "$ref" \
        || { echo "ww3-gpu: --ww3-noaa run $i wrote a different $nc0 from run 1" >&2; exit 1; }
      w3s+=("$(cat "$runs/ww3-noaa-$case_name/ww3_shel.wall")")
      di="$runs/ww3-gpu-$case_name-$backend/kokkos-$i"
      prepare "$di"
      w1s+=("$(run_model "$di" 1)")
      cmp -s "$d1/$nc0" "$di/$nc0" || { echo "ww3-gpu: Kokkos run $i wrote a different $nc0 from run 1" >&2; exit 1; }
    done
    median() { printf '%s\n' "$@" | sort -g | awk '{ a[NR] = $1 } END { if (NR % 2) print a[(NR + 1) / 2]; else printf "%.2f\n", (a[NR / 2] + a[NR / 2 + 1]) / 2 }'; }
    m3="$(median "${w3s[@]}")" m1="$(median "${w1s[@]}")"
    load1="$(cut -d' ' -f1 /proc/loadavg)"
    # Unset is libgomp's default; run_model makes OMP_PROC_BIND false when it is unset.
    omp_env="OMP_PROC_BIND=${OMP_PROC_BIND:-false} OMP_PLACES=${OMP_PLACES:-unset} OMP_WAIT_POLICY=${OMP_WAIT_POLICY:-unset} GOMP_SPINCOUNT=${GOMP_SPINCOUNT:-unset}"
    echo ">> $repeat runs each, every output byte-identical to run 1's: --ww3-noaa ${w3s[*]} s, median ${m3} s; --ww3-gpu ($backend, $kname) ${w1s[*]} s, median ${m1} s"
    awk -v a="$m3" -v b="$m1" -v c="$backend" -v t="$base_desc" 'BEGIN { if (b > 0) printf ">> median --ww3-noaa (%s) / median --ww3-gpu (%s): %.2fx (above 1: the Kokkos run is faster; NOAA runs WW3 on many cores under MPI)\n", t, c, a / b }'
    cpu_name="$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')"
    echo ">> machine: $cpu_name, $(nproc) threads${dev_name:+; $dev_name}; $lab_switch, OMP_NUM_THREADS=${OMP_NUM_THREADS:-unset, all threads}, $omp_env; load average (1 min) $load0 before the timed runs, $load1 after"
    # Written only here, after the parity above and the per-run output checks passed:
    # `python3 scripts/results.py record <file>` turns it into a bench/results/ record.
    perf="$runs/ww3-gpu-$case_name-$backend/perf.tsv"
    tool="$(gfortran --version 2>/dev/null | head -1 || true)"
    [ "$backend" != cuda ] || tool="$tool; $(nvcc --version 2>/dev/null | tail -1 || true)"
    kver="$(grep -o 'kokkos-[a-z]*-[0-9][0-9.]*[0-9]' "${kbuild:-/dev/null}/CMakeCache.txt" 2>/dev/null | head -1 || true)"
    # The model only: nvidia-smi -L also prints the card's UUID, a fingerprint a record must not carry.
    gpu_model="" driver=""
    if [ "$backend" = cuda ]; then
      gpu_model="$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)"
      driver="$(nvidia-smi --query-gpu=driver_version --format=csv,noheader | head -1)"
    fi
    dirty=""; [ -z "$(git -C "$root" status --porcelain 2>/dev/null)" ] || dirty="yes"
    # The image has no git: its BUILD_INFO names the commits it was built from.
    info="${pre:+$(dirname "$pre")/BUILD_INFO}"
    c_gpu="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || sed -n 's/^ww3-gpu //p' "${info:-/dev/null}" 2>/dev/null || true)"
    c_ww3="$(git -C "${ww3:-$root/WW3}" rev-parse --short HEAD 2>/dev/null || sed -n 's/^WW3 \([^ ]*\).*/\1/p' "${info:-/dev/null}" 2>/dev/null || true)"
    {
      printf 'case\t%s\nbackend\t%s\nparts\t%s\nrepeat\t%s\n' "$case_name" "$backend" "$kname" "$repeat"
      printf 'ww3_s\t%s\nww3gpu_s\t%s\nww3_median_s\t%s\nww3gpu_median_s\t%s\n' "${w3s[*]}" "${w1s[*]}" "$m3" "$m1"
      printf 'baseline\t%s\nswitch\t%s\nomp_num_threads\t%s\n' "$base_desc" "$lab_switch" "${OMP_NUM_THREADS:-}"
      printf 'omp_env\t%s\nload_1min\t%s %s\n' "$omp_env" "$load0" "$load1"
      printf 'cpu\t%s\nthreads\t%s\ngpu\t%s\ndriver\t%s\n' "$cpu_name" "$(nproc)" "$gpu_model" "$driver"
      printf 'toolchain\t%s\nkokkos\t%s\n' "$tool" "$kver"
      printf 'commit\t%s\nww3_commit\t%s\ndirty\t%s\ndate\t%s\n' "$c_gpu" "$c_ww3" "$dirty" "$(date -u +%F)"
      printf 'command\t%s\n' "${WW3GPU_MPI_RANKS:+WW3GPU_MPI_RANKS=$WW3GPU_MPI_RANKS }${WW3GPU_SWITCH:+WW3GPU_SWITCH=$WW3GPU_SWITCH }bash scripts/ww3-gpu.sh --ww3-gpu $case_name --$device${kokkos:+ --kokkos $kokkos} --repeat $repeat"
    } > "$perf"
    echo ">> perf summary: $perf (python3 scripts/results.py record $perf writes the bench/results/ record)"
  fi
fi
