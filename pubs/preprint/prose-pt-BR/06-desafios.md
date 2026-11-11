# Desafios: os testes de regressão são grandes {#sec:challenges}

Os testes de regressão do WW3 são o único juiz do porte Kokkos, e esses testes são grandes. A
@tbl:regtests dá o tamanho da matriz no commit `80644ba9` do fork, contado em 2026-10-18.

| Conjunto de entradas do `matrix.base`                              | Entradas          | Fração |
|:-------------------------------------------------------------------|:------------------|:-------|
| Todas as entradas                                                  | 758, em 60 testes | 100 %  |
| Entradas com a DIA (`NL1`)                                         | 310               | 41 %   |
| Entradas com a física ST4 (`ST4`)                                  | 136               | 18 %   |
| Entradas que o preset residente consegue selecionar                | 7 (todas seriais) | 0,9 %  |
| Regtests selecionados do ww3-gpu                                   | 5, e o par `ww3_ts1` | 0,7 % |

: Tamanho da matriz de regressão do WW3. Fonte: ww3-gpu `regtests/README.md` em `58167d7`,
contagens de `just ww3-gpu-matrix-list`. A contagem de todas as entradas é o número de linhas que
o `matrix.base` escreve, e uma seção de máquina consegue desligar algumas dessas linhas.
{#tbl:regtests}

Uma execução das 310 entradas `NL1` leva dias. O conjunto selecionado leva de uma a três horas
numa estação de trabalho (Intel i9-14900, 32 threads de hardware); esse tempo é uma estimativa, e
não uma medida. Por isso, uma execução da matriz completa não consegue acompanhar cada commit. O
ww3-gpu roda o conjunto selecionado como proteção e adia a matriz completa para uma fase de
validação. Até essa fase, um argumento estático sustenta a paridade de bits: o GIMPLE em `-O3` ou
a desmontagem dos dois compiladores.
