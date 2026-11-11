# The division fact, stated and proved {#sec:division}

Vulkan allows an error of 2.5 ULP in the `f32` division `OpFDiv`. Thus a SPIR-V division does
not always return the correctly rounded quotient, and the Fortran division does. chaceon
rewrites each `f32` division $a/b$: it widens $a$ and $b$ to `f64`, divides, and rounds the
result to `f32` [@chaceonlean]. The claim is that this result is the correctly rounded `f32`
quotient when the `f64` division has a relative error of at most $\tfrac{5}{2}\cdot 2^{-52}$,
for a quotient in the normal range.

Write $a = A\cdot 2^{e_a}$ and $b = B\cdot 2^{e_b}$, with integers $0 < A, B < 2^{24}$. The
factor $2^{e_a-e_b}$ is common to the quotient and to the `f32` grid, so let $q = A/B$. Two
adjacent `f32` values in the normal range are $k/2^{s}$ and $(k+1)/2^{s}$, with
$2^{23} \le k < 2^{24}$ and an integer $s$. Their midpoint is $m = M/2^{t}$, with $M = 2k+1$ and
$t = s+1$. Thus $M$ is odd, $2^{24} \le M < 2^{25}$, and $t$ is an integer that can be negative.

**Lemma 1 (no tie).** $q \ne m$.

*Proof.* Assume $A/B = M/2^{t}$. If $t \ge 0$, then $A\cdot 2^{t} = B\cdot M$. $M$ is odd, so $M$
divides $A$. But $0 < A < 2^{24} \le M$, a contradiction. If $t < 0$, then
$A = B\cdot M\cdot 2^{-t} \ge M \ge 2^{24}$, a contradiction. $\mdlgwhtsquare$

**Lemma 2 (margin).** $|q - m| \ge m/2^{49}$.

*Proof.* If $t \ge 0$, then $q - m = (A\cdot 2^{t} - B\cdot M)/(B\cdot 2^{t})$. The numerator is
an integer, and Lemma 1 says that it is not zero. Thus
$|q - m| \ge 1/(B\cdot 2^{t}) = m/(B\cdot M) > m/2^{49}$, because $B\cdot M < 2^{24}\cdot 2^{25}$.
If $t < 0$, then $m = M\cdot 2^{-t} \ge 2M \ge 2^{25}$ and $q \le A < 2^{24} \le m/2$. Thus
$|q - m| > m/2 > m/2^{49}$. $\mdlgwhtsquare$

**Lemma 3 (same side).** Let $r$ be a rational with $|r - q| \le \varepsilon q$, where
$\varepsilon = \tfrac{5}{2}\cdot 2^{-52}$. Then $r < m$ if and only if $q < m$, and $r \ne m$.

*Proof.* Let $\delta = 2^{-49}$, so $\varepsilon = \tfrac{5}{16}\delta$. Lemma 2 gives
$|q - m| \ge \delta m$. If $q < m$, then $r - q \le \varepsilon q < \delta q < \delta m \le m - q$,
so $r < m$. If $q > m$, then $q \ge (1+\delta)m$ and $r \ge (1-\varepsilon)q \ge
(1-\varepsilon)(1+\delta)m$. Here $(1-\varepsilon)(1+\delta) = 1 + \delta - \varepsilon -
\varepsilon\delta > 1$, because $\delta - \varepsilon = \tfrac{11}{16}\delta > \varepsilon\delta$.
Thus $r > m$. Lemma 1 excludes $q = m$. $\mdlgwhtsquare$

**Corollary.** Round to nearest between two adjacent grid values is the comparison with their
midpoint. $r$ and $q$ are on the same side of every midpoint of the normal range, and neither is
on one. Thus the `f64` result $r$ and the exact quotient $q$ round to the same `f32` value.

The Lean 4 file `lean/Chaceon/Division.lean` of chaceon states these facts over $\mathbb{Q}$ as
the theorems `no_tie`, `margin`, `same_side` and `rounds_same`. The chaceon record says that the
file builds with no `sorry`, and that each theorem uses only the axioms `propext`,
`Classical.choice` and `Quot.sound` [@chaceonlean]. That build was not run again for this note.

The fact has these limits [@chaceonlean]:

- It does not cover a subnormal quotient. There, a midpoint has $M < 2^{24}$ and a tie can
  occur: $1/2 = M/2^{t}$ with $M = 1$ and $t = 1$.
- It does not cover overflow, a zero, an infinity or a NaN.
- The corollary is about two adjacent values and one midpoint at a time. It does not define
  round to nearest over the whole grid.
- It assumes that the widening to `f64` is exact, that the conversion to `f32` rounds to
  nearest, and that the device meets the 2.5 ULP limit. No Vulkan device has run the rewritten
  code ⚠.
- It is a fact about rational numbers. It says nothing about the Rust or the SPIR-V code.
