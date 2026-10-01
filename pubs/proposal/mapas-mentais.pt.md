# Mapas mentais da proposta (PT-BR)

Diagramas Mermaid que resumem a Proposta de Projeto de Graduação em `pubs/proposal/pt/`.
O GitHub renderiza os blocos abaixo diretamente; localmente, qualquer visualizador Mermaid serve.

## 1. Visão geral da proposta

```mermaid
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
      Kernels Kokkos com paridade e tempo
      Recomendação de operação
```

## 2. A escada de otimização e os critérios de paridade

```mermaid
flowchart TD
    R[Execução de referência congelada<br/>código, switches, namelists, grade, forçante] --> B[Benchmark reprodutível<br/>tempo por hora de previsão]
    B --> P[Perfil por rotina e por fase<br/>1, 4 e 16 processos MPI]
    P --> E1

    subgraph E1[Etapa 1 · Opções de compilação]
        direction LR
        E1a[compilador, flags, switches,<br/>MPI x OpenMP] --> E1g{bit a bit<br/>ou arredondamento?}
    end
    E1g -- sim --> E2
    E1g -- não --> X1[descartada]

    subgraph E2[Etapa 2 · Configuração da execução]
        direction LR
        E2a[decomposição, passos de tempo,<br/>saídas, restart, forçante] --> E2g{matriz do WW3<br/>bit a bit?}
    end
    E2g -- sim --> E3
    E2g -- não --> X2[descartada]

    subgraph E3[Etapa 3 · Fortran moderno]
        direction LR
        E3a[rotinas do topo do perfil,<br/>uma de cada vez, mesma aritmética] --> E3g{tolerância por campo<br/>e teste por rotina?}
    end
    E3g -- sim --> E4
    E3g -- não --> X3[descartada]

    subgraph E4[Etapa 4 · Kernels C++/Kokkos]
        direction LR
        E4a[só rotinas ainda dominantes<br/>após a etapa 3] --> E4g{ganho medido e<br/>paridade?}
    end
    E4g -- sim --> OP[Entra na configuração operacional]
    E4g -- não --> LIM[Medida do limite,<br/>recomendação de não operar em GPU]

    classDef gate fill:#fff3cd,stroke:#856404;
    class E1g,E2g,E3g,E4g gate;
```

## 3. Decisão de operação para um kernel em GPU

```mermaid
flowchart LR
    K[Kernel reescrito<br/>em C++/Kokkos] --> S1{Serial Kokkos<br/>idêntico bit a bit ao C?}
    S1 -- não --> F1[corrigir tradução]
    S1 -- sim --> S2{Paridade com o Fortran<br/>no caso operacional?}
    S2 -- não --> F2[não incorporado]
    S2 -- sim --> S3{Tempo do caso operacional<br/>menor com o kernel no H100?}
    S3 -- não --> F3[Relatório do limite:<br/>tráfego CPU-GPU por passo]
    S3 -- sim --> D[Decisão com o LabECO:<br/>tabela de tempo + relatório de paridade]
    D --> OP[Configuração operacional]
```

## 4. Testes hoje: WW3 e WW4 (levantamento de 15/09/2026)

```mermaid
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
      Atividade recente
        limpeza de avisos de compilação
        correções da CI e do Spack
        falhas intermitentes da matriz em aberto
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
      L1 e L2 reescritos em julho de 2026
      Build migrando para CMake puro
      Arquitetura CPU-GPU em aberto, Kokkos proposto
    O que o projeto constrói
      Comparador por campo com tolerâncias versionadas
      Testes por rotina com entradas capturadas
      Kernels testados na estrutura L1 e L2 do WW4
```

## 5. Interoperabilidade Fortran e Kokkos

```mermaid
flowchart TB
    subgraph BIN[Um único binário do WW3]
        direction LR
        F[Rotina Fortran original] 
        SW{Chave de execução}
        C[Interface bind C<br/>ISO_C_BINDING]
        K[Kernel Kokkos<br/>Views com o leiaute dos<br/>vetores espectrais do WW3]
        SW -- caminho original --> F
        SW -- caminho novo --> C --> K
    end
    K --> CPU[Backend serial ou OpenMP<br/>sem cópia de dados]
    K --> GPU[Backend CUDA no H100<br/>tráfego CPU-GPU medido por passo]
    BIN --> M[Matriz de regressão do WW3<br/>e comparador por campo<br/>rodam os dois caminhos sem recompilar]
```

## 6. Validação em escada da tradução (receita do FESOM2)

```mermaid
flowchart LR
    A[Fortran original] -- tradução literal,<br/>assistente LLM dirigido<br/>pelos especialistas --> B[C de referência]
    B -- verificado contra o Fortran<br/>em entradas capturadas --> B
    B -- expressão em Kokkos --> C[Kokkos, backend serial]
    C -- deve ser idêntico<br/>bit a bit ao C --> C
    C -- mesmo código --> D[Kokkos, backend CUDA]
    D -- comparação estatística<br/>e cronometragem no H100 --> D
    subgraph T[Testes por kernel]
        L1[L1 · espectro sintético JONSWAP<br/>tolerâncias declaradas]
        L2[L2 · caso de regressão mais próximo<br/>e caso operacional]
    end
    C --> T
    D --> T
```

## 7. Cronograma

```mermaid
gantt
    title Cronograma do projeto de graduação
    dateFormat YYYY-MM-DD
    axisFormat %m/%Y
    section Preparação
    Revisão bibliográfica e registro da configuração      :a1, 2026-10-01, 2026-10-31
    Benchmark reprodutível e perfil da referência          :a2, 2026-11-01, 2026-11-30
    section Otimização sem alterar código
    Etapa 1 · opções de compilação                         :b1, 2026-12-01, 2026-12-31
    Etapa 2 · configuração, comparador e testes por rotina :b2, 2027-01-01, 2027-01-31
    section Reescrita
    Etapa 3 · refatoração em Fortran moderno               :c1, 2027-02-01, 2027-02-28
    Etapa 4 · kernels C++/Kokkos, paridade e H100          :c2, 2027-03-01, 2027-03-31
    section Fechamento
    Decisão de operação, relatório final                   :d1, 2027-04-01, 2027-04-30
    Defesa                                                 :milestone, d2, 2027-05-01, 0d
    section Marco externo
    Primeiro lançamento previsto do WW4                    :milestone, w4, 2027-01-15, 0d
```

## 8. Riscos e mitigações

```mermaid
mindmap
  root((Riscos))
    Divergência silenciosa após reescrita
      Critérios de paridade
      Testes por rotina com entradas capturadas
    Etapa 4 consumir o tempo das anteriores
      Ordem fixa das etapas
      Só reescrever rotina com custo residual medido
    Ganho em GPU limitado pelo tráfego de dados
      Estado permanece no Fortran
      Resultado vira a medida do limite
      Recomendação de não operar em GPU
    Propagação própria da grade não estruturada
      Etapas 3 e 4 priorizam os termos de fonte
    Arquitetura do WW4 mudar
      Kokkos é a camada proposta no próprio WW4
      Artefatos no padrão L1 e L2 do WW4
```
