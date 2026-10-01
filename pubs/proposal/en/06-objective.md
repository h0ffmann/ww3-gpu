# OBJECTIVE

The general objective is to reduce the run time of the operational WAVEWATCH III cycle maintained by LabECO/UFSC for ReNOMO, with measured and reproducible gains, without the results differing from the current ones by more than the tolerances agreed with the laboratory. A single criterion (measured gain and results within tolerance) will also decide whether C++/Kokkos kernels running on an NVIDIA H100 GPU become part of the operational configuration.

The specific objectives are:

1. To record and freeze the operational configuration of each case (source revision, switches, namelists, grid, forcing, outputs and hardware) and to build a reproducible benchmark of the reference run, with a defined metric: run time per forecast hour.
2. To profile the reference run by routine and by phase (source terms, propagation, communication, input and output), on one and on several MPI processes.
3. To quantify the gain of the build, configuration and Fortran refactoring rungs, each with its check of agreement with the reference, and to deliver the best configuration to the laboratory as a documented build.
4. To build the validation infrastructure WW3 lacks: a field-by-field comparator with versioned tolerances and per-routine unit tests on inputs captured from the operational case, integrated with the model's regression matrix.
5. To rewrite in C++/Kokkos, in the order given by the profile, the routines that remain dominant, validate them on CPU against the original Fortran, measure the gain on GPU (H100) and decide, on gain and agreement of results, whether they enter the operational configuration.
6. To publish tooling, results and recommendations in the project repository, in a form that lets the laboratory repeat the measurements, subject to the disclosure policy described in the methodology.
