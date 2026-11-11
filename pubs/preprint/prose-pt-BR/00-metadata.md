---
title: "Modelos de ondas em GPUs"
subtitle: "Portes do WAVEWATCH III em C++/Kokkos e em Rust que precisam dar os bits do Fortran"
author:
  - name: Matheus Hoffmann
    orcid: 0009-0009-1056-7661
    affiliation: Escola Politécnica, Universidade Federal do Rio de Janeiro, Brasil
  - name: Pedro Veras Guimarães
    orcid: 0000-0002-1686-4722
    affiliation: Coorientador do primeiro autor. LabECO, Departamento de Engenharia Mecânica, Universidade Federal de Santa Catarina, Brasil
  - name: Pedro Cruz
    orcid: 0000-0002-2019-133X
    affiliation: Orientador do primeiro autor. Grupo de Teleinformática e Automação (GTA), DEL, Escola Politécnica, Universidade Federal do Rio de Janeiro, Brasil
date: "2026-11-11"
version: "v0 (preprint, sem revisão por pares)"
lang: pt-BR
babel: brazilian
abstract-title: Resumo
keywords-title: Palavras-chave
abstract: |
  Este texto relata dois portes do modelo espectral de ondas WAVEWATCH III (WW3). Agentes de
  programação escreveram os dois portes, e os dois portes têm uma regra de aceitação só: o porte
  precisa dar os mesmos bits do Fortran. Uma tolerância é um diagnóstico e nunca é o veredito. O
  *ww3-gpu* mantém o WW3, e o Fortran chama código C++ escrito com Kokkos, atrás de uma flag de
  execução para cada rotina. A referência do ww3-gpu é o caminho Fortran do mesmo executável. O
  *chaceon* substitui o WW3 por Rust, compilado para a CPU, para SPIR-V (rust-gpu) e para PTX
  (Rust-CUDA). Nenhum Fortran roda no processo do chaceon, e a referência do chaceon é um WW3
  compilado em separado. O ww3-gpu porta a aproximação de interação discreta (DIA) da
  transferência entre quatro ondas. Na CPU, a DIA portada é idêntica byte a byte nas 100 entradas
  seriais e nas 25 entradas OpenMP da matriz de regressão do WW3 que selecionam a DIA. A DIA
  ocupa 48,4 % do tempo próprio em uma thread, num caso de 9.520 pontos. O chaceon roda
  `ww3_grid`, `ww3_strt`, `ww3_prep` e `ww3_shel` sem Fortran, e 96 arquivos de saída são
  idênticos byte a byte à referência. Nenhum dos dois portes rodou o modelo inteiro numa GPU real,
  e este texto não afirma ganho de velocidade. O texto compara como cada projeto define e confere
  a própria evidência.
keywords:
  - WAVEWATCH III
  - reprodutibilidade bit a bit
  - Kokkos
  - Rust
  - porte para GPU
  - agentes de programação
secPrefix: [Seção, Seções]
tblPrefix: [Tabela, Tabelas]
figPrefix: [Figura, Figuras]
tableTitle: Tabela
figureTitle: Figura
---
