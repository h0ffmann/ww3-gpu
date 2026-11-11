# O fato sobre a divisão, enunciado e provado {#sec:division}

O Vulkan permite um erro de 2,5 ULP na divisão `OpFDiv` de `f32`. Por isso, uma divisão em SPIR-V
nem sempre devolve o quociente corretamente arredondado, e a divisão do Fortran devolve. O chaceon
reescreve cada divisão $a/b$ de `f32`: o chaceon alarga $a$ e $b$ para `f64`, divide e arredonda o
resultado para `f32` [@chaceonlean]. A afirmação é esta: o resultado é o quociente `f32`
corretamente arredondado quando a divisão em `f64` tem erro relativo de no máximo
$\tfrac{5}{2}\cdot 2^{-52}$, para um quociente na faixa normal.

Escreva $a = A\cdot 2^{e_a}$ e $b = B\cdot 2^{e_b}$, com inteiros $0 < A, B < 2^{24}$. O fator
$2^{e_a-e_b}$ é comum ao quociente e à grade de `f32`, então seja $q = A/B$. Dois valores `f32`
adjacentes na faixa normal são $k/2^{s}$ e $(k+1)/2^{s}$, com $2^{23} \le k < 2^{24}$ e $s$
inteiro. O ponto médio desses valores é $m = M/2^{t}$, com $M = 2k+1$ e $t = s+1$. Por isso, $M$
é ímpar, $2^{24} \le M < 2^{25}$, e $t$ é um inteiro que pode ser negativo.

**Lema 1 (sem empate).** $q \ne m$.

*Prova.* Suponha $A/B = M/2^{t}$. Se $t \ge 0$, então $A\cdot 2^{t} = B\cdot M$. $M$ é ímpar,
então $M$ divide $A$. Mas $0 < A < 2^{24} \le M$, uma contradição. Se $t < 0$, então
$A = B\cdot M\cdot 2^{-t} \ge M \ge 2^{24}$, uma contradição. $\mdlgwhtsquare$

**Lema 2 (margem).** $|q - m| \ge m/2^{49}$.

*Prova.* Se $t \ge 0$, então $q - m = (A\cdot 2^{t} - B\cdot M)/(B\cdot 2^{t})$. O numerador é um
inteiro, e o Lema 1 diz que o numerador não é zero. Por isso,
$|q - m| \ge 1/(B\cdot 2^{t}) = m/(B\cdot M) > m/2^{49}$, porque $B\cdot M < 2^{24}\cdot 2^{25}$.
Se $t < 0$, então $m = M\cdot 2^{-t} \ge 2M \ge 2^{25}$ e $q \le A < 2^{24} \le m/2$. Por isso,
$|q - m| > m/2 > m/2^{49}$. $\mdlgwhtsquare$

**Lema 3 (mesmo lado).** Seja $r$ um racional com $|r - q| \le \varepsilon q$, onde
$\varepsilon = \tfrac{5}{2}\cdot 2^{-52}$. Então $r < m$ se e somente se $q < m$, e $r \ne m$.

*Prova.* Seja $\delta = 2^{-49}$, então $\varepsilon = \tfrac{5}{16}\delta$. O Lema 2 dá
$|q - m| \ge \delta m$. Se $q < m$, então $r - q \le \varepsilon q < \delta q < \delta m \le m - q$,
logo $r < m$. Se $q > m$, então $q \ge (1+\delta)m$ e $r \ge (1-\varepsilon)q \ge
(1-\varepsilon)(1+\delta)m$. Aqui $(1-\varepsilon)(1+\delta) = 1 + \delta - \varepsilon -
\varepsilon\delta > 1$, porque $\delta - \varepsilon = \tfrac{11}{16}\delta > \varepsilon\delta$.
Logo $r > m$. O Lema 1 exclui $q = m$. $\mdlgwhtsquare$

**Corolário.** O arredondamento para o mais próximo entre dois valores adjacentes da grade é a
comparação com o ponto médio desses valores. $r$ e $q$ ficam do mesmo lado de todo ponto médio da
faixa normal, e nenhum dos dois fica sobre um ponto médio. Por isso, o resultado $r$ em `f64` e o
quociente exato $q$ arredondam para o mesmo valor `f32`.

O arquivo Lean 4 `lean/Chaceon/Division.lean` do chaceon enuncia esses fatos sobre $\mathbb{Q}$,
nos teoremas `no_tie`, `margin`, `same_side` e `rounds_same`. O registro do chaceon diz que o
arquivo compila sem `sorry` e que cada teorema usa só os axiomas `propext`, `Classical.choice` e
`Quot.sound` [@chaceonlean]. Essa compilação não foi repetida para este texto.

O fato tem estes limites [@chaceonlean]:

- O fato não cobre um quociente subnormal. Nesse caso, um ponto médio tem $M < 2^{24}$ e um empate
  pode ocorrer: $1/2 = M/2^{t}$ com $M = 1$ e $t = 1$.
- O fato não cobre overflow, zero, infinito ou NaN.
- O corolário trata de dois valores adjacentes e de um ponto médio por vez. O corolário não define
  o arredondamento para o mais próximo sobre a grade inteira.
- O fato supõe que o alargamento para `f64` é exato, que a conversão para `f32` arredonda para o
  mais próximo e que o dispositivo cumpre o limite de 2,5 ULP. Nenhum dispositivo Vulkan rodou o
  código reescrito ⚠.
- O fato trata de números racionais. O fato não diz nada sobre o código Rust ou SPIR-V.
