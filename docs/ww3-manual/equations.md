# Governing equations

Summary of the WW3 7.14 manual, sections 2.1, 2.2, 2.3.1 and 2.6. Evidence and provenance:
[README.md](README.md). At the pin: `manual/eqs/intro.tex`, `prop.tex`, `source.tex`,
`output.tex`. Equation numbers are those of the supplied PDF.

## Phase parameters and the spectrum (2.1)

Linear theory is applied locally, under the geometrical optics approximation: depth and current
vary on scales much larger than one wave. Diffraction, scattering and interference are left out
of the propagation and can come back as source terms.

$$\sigma^2 = gk\tanh kd \quad (2.1), \qquad \omega = \sigma + \mathbf{k}\cdot\mathbf{U} \quad (2.2)$$

$\sigma$ is the relative (intrinsic) radian frequency, seen moving with the current; $\omega$ is
the absolute one; $d$ is the mean depth and $\mathbf{U}$ the depth- and time-averaged current.
The direction $\theta$ is normal to the crest, the direction of $\mathbf{k}$.

- The model's basic spectrum is the wavenumber-direction variance density $F(k,\theta)$, chosen
  for its invariance when depth varies. The **prognostic variable is the action density**
  $N(k,\theta) \equiv F(k,\theta)/\sigma$, because action, not energy, is conserved on a current.
- Output is the frequency-direction spectrum, by the Jacobian
  $F(f_r,\theta) = \frac{2\pi}{c_g}F(k,\theta)$ (2.4), with
  $c_g = \partial\sigma/\partial k = n\,\sigma/k$, $n = \tfrac12 + \frac{kd}{\sinh 2kd}$ (2.6).
- In the code the stored array is `a(ith,ik,isea)` ([grids.md](grids.md)).

## The balance equation (2.2)

$$\frac{DN}{Dt} = \frac{S}{\sigma} \quad (2.7)$$

WW3 uses the conservation form, velocities inside the derivatives, so that the discrete equation
conserves action. Cartesian:

$$\frac{\partial N}{\partial t} + \nabla_x\cdot\dot{\mathbf{x}}N
 + \frac{\partial}{\partial k}\dot kN + \frac{\partial}{\partial\theta}\dot\theta N
 = \frac{S}{\sigma} \quad (2.8)$$

$$\dot{\mathbf{x}} = \mathbf{c}_g + \mathbf{U} \;(2.9), \quad
\dot k = -\frac{\partial\sigma}{\partial d}\frac{\partial d}{\partial s}
         - \mathbf{k}\cdot\frac{\partial\mathbf{U}}{\partial s} \;(2.10), \quad
\dot\theta = -\frac1k\left[\frac{\partial\sigma}{\partial d}\frac{\partial d}{\partial m}
         + \mathbf{k}\cdot\frac{\partial\mathbf{U}}{\partial m}\right] \;(2.11)$$

$s$ runs along $\theta$ and $m$ normal to it. Spherical, with longitude $\lambda$, latitude
$\phi$ and the Earth's radius $R$:

$$\frac{\partial N}{\partial t}
 + \frac{1}{\cos\phi}\frac{\partial}{\partial\phi}\dot\phi N\cos\theta
 + \frac{\partial}{\partial\lambda}\dot\lambda N
 + \frac{\partial}{\partial k}\dot kN + \frac{\partial}{\partial\theta}\dot\theta_gN
 = \frac{S}{\sigma} \quad (2.12)$$

$$\dot\phi = \frac{c_g\cos\theta + U_\phi}{R}, \quad
\dot\lambda = \frac{c_g\sin\theta + U_\lambda}{R\cos\phi}, \quad
\dot\theta_g = \dot\theta - \frac{c_g\tan\phi\cos\theta}{R} \quad (2.13\ \text{to}\ 2.15)$$

The last term of (2.15) is the great-circle correction. (2.12) to (2.15) are as
`manual/eqs/prop.tex:67` to `:84` prints them at the pin (v). ⚠ They put $\cos\theta$ in the
latitude speed, while the manual's next sentence and section 6.5.1 say $\theta=0$ is towards the
east: the manual is not consistent here, and the convention is settled by the code, not by this
page.
Inside the model directions are Cartesian: $\theta=0$ is propagation towards the east, positive
`ix`, and $\theta=90^\circ$ towards the north (6.5.1). Output directions use other conventions.

Unresolved islands enter at the level of the numerical scheme (3.4.7) or as a source term
(`UOST`), not in these equations.

## The source term sum (2.3.1)

$$S = S_{ln} + S_{in} + S_{nl} + S_{ds} + S_{bot} + S_{db} + S_{tr} + S_{sc} + S_{ice} + S_{ref}
 + S_{user} \quad (2.16)$$

Linear input, wind input, quadruplet interactions, whitecapping, bottom friction, depth-induced
breaking, triads, bottom scattering, ice, shoreline and iceberg reflection, and a user slot.
The terms are defined for the energy spectrum; most are computed for action, $\mathcal S \equiv
S/\sigma$. Explicit $S_{nl}$ is what makes the model third generation. The options for each term
are in [source-terms.md](source-terms.md).

**The tail.** The spectrum is integrated up to a cut-off $f_{hf}$, ideally the highest discrete
frequency, and a parametric tail is attached above it:

$$F(f_r,\theta) = F(f_{r,hf},\theta)\left(\frac{f_r}{f_{r,hf}}\right)^{-m} \;(2.17), \qquad
N(k,\theta) = N(k_{hf},\theta)\left(\frac{f_r}{f_{r,hf}}\right)^{-m-2} \;(2.18)$$

$m$ and $f_{hf}$ depend on the input and dissipation package ($m=4.5$ for `ST1`; `ST3` and `ST4`
code the stress tail with $f^{-5}$). The intra-spectral propagation uses the same tail as its
high-wavenumber boundary condition (3.5.1).

**Wind and current.** The wind can be taken in a fixed frame or relative to the current, chosen
at compile time (`RWND`); the output wind is never corrected for the current.

**Ice concentration.** Input and dissipation are scaled by the open-water fraction; $S_{nl}$ is
applied in open water and in ice alike. The scaling does not depend on the package.

## Output parameters (2.6)

Ten groups of gridded fields, each with a flag name used in `ww3_shel` and the post-processors.
The spectral averages behind them are $\bar z = E^{-1}\iint zF\,df_r\,d\theta$ (2.22) with
$E = \iint F\,df_r\,d\theta$ (2.23); $H_s = 4\sqrt E$.

| Group | Fields (flag names) |
|---|---|
| I, forcing | `DPT` depth, `CUR` current, `WND` wind, `AST` air-sea temperature difference, `WLV` water level, `ICE` concentration, `IBG` icebergs, `D50`, `IC1` ice thickness, `IC5` floe diameter |
| II, mean wave parameters | `HS`, `LM` mean length, `T02`, `T0M1`, `T01`, `FP` peak frequency, `DIR` mean direction, `SPR` spread, `DP` peak direction, `HIG` infragravity height, the space-time extremes `MXE` `MXES` `MXH` `MXHC` `SDMH` `SDMHC`, `WBT` breaking probability, `TP` |
| III, spectral parameters | `EF` frequency spectrum, `TH1M`, `STH1M`, `TH2M`, `STH2M`, `WN` wavenumbers |
| IV, partitions | `PHS`, `PTP`, `PLP`, `PSP`, `PWS`, `PDP`, `PQP`, `PPE`, `PGW`, `PSW`, `PTE`, `PT1`, `PT2`, `PEP`, `TWS`, `PNR` |
| V, atmosphere-wave layer | `UST` friction velocity, `CHA` Charnock, `CGE` energy flux, `FAW`, `TAW`, `TWA`, `WCC` whitecap cover, `WCF`, `WCH`, `WCM` |
| VI, wave-ocean layer | `SXY` radiation stresses, `TWO`, `BHD` Bernoulli head, `FOC`, `TUS`, `USS` surface Stokes drift, `P2S`, `USF`, `P2L`, `TWI`, `FIC`, `USP`, `TOC` total momentum to the ocean |
| VII, wave-bottom layer | `ABR`, `UBR` near-bottom excursion and orbital velocity, `BED`, `FBB`, `TBB` |
| VIII, spectrum parameters | `MSS` mean square slopes, `MSC` tail level, `MSD`, `MCD`, `QP` peakedness |
| IX, numerical diagnostics | `DTD` average source term time step, `FC` cut-off frequency, `CFX`, `CFD`, `CFK` maximum CFL numbers in space, direction and wavenumber |
| X, user | `U1`, `U2` |

`UST` depends on the package selected. `DIR` is in the meteorological convention in the output,
unlike the model's internal Cartesian direction. The list is longer at the pin (skewness, `QKK`);
read `manual/eqs/output.tex` there, and `w3iogomd.F90` for what is computed.

The wind-sea fraction of a partition (2.266) and the partitioning methods are in 3.12; the
Stokes drift profile and the slopes for near-nadir backscatter are derived parameters (2.7).
