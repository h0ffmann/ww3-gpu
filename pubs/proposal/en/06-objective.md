# OBJECTIVE

The general objective is to reduce, in a measured and reproducible way, the run time of the operational WAVEWATCH III cycle maintained by LabECO/UFSC within ReNOMO, without changing its scientific results beyond the tolerances agreed with the laboratory, exhausting configuration and build optimisations before rewriting any code, and to decide by the same criterion — measured gain and no regression — whether C++/Kokkos kernels running on an NVIDIA H100 GPU enter the operational configuration.

The specific objectives are:

1. To freeze and document the operational configuration (source revision, switches, namelists, grid, forcing, outputs, hardware) and to build a reproducible benchmark of the reference run, with a defined metric (run time per forecast hour).
2. To produce a profile of the reference run by routine and by phase (source terms, propagation, communication, input and output), on one and on several MPI processes.
3. To quantify the gain of the build, configuration and Fortran refactoring rungs, each with its parity evidence against the reference, and to deliver the best configuration to the laboratory as a documented build.
4. To build the validation infrastructure WW3 lacks: a per-field comparator with versioned tolerances and per-routine unit tests on captured inputs, integrated with the model's regression matrix.
5. To rewrite in C++/Kokkos, in profile order, the kernels that remain dominant, validate them against the original Fortran on CPU, measure their gain on GPU (H100) and decide, on gain and parity, whether they enter the operational configuration.
6. To publish tooling, results and recommendation in the open repository, in a form the laboratory can rerun.
