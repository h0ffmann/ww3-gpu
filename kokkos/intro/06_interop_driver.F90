! kokkos/intro/06_interop_driver.F90
! Lesson 11.6 -- the bind(C) boundary: Fortran side.
!
! This is the caller WW3 will eventually look like. The interface block is the
! contract: iso_c_binding kinds on every argument, VALUE for the scalars C takes by
! value, and a plain assumed-size array for the buffer, which passes the address of
! the first element -- exactly what the C++ side wraps in an unmanaged View.
!
! The array stays Fortran's. C++ writes through the pointer and returns; nothing is
! copied, and nothing is freed on the other side.
!
! SPDX-License-Identifier: MIT
program ww_intro_06
  use, intrinsic :: iso_c_binding, only: c_int, c_float
  implicit none

  interface
     subroutine ww_intro_init() bind(c, name='ww_intro_init')
     end subroutine ww_intro_init

     subroutine ww_intro_scale(n, x, s) bind(c, name='ww_intro_scale')
       import :: c_int, c_float
       integer(c_int), value :: n
       real(c_float), intent(inout) :: x(*)
       real(c_float), value :: s
     end subroutine ww_intro_scale

     subroutine ww_intro_finalize() bind(c, name='ww_intro_finalize')
     end subroutine ww_intro_finalize
  end interface

  integer(c_int), parameter :: n = 1000
  real(c_float), parameter :: s = 2.5_c_float
  real(c_float) :: x(n)
  integer :: i
  real(c_float) :: worst

  do i = 1, n
     x(i) = real(i, c_float)
  end do

  call ww_intro_init()
  call ww_intro_scale(n, x, s)
  call ww_intro_finalize()

  worst = 0.0_c_float
  do i = 1, n
     worst = max(worst, abs(x(i) - s * real(i, c_float)))
  end do

  write (*, '(a,i0,a,f6.3,a,es9.2)') '06_interop_bindc: scaled n=', n, ' by s=', s, &
       ' max |error|=', worst

  if (worst > 1.0e-4_c_float) then
     write (*, '(a)') '06_interop_bindc: FAILED -- Fortran array was not scaled correctly'
     error stop 1
  end if
end program ww_intro_06
