# O que cada projeto encontrou {#sec:found}

**ww3-gpu: a referência é o código otimizado.** A identidade byte a byte da DIA precisou de um
fato que o texto-fonte não dá. O gfortran expande `x**11` como uma cadeia de multiplicações em
`-O0` e como outra cadeia em `-O3`. As duas cadeias discordam em 643.427 de 1.000.000 entradas
de precisão simples. Antes dessa descoberta, o diário de pesquisa explicava as diferenças
restantes como arredondamento amplificado pelo WW3. Essa explicação estava errada. Por isso, a
referência do porte é o código que o build do WW3 produz, lido como GIMPLE ou desmontagem, e não o
texto Fortran.

O porte do ww3-gpu não tem testes unitários próprios. Só os testes de regressão do WW3,
comparados pelo `test.comp`, julgam o porte.

**chaceon: uma execução sem Fortran.** Os programas Rust `ww3_grid`, `ww3_strt`, `ww3_prep` e
`ww3_shel` rodam em sequência a partir das namelists. Os programas escrevem o `mod_def.ww3`, a
forçante, as saídas em grade e em pontos, o arquivo de trajetória e os dois restarts. Os 96
arquivos são idênticos byte a byte à referência. As fronteiras dão 214 comparações idênticas em
34 casos, e os pós-processadores de trajetória dão 74. O `ww3_shel --threads N` e um build MPI
dão os mesmos bytes.

Os arquivos continuam idênticos quando o `W3SRCE`, o `W3XYP3` e o `W3KTP3` são compilados para
SPIR-V e rodam num driver Vulkan de CPU. A enumeração não alcança os $2^{64}$ pares de operandos
`f32`. Nesse caso, uma prova em Lean enuncia o fato sobre a divisão que o porte usa. A prova não
diz nada sobre a igualdade entre Rust e Fortran, porque o Fortran não tem semântica formal
utilizável.

**Por que o ww3-gpu ainda não tem prova, e o que vem depois.** Nesta primeira fase, o ww3-gpu não
adotou assistente de prova [@adr0001]. Há quatro motivos:

1. Nenhum assistente de prova lê Fortran ou C++. Uma prova trata de um modelo que uma pessoa
   escreve. A prova não diz nada sobre os binários do gfortran e do nvcc enquanto ninguém mostrar
   que o modelo concorda com esses binários, e essa concordância é a questão em aberto.
2. A igualdade de bits é uma propriedade de dois binários, compilados com flags dadas. A igualdade
   de bits não é uma propriedade de dois textos-fonte.
3. Para código IEEE-754 sem desvios, a igualdade em todas as entradas é decidível. Consultas SMT na
   teoria `QF_FP` e varreduras sobre as $2^{32}$ entradas `float32` respondem a pergunta em
   segundos ou minutos. Uma prova interativa leva meses para cada rotina.
4. Agentes precisam escrever cada prova, e uma pessoa precisa revisar cada prova. A prova é um
   segundo artefato para cada rotina.

Sem prova, o porte da DIA tem a evidência mais forte abaixo de uma prova. O porte é idêntico
byte a byte nas 125 entradas da matriz de regressão do WW3 que selecionam a DIA (100 seriais, 25
OpenMP). A seção 1 do kernel também passou por uma varredura das $2^{32}$ entradas `float32`, com
0 diferenças [@adr0001]. Por isso, os testes do próprio modelo validam o porte da DIA em toda
entrada que chama o porte. Essa validação não é uma prova formal, e este texto não chama a
validação de prova.

Uma primeira camada formal já existe ao lado dos portes. O pacote Lean 4 `ocean-waves`
[@oceanwaves] enuncia a equação de balanço de ação discretizada e prova estas propriedades, sem
`sorry` e sem `axiom`:

- $\sigma^2(k)$ cresce estritamente com $k$;
- $c/2 \le c_g \le c$;
- a condição CFL de profundidade mantém todo número de Courant em $[0, 1]$;
- a propagação upwind e uma troca de quadrupletos no formato da DIA conservam a ação;
- o passo semi-implícito do termo-fonte mantém a ação positiva para todo passo de tempo.

O pacote usa aritmética `Rat` exata num modelo reduzido: uma dimensão espacial, profundidade fixa
e um modelo de Padé para $\tanh$. Por isso, o pacote também não diz nada sobre bits. O Agda é o
próximo passo. O Agda não tem tipo de ponto flutuante de 32 bits, então o Agda não consegue provar
um fato sobre bits. O papel do Agda é uma semântica de referência total e executável de cada
termo-fonte. Cada índice de tabela terá um tipo `Fin`, e toda leitura fica dentro dos limites para
toda grade, não só para uma grade. As varreduras, as consultas ao solver e os testes de regressão
do WW3 continuam a decidir os bits.
