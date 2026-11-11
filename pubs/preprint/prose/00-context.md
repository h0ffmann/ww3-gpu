`\setcounter{section}{-1}`{=latex}

# Context: an ocean model ported by a language model {#sec:context}

Koldunov et al. [@koldunov2026] report the port of a whole ocean model by a large language model.
The model is FESOM2, an ocean and sea-ice model, and the authors are at the Alfred Wegener
Institute. Claude Code, with Claude Opus 4.7, translated it from Fortran to C and then from C to
C++/Kokkos. The people set the strategy, reviewed the plans and the diffs, and found physics and
validation errors. The agent read, translated, built the test harnesses, debugged, profiled and
wrote the documentation.

The C stage is the centre of their method. The clean C code is the reference, and each Kokkos
kernel keeps its C original as a "twin". The acceptance rule changes at each step. The C model is
compared with the Fortran by statistics: over five years, the RMS difference of the mean is
0.006 °C in sea-surface temperature. Only the Kokkos Serial model is byte-identical, and only to
the C model. The authors write that bit-for-bit agreement between Fortran and C "across compilers and
arithmetic orderings is neither attainable nor necessary". On the GPU, they accept differences up to a recorded ceiling for each
field.

The successor of WW3 is also in progress. NCEP Office Note 525 [@on525] chose a bottom-up
rewrite, with a C++ core first and a Rust core later. The same report demotes Fortran to a
solver-only language. The WAVEWATCH IV repository [@ww4] is pre-alpha: on 2026-11-11 its
`develop` branch had 40 commits, from 2025-11-17 to 2026-10-16 (`875f49a`). Its 97 source files
are C++ (66 `.cpp`, 31 `.h`), and no physics kernel of WW3 is in it yet.

Thus the core kernels of the wave model stay in Fortran for some years. This is the first reason
why the first author will continue to maintain ww3-gpu and chaceon. They make the Fortran
kernels of WW3 run on GPUs now, without a change to their bits.

This note starts from that result and asks a narrower question. Can an agent port a spectral wave
model, and give the bits of the Fortran itself? The two ports below have no C stage, and their
reference is the Fortran. They also use two practices that FESOM2 reports: a strictly literal
translation, and a `file:line` with the literal value for each statement about the Fortran.
