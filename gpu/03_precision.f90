! Measure what double precision actually costs on YOUR card.
!
!   nvfortran -O3 -acc -gpu=cc89 -o 03_precision 03_precision.f90
!   ./03_precision
!
! Why this matters: the RTX 4090 (AD102) runs FP64 at roughly 1/64 the rate
! of FP32 -- about 1.3 TFLOP/s against about 82. That is deliberate market
! segmentation, not a defect. A data-centre card (A100, H100) is 1/2.
!
! WW3 computes mostly in default 4-byte real, so this may never bite you.
! But if anything in your build forces -r8, or a library you link is built
! double, you will fall off a cliff -- and you will blame the port rather
! than the precision. Measure the ratio once so you recognise it.
!
! Run it, then compare against the nominal 1/64. If you see roughly 60x,
! that is your card doing exactly what the spec sheet says.

program precision_test
  implicit none
  integer, parameter :: sp = selected_real_kind(6, 37)
  integer, parameter :: dp = selected_real_kind(15, 307)
  integer, parameter :: n = 20000000, reps = 20

  real(sp), allocatable :: as(:), bs(:)
  real(dp), allocatable :: ad(:), bd(:)
  real(sp) :: ssum
  real(dp) :: dsum
  integer :: i, r
  ! NOTE: cpu_time is host CPU time. For real timing comparisons use
  ! SYSTEM_CLOCK -- see ../bench/kernel_bench.f90. cpu_time SUMS across
  ! OpenMP threads and will make a threaded run look slower than serial.
  real :: t0, t1, tsingle, tdouble

  allocate(as(n), bs(n), ad(n), bd(n))

  !$acc data create(as, bs, ad, bd)

  !$acc parallel loop
  do i = 1, n
     as(i) = 1.0_sp + real(mod(i, 100), sp)
     bs(i) = 2.0_sp
  end do
  !$acc parallel loop
  do i = 1, n
     ad(i) = 1.0_dp + real(mod(i, 100), dp)
     bd(i) = 2.0_dp
  end do

  ! --- single ------------------------------------------------------
  ssum = 0.0_sp
  call cpu_time(t0)
  do r = 1, reps
     !$acc parallel loop reduction(+:ssum)
     do i = 1, n
        ssum = ssum + as(i) * bs(i) + sqrt(as(i))
     end do
  end do
  call cpu_time(t1)
  tsingle = t1 - t0

  ! --- double ------------------------------------------------------
  dsum = 0.0_dp
  call cpu_time(t0)
  do r = 1, reps
     !$acc parallel loop reduction(+:dsum)
     do i = 1, n
        dsum = dsum + ad(i) * bd(i) + sqrt(ad(i))
     end do
  end do
  call cpu_time(t1)
  tdouble = t1 - t0

  !$acc end data

  print '(a,f10.4,a)', ' single precision : ', tsingle, ' s'
  print '(a,f10.4,a)', ' double precision : ', tdouble, ' s'
  print '(a,f8.2,a)',  ' ratio            : ', tdouble / max(tsingle, 1.0e-9), 'x slower'
  print *
  print *, ' RTX 4090 nominal FP64:FP32 is about 1:64.'
  print *, ' A100/H100 is about 1:2. That gap is the whole argument for'
  print *, ' data-centre cards in climate and ocean modelling.'
  print '(a,es16.8,a,es16.8)', ' sums (ignore, keeps the optimiser honest): ', ssum, ' ', dsum

  deallocate(as, bs, ad, bd)
end program precision_test
