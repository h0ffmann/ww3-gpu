# What each project found {#sec:found}

**ww3-gpu: the reference is the optimised code.** Byte identity for the DIA needed one fact that
the source text does not give. gfortran expands `x**11` as one chain of multiplications at `-O0`
and as a different chain at `-O3`. The two chains disagree on 643 427 of 1 000 000
single-precision inputs. Before this was found, the research log explained the remaining
differences as round-off that WW3 amplifies. That explanation was wrong. Thus the reference of the
port is the code that the WW3 build produces, read as GIMPLE or disassembly, not the Fortran text.
The port has no unit tests of its own. Only the WW3 regression tests, compared by `test.comp`,
judge it.

**chaceon: a run with no Fortran in it.** The Rust programs `ww3_grid`, `ww3_strt`, `ww3_prep` and
`ww3_shel` run in sequence from the namelists. They write `mod_def.ww3`, the forcing, the gridded
and point outputs, the track file and both restarts. All 96 files are byte-identical to the
reference. The boundaries give 214 identical comparisons over 34 cases, and the track
post-processors give 74. `ww3_shel --threads N` and an MPI build give the same bytes. The files
stay identical when `W3SRCE`, `W3XYP3` and `W3KTP3` are compiled to SPIR-V and run on a CPU
Vulkan driver. Enumeration cannot reach the $2^{64}$ pairs of `f32` operands. There, a Lean proof
checks the division fact that the port uses. @sec:division states that fact and proves it. The
proof says nothing about whether Rust equals Fortran, because Fortran has no usable formal
semantics.

**Why ww3-gpu has no proof yet, and what comes next.** In this first phase, ww3-gpu adopted no
proof assistant [@adr0001]. There are four reasons:

1. No proof assistant reads Fortran or C++. A proof is about a model that a person writes. It says
   nothing about the gfortran and nvcc binaries unless somebody shows that the model agrees with
   them, and that agreement is the open question.
2. Bit equality is a property of two binaries, built with given flags. It is not a property of two
   source texts.
3. For straight-line IEEE-754 code, equality on every input is decidable. SMT queries in the
   `QF_FP` theory and sweeps over all $2^{32}$ `float32` inputs answer it in seconds to minutes.
   An interactive proof takes months for each routine.
4. Agents must write each proof, and a person must review it. This is a second artefact for each
   routine.

Without a proof, the evidence of the DIA port is a set of runs. At the fork commit `80644ba9`,
310 entries of the WW3 regression matrix select the DIA (@tbl:regtests). 125 of them were run, at
the fork commit `6d1bcdfe`: all 100 serial entries [@matrixserial] and 25 of the 50 OpenMP
entries [@matrixomp]. The other 25 OpenMP entries run `ww3_multi` with three grids, which the
port refuses. No MPI or hybrid entry was run. On each of the 125 entries, the output is
byte-identical to the Fortran DIA of the same executable. Section 1 of the kernel was also swept
over all $2^{32}$ `float32` inputs, with 0 differences [@adr0001]. Thus the tests of the model
that the port replaces validate it on 125 of 310 entries, not on every entry that calls it. This
validation is not a formal proof, and the note does not call it one.

A first formal layer is already next to the ports. The Lean 4 package `ocean-waves`
[@oceanwaves] states the discretised action balance equation and proves these properties, with
no `sorry` and no `axiom`:

- $\sigma^2(k)$ increases strictly with $k$;
- $c/2 \le c_g \le c$;
- the depth CFL condition keeps every Courant number in $[0, 1]$;
- upwind propagation and a DIA-shaped quadruplet exchange conserve action;
- the semi-implicit source step keeps the action positive for every time step.

The package uses exact `Rat` arithmetic on a reduced model: one space dimension, a fixed depth
and a Padé model of $\tanh$. Thus it also says nothing about bits. Agda is the next step. Agda
has no 32-bit float type, so it cannot prove a fact about bits. Its role is a total, executable
reference semantics of each source term. Each table index will have a `Fin` type, so every read
is in bounds for every grid, not only for one grid. The sweeps, the solver queries and the WW3
regression tests continue to decide the bits.
