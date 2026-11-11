# Numerical approaches

Summary of the WW3 7.14 manual, chapter 3 and appendix A. Evidence and provenance:
[README.md](README.md). At the pin: `manual/num/<file>.tex` (`basics`, `depth`, `space_trad_*`,
`spec_*`, `source`, `ice`, `nest`) and `manual/app/tstep.tex`. Equation numbers are those of the
supplied PDF. Grids and nesting are in [grids.md](grids.md).

## Spectral grid (3.1)

- Directions: constant increment $\Delta\theta$ over the full circle, `NTH` bins; the first
  direction can be offset by a fraction of $\Delta\theta$ (3.43).
- Frequencies: logarithmic, $\sigma_{m+1} = X_\sigma\sigma_m$ (3.1), `NK` bins, $X_\sigma$ about
  1.1 (`SPECTRUM%XFR`).
- The frequency grid is invariant in space and time; the **wavenumber grid is local**, from the
  dispersion relation and the local depth. Solving on it keeps the resolution in shallow water
  and absorbs the kinematic shoaling. The equation solved is for $N/c_g$:

$$\frac{\partial}{\partial t}\frac{N}{c_g} + \frac{\partial}{\partial x}\frac{\dot xN}{c_g}
 + \frac{\partial}{\partial y}\frac{\dot yN}{c_g}
 + \frac{\partial}{\partial\kappa}\frac{\dot\kappa N}{c_g}
 + \frac{\partial}{\partial\theta}\frac{\dot\theta N}{c_g} = \frac{S}{\sigma c_g} \quad (3.2)$$

## The fractional steps (3.2)

`W3WAVE` integrates one global step $\Delta t_g$ of (3.2) as a sequence:

| Step | What | Interval | Routine family |
|---|---|---|---|
| 1 | update of the water level, re-gridding of the spectrum in $k$ (3.3) | when the level changes | `W3ULEV` |
| 2 | intra-spectral propagation, part 1 ($\kappa$ and $\theta$) | $\Delta t_g/2$ | `W3KTPn` |
| 3 | spatial propagation | $\Delta t_g$ | `W3XYPn`, `W3XYPUG`, `W3PSMC` |
| 4 | intra-spectral propagation, part 2 | $\Delta t_g/2$ | `W3KTPn` |
| 5 | source terms without ice | $\Delta t_g$ | `W3SRCE` |
| 6 | ice source terms | $\Delta t_g$ | `W3SICn`, `W3SISn`, called from `W3SRCE` |

At the pin the calls sit in this order in `model/src/w3wavemd.F90` of NOAA-EMC/WW3 `03adda91`
(v): `W3ULEV` `:1434`, `W3KTP1/2/3` `:1842` to `:1858`, `W3XYPUG` `:1970`, `W3XYP1/2/3` `:1985`
to `:1991`, `W3KTP1/2/3` again `:2165` to `:2181`, `W3SRCE` `:2311`. Two more `W3SRCE` calls
(`:1618`, `:2285`) carry the arguments `srce_imp_pre` and `srce_imp_post`, for the implicit
unstructured solver ⚠ (their role was not read). The ice routines are called inside `W3SRCE`
(`model/src/w3srcemd.F90:1367` to `:1388`). The manual says the order of spatial and
intra-spectral propagation is alternated between steps for accuracy.

The splitting is what allows each part to be vectorised and parallelised on its own: steps 2, 4,
5 and 6 act on one spectrum at a time (a loop over sea points), step 3 on one spectral component
at a time over the whole grid.

## Four time steps (3.2, appendix A)

Set per grid in `ww3_grid`: `TIMESTEPS%DTMAX`, `DTXY`, `DTKTH` (v, `model/nml/ww3_grid.nml:71`
to `:73` at the pin) and the source term minimum `DTMIN` ⚠.

1. **Global** $\Delta t_g$: every term has been integrated; the step for output, coupling,
   interpolation of the forcing, and exchange between grids. The model shortens it to land on a
   requested input or output time, so an output interval that is not a multiple of $\Delta t_g$
   changes the time stepping and so the results (appendix A).
2. **Spatial propagation (CFL)**: the user gives the maximum step for the lowest frequency,
   $\Delta t_{p,r}$, without current; for frequency $m$,
   $\Delta t_{p,m} = \frac{\dot x_{p,r}}{\dot x_{p,m}}\Delta t_{p,r}$ (3.4). Low frequencies take
   several sub-steps per $\Delta t_g$, high ones a single step. The maximum group velocity is
   about 1.15 times the deep-water one of the lowest frequency. $\Delta t_g$ of 2 to 4 times the
   CFL step is usual on spherical grids. Not used on triangles, where the sub-steps are found
   from the CFL number.
3. **Intra-spectral**: at most $\Delta t_g$; half of it avoids a $2\Delta t$ oscillation where
   refraction is strong.
4. **Source terms**: dynamic, per point and per global step (below), with a user minimum,
   typically 5 to 15 s.

## Depth change in time (3.3)

A change of water level changes the local wavenumber grid. The spectrum is moved to the new
grid by a conservative interpolation of discrete action, with the parametric tail at high
wavenumbers. It is done only where the smallest $kd$ is below 4, the point is not ice covered,
and the largest wavenumber change is at least $0.05\,\Delta k$.

## Spatial propagation on regular grids (3.4.2)

The propagated quantity is $\mathcal N \equiv N c_g^{-1}\cos\phi$ on a sphere, $N c_g^{-1}$ on a
plane (3.5). Land absorbs: action moving towards land is removed, none comes from it. Active
boundary points supply prescribed spectra.

| Switches | Scheme | Notes |
|---|---|---|
| `PR1` | first-order upwind, flux form (3.6 to 3.9) | two-dimensional step, CFL on the vector norm, so its maximum step is $1/\sqrt2$ of the third-order one; the only scheme for a tripole grid |
| `UNO` | second-order Upstream Non-Oscillatory (Li 2008), mid-flux value (3.10) | non-oscillatory for CFL below 1 |
| `UQ` | third-order QUICKEST with the ULTIMATE total variation diminishing limiter (3.12 to 3.20) | the default; one dimension at a time, alternating which comes first; stable for $|C|\le1$; first-order upwind next to land and boundary points (3.21, 3.22) |

`PR2` and `PR3` select the higher-order schemes with a method against the Garden Sprinkler
Effect (GSE), the breaking of a continuous swell field into separate fields that a discrete
spectrum produces when numerical diffusion is low (3.4.6):

- `PR2`: Booij and Holthuijsen (1987) diffusion, along and across the propagation direction, as
  a function of a swell age $T_s$; its stability can limit the time step.
- `PR3`: Tolman (2002) spatial averaging over $\pm\gamma_{a,s}\Delta c_g\Delta t$ along and
  $\pm\gamma_{a,n}c_g\Delta\theta\Delta t$ across (3.42), both factors 1.5 by default; the
  default, never limits the time step.

**Unresolved obstacles** (3.4.7): the flux through a cell face is multiplied by a transparency
between 0 and 1 (3.44), defined on a staggered grid or at cell centres, read by `ww3_grid`. The
same mechanism carries continuous ice blocking (`IC0`, 3.8). The source term alternative is
`UOST`.

## Intra-spectral propagation (3.5)

$$\frac{\partial N}{\partial t} + \frac{\partial}{\partial k}\dot k_gN
 + \frac{\partial}{\partial\theta}\dot\theta_gN = 0 \;(3.49), \qquad
\dot k_g = \frac{\partial\sigma}{\partial d}\frac{\mathbf U\cdot\nabla_xd}{c_g}
 - \mathbf k\cdot\frac{\partial\mathbf U}{\partial s} \;(3.50)$$

- $\theta$ is periodic and needs no boundary condition. In $k$: no action enters at the low
  end; the flux at the high end assumes the tail (2.18).
- Depth gradients use central differences, one-sided next to land.
- **The refraction filter**:
  $\dot\theta = X_{rd}(\lambda,\phi,k)\,(\dot\theta_d + \dot\theta_c + \dot\theta_g)$ (3.51;
  v, `manual/num/spec.tex:53` at the pin), the depth, current and great-circle parts. $X_{rd}$ is found per
  wavenumber and point so that the CFL number of depth refraction stays below a user value,
  0.7 by default. It acts as a reduction of the bottom slope for long waves in very shallow
  water. The CFL maxima in the output (`CFD`, `CFK`) are taken before the filter.
- Schemes follow the spatial choice: first order (3.52), `UNO` with local gradients in $k$
  (3.53), `UQ` adapted to the variable $k$ spacing (3.54 to 3.59).

## Source term integration (3.6)

The numerical core of `W3SRCE`. Semi-implicit, as in WAM:

$$\Delta N(k,\theta) = \frac{\mathcal S(k,\theta)}{1-\epsilon D(k,\theta)\Delta t} \quad (3.61)$$

$D$ is the diagonal of $\partial S/\partial N$, which each source term routine returns beside
$S$. $\epsilon=1$ now ($0.5$ originally); `NL5` uses $\epsilon=0$. (3.61) is as
`manual/num/source.tex:23` prints it at the pin (v), a change per unit time; the update with its
$\Delta t_d$ is (3.65).

**Dynamic time step** (Tolman 1992). Within $\Delta t_g$, for sub-step $n$:

$$\Delta t_d^n = \min_{f<f_{hf}}\left[\frac{\Delta N_m}{|S|}
 \left(1+\epsilon D\frac{\Delta N_m}{|S|}\right)^{-1}\right] \;(3.62),\quad
\Delta t_d^n = \max[\Delta t_d^n,\Delta t_{d,min}] \;(3.63),\quad
\Delta t_d^n = \min\Big[\Delta t_d^n,\ \Delta t_g-\sum_{i<n}\Delta t_d^i\Big] \;(3.64)$$

$$N^n = \max\left[0,\ N^{n-1} + \frac{S\,\Delta t_d}{1-\epsilon D\Delta t_d}\right] \quad (3.65)$$

The allowed change per sub-step is the smaller of a parametric and a relative one:

$$\Delta N_m = \min[\Delta N_p,\Delta N_r] \;(3.66),\quad
\Delta N_p = X_p\frac{\alpha}{\pi}\frac{(2\pi)^4}{g^2}\frac{1}{\sigma k^3} \;(3.67),\quad
\Delta N_r = X_r\max[N,N_f] \;(3.68)$$

$$N_f = \max\left[\Delta N_p(k_{max},\theta),\ X_f\max_{\forall k,\theta}N(k,\theta)\right]
 \quad (3.69)$$

with $\alpha = 0.62\times10^{-4}$ and defaults $X_p=0.15$, $X_r=0.10$, $X_f=0.05$ (table 3.1).
Consequences a port has to keep:

- The sub-step count differs per grid point and per global step: the loop over sea points has
  no fixed trip count inside, and the source terms are recomputed at every sub-step.
- The minimum over the spectrum in (3.62) is a reduction over $(k,\theta)$ below $f_{hf}$; the
  clamp to zero in (3.65) and the limiter are part of the arithmetic.
- Where (3.63) sets the step, the change is limited to $\Delta N_p$: a limiter as in WAM.
- The average sub-step is the output field `DTD`.

**Seeding** (`SEED`): with no linear input, a minimum action at
$\sigma_{seed} = \min(\sigma_{max},2\pi f_{hf})$, aligned with the wind and proportional to the
limiter (3.70), lets waves start from a calm sea; $X_{seed}=1$ by default.

**Surf limiter** (`MLIM`): $E_m = \frac{1}{16}\left[\gamma_{lim}\tanh(\bar kd)/\bar k\right]^2$
(3.71); when $E > E_m$ the spectrum is rescaled. $\gamma_{lim}=1.6$, a safety valve, looser than
`DB1`.

## Ice source terms (3.7, 3.8)

Dissipation $\mathcal S_{id} = \beta_{id}N$ and scattering
$\mathcal S_{is} = \int\beta_{is}[N(k,\theta')-N(k,\theta)]\,d\theta'$ are linear, so their step
is solved exactly, after diagonalising the `NTH` by `NTH` matrix. For isotropic scattering, with
$\bar N$ the directional mean:

$$N(t+\Delta t_g) = e^{\beta_{id}\Delta t_g}\bar N(t)
 + e^{(\beta_{id}+\beta_{is})\Delta t_g}\left[N(t)-\bar N(t)\right] \quad (3.78)$$

`IC0` instead removes points whose ice concentration exceeds a critical value (default 0.5) and
re-seeds them with a low-energy Pierson-Moskowitz spectrum when the ice retreats; the ice map is
updated about halfway between the valid times of two ice fields.

## Forcing in time (3.9, 3.10)

Winds and currents are updated every $\Delta t_g$ and stand for the end of the step. In time:
linear in speed and direction by default (`WNT1`, `CRT1`), optionally corrected to conserve
energy (`WNT2`), or held constant (`WNT0`). In space, in the field preprocessor: bilinear on
vector components, with the same optional corrections (`WNX*`, `CRX*`). With `TIDE`, levels and
currents can be rebuilt from tidal constituents at run time.

## Diagnostics computed from the spectrum (3.11 to 3.13)

Space-time extremes (`MISC` namelist `STDX`, `STDY`, `STDT`; off at -1); spectral partitioning
by a watershed algorithm in `W3PARTMD`, with five ways to label sea and swell (`PTM` 1 to 5 in
`MISC`); tracking of wave systems in space and time (`ww3_systrk`). None of them feeds back on
the spectrum.
