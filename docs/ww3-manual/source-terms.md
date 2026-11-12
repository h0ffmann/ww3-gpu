# Source terms

Summary of the WW3 7.14 manual, sections 2.3 to 2.5. Evidence and provenance:
[README.md](README.md). At the pin: `manual/eqs/<SWITCH>.tex`, one file per switch. Equation and
table numbers are those of the supplied PDF.

The two terms this repository has ported or planned first, `NL1` (`W3SNL1`) and `ST4`
(`W3SIN4`, `W3SDS4`), are given in detail; the others get what they are, their switch and their
file. A source file named here exists at NOAA-EMC/WW3 `03adda91` (v,
`git ls-tree 03adda91 model/src/`); that it holds the routine named is from the manual's module
list (6.3) or `⚠` where that list is silent.

## Every option, by switch

Exactly one switch of each group is selected at compile time ([switches.md](switches.md)).

| Term | Switch | What it is | File at the pin |
|---|---|---|---|
| $S_{nl}$ | `NL1` | Discrete Interaction Approximation (DIA), from WAM | `w3snl1md.F90` (`W3SNL1`, `INSNL1`) |
| | `NL2` | Webb-Resio-Tracy (WRT) exact Boltzmann integral, $10^3$ to $10^4$ times the DIA's cost; writes `quad*.bqf` tables per depth | `w3snl2md.F90`, `mod_xnl4v5.f90`, `serv_xnl4v5.f90` |
| | `NL3` | Generalized Multiple DIA (GMD): arbitrary quadruplets, arbitrary depth, several quadruplets; default reproduces the DIA | `w3snl3md.F90` |
| | `NL4` | Two-Scale Approximation (TSA, `INDTSA=1`) or Full Boltzmann Integral (`0`) | `w3snl4md.F90` |
| | `NL5` | Generalized Kinetic Equation (GKE), quasi-resonant interactions, single grid point only, explicit integration ($\epsilon=0$) | `w3snl5md.F90`, `w3gkemd.F90` ⚠ |
| | `NLS` | conservative high-frequency filter, added to an $S_{nl}$, never instead of one | `w3snlsmd.F90` |
| $S_{in}+S_{ds}$ | `ST1` | WAM cycle 3 (Snyder et al. 1981, Komen et al. 1984) | `w3src1md.F90` |
| | `ST2` | Tolman and Chalikov (1996); stability correction with `STAB2` | `w3src2md.F90` |
| | `ST3` | WAM cycle 4, ECWAM (Janssen 1991; BJA, Bidlot et al. 2005, is the default) | `w3src3md.F90` |
| | `ST4` | saturation-based dissipation (Ardhuin et al. 2010; Filipot and Ardhuin 2012; Romero 2019) | `w3src4md.F90`, swell part in `w3swldmd.F90` |
| | `ST6` | observation-based (Rogers et al. 2012, Zieger et al. 2015), calibrated with flux `FLX4` | `w3src6md.F90`, `w3swldmd.F90` |
| $S_{ln}$ | `LN1`, `SEED` | Cavaleri and Malanotte-Rizzoli (1981) with Tolman's low-frequency filter; or spectral seeding (3.6) | `w3sln1md.F90` |
| $S_{bot}$ | `BT1` | JONSWAP, linear, $\Gamma=-0.067\ \mathrm{m^2 s^{-3}}$ by default (`SBT1`, `GAMMA`) | `w3sbt1md.F90` |
| | `BT4` | SHOWEX, Grant and Madsen eddy viscosity with ripple roughness | `w3sbt4md.F90` |
| | `BT8`, `BT9` | viscous fluid mud, Dalrymple and Liu (numerical) and Ng (analytical) | `w3sbt8md.F90`, `w3sbt9md.F90` |
| $S_{db}$ | `DB1` | Battjes and Janssen (1978), $\gamma=0.73$, $\alpha=1.0$; the Miche-style limiter is `MLIM` | `w3sdb1md.F90` |
| $S_{tr}$ | `TR1` | Lumped Triad Approximation (LTA, Eldeberky 1996), from SWAN | `w3str1md.F90` |
| $S_{sc}$ | `BS1` | bottom scattering (Ardhuin and Magne 2007) | `w3sbs1md.F90` |
| unresolved obstacles | `UOST` | source term form, any mesh (Mentaschi et al.) | `w3uostmd.F90` ⚠ |
| $S_{ice}$, dissipation | `IC1` to `IC5` | uniform $k_i$; Liu et al. boundary layer; Wang and Shen visco-elastic; empirical forms (`IC4METHOD`); effective medium models | `w3sic1md.F90` to `w3sic5md.F90` |
| $S_{ice}$, scattering | `IS1`, `IS2` | isotropic diffusive scattering; floe-size dependent scattering with ice break-up | `w3sis1md.F90` ⚠, `w3sis2md.F90` |
| ice as blocking | `IC0` | ice as land above a critical concentration; a propagation effect (3.8), exclusive with `IC1` to `IC3` | in the propagation |
| $S_{ref}$ | `REF1` | reflection at shorelines and icebergs | `w3ref1md.F90` |
| infragravity | `IG1` | second-order spectrum and free infragravity sources | `w3gig1md.F90` ⚠ |
| stress for coupling | `FLD1`, `FLD2` | sea-state dependent $\tau$, Reichl et al. (2014), Donelan et al. (2012) | `w3fld1md.F90`, `w3fld2md.F90` ⚠ |
| user | `XXn` | empty slot | none |

`W3SRCE` in `w3srcemd.F90` calls them and integrates the sum ([numerics.md](numerics.md)).

## `NL1`: the DIA (2.3.2)

Computed for $F(f_r,\theta)$, not for $N(k,\theta)$, so that the term conserves for the spectrum
that is the model's product; converted with (2.4). The quadruplet has $\mathbf k_1=\mathbf k_2$:

$$\mathbf k_1+\mathbf k_2=\mathbf k_3+\mathbf k_4,\quad \sigma_2=\sigma_1,\quad
\sigma_3=(1+\lambda_{nl})\sigma_1,\quad \sigma_4=(1-\lambda_{nl})\sigma_1 \quad (2.19)$$

$$\begin{pmatrix}\delta S_{nl,1}\\ \delta S_{nl,3}\\ \delta S_{nl,4}\end{pmatrix}
 = D\begin{pmatrix}-2\\ 1\\ 1\end{pmatrix} C g^{-4} f_{r,1}^{11}
 \left[F_1^2\left(\frac{F_3}{(1+\lambda_{nl})^4}+\frac{F_4}{(1-\lambda_{nl})^4}\right)
 -\frac{2F_1F_3F_4}{(1-\lambda_{nl}^2)^4}\right] \quad (2.20)$$

- $\lambda_{nl}=0.25$ for every package. $C$ depends on the package (table 2.1): $1.00\times10^7$
  for `ST2`, $2.78\times10^7$ for WAM-3, $2.50\times10^7$ for `ST4`, $3.00\times10^7$ for `ST6`.
  The namelist is `SNL1` (`NLPROP` is $C$, table 2.8).
- Shallow water scaling, still with the deep-water dispersion relation:
  $D = 1+\frac{c_1}{\bar kd}\left[1-c_2\bar kd\right]e^{-c_3\bar kd}$ (2.21), with $c_1=5.5$,
  $c_2=5/6$, $c_3=1.25$.
- The relative depth is $\bar kd = 0.75\,\hat kd$ (2.24) with
  $\hat k = \left(\overline{1/\sqrt k}\right)^{-2}$ (2.25), and is not allowed below 0.5.
- The $f^{11}$ factor is where a compiler's expansion of an integer power decides the last bit
  ([log 2026-10-14](../log/2026-10-14-snl1-bitwise-af11.md)).

At the pin `manual/eqs/NL1.tex` is rewritten around the same $\delta S_{nl}$ (two mirror
quadruplets, each component exchanging with 12 others before the bilinear interpolation), and
`NL1` also holds the Gaussian Quadrature Method, selected by a negative `IQTYPE` in `SNL1` (v,
`model/src/w3gridmd.F90:886`, `:1023` at `03adda91`). The supplied PDF has neither. At MetOcean's 7.14, the `--ww3` reference, `NL1` is the DIA alone: no GQM text, no
`W3SNLGQM` (v, read at `92e2cf66` on 2026-11-12; [README](README.md#at-metoceans-714-the-reference)).

## `ST3`: the input `ST4` starts from (2.3.10)

Miles' growth theory as modified by Janssen (1982, 1991):

$$S_{in}(k,\theta) = \frac{\rho_a}{\rho_w}\frac{\beta_{max}}{\kappa^2}e^{Z}Z^4
 \left(\frac{u_\star}{C}+z_\alpha\right)^2\cos^{p_{in}}(\theta-\theta_u)\,\sigma N(k,\theta)
 + S_{out}(k,\theta) \quad (2.105)$$

$$Z = \log(kz_1) + \frac{\kappa}{\cos(\theta-\theta_u)\,(u_\star/C+z_\alpha)} \quad (2.106)$$

The roughness $z_1 = \alpha_0\tau/\sqrt{1-\tau_w/\tau}$ (2.108) and
$U_{10} = \frac{u_\star}{\kappa}\log(z_u/z_1)$ (2.107) give $u_\star$ implicitly from $U_{10}$
and the wave-supported stress $\tau_w$; the relation is tabulated (`tabu_stress`,
`calc_ustar`). $\tau_w$ (2.109) is the resolved integral of $S_{in}/C$ plus a high-frequency
part $\tau_{hf}$ (2.110) from an $f^{-5}$ tail, tabulated in `tabu_tauhf`. The term is sensitive
to the spectral level at $k_{max}$: a higher level raises $u_\star$ and feeds back on the input.
`STAB3` adds the Abdalla and Bidlot (2002) gustiness correction and needs the air-sea
temperature difference.

Dissipation (2.113): $S_{ds} = C_{ds}\bar\alpha^2\bar\sigma\left[\delta_1\frac{k}{\bar k}
+\delta_2\left(\frac{k}{\bar k}\right)^2\right]N$, with $\bar k$ and $\bar\sigma$ defined with a
power $p$ (2.114, 2.115). Namelists `SIN3`, `SDS3`; tables 2.5 and 2.6 give WAM4, BJA and Bidlot
(2012).

## `ST4`: saturation-based dissipation (2.3.11)

**Input.** The positive part is (2.105) with $u_\star$ reduced per wavenumber by sheltering:

$$(u'_\star)^2 = \left|u_\star^2(\cos\theta_u,\sin\theta_u)
 - |s_u|\int_0^k\!\!\int_0^{2\pi}\frac{S_{in}(k',\theta)}{C}(\cos\theta,\sin\theta)\,dk'\,d\theta
 \right| \quad (2.116)$$

$s_u$ (`TAUWSHELTER`) tunes the stress at high winds. Because of the integral from 0 to $k$, the
input at one wavenumber depends on the input at all lower ones: the loop over `ik` in `W3SIN4`
carries a running sum and is not independent per frequency.

**Swell dissipation** (Ardhuin et al. 2009), the negative part $S_{out}$, mixes a viscous and a
turbulent boundary layer term:

$$S_{out} = r_{vis}S_{out,vis} + r_{tur}S_{out,tur} \;(2.119),\quad
S_{out,vis} = -s_5\frac{\rho_a}{\rho_w}\left\{2k\sqrt{2\nu\sigma}\right\}N \;(2.117),\quad
S_{out,tur} = -\frac{\rho_a}{\rho_w}\left\{16f_e\sigma^2u_{orb,s}/g\right\}N \;(2.118)$$

with weights $r_{vis,tur} = 0.5\,[1\mp\tanh((Re-Re_c)/s_7)]$ (2.120, 2.121), a Reynolds number
$Re = 2u_{orb,s}H_s/\nu_a$, the significant orbital velocity
$u_{orb,s} = 2\left[\iint\sigma^3N\,dk\,d\theta\right]^{1/2}$ (2.122; v,
`manual/eqs/ST4.tex:82` at `03adda91`), and a friction factor
$f_e = s_1f_{e,GM} + [|s_3|+s_2\cos(\theta-\theta_u)]\,u_\star/u_{orb}$ (2.123) from Grant and
Madsen's theory, tabulated in `tabu_swellft`.

**Dissipation.** The saturation spectrum is $B(k,\theta) = \sigma k^3N(k,\theta)$ (2.124).
`SDSBCHOICE` selects how it is used: 1, integrated over a directional sector of half-width
$\Delta_\theta$ (`SDSDTH`, 80°) (Ardhuin et al. 2010); 2, over frequency bands (Filipot and
Ardhuin 2012); 3, the local value (Romero 2019).

$$B'(k,\theta) = \int_{\theta-\Delta_\theta}^{\theta+\Delta_\theta}
 \sigma k^3\cos^{s_B}(\theta-\theta')N(k,\theta')\,d\theta' \;(2.125), \qquad
B(k) = \max_\theta B'(k,\theta) \;(2.128)$$

$$S_{ds}(k,\theta) = \sigma\frac{C_{ds}^{sat}}{B_r^2}\left[\delta_d\max\{B(k)-B_r,0\}^2
 + (1-\delta_d)\max\{B'(k,\theta)-B_r,0\}^2\right]N(k,\theta)
 + S_{bk,cu}(k,\theta) + S_{turb}(k,\theta) \quad (2.127)$$

(2.127) is written here as `manual/eqs/ST4.tex` has it at the pin. In the supplied PDF both $B$
are multiplied by a long-wave modulation factor
$M_l = 1+M_\theta\sqrt{\mathrm{mss}}+N_\theta\sqrt{\mathrm{nss}}$ (2.126), which upstream removed
in `3952826` (2024-01-16) ([README.md](README.md), "The pin").

The cumulative term is the rate at which breakers of lower frequency sweep over shorter waves,
with $r_{cu}=0.5$:

$$S_{bk,cu}(k,\theta) = -\frac{14.2\,C_{cu}}{\pi^2}N(k,\theta)\int_0^{r_{cu}^2k}\!\!\int_0^{2\pi}
 \max\left\{\sqrt{B(f',\theta')}-\sqrt{B_r},0\right\}^2 d\theta'\,dk' \quad (2.136)$$

so the dissipation at one wavenumber depends on the saturation at all lower ones, as the input
does. The wave-turbulence term is
$S_{turb} = -2C_{turb}\sigma\cos(\theta_u-\theta)\,k\frac{\rho_au_\star^2}{g\rho_w}N$ (2.137).
With `SDSBCHOICE=3` the breaking crest density is
$\Lambda(\mathbf k) = \frac1k\exp\left(-\frac{B_r}{B(\mathbf k)}\right)M_L M_W$ (2.131) with a
wind factor $M_W$ (2.132), and the term is (2.134).

**Parameters.** Namelists `SIN4` and `SDS4`; tables 2.7 and 2.8 give the sets T471, T471f, T405,
T500, T601 and T700 (T700 is `SDSBCHOICE=3`). Names the tables print: `ZWND`, `ALPHA0`,
`BETAMAX`, `SINTHP`, `ZALP`, `TAUWSHELTER`, `SWELLF` to `SWELLF7`, `Z0RAT`, `Z0MAX`;
`SDSBCHOICE`, `FXFM3`, `SDSC2`, `SDSBCK`, `SDSHCK`, `SDSDTH`, `SDSSTRAIN`, `SDSBR`, `SDSBT`,
`SDSCUM`, `SDSCOS`, `SDSP`, `SDSC5`, `SDSC6`. $\beta_{max}$ is the one retuned per wind product
(1.43 for T471 with ECMWF winds, 1.33 for T471f with CFSR). The values are `⚠` until read in
`w3gridmd.F90`: the manual's bold-face marking of defaults does not survive in the text.

**Routines at the pin** (v, `model/src/w3src4md.F90` at NOAA-EMC/WW3 `03adda91`): `W3SPR4`
`:142` (mean parameters), `W3SIN4` `:423` (input and stress), `INSIN4` `:1014`, `TABU_STRESS`
`:1290`, `TABU_TAUHF` `:1427`, `TABU_TAUHF2` `:1598`, `CALC_USTAR` `:1849`, `W3SDS4` `:2043`
(dissipation). The line numbers of the fork differ; `python3 scripts/route.py where W3SDS4`
gives them.

## Other packages, in one paragraph each

- **`ST1`** (2.3.8): $S_{in} = C_{in}\frac{\rho_a}{\rho_w}\max\left[0,\frac{28u_*}{c}
  \cos(\theta-\theta_w)-1\right]\sigma N$ (2.61), $C_{in}=0.25$, with
  $u_* = u_{10}\sqrt{(0.8+0.065u_{10})10^{-3}}$ (2.62); $S_{ds} = C_{ds}\hat\sigma\frac{k}{\hat k}
  \left(\frac{\hat\alpha}{\hat\alpha_{PM}}\right)^2N$ (2.63), $C_{ds}=-2.36\times10^{-5}$. Tail
  $m=4.5$, $f_{hf}=\max[2.5\hat f_r, 4f_{PM}]$.
- **`ST2`** (2.3.9): $S_{in}=\sigma\beta N$ with $\beta$ a piecewise function of the
  non-dimensional frequency $\tilde\sigma_a$ and a drag coefficient at the height of the
  apparent wavelength (2.69 to 2.72), tabulated by `INPTAB`; two dissipation constituents, low
  and high frequency. `FLX2` and `FLX3` are the friction velocity of this input (5.9.1).
- **`ST6`** (2.3.12): input with air-flow separation and steepness dependence, a negative input
  for adverse wind, whitecapping with a threshold spectral density and a cumulative part, swell
  dissipation by wave-turbulence interaction. Calibrated with the drag of `FLX4`; the bulk
  tuning knob is the `FLX4` namelist factor. The DIA constant becomes $3.00\times10^7$.
- **`DB1`** (2.3.18): a bulk dissipation from the fraction of breaking waves $Q_b$, found
  iteratively from a Rayleigh distribution truncated at $H_{max}=\gamma d$, spread over the
  spectrum without changing its shape.
- **Ice** (2.4): `IC1` to `IC5` are dissipation through an imaginary wavenumber $k_i$, with up
  to five input fields $C_{ice,1\ldots5}$ whose meaning depends on the switch; `IS1` and `IS2`
  conserve energy and redistribute it in direction. They are integrated in their own fractional
  step, exactly ([numerics.md](numerics.md), 3.7).
- **Air-sea processes** (2.5): `FLD1` and `FLD2` compute a sea-state dependent stress to hand to
  a coupled model; they do not change the wave solution.
