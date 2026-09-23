! examples/02-regional-real-forcing/make_bathy.F90
! Turn a GEBCO (or any lat/lon/elevation netCDF) bathymetry tile into the WW3
! ASCII grid files bathy.inp and mask.inp that match &RECT_NML in ww3_grid.nml.
!
! WW3's grid preprocessor does not read netCDF bathymetry: you hand it plain
! ASCII arrays and declare the layout (IDLA = 1: one row per line, bottom row
! first, which is the order an ascending latitude axis already has).
!
! Get the source data
!   GEBCO 2024 grid, subsetted download for 52W-44W / 32S-24S:
!       https://download.gebco.net/
!   Save it as gebco.nc in this directory. GEBCO's convention -- negative
!   below sea level -- is already WW3's, which is why ww3_grid.nml sets
!   DEPTH%SF = 1.
!
! Sampling: NEAREST NEIGHBOUR. The Python this replaces did a bilinear
! interpolation; this takes the source cell whose centre is closest to each
! model point. On a 15-arcsecond source and a 0.1 degree target that is a
! downgrade you will not see in the results -- both methods throw away ~99 %
! of the source cells, and both can miss a whole shoal -- but it is a
! downgrade. For production work you would want area-weighted (conservative)
! averaging instead; see the README.
!
! Build and run (run.sh does this for you, inside `just ww3`):
!     gfortran -O2 -std=f2018 $(nf-config --fflags) -o make_bathy make_bathy.F90 $(nf-config --flibs)
!     ./make_bathy [gebco.nc]
!
! SPDX-License-Identifier: MIT
program make_bathy
  use, intrinsic :: iso_fortran_env, only: real32, real64, error_unit
  use, intrinsic :: ieee_arithmetic, only: ieee_is_nan
  use netcdf
  implicit none

  ! Must match &RECT_NML in ww3_grid.nml.
  real(real64), parameter :: x0 = -52.0_real64, y0 = -32.0_real64
  real(real64), parameter :: sx = 0.1_real64,   sy = 0.1_real64
  integer,      parameter :: nx = 81, ny = 81

  integer, parameter :: land = 0, sea = 1, boundary = 2
  real(real32), parameter :: land_fill = 100.0_real32   ! gaps -> treat as land, fail safe
  real(real32), parameter :: wet_below = -0.1_real32    ! matches GRID%ZLIM

  character(len=:), allocatable :: src
  character(len=32) :: latname, lonname, zname
  integer :: ncid, varid, dimids(2), londimid(1), latdimid(1), nlon, nlat
  real(real64), allocatable :: lon(:), lat(:)
  real(real32), allocatable :: zsrc(:, :)     ! (lon, lat) in Fortran order
  real(real32) :: z(nx, ny)
  integer :: mask(nx, ny)
  real(real64) :: dlon, dlat, x, y
  integer :: i, j, il, jl, unit, n_sea, n_bnd, arglen

  ! --- source file ------------------------------------------------------
  if (command_argument_count() >= 1) then
     call get_command_argument(1, length=arglen)
     allocate (character(len=arglen) :: src)
     call get_command_argument(1, src)
  else
     src = 'gebco.nc'
  end if

  if (nf90_open(src, nf90_nowrite, ncid) /= nf90_noerr) then
     write (error_unit, '(3a)') "could not open '", src, "'. See the header of make_bathy.F90 for where to get it."
     stop 1
  end if

  ! GEBCO calls them lat/lon and elevation; be tolerant.
  latname = first_present(ncid, [character(len=32) :: 'lat', 'latitude', 'y'])
  lonname = first_present(ncid, [character(len=32) :: 'lon', 'longitude', 'x'])
  zname   = first_present(ncid, [character(len=32) :: 'elevation', 'z', 'Band1', 'depth'])
  if (latname == '' .or. lonname == '' .or. zname == '') then
     write (error_unit, '(a)') 'could not find lat/lon/elevation variables -- ncdump -h the file and rename'
     stop 1
  end if

  ! GEBCO stores elevation(lat, lon) in C order, i.e. (lon, lat) as Fortran sees
  ! it. Some tools write the other order; read whichever it is and transpose.
  call check(nf90_inq_varid(ncid, trim(lonname), varid), 'inq '//trim(lonname))
  call check(nf90_inquire_variable(ncid, varid, dimids=londimid), 'dims of '//trim(lonname))
  call check(nf90_inquire_dimension(ncid, londimid(1), len=nlon), 'dim of '//trim(lonname))
  allocate (lon(nlon))
  call check(nf90_get_var(ncid, varid, lon), 'read '//trim(lonname))
  call check(nf90_inq_varid(ncid, trim(latname), varid), 'inq '//trim(latname))
  call check(nf90_inquire_variable(ncid, varid, dimids=latdimid), 'dims of '//trim(latname))
  call check(nf90_inquire_dimension(ncid, latdimid(1), len=nlat), 'dim of '//trim(latname))
  allocate (lat(nlat))
  call check(nf90_get_var(ncid, varid, lat), 'read '//trim(latname))

  call check(nf90_inq_varid(ncid, trim(zname), varid), 'inq '//trim(zname))
  call check(nf90_inquire_variable(ncid, varid, dimids=dimids), 'dims of '//trim(zname))
  allocate (zsrc(nlon, nlat))
  if (dimids(1) == londimid(1)) then
     call check(nf90_get_var(ncid, varid, zsrc), 'read '//trim(zname))
  else
     block
       real(real32), allocatable :: tmp(:, :)
       allocate (tmp(nlat, nlon))
       call check(nf90_get_var(ncid, varid, tmp), 'read '//trim(zname))
       zsrc = transpose(tmp)
     end block
  end if
  call check(nf90_close(ncid), 'close')

  ! Source spacing, used to tell "nearest" from "outside the tile".
  dlon = abs(lon(nlon) - lon(1)) / real(max(nlon - 1, 1), real64)
  dlat = abs(lat(nlat) - lat(1)) / real(max(nlat - 1, 1), real64)

  ! --- nearest-neighbour sampling onto the model grid -------------------
  do j = 1, ny
     y = y0 + sy * real(j - 1, real64)
     jl = minloc(abs(lat - y), dim=1)
     do i = 1, nx
        x = x0 + sx * real(i - 1, real64)
        il = minloc(abs(lon - x), dim=1)
        if (abs(lon(il) - x) > dlon .or. abs(lat(jl) - y) > dlat &   ! outside the tile
            .or. ieee_is_nan(zsrc(il, jl))) then
           z(i, j) = land_fill
        else
           z(i, j) = zsrc(il, jl)
        end if
     end do
  end do

  ! IDLA = 1 means bottom row (j=1, southernmost) first, which is the order
  ! j already runs in. Good.
  open (newunit=unit, file='bathy.inp', status='replace', action='write')
  do j = 1, ny
     write (unit, '(*(f0.1, :, 1x))') (z(i, j), i = 1, nx)
  end do
  close (unit)

  ! --- mask: wet where deeper than ZLIM, plus the open-boundary rows ------
  mask = merge(sea, land, z < wet_below)

  ! Mark the southern and eastern edges (one cell in) as open boundaries,
  ! matching INBND_POINT in ww3_grid.nml. Only where they are wet. (The Python
  ! this replaces re-marked the shared corner (nx-1, 2) -- INBND_POINT(2) -- as
  ! land on the second pass; here it stays a boundary point.)
  do i = 2, nx - 1
     if (mask(i, 2) == sea) mask(i, 2) = boundary
  end do
  do j = 2, ny - 1
     if (mask(nx - 1, j) == sea) mask(nx - 1, j) = boundary
  end do

  open (newunit=unit, file='mask.inp', status='replace', action='write')
  do j = 1, ny
     write (unit, '(*(i0, :, 1x))') (mask(i, j), i = 1, nx)
  end do
  close (unit)

  n_sea = count(mask == sea)
  n_bnd = count(mask == boundary)
  write (*, '(a, i0, a, i0, a)') 'wrote bathy.inp and mask.inp  (', nx, ' x ', ny, ')'
  write (*, '(a, i0)') '  sea points      : ', n_sea
  write (*, '(a, i0)') '  boundary points : ', n_bnd
  write (*, '(a, i0)') '  land points     : ', nx * ny - n_sea - n_bnd
  write (*, '(a, i0, a, i0, a)') '  depth range     : ', nint(minval(z)), ' to ', nint(maxval(z)), &
       ' m (negative = water)'
  write (*, '(a)') '  sampling        : nearest neighbour (see the header for the caveat)'
  if (n_sea < 100) write (*, '(a)') '  !! suspiciously few sea points -- check your longitude convention'

contains

  !> The first of `names` that is a variable in the file, or '' if none is.
  function first_present(nc, names) result(found)
    integer,          intent(in) :: nc
    character(len=*), intent(in) :: names(:)
    character(len=32) :: found
    integer :: k, vid
    found = ''
    do k = 1, size(names)
       if (nf90_inq_varid(nc, trim(names(k)), vid) == nf90_noerr) then
          found = names(k)
          return
       end if
    end do
  end function first_present

  !> Abort with the library's message on a failed NetCDF call.
  subroutine check(status, what)
    integer,          intent(in) :: status
    character(len=*), intent(in) :: what
    if (status /= nf90_noerr) then
       write (error_unit, '(4a)') 'netcdf: ', what, ': ', trim(nf90_strerror(status))
       stop 1
    end if
  end subroutine check

end program make_bathy
