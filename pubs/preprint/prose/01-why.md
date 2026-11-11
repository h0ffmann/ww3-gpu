# Why bit for bit {#sec:why}

WW3 integrates the action balance equation

$$\frac{\partial N}{\partial t} + \nabla\cdot(\dot{\mathbf{c}}\,N) = \frac{S}{\sigma},$$

where the source term $S$ is the sum of wind input, dissipation, nonlinear transfer and bottom
terms. A port that changes the order of a floating-point sum changes $N$. A forecast system that
was validated against observations must then be validated again. Thus both projects keep the
arithmetic of the Fortran. They do not reorder a reduction, they do not use a fused multiply-add
(`-ffp-contract=off` on both sides), and they do not widen a tolerance. A difference is a defect
of the port until two runs of the Fortran itself give different bytes in the same file.
