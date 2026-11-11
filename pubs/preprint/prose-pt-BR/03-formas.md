# Duas formas para uma regra {#sec:shapes}

|                         | ww3-gpu (C++/Kokkos)                                                  | chaceon (Rust)                                   |
|:---------------------|:---------------------------------------|:-------------------------------------|
| Forma                   | Dentro do WW3: o Fortran chama C++ por um shim `bind(C)`, uma flag para cada rotina | Substitui o WW3: nenhum Fortran num processo do chaceon |
| Referência              | NOAA-EMC/WW3 `03adda91`, por um fork que acrescenta 5.687 linhas e não remove nenhuma | MetOcean WW3 7.14, `92e2cf66` |
| Paridade na CPU contra  | o caminho Fortran do mesmo executável                                 | uma referência compilada em separado             |
| Alvos de dispositivo    | Kokkos 5.2.2, backend CUDA (`sm_89`); OpenMP na CPU                   | rust-gpu (SPIR-V, Vulkan) e Rust-CUDA (PTX)      |
| Espectro residente no dispositivo | sim, no backend OpenMP                                      | sim, em SPIR-V no Vulkan de CPU (lavapipe)       |
| Tamanho do porte        | 62.257 linhas de C++, 355 arquivos (2026-10-14)                       | 29.229 linhas de porte em Rust, 5.936 linhas de ferramentas |
| Métodos formais         | consultas SMT, varreduras exaustivas                                  | uma prova em Lean 4 sobre a divisão de `f32`     |
| Licença                 | MIT, kernels em LGPL-3.0                                              | MPL-2.0                                          |

: Os dois portes. Fontes: chaceon `docs/WW3_GPU_PARITY_202611.md` (2026-11-09) e `README.md` em
`0c098fe` [@chaceon]; ww3-gpu `kokkos/PORT_STATUS.md` em `58167d7` [@ww3gpu]. {#tbl:shapes}

| Métrica                     | ww3-gpu                         | chaceon                         |
|:----------------------------|:--------------------------------|:--------------------------------|
| Repositório criado          | 2026-09-12, 01h31 UTC           | 2026-10-23, 03h58 UTC           |
| Primeiro commit no `main`   | 2026-09-11, 13h47 UTC, `b43a6d3` | 2026-10-23, 03h58 UTC, `3bad31a` |
| Último commit no `main`     | 2026-11-11, 03h16 UTC, `efc0211` | 2026-11-09, 08h53 UTC, `0c098fe` |
| Último push, qualquer branch | 2026-11-11, 03h31 UTC          | 2026-11-09, 08h53 UTC           |
| Commits no `main`           | 800                             | 110                             |

: Os dois repositórios. Fonte: a API REST do GitHub (`repos/h0ffmann/<repo>` para `created_at` e
`pushed_at`, `commits?sha=main` para os commits), lida em 2026-11-11 às 03h35 UTC. O primeiro
commit do ww3-gpu é mais antigo que o repositório porque o commit foi feito antes do primeiro
push. {#tbl:repos}

A rota Kokkos mantém todos os programas do WW3 e substitui uma rotina de cada vez. Por isso, um
regtest consegue isolar o efeito de uma rotina. A rota Rust precisa reconstruir os programas, os
formatos de arquivo e o laço de tempo antes de comparar uma execução inteira. Depois disso, nenhum
Fortran sobra para dividir estado com o porte.

O registro de progresso do ww3-gpu para a NOAA [@noaarecord] guarda os resultados de CPU do
runtime ww3-gpu inteiro, com o espectro residente, e os limites desses resultados. O porte da DIA
também está publicado sozinho, como ww3-dia-gpu-v2 [@diagpuv2]. O repositório ww3-dia-gpu-v2
compara o porte com um kernel CUDA anterior da mesma rotina.

A @fig:runtimes mostra os seis runtimes de um comando só, `scripts/ww3-gpu.sh`. O `--ww3` roda o
Fortran do WW3 e é a referência: cada um dos outros runtimes precisa dar os bits do `--ww3`. O
`--ww3-gpu` compila o fork com a switch KOKKOS e liga o porte Kokkos. Com `--cpu`, o `--ww3-gpu`
roda em OpenMP. Com `--gpu`, roda em CUDA. Nesse executável, uma flag para cada rotina escolhe o
código Fortran ou o código Kokkos. O `--chaceon`, ou `--cceon`, roda o WW3 reescrito em Rust, sem
Fortran no processo, na CPU ou em uma GPU. O `--ww4` é para o WAVEWATCH IV quando ele for
lançado. Hoje, o `--ww4` compila o código de desenvolvimento a partir de um fork e para com o
código de saída 5 quando o caso precisa de uma parte não portada. O `--ww3` compila o WW3 7.14 da
MetOcean em `92e2cf66`, e o script compara os arquivos de uma execução do `--chaceon` com os do
`--ww3`, compilado com o switch do chaceon. Duas setas da figura são o desenho, e ainda não o que
o script faz ⚠. O script compara o `--ww3-gpu` com o WW3 da NOAA em `03adda91`, a base do fork,
até o fork passar para o WW3 da MetOcean. E o `--chaceon --gpu` para com o código de saída 5,
porque o chaceon ainda não rodou em uma GPU real.

![Os seis runtimes do ww3-gpu. Uma seta tracejada aponta para a execução que precisa dar os mesmos
bits: as cinco apontam para o `--ww3`, a referência. Hoje o script compara o `--ww3-gpu` com o WW3
da NOAA em `03adda91`. Desenho do Archify a partir de
`docs/archify/ww3-gpu-runtimes-pt.json` [@ww3gpu].](figures/runtimes.pt-BR.pdf){#fig:runtimes width=100%}
