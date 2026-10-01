# Exercise 12 — `Hs` from an action spectrum with a team `parallel_reduce`

**Lesson:** [11 — Kokkos and modern C++](../course/11-kokkos-and-modern-cpp.md).
**Time:** ~60 min. **Solution:** [`solutions/ex12_reduce.cpp`](solutions/ex12_reduce.cpp),
built by [`solutions/CMakeLists.txt`](solutions/CMakeLists.txt).

## Goal

Compute the significant wave height at every sea point of a WW3-shaped state array with
Kokkos, in the shape a ported source term has: **one team per sea point**, the spectral
sum done by the team, one thread writing the result. `kokkos/intro/02_parallel_for.cpp`
integrates *one* spectrum with an `MDRangePolicy`; this is the next step.

WW3 stores **action** density `A(theta, k) = E / sigma`, theta fastest (Fortran order),
for `NSEA` points: `A(NTH, NK, NSEA)`. So

```
Hs = 4 sqrt(m0),   m0 = sum_k sum_theta A(theta, k) * sigma_k * dtheta * dsigma_k
```

The `sigma_k` inside the sum turns action back into energy. Leave it out and every `Hs`
is wrong by a smooth, plausible-looking factor. That is why the exercise is
self-checking against something computed independently.

## Steps

1. **A standalone CMake project.** `find_package(Kokkos REQUIRED)`, an include path to
   `kokkos/src`, `target_link_libraries(... Kokkos::kokkos)`. The headers you need
   (`ww_kokkos/real.hpp`, `ww_kokkos/spectrum_fixtures.hpp`) are header-only, so no link
   against the lab's library. `solutions/CMakeLists.txt` is the minimum.

2. **The state.** `Kokkos::View<ww::Real***, Kokkos::LayoutLeft, DeviceSpace> a("A", nth, nk, nsea)`.
   `LayoutLeft` *is* Fortran order. Fill it with `ww::jonswap(..., gamma = 1) *
   ww::cos2_spread(...) / sigma` at a different fetch per point, with a
   `MDRangePolicy<Rank<3>>`.

3. **The kernel.** `TeamPolicy<>(nsea, Kokkos::AUTO)`; inside, `team.league_rank()` is
   the point, `Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, nth*nk), ..., m0)`
   sums, `Kokkos::single(Kokkos::PerTeam(team), ...)` writes `hs(isea)`. Accumulate in
   `double` even though the state is `float`: float32 state, float64 sums is the lab's
   rule (lesson 11).

4. **The check.** `ww::jonswap_m0(u10, fetch)` is the exact `m0` of the gamma = 1 form.
   Copy `hs` back with `create_mirror_view_and_copy`, compare each point, exit non-zero
   if any is more than 2 % off (the residual is the geometric grid's tail truncation;
   `L1_test_intro` uses the same bound).

5. Build and run inside `just ww3`, then again with `OMP_NUM_THREADS=1` and `=8`. Same
   digits? Should they be?

## What to hand in

The program, its printed table, and two sentences on why `Kokkos::single` is needed
(what happens without it on the OpenMP backend, and on CUDA?).

## Going further

- Replace `TeamThreadRange` with a two-level `TeamThreadRange` over `k` and
  `ThreadVectorRange` over `theta`. Which is faster on your CPU? On the 4090
  (`just kokkos-cuda-test` shows how to build for it)?
- Put the spectrum of the current point into team scratch first
  (`kokkos/intro/05_team_scratch.cpp`): that is the DIA kernel's structure, and the
  subject of lesson 12.
