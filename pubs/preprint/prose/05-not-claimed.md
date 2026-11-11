# What is not claimed {#sec:limits}

1. Neither project has run the whole model on a real GPU. The PTX of chaceon has never run.
   ww3-gpu has run one DIA benchmark kernel on an RTX 4090 [@diagpuv2].
2. This note claims no speed-up. The rules of ww3-gpu mark its recorded timings [@noaarecord]
   "regtest pending", and chaceon has no timings.
3. On a GPU, bit identity also needs the `expf` and `logf` of the device to return the bits of
   glibc. ww3-gpu sends its kernels to device copies of the glibc 2.44 functions. These copies
   equal the host functions on every input that was checked. No device run has compared them
   yet. Until one does, a fixed tolerance judges a GPU run.
4. The two references are different lines of WW3 (NOAA-EMC and MetOcean). Thus the comparison of
   the two ports is by routine name, not by source text. Both ledgers contain 85 routines.
