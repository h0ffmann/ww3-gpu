! bench/dia/bench_dia.F90
! The DIA three ways on the same inputs, on the CPU: WW3's own W3SNL1 (the Fortran as WW3's CMake
! compiled it, linked from libww3), the Kokkos port through the shim WW3 calls (v2), and
! ww3-dia-gpu's kernel on the CPU (v1, bench/dia/v1_cpu.cpp). A benchmark, not a test of the port
! (ADR-0010): the one check it makes is that v2's S and D equal the Fortran's bit for bit on these
! inputs, so a timing never describes a port that disagrees.
!
!   bench_dia [npts [ncalls [warmup]]]     defaults 10000 20 3; reads ./mod_def.ww3
!
! Threads come from OMP_NUM_THREADS, for every variant: the Fortran and v1 loop over points with
! !$OMP, v2 runs on Kokkos' OpenMP backend. Variants:
!   fortran      W3SNL1 once per point (the grid's tables from W3IOGR, which calls INSNL1)
!   v2_batch     WW_SNL1 once for all points: phase 1's copy in, kernel, copy out
!   v2_point     WW_SNL1 once per point inside !$OMP CRITICAL, as W3SRCE calls it today
!   v1           v1's SA1/SA2 kernel on its own synthetic inputs, same points and bins
! Each variant: <warmup> calls, then <ncalls> timed calls; the time printed is ms per call.
!
! The spectra are synthetic and the same for the first three: a JONSWAP spectrum (gamma 3.3,
! alpha 0.0081) with cos^2 spreading, peak frequency 0.07 to 0.2 Hz and depth 10 to 1000 m
! varying over the points, in WW3's action form A(k,theta) = E(sigma,theta) Cg / sigma.
program bench_dia
  use, intrinsic :: iso_c_binding, only: c_int, c_float
  use constants, only: grav, tpi
  use w3gdatmd, only: w3nmod, w3setg, nk, nth, nspec, sig, th, xfr, dth, lam, snlc1, kdcon, &
                      kdmn, snls1, snls2, snls3, fachfe, iqtpe
  use w3wdatmd, only: w3ndat, w3setw
  use w3adatmd, only: w3naux, w3seta
  use w3odatmd, only: w3nout, w3seto
  use w3iogrmd, only: w3iogr
  use w3snl1md, only: w3snl1
  use w3kokkosmd, only: ww_kokkos_init, ww_snl1_init, ww_snl1, ww_snl1_last_error
!$ use omp_lib, only: omp_get_max_threads
  implicit none

  interface
    subroutine v1_init(ngrid, nk_, nth_) bind(c, name='v1_init')
      import :: c_int
      integer(c_int), value :: ngrid, nk_, nth_
    end subroutine v1_init
    subroutine v1_run() bind(c, name='v1_run')
    end subroutine v1_run
    real(c_float) function v1_checksum() bind(c, name='v1_checksum')
      import :: c_float
    end function v1_checksum
  end interface

  integer :: npts, ncalls, nwarm, ip, ik, ith, isp, ic, nthreads, ierr, ndiff_s, ndiff_d
  character(len=32) :: arg
  real, allocatable :: a(:, :), cg(:, :), kd(:), sf(:, :), df(:, :), sk(:, :), dk(:, :)
  real :: fp, depth, thm, f, sgm, k, ef, dirf, e, kdsum, esum, r, sink
  real :: kd_one(1)
  real(kind=8) :: t_fortran, t_batch, t_point, t_v1

  npts = 10000; ncalls = 20; nwarm = 3
  if (command_argument_count() >= 1) then
    call get_command_argument(1, arg); read (arg, *) npts
  end if
  if (command_argument_count() >= 2) then
    call get_command_argument(2, arg); read (arg, *) ncalls
  end if
  if (command_argument_count() >= 3) then
    call get_command_argument(3, arg); read (arg, *) nwarm
  end if
  nthreads = 1
!$ nthreads = omp_get_max_threads()

  ! The setup sequence of WW3's own programs (ww3_outp.F90:321-330), then the model definition.
  call w3nmod(1, 6, 6)
  call w3setg(1, 6, 6)
  call w3ndat(6, 6)
  call w3setw(1, 6, 6)
  call w3naux(6, 6)
  call w3seta(1, 6, 6)
  call w3nout(6, 6)
  call w3seto(1, 6, 6)
  call w3iogr('READ', 20)
  if (iqtpe <= 0) stop 'bench_dia: this mod_def.ww3 selects GQM (IQTYPE <= 0), not the DIA'

  allocate (a(nspec, npts), cg(nk, npts), kd(npts), sf(nspec, npts), df(nspec, npts), &
            sk(nspec, npts), dk(nspec, npts))
  do ip = 1, npts
    r = real(mod(ip - 1, 97)) / 96.
    fp = 0.07 + 0.13 * r
    depth = 10. * 100.**(real(mod(ip - 1, 89)) / 88.)
    thm = tpi * real(mod(ip - 1, 31)) / 31.
    kdsum = 0.; esum = 0.
    do ik = 1, nk
      sgm = sig(ik)
      f = sgm / tpi
      k = sgm * sgm / grav
      do ic = 1, 30                       ! sigma^2 = g k tanh(k d), Newton
        k = k - (grav * k * tanh(k * depth) - sgm * sgm) / &
            (grav * tanh(k * depth) + grav * k * depth / cosh(k * depth)**2)
      end do
      cg(ik, ip) = 0.5 * sgm / k * (1. + 2. * k * depth / sinh(min(2. * k * depth, 80.)))
      ef = 0.0081 * grav**2 * tpi**(-4) * f**(-5) * exp(-1.25 * (fp / f)**4) * &
           3.3**exp(-(f - fp)**2 / (2. * merge(0.07, 0.09, f <= fp)**2 * fp**2))
      do ith = 1, nth
        isp = ith + (ik - 1) * nth
        dirf = max(0., cos(th(ith) - thm))**2 * 2. / (tpi / 2.)
        e = ef * dirf / tpi
        a(isp, ip) = e * cg(ik, ip) / sgm
        kdsum = kdsum + e * k * depth
        esum = esum + e
      end do
    end do
    kd(ip) = kdsum / max(esum, tiny(esum))
  end do

  ! v2's setup is W3INIT's under the KOKKOS switch (w3initmd.F90, section 2.a.1).
  ierr = ww_kokkos_init(-1_c_int)
  if (ierr /= 0) stop 'bench_dia: ww_kokkos_init failed'
  ierr = ww_snl1_init(1_c_int, nk, nth, xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, &
                      snls3, fachfe, sig(1:nk))
  if (ierr /= 0) stop 'bench_dia: ww_snl1_init failed'
  call v1_init(int(npts, c_int), int(nk, c_int), int(nth, c_int))

  t_fortran = timed(1)
  t_batch = timed(2)
  ndiff_s = count(transfer(sf, 1, size(sf)) /= transfer(sk, 1, size(sk)))
  ndiff_d = count(transfer(df, 1, size(df)) /= transfer(dk, 1, size(dk)))
  t_point = timed(3)
  t_v1 = timed(4)
  sink = v1_checksum()

  write (*, '(a,i0,a,i0,a,i0,a,i0,a,i0)') 'grid nk=', nk, ' nth=', nth, ' nspec=', nspec, &
       ' points=', npts, ' threads=', nthreads
  write (*, '(a,f12.4)') 'fortran_ms  ', t_fortran
  write (*, '(a,f12.4)') 'v2_batch_ms ', t_batch
  write (*, '(a,f12.4)') 'v2_point_ms ', t_point
  write (*, '(a,f12.4)') 'v1_ms       ', t_v1
  write (*, '(a,i0,a,i0,a,i0)') 'v2_vs_fortran_bits_differing S=', ndiff_s, ' D=', ndiff_d, &
       ' of ', size(sf)
  if (sink /= sink) write (*, *) 'v1 checksum is NaN'
  if (ndiff_s /= 0 .or. ndiff_d /= 0) stop 1

contains

  ! ms per call of variant <which>, after nwarm untimed calls.
  real(kind=8) function timed(which)
    integer, intent(in) :: which
    integer :: i
    integer(kind=8) :: c0, c1, rate
    do i = 1, nwarm + ncalls
      if (i == nwarm + 1) call system_clock(c0, rate)
      select case (which)
      case (1)
        !$omp parallel do schedule(static)
        do ip = 1, npts
          call w3snl1(a(:, ip), cg(:, ip), kd(ip), sf(:, ip), df(:, ip))
        end do
        !$omp end parallel do
      case (2)
        call ww_snl1(1_c_int, int(npts, c_int), a, cg, kd, sk, dk)
        if (ww_snl1_last_error() /= 0) stop 'bench_dia: ww_snl1 failed'
      case (3)
        !$omp parallel do schedule(static) private(kd_one)
        do ip = 1, npts
          kd_one(1) = kd(ip)
          !$omp critical (ww_kokkos_snl1)
          call ww_snl1(1_c_int, 1_c_int, a(:, ip), cg(:, ip), kd_one, sk(:, ip), dk(:, ip))
          !$omp end critical (ww_kokkos_snl1)
        end do
        !$omp end parallel do
      case (4)
        call v1_run()
      end select
    end do
    call system_clock(c1)
    timed = 1000.d0 * real(c1 - c0, 8) / real(rate, 8) / real(ncalls, 8)
  end function timed

end program bench_dia
