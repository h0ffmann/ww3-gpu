# Por que bit a bit {#sec:why}

O WW3 integra a equação de balanço de ação

$$\frac{\partial N}{\partial t} + \nabla\cdot(\dot{\mathbf{c}}\,N) = \frac{S}{\sigma},$$

em que o termo-fonte $S$ é a soma da entrada de vento, da dissipação, da transferência não linear
e dos termos de fundo. Um porte que muda a ordem de uma soma em ponto flutuante muda $N$. Um
sistema de previsão validado contra observações precisa então ser validado de novo. Por isso, os
dois projetos mantêm a aritmética do Fortran. Os dois projetos não reordenam reduções, não usam
multiplicação-adição fundida (`-ffp-contract=off` nos dois lados) e não alargam tolerâncias. Uma
diferença é defeito do porte até duas execuções do próprio Fortran darem bytes diferentes no
mesmo arquivo.
