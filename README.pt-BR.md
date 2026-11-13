<div align="center">

<img src="https://wave.piabrota.art/favicon.svg" width="120" alt="" />

# ww3-gpu

<sub>um modelo de previsão de ondas rodando mais rápido em placas de vídeo, sem mudar as respostas · <a href="https://wave.piabrota.art/pt-BR/">site</a> · <a href="README.md">english</a></sub>

[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.23221351-1682D4?style=flat-square)](https://doi.org/10.5281/zenodo.23221351)
[![aiXiv](https://img.shields.io/badge/aiXiv-2611.01104-B31B1B?style=flat-square)](https://aixiv.online/abs/2611.01104)
[![versão](https://img.shields.io/github/v/tag/h0ffmann/ww3-gpu?style=flat-square&label=vers%C3%A3o&color=FF4F00)](RELEASES.md)
[![CI](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml/badge.svg)](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml)
[![licença](https://img.shields.io/badge/licen%C3%A7a-MIT%20%2B%20kernels%20LGPL--3.0-1682D4?style=flat-square)](#licença)
[![read in english](https://img.shields.io/badge/read%20in-english-3FB950?style=flat-square)](README.md)

</div>

> [!IMPORTANT]
> **Usado pela Universidade Federal de Santa Catarina (UFSC).** A previsão operacional de ondas do
> LabECO/UFSC, [ondaseventos.ufsc.br](https://ondaseventos.ufsc.br/), é a primeira usuária do
> ww3-gpu; o [marola](https://github.com/marola-dev/marola) é o segundo.

## o problema

**A previsão de ondas sai de um modelo de computador.** O WAVEWATCH III® (WW3) é o modelo de ondas
que a agência de meteorologia dos Estados Unidos (NOAA, National Oceanic and Atmospheric
Administration) e muitos grupos de pesquisa usam para prever ondas. Ele divide o oceano numa grade
de pontos. Em cada ponto, guarda quanta energia de onda vai em cada direção e em cada período de
onda (essa tabela se chama *espectro de ondas*). Depois, passo a passo no tempo, calcula como o
vento acrescenta energia, como a arrebentação retira energia, como as ondas trocam energia entre
si e como a energia se move pela grade.

**Esse cálculo é lento.** O WW3 é escrito em Fortran, uma linguagem de programação para computação
científica, e roda em processadores comuns (CPUs). Uma grade mais fina ou uma previsão mais longa
custa mais tempo de computação; um modelo mais rápido dá a previsão mais cedo, uma grade mais fina
ou mais previsões pelo mesmo custo.

**Placas de vídeo fazem isso mais rápido.** Uma GPU (unidade de processamento gráfico) é um chip
com milhares de núcleos pequenos que fazem o mesmo cálculo em muitos dados de uma vez. Um modelo de
ondas faz o mesmo cálculo em cada ponto da grade, e por isso combina com uma GPU. O WW3 não tem
versão oficial para GPU.

## o que o projeto faz

Reescrevemos as partes mais lentas do WW3 em C++ com
[Kokkos](kokkos/README.md#why-kokkos-not-fortran-90-in-plain-words), uma biblioteca com a qual o
mesmo código roda nos núcleos da CPU ou numa GPU NVIDIA. Reescrevemos uma rotina (uma parte
autocontida do modelo) por vez, a começar pela que gasta mais tempo de execução.

**A regra: as respostas não podem mudar.** Cada rotina reescrita tem de dar exatamente os mesmos
números do Fortran original, até o último dígito binário ("bit a bit"), dentro do mesmo programa.
Se os números diferem, a reescrita está errada. Números idênticos significam que o código mais
rápido não muda nada em que um previsor confia.

Para facilitar a comparação, não editamos o WW3. Uma cópia do WW3 (um *fork*) acrescenta uma chave
a cada rotina reescrita. Na hora de rodar, você escolhe o Fortran original ou o código novo, roda o
mesmo caso duas vezes e compara os arquivos de saída byte a byte.

## onde o projeto está

Os números abaixo vêm do [`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md), o registro do port,
gerado por `just port-score`. A fração do tempo de execução foi medida numa grade de teste de
9 520 pontos, num núcleo de CPU.

| Etapa | Rotinas do WW3 | Fração do tempo de execução que cobrem |
|---|---|---|
| reescritas: existe uma versão em C++ | 344 de 1 023 | 99,3 % |
| conectadas: o WW3 chama a versão em C++ quando a chave está ligada | 72 | 99,3 % |
| conferidas com a bateria de testes do próprio WW3 | 14 | 48,6 % |

O que já foi mostrado, e o que não foi:

- **Mostrado:** a rotina que gastava mais tempo (`W3SNL1`, a troca de energia entre ondas) dá
  saída idêntica byte a byte ao Fortran em todos os 100 casos de um núcleo da bateria de testes do
  WW3 que a usam, em CPU.
- **Mostrado:** numa placa de vídeo (NVIDIA RTX 4090), essa rotina levou 0,047 ms para 1 000 pontos
  da grade, contra 24,96 ms num núcleo de CPU
  ([registro do benchmark](bench/results/snl1-i9-14900-rtx4090-2026-09-15.json)). Esse é só o
  cálculo: com as cópias dos dados para a placa e de volta, uma chamada levou 0,75 ms, e por isso o
  projeto passa a manter os dados na placa durante toda a execução.
- **Inicial:** com o passo de tempo inteiro em C++ numa CPU, o caso de 9 520 pontos rodou em
  17,98 s, contra 41,92 s do Fortran (2,33 vezes mais rápido), com saída idêntica. É uma única
  execução numa máquina ocupada, ainda não um benchmark de verdade
  ([entrada do diário](docs/log/2026-10-17-resident-timing-cpu-gpu.md)).
- **Ainda não:** o modelo completo ainda não rodou numa GPU. A maioria das rotinas conectadas
  ainda não passou pela bateria de testes do WW3; essa conferência fica para uma fase de validação
  posterior ([registro da decisão](docs/ADRs.md#adr-0016)).

No repositório inteiro, `(v)` marca uma afirmação conferida com uma fonte e `⚠` marca uma que não
foi conferida.

## publicações

| O quê | Ler |
|---|---|
| Preprint, *Wave models on GPUs* (sem revisão por pares), de Matheus Hoffmann, Pedro Veras Guimarães, PhD (UFSC) e Pedro Cruz, D.Sc. (UFRJ). Compara este port em C++ com uma reescrita do WW3 em Rust presa à mesma regra | [aiXiv:2611.01104](https://aixiv.online/abs/2611.01104), [PDF em português](pubs/preprint/wave-models-on-gpus.pt-BR.pdf), [PDF em inglês](pubs/preprint/wave-models-on-gpus.pdf), [fonte](pubs/preprint/README.md) |
| Proposta de projeto de graduação, Departamento de Engenharia Elétrica (DEL) da Universidade Federal do Rio de Janeiro (UFRJ), com coorientação no LabECO/UFSC | [PDF em português](pdf/proposal_pt.pdf), [PDF em inglês](pdf/proposal_en.pdf), [fonte](pubs/proposal/) |
| Registro de progresso para os modeladores de ondas da NOAA: os resultados em CPU, seus limites e o que ainda não foi mostrado (em inglês) | [Markdown](docs/NOAA_PROGRESS_RECORD_202610.md), [PDF](pdf/NOAA_PROGRESS_RECORD_202610.pdf) |
| Relatório curto em português simples para o Grupo de Teleinformática e Automação (GTA) do DEL/UFRJ | [PDF](pdf/relatorio-gta-pedro-cruz.pdf) |
| Arquivo do software, cada versão com seu próprio DOI (identificador de objeto digital) | [Zenodo](https://doi.org/10.5281/zenodo.23221351), [notas das versões](RELEASES.md) |
| Site, com o andamento do port | [wave.piabrota.art/pt-BR](https://wave.piabrota.art/pt-BR/) |

## experimente

Você precisa do [Nix](https://nixos.org/download/) (um gerenciador de pacotes que instala as
versões exatas dos compiladores) e do [just](https://github.com/casey/just) (um executor de
comandos). Docker também serve; veja o [guia detalhado](docs/REPO_GUIDE_202611.pt.md#três-ambientes).

```console
git clone --recurse-submodules https://github.com/h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init   # baixa a cadeia de ferramentas fixada (uma vez por clone)
just smoke            # roda um caso pequeno do WW3 com o Fortran, depois com o código em C++, e compara (segundos)
just port-score       # quanto do WW3 roda no código em C++
```

`just` sozinho lista todas as tarefas.

## para onde ir depois

| Se você quer | Leia |
|---|---|
| achar o significado de um termo ou sigla | [`docs/GLOSSARY.md`](docs/GLOSSARY.md) (em inglês) |
| ver como o repositório roda o WW3, os modos de execução, os casos de teste, as imagens Docker e as verificações de saúde | [`docs/REPO_GUIDE_202611.pt.md`](docs/REPO_GUIDE_202611.pt.md), o guia detalhado |
| ver o estado de cada rotina | [`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md) (em inglês) |
| ver o sistema em figuras | [`docs/WW3_GPU_VS_NOAA_202610.md`](docs/WW3_GPU_VS_NOAA_202610.md) (em inglês) |
| saber o que falta para o uso operacional | [`docs/PRODUCTION_READINESS_202610.md`](docs/PRODUCTION_READINESS_202610.md) (em inglês) |
| ler as decisões e o diário de pesquisa | [`docs/ADRs.md`](docs/ADRs.md), [`docs/log/`](docs/log/README.md) |
| contribuir, ou trabalhar aqui com um agente de código | [`CONTRIBUTING.md`](CONTRIBUTING.md), [`AGENTS.md`](AGENTS.md) |

## trabalhos relacionados

- O [marola](https://github.com/marola-dev/marola) ([marola.dev](https://marola.dev/)) é uma
  plataforma aberta e sem fins lucrativos de condições do mar e balneabilidade nas praias
  brasileiras, do mesmo autor. Rodar um modelo de ondas detalhado para as suas próprias baías é o
  próximo passo planejado, e este repositório é a base.
- O [chaceon](https://github.com/h0ffmann/chaceon) é o WW3 reescrito em Rust, preso à mesma regra
  bit a bit.
- O único outro port do WW3 para GPU publicado é o de
  [Ikuyajolu et al. (2023)](https://gmd.copernicus.org/articles/16/1445/2023/), cerca de 1,3 vez
  mais rápido que 42 núcleos de CPU; ele não entrou no WW3.
- Mais, com cada vínculo do registro no Zenodo: [`RELATED_WORK.md`](RELATED_WORK.md).

## como citar

Cite o DOI conceito [10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351); ele sempre
aponta para a versão mais recente. O botão **Cite this repository** do GitHub exporta APA e BibTeX
a partir do [`CITATION.cff`](CITATION.cff).

ABNT (NBR 6023):

> HOFFMANN, Matheus. **WW3 GPU Lab**: agentic coding and agentic research on a bit-for-bit GPU
> port of WAVEWATCH III. [S. l.]: Zenodo, 2026. DOI 10.5281/zenodo.23221351.
> Disponível em: https://doi.org/10.5281/zenodo.23221351.

O BibTeX está no [README em inglês](README.md#how-to-cite). Cite o WW3 separadamente, pelo manual
do usuário do WAVEWATCH III Development Group da versão que você rodou.

## licença

Este repositório é MIT ([`LICENSE`](LICENSE)). Os arquivos em C++ traduzidos do Fortran do WW3 são
obras derivadas do WW3 e são LGPL-3.0-or-later, como diz o cabeçalho de cada arquivo. Nenhum código
do WW3 é copiado para cá; os scripts o baixam. A divisão completa está no [`NOTICE`](NOTICE) e em
[`LICENSES/`](LICENSES/).

WAVEWATCH III® é marca registrada do National Weather Service da NOAA, usada aqui só para nomear
esse software. Este é um projeto independente, sem vínculo, patrocínio ou endosso da NOAA.
