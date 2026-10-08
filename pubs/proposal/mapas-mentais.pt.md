# Mapas mentais da proposta (PT-BR)

Diagramas Mermaid que resumem a Proposta de Projeto de Graduação em `pubs/proposal/pt/`.
O GitHub renderiza os blocos abaixo diretamente; localmente, qualquer visualizador Mermaid serve.
Cada diagrama traz, logo abaixo, um quadro *Como ler esta figura* para quem chega ao tema agora,
e a galeria [`pubs/figures/`](../figures/README.md) reúne as versões renderizadas.

## 1. Visão geral da proposta

```mermaid
%% figure: proposta-visao-geral
%% title: Do que trata a proposta, em um único mapa?
mindmap
  root((Otimização operacional do WW3 para GPU))
    Contexto
      LabECO/UFSC roda o WW3 para a ReNOMO
      Custo de uma previsão é o tempo de execução
      WW3 em Fortran, MPI e OpenMP, fixado por switches
    Problema de engenharia
      Benchmark reprodutível
      Perfil por rotina e por fase
      Otimizações de profundidade crescente
      Nenhuma etapa aceita com regressão de resultados
    Quatro etapas em ordem
      1 Opções de compilação
      2 Configuração da execução
      3 Refatoração em Fortran moderno
      4 Kernels em C++/Kokkos e GPU H100
    Relação com o WW4
      Reescrita do NOAA, sem física ainda
      WW3 segue operacional por anos
      Testes L1 e L2 do WW4 reaproveitados
    Entregas
      Registro de configuração
      Tabelas de benchmark e perfil
      Melhor build documentado
      Comparador por campo e testes por rotina
      Kernels Kokkos com concordância e tempo
      Recomendação de operação
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** O projeto mede quanto tempo o WW3 leva para produzir a previsão de ondas do LabECO e reduz esse tempo em quatro etapas, aceitando só as mudanças cujos resultados fiquem dentro das tolerâncias acordadas com o laboratório.

**Como ler.** O centro é o tema; cada ramo é uma parte da proposta, e as folhas resumem o que o texto diz sobre ela. Só o ramo das etapas tem ordem: elas são numeradas na sequência em que serão executadas.

**Fora da figura.** Os números citados no texto e as referências; o mapa resume a estrutura da proposta, não resultados.

**Evidência.** Capítulos `pt/03-theme.md` a `pt/07-methodology.md` (v).

</details>

## 2. A escada de otimização e os critérios de concordância

```mermaid
%% figure: proposta-escada-otimizacao
%% title: Que critério cada etapa de otimização precisa cumprir antes da seguinte?
flowchart TD
    R[Execução de referência congelada<br/>código, switches, namelists, grade, forçante] --> B[Benchmark reprodutível<br/>tempo por hora de previsão]
    B --> P[Perfil por rotina e por fase<br/>1, 4 e 16 processos MPI]
    P --> E1a

    subgraph E1[Etapa 1 · Opções de compilação]
        direction LR
        E1a[compilador, flags, switches,<br/>forçante, MPI x OpenMP] --> E1g{bit a bit ou<br/>dentro da tolerância?}
    end
    E1g -- sim --> E2a
    E1g -- não --> X1[descartada]

    subgraph E2[Etapa 2 · Configuração da execução]
        direction LR
        E2a[passos de tempo,<br/>saídas, restart] --> E2g{dentro da tolerância<br/>por campo?}
    end
    E2g -- sim --> E3a
    E2g -- não --> X2[descartada]

    subgraph E3[Etapa 3 · Fortran moderno]
        direction LR
        E3a[rotinas do topo do perfil,<br/>uma de cada vez, mesma aritmética] --> E3g{tolerância por campo<br/>e teste por rotina?}
    end
    E3g -- sim --> E4a
    E3g -- não --> X3[descartada]

    subgraph E4[Etapa 4 · Kernels C++/Kokkos]
        direction LR
        E4a[só rotinas ainda dominantes<br/>após a etapa 3] --> E4g{ganho medido e<br/>concordância?}
    end
    E4g -- sim --> OP[Entra na configuração operacional]
    E4g -- não --> LIM[Medida do limite,<br/>recomendação de não operar em GPU]

```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** As otimizações vão da mais barata (opções de compilação) à mais cara (reescrita para GPU), e cada alteração só avança se os resultados continuarem concordando com a execução de referência e, na etapa 4, se houver ganho medido.

**Como ler.** De cima para baixo. As três caixas do topo fixam a referência e os instrumentos de medida (*benchmark* e perfil). Cada grupo é uma etapa: à esquerda, o que muda; à direita, o losango com o critério de aceite. Um *sim* leva à etapa seguinte; um *não* descarta a alteração nas etapas 1 a 3 e, na etapa 4, leva ao relatório do limite.

**Fora da figura.** Os valores das tolerâncias, que o grupo de pesquisa do LabECO definirá para cada campo, e o retorno da alteração reprovada à reescrita.

**Evidência.** `pt/04-scope.md` (as quatro etapas) e `pt/07-methodology.md`, parágrafo *Critérios de concordância e ordem dos testes* (v).

</details>

## 3. Decisão de operação para um kernel em GPU

```mermaid
%% figure: proposta-decisao-operacao
%% title: Quando um kernel em GPU entra na configuração operacional?
flowchart LR
    K[Kernel reescrito<br/>em C++/Kokkos] --> S1{Serial Kokkos<br/>idêntico bit a bit ao C?}
    S1 -- não --> F1[corrigir tradução]
    S1 -- sim --> S2{Concordância com o Fortran<br/>no caso operacional?}
    S2 -- não --> F2[não incorporado]
    S2 -- sim --> S3{Tempo do caso operacional<br/>menor com o kernel no H100?}
    S3 -- não --> F3[Relatório do limite:<br/>tráfego CPU-GPU por passo]
    S3 -- sim --> D[Decisão com o LabECO:<br/>tabela de tempo + relatório de concordância]
    D --> OP[Configuração operacional]
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** Uma rotina reescrita para GPU só passa a ser usada nas previsões se seus resultados concordarem com os da original dentro das tolerâncias e se ela tornar a previsão completa mais rápida no H100.

**Como ler.** Da esquerda para a direita. Cada losango é um teste, feito nessa ordem; um *não* termina em uma caixa que diz o que acontece no lugar, e só três respostas *sim* levam à decisão com o LabECO.

**Fora da figura.** As tolerâncias de cada campo e o protocolo de medição de tempo (pelo menos cinco repetições, mediana e intervalo interquartil).

**Evidência.** `pt/07-methodology.md`: o parágrafo de abertura (protocolo de tempo) e os parágrafos *Interoperabilidade e testes dos kernels* e *Riscos* (v).

</details>

## 4. Testes hoje: WW3 e WW4 (levantamento de 15/09/2026)

```mermaid
%% figure: testes-ww3-ww4
%% title: Que testes o WW3 e o WW4 ofereciam em setembro e outubro de 2026, e o que o projeto acrescenta?
mindmap
  root((Testes))
    WW3 develop 7.14
      62 casos de regressão em regtests
      Matriz de scripts compila combinações de switches
      Comparação arquivo a arquivo com cmp e diff
        idêntico ou não idêntico
        sem tolerância numérica
      Variantes bit a bit
        restart
        número de threads
        número de processos
      Testes unitários só de entrada e saída
        5 programas via CTest, desde 2023
      CI pública
        build GNU e Intel
        um único caso de regressão com MPI
        matriz completa só nas máquinas do NCEP
    WW4 develop
      Criado em novembro de 2025
      36 commits, nenhuma release
      Fase II concluída em março de 2026
      Núcleo C++ sem física
      Quatro níveis de teste
        L1 unitário, GoogleTest
        L2 integração, GoogleTest
        L3 funcional, ainda não existe
        L4 regressão, ainda não existe
      L1 e L2 cobrem o código existente
      Arquitetura CPU-GPU em aberto, Kokkos proposto
    O que o projeto constrói
      Comparador por campo com tolerâncias versionadas
      Testes por rotina com entradas capturadas
      Kernels testados na estrutura L1 e L2 do WW4
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** O WW3 só informa se duas execuções são idênticas ou não, e o WW4 ainda não tem testes do modelo completo; o projeto constrói o comparador com tolerâncias e os testes por rotina que faltam.

**Como ler.** Três ramos saem do centro: o WW3, o WW4 e o que o projeto constrói. As folhas são fatos do levantamento, e as mais externas detalham a folha de onde saem.

**Fora da figura.** Os nomes dos casos de regressão e dos programas de teste. O levantamento do WW3 é de 15/09/2026 e o do WW4, de 07/10/2026; o estado do WW4 pode ter mudado desde então.

**Evidência.** `pt/05-justification.md`, parágrafos *Estado dos testes do WW3* e *Estado do WW4* (v). ⚠ Quatro folhas não estão nesses parágrafos: "36 commits, nenhuma release", no ramo do WW4, e as três folhas de "CI pública", no ramo do WW3; vêm do levantamento e não foram reverificadas.

</details>

## 5. Interoperabilidade Fortran e Kokkos

```mermaid
%% figure: proposta-interoperabilidade
%% title: Como a rotina original em Fortran e o kernel em Kokkos convivem em um único executável?
flowchart TB
    subgraph BIN[Um único binário do WW3]
        direction LR
        F[Rotina Fortran original] 
        SW{Chave de execução}
        C[Interface bind C<br/>ISO_C_BINDING]
        K[Kernel Kokkos<br/>Views com o leiaute dos<br/>vetores espectrais do WW3]
        SW -- caminho original --> F
        SW -- caminho novo --> C -- chama --> K
    end
    K -- roda em --> CPU[Backend serial ou OpenMP<br/>sem cópia de dados]
    K -- roda em --> GPU[Backend CUDA no H100<br/>tráfego CPU-GPU medido por passo]
    M[Matriz de regressão do WW3<br/>e comparador por campo] -. testam os dois caminhos<br/>sem recompilar .-> BIN
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** O código novo em C++/Kokkos entra ao lado do Fortran original, e não no lugar dele; uma chave lida durante a execução escolhe qual dos dois roda, e os dois caminhos são testados com o mesmo executável.

**Como ler.** A caixa grande é um único executável do WW3. O losango é a chave de execução; suas duas setas são os caminhos que uma chamada pode seguir, o Fortran original ou o novo, pela interface em C até o kernel em Kokkos. Abaixo da caixa, o mesmo kernel roda em CPU ou em GPU. A seta tracejada é o teste: a matriz de regressão e o comparador exercitam os dois caminhos do mesmo executável.

**Fora da figura.** A alternativa de manter os dados na GPU entre um passo de tempo e outro, que a proposta deixa para trabalho futuro (parágrafo *Riscos*).

**Evidência.** `pt/07-methodology.md`, parágrafo *Interoperabilidade e testes dos kernels* (v).

</details>

## 6. Validação em escada da tradução (receita do FESOM2)

```mermaid
%% figure: proposta-validacao-traducao
%% title: Como cada versão traduzida é verificada contra a anterior?
flowchart LR
    A[1 · Fortran original] -- tradução literal,<br/>assistente LLM dirigido<br/>pelos especialistas --> B[2 · C de referência]
    B -- reescrito<br/>com Kokkos --> C[3 · Kokkos,<br/>backend serial]
    C -- mesmo código,<br/>compilado para GPU --> D[4 · Kokkos,<br/>backend CUDA]
    B -. verificado contra,<br/>entradas capturadas .-> A
    C -. idêntico bit a bit a .-> B
    D -. comparação estatística,<br/>cronometragem no H100 .-> C
    subgraph T[Testes por kernel]
        L1[L1 · espectro sintético JONSWAP<br/>tolerâncias declaradas]
        L2[L2 · caso de regressão mais próximo<br/>e caso operacional]
    end
    C -- precisa passar --> T
    D -- precisa passar --> T
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** O Fortran é traduzido para C e depois para C++/Kokkos em passos pequenos, e cada versão é verificada contra a anterior: o Kokkos serial deve ser idêntico bit a bit ao C, e a versão em GPU é comparada estatisticamente com a de CPU.

**Como ler.** As caixas numeradas são quatro versões da mesma rotina, feitas nessa ordem. A seta contínua diz como a versão seguinte é escrita; a seta tracejada aponta de volta para a versão contra a qual ela é verificada, e o rótulo diz o rigor da verificação. As duas versões em Kokkos também precisam passar nos testes L1 e L2 do quadro *Testes por kernel*, que segue a estrutura de testes do WW4.

**Fora da figura.** As tolerâncias de cada teste e o atalho usado no `W3SNL1`, em que a etapa em C foi dispensada e o Kokkos serial foi comparado diretamente com o Fortran (`course/12-porting-a-kernel-w3snl1.md`).

**Evidência.** `pt/07-methodology.md`, parágrafo *Interoperabilidade e testes dos kernels*, que segue Koldunov et al. (2026) (v).

</details>

## 7. Cronograma

```mermaid
%% figure: proposta-cronograma
%% title: Quando acontece cada etapa do projeto?
gantt
    title Cronograma do projeto de graduação
    dateFormat YYYY-MM-DD
    axisFormat %m/%Y
    todayMarker off
    section Preparação
    Revisão bibliográfica e registro da configuração      :a1, 2026-10-01, 2026-10-31
    Benchmark, grades simplificadas, comparador e perfil   :a2, 2026-11-01, 2026-11-30
    section Otimização sem alterar código
    Etapa 1 · opções de compilação                         :b1, 2026-12-01, 2026-12-31
    Etapa 2 · configuração e testes por rotina             :b2, 2027-01-01, 2027-01-31
    section Reescrita
    Etapa 3 · refatoração em Fortran moderno               :c1, 2027-02-01, 2027-02-28
    Etapa 4 · kernels C++/Kokkos, concordância e H100      :c2, 2027-03-01, 2027-03-31
    section Fechamento
    Decisão de operação, relatório final                   :d1, 2027-04-01, 2027-04-30
    Defesa                                                 :milestone, d2, 2027-05-01, 0d
    section Marco externo
    Primeiro lançamento do WW4 (ON 525, meados de 2027)    :milestone, w4, 2027-07-01, 0d
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** O projeto ocupa dois períodos letivos, de outubro de 2026 a abril de 2027, com uma atividade por mês e a defesa a partir de maio de 2027.

**Como ler.** O eixo horizontal é o tempo, em meses; cada barra é uma atividade, e cada seção agrupa atividades afins. Os marcos (losangos, sem duração) são a defesa e, como referência externa, o primeiro lançamento previsto do WW4.

**Fora da figura.** Os ajustes de datas com os orientadores, que o texto prevê.

**Evidência.** Tabela de `pt/08-schedule.md` (v); o marco do WW4 vem de `pt/05-justification.md`, que o situa em meados de 2027 (v). ⚠ Os dias 01/05/2027 (a defesa ocorre "a partir de" maio) e 01/07/2027 são só a posição no gráfico.

</details>

## 8. Riscos e mitigações

```mermaid
%% figure: proposta-riscos
%% title: Quais são os riscos do projeto e como cada um é mitigado?
mindmap
  root((Riscos e respostas))
    Risco 1 · divergência de resultados que passe despercebida
      Resposta: critérios de concordância
      Resposta: testes por rotina com entradas capturadas
    Risco 2 · etapa 4 consumir o tempo das anteriores
      Resposta: ordem fixa das etapas
      Resposta: só reescrever rotina com custo residual medido
    Risco 3 · ganho em GPU limitado pela transferência de dados
      Causa: estado permanece no Fortran
      Resposta: o resultado vira a medida do limite
      Próximo passo: dados residentes na GPU
    Risco 4 · atraso nos casos do LabECO ou no acesso ao H100
      Resposta: regtest oficial como substituto provisório
      Resposta: medições na GPU disponível, com ressalva
```

<details open>
<summary>Como ler esta figura</summary>

**Em uma frase.** São quatro riscos, e para cada um a proposta já define a resposta: testes contra divergências, ordem fixa das etapas, medida do limite imposto pela transferência CPU–GPU e substitutos provisórios para atrasos.

**Como ler.** Cada ramo é um risco, numerado do principal ao quarto, na ordem do texto. As folhas dizem o que o projeto fará a respeito (*Resposta*); no risco 3, uma folha dá a causa e outra, o passo seguinte ao projeto.

**Fora da figura.** A probabilidade e o impacto de cada risco, que o texto não quantifica.

**Evidência.** `pt/07-methodology.md`, parágrafo *Riscos* (v).

</details>
