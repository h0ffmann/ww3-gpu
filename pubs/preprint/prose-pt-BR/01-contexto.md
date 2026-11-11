`\setcounter{section}{-1}`{=latex}

# Contexto: um modelo oceânico portado por um modelo de linguagem {#sec:context}

Koldunov et al. [@koldunov2026] relatam o porte de um modelo oceânico inteiro por um modelo de
linguagem grande. O modelo é o FESOM2, um modelo de oceano e gelo marinho, e os autores trabalham
no Alfred Wegener Institute. O Claude Code, com o Claude Opus 4.7, traduziu o FESOM2 de Fortran
para C e depois de C para C++/Kokkos. As pessoas definiram a estratégia, revisaram os planos e os
diffs e encontraram erros de física e de validação. O agente leu, traduziu, construiu os
ambientes de teste, depurou, mediu o desempenho e escreveu a documentação.

A etapa em C é o centro do método de Koldunov et al. O código C limpo é a referência, e cada
kernel Kokkos mantém o original em C como "gêmeo". A regra de aceitação muda a cada etapa. Os
autores comparam o modelo em C com o Fortran por estatística: em cinco anos, a diferença RMS da
média é 0,006 °C na temperatura da superfície do mar. Só o modelo Kokkos Serial é idêntico byte a
byte, e só ao modelo em C. Os autores escrevem que a concordância bit a bit entre Fortran e C
"across compilers and arithmetic orderings is neither attainable nor necessary". Na GPU, os
autores aceitam diferenças até um teto registrado para cada campo.

O sucessor do WW3 também está em desenvolvimento. A NCEP Office Note 525 [@on525] escolheu uma
reescrita de baixo para cima, com um núcleo em C++ primeiro e um núcleo em Rust depois ⚠. O mesmo
relatório rebaixa o Fortran a uma linguagem só de solvers ⚠. O repositório do WAVEWATCH IV [@ww4]
está em pré-alfa: em 2026-11-11, o branch `develop` tinha 40 commits, de 2025-11-17 a 2026-10-16
(`875f49a`). Os 97 arquivos-fonte do WAVEWATCH IV são C++ (66 `.cpp`, 31 `.h`), e nenhum kernel
de física do WW3 está no repositório ainda.

Por isso, os kernels centrais do modelo de ondas continuam em Fortran por alguns anos ⚠. Esse é o
primeiro motivo para o primeiro autor continuar a manter o ww3-gpu e o chaceon. Os dois projetos
fazem os kernels Fortran do WW3 rodarem em GPU agora, sem mudança nos bits.

Este texto parte desse resultado e faz uma pergunta mais estreita. Um agente consegue portar um
modelo espectral de ondas e dar os bits do próprio Fortran? Os dois portes abaixo não têm etapa
em C, e a referência dos dois portes é o Fortran. Os dois portes também usam duas práticas que o
FESOM2 relata: tradução estritamente literal e, para cada afirmação sobre o Fortran, um
`file:line` com o valor literal lido ali.
