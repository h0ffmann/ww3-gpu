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

This note starts from that result and asks a narrower question. Can an agent port a spectral wave
model, and give the bits of the Fortran itself? The two ports below have no C stage, and their
reference is the Fortran. They also use two practices that FESOM2 reports: a strictly literal
translation, and a `file:line` with the literal value for each statement about the Fortran.
