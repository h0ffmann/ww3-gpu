# What "WW3 7.14" means for ondaseventos, and what metocean/WW3 is

2026-10-29, agent session `ef7f3ca1` for the owner; no issue. Goal branch
`goal/ef7f3ca1-4821-metocean-ww3`; the fork rebase it leaves is
[plan 2026-10-29-001](../plans/2026-10-29-001-metocean-upstream-plan.md).

## Question

Which WW3 does LabECO's operational system (ondaseventos, `labeco/ondaseventos-forecast` on
`codigos.ufsc.br`) build? And what changes if this repository's upstream moves from NOAA-EMC/WW3
to `metocean/WW3`, which the owner named as the 7.14 that ondaseventos uses?

## What was done

- Tried to read `https://codigos.ufsc.br/labeco/ondaseventos-forecast` from the owner's
  workstation (Apple M1 Pro, macOS 26.5): HTTPS redirects to `/users/sign_in`, the GitLab API
  answers `404 Project Not Found` (`curl https://codigos.ufsc.br/api/v4/projects/labeco%2Fondaseventos-forecast`),
  and SSH to port 22 times out. No GitLab token on the machine. The repository was **not read** ⚠.
- Read `metocean/WW3` with `gh api` (refs, `VERSION`, compares) and NOAA-EMC/WW3 for the same
  SHAs. Fetched `manual/` and two source files of both pins, sparse and shallow, into a scratch
  directory, outside this repository.
- Repointed `docker/Dockerfile.ondaseventos` at `metocean/WW3` `92e2cf66` and built it:
  `docker build -f docker/Dockerfile.ondaseventos -t ww3-ondaseventos:mos-92e2cf6 .`.

## Result

- `metocean/WW3` is not a GitHub fork (`gh api repos/metocean/WW3 --jq .parent` is null) (v). Its
  refs, read 2026-10-29 (v):

  | Ref | Commit | What it is |
  |---|---|---|
  | tag `7.14` | `3eb8161` (2023-06-08) | a NOAA-EMC/WW3 develop commit, nothing more (`gh api repos/NOAA-EMC/WW3/commits/3eb8161` resolves) |
  | `develop` | `b546e5a` (2026-02-27) | NOAA develop up to `57f0705` plus one MOS commit and its revert; NOAA's `03adda91` is 18 commits ahead of `57f0705` |
  | `mos-custom`, tag `7.14.mos-92e2cf6` | `92e2cf66` (2026-09-01) | tag `7.14` plus ten MetOcean commits (`gh api repos/metocean/WW3/compare/7.14...mos-custom`) |
  | tag `7.14.mos-cfcd089` | `6be63b1` (2026-08-19) | the ninth of those ten |

- `VERSION` reads `7.14` on every one of them and at NOAA's `03adda91` (v). "7.14" names no
  single source; a commit does.
- The ten MOS commits change the arithmetic of ST4, the physics this repository is porting
  (`W3SIN4` and `W3SRCE`), at `92e2cf66` (v):
  - `model/src/w3src4md.F90:680` `UST=MAX(USTAR,0.0001)` (NOAA `03adda91` `:695` `UST=USTAR`);
  - `:780` `IF (ISNAN(TEMP2)) TEMP2=0.` and `:835` `IF (ISNAN(TEMP)) TEMP = 0.` in `W3SIN4`;
  - `:1683` `UST=SQRT(MAX(TAUW,0.0001))` in `TABU_TAUHF2` (NOAA `:1787` `MAX(TAUW,0.)`);
  - `model/src/w3srcemd.F90:1048`, `:1071` `USTAR=0.001` at `IT=0`, and `:1178`, `:1891`
    `USTAR = MAX ( 0.001 , USTAR )` before `FFXPM / USTAR`.

  They also add `ww3_prnc_sea` (built only with `SMC`) and `ww3_swanbnd`, and change 15 other
  files of `model/src`, I/O and grid set-up among them (`w3gridmd.F90` +339 −47, `w3iogomd.F90`
  +356 −30). MetOcean's own `model/bin/switch_ST4` is
  `F90 NOGRB SEED NC4 DIST MPI PR3 UQ FLX0 ST4 STAB0 NL1 BT4 DB1 MLIM … WCOR RWND … O7 07a` (v).
- The image builds: 20 `ww3_*` programs including `ww3_swanbnd`, `BUILD_INFO` names
  `https://github.com/metocean/WW3 92e2cf66…`, on arm64 under Docker Desktop (v, image
  `sha256:4fd4794b5ca7`). It was not run on a case ⚠, and its switch file is this repository's
  guess, not ondaseventos' ⚠.
- This repository's WW3 (`WW3/`, the h0ffmann/WW3 fork) is 47 commits on NOAA `03adda91`, every
  one additive, in 16 files, `w3srcemd.F90` (+456) and `w3initmd.F90` (+119) among them
  (`gh api repos/h0ffmann/WW3/compare/03adda91...96892c95`) (v). MOS edits both files.
- NOAA-EMC/WW3 `03adda91` is 150 commits ahead of `3eb8161`, MetOcean's base (v). The routines
  already ported changed in that span: `w3snl1md.F90` differs in 1 379 diff lines between the two
  bases and `w3src4md.F90` in 768 (`diff` of the two files, `<`/`>` lines counted) (v).

## What it changes

- `docker/Dockerfile.ondaseventos` now builds `metocean/WW3` `7.14.mos-92e2cf6`; `VERSIONS.md`
  records it with the ⚠ that ondaseventos' own build was not read.
- The `WW3-noaa/` submodule, the fork's base and every `file:line` at the pin stay on NOAA-EMC/WW3
  for now (the owner, 2026-10-29: "metadata now, fork later"). Moving them is
  [plan 2026-10-29-001](../plans/2026-10-29-001-metocean-upstream-plan.md): a Kokkos
  kernel translated from NOAA's `W3SIN4` does not reproduce MetOcean's, so the bit-for-bit claims
  of the ST4 ports do not carry over.
- To close the ⚠: read ondaseventos' Dockerfile, switch file or `BUILD_INFO` with access to
  `codigos.ufsc.br` (the owner can run `git ls-remote https://codigos.ufsc.br/labeco/ondaseventos-forecast.git`
  with a token) and record the commit and switch it builds.
