---
title: "One rule, two languages"
subtitle: "C++/Kokkos and Rust ports of WAVEWATCH III that must give the Fortran's bits"
author:
  - name: Matheus Hoffmann
    orcid: 0009-0009-1056-7661
    affiliation: Escola Politécnica, Universidade Federal do Rio de Janeiro, Brazil
  - name: Pedro Veras Guimarães
    orcid: 0000-0002-1686-4722
    affiliation: Co-advisor of the first author. LabECO, Departamento de Engenharia Mecânica, Universidade Federal de Santa Catarina, Brazil
  - name: Pedro Cruz
    orcid: 0000-0002-2019-133X
    affiliation: Advisor of the first author. Grupo de Teleinformática e Automação (GTA), DEL, Escola Politécnica, Universidade Federal do Rio de Janeiro, Brazil
date: "11 November 2026"
version: "v0 (preprint, not peer reviewed)"
abstract: |
  We report two ports of the WAVEWATCH III (WW3) spectral wave model. Coding agents wrote both
  ports, and both ports have one acceptance rule: the port must give the same bits as the
  Fortran. A tolerance is a diagnostic, never the verdict. *ww3-gpu* keeps WW3, and the Fortran
  calls C++ written with Kokkos, behind one run-time flag for each routine. Its reference is the
  Fortran path of the same executable. *chaceon* replaces WW3 with Rust, compiled for the CPU,
  for SPIR-V (rust-gpu) and for PTX (Rust-CUDA). No Fortran runs in its process, and its
  reference is a WW3 that is compiled separately. ww3-gpu ports the discrete interaction
  approximation (DIA) of the four-wave transfer. On the CPU, it is byte-identical on all 100
  serial and 25 OpenMP entries of the WW3 regression matrix that select it. The DIA takes 48.4 % of single-thread self
  time on a case of 9 520 points. chaceon runs `ww3_grid`, `ww3_strt`, `ww3_prep` and `ww3_shel`
  without Fortran, and 96 output files are byte-identical to the reference. Neither port has run
  the whole model on a real GPU, and this note claims no speed-up. We compare how each project
  defines and checks its evidence.
keywords:
  - WAVEWATCH III
  - bit-for-bit reproducibility
  - Kokkos
  - Rust
  - GPU porting
  - coding agents
lang: en-US
papersize: a4
fontsize: 10pt
geometry:
  - top=14mm
  - bottom=17mm
  - left=18mm
  - right=18mm
link-citations: true
header-includes: |
  ```{=latex}
  \AtBeginEnvironment{longtable}{\small}
  \newfontfamily\markfont{DejaVuSans.ttf}
  \catcode`\⚠=\active
  \protected\def⚠{{\markfont\char"26A0\relax}}
  ```
colorlinks: true
---
