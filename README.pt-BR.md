# WW3 GPU Lab

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23221351.svg)](https://doi.org/10.5281/zenodo.23221351)
[![CI](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml/badge.svg)](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml)
[![Licença: MIT + kernels LGPL-3.0](https://img.shields.io/badge/licen%C3%A7a-MIT%20%2B%20kernels%20LGPL--3.0-blue)](README.md#licensing)
[![Read in English](https://img.shields.io/badge/read%20in-English-green)](README.md)

Um livro em construção sobre programação com agentes (*agentic coding*) e pesquisa com agentes
(*agentic research*), compilado a partir deste repositório e escrito em torno de um problema: levar
os trechos mais caros do WAVEWATCH III® (WW3), o modelo espectral de ondas de terceira geração da
NOAA, para a GPU sem mudar a resposta.

O repositório é a fonte do livro e a sua evidência. As 16 lições são os capítulos, gerados em PDF
a cada merge ([`pdf/ww3-lab-course.pdf`](pdf/ww3-lab-course.pdf)); o código que elas descrevem
está ao lado: um toolchain Fortran/MPI/NetCDF fixado com Nix que compila o WW3 e roda um teste de
regressão com um comando só, e um port para C++/Kokkos do termo de interações não lineares DIA
(`W3SNL1`), escrito com agentes de código, que reproduz a saída do Fortran bit a bit nos backends
Serial, OpenMP e CUDA. Numa RTX 4090, ele processa 1.000 pontos de mar em 0,047 ms, contra 24,96 ms
no serial ([`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md)). Os planos, as decisões e as medições
que os capítulos citam trazem a evidência (`(v)` conferido, `⚠` não conferido) e o comando que
reproduz cada número. O mesmo repositório é a base de um projeto de graduação na Escola
Politécnica da UFRJ, com coorientação no LabECO da UFSC.

O público principal são cientistas: doutores, pós-doutorandos e pesquisadores independentes em
modelagem de ondas, métodos numéricos e HPC. O segundo leitor é o pesquisador ou engenheiro de
outra área que quer usar agentes de código em software científico sem perder a resposta. O
material técnico (lições, documentação, código) está em inglês; esta página resume o projeto em
português. Se o repositório for útil no seu trabalho, [cite-o](#como-citar).

## O livro

*Without Changing the Answer* (título provisório) segue a tradição dos livros técnicos
autopublicados a partir de um repositório público, como *Thinking with Types*, de Sandy Maguire,
e *Practical FP in Scala*, de Gabriel Volpe `(v)`
[MIP-0014 §4](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md)
do marola, lida em 2026-10-08. O assunto é um método, não só um modelo: como agentes de código
portam Fortran sob um critério bit a bit e uma escada de validação (programação com agentes), e
como planos, avaliações e medições são escritos de modo que um agente possa ser cobrado por eles,
cada afirmação com a sua evidência (pesquisa com agentes). O repositório é conduzido do jeito que o
livro diz que se conduz um, então as regras em [`AGENTS.md`](AGENTS.md), os documentos de desenho
em [`docs/WFIPs/`](docs/WFIPs/README.md), as decisões em [`docs/ADRs/`](docs/ADRs/README.md) e o
diário de pesquisa em [`docs/log/`](docs/log/README.md) são capítulos à espera.

A [ADR-0003](docs/ADRs/ADR-0003-repository-is-a-book.md) registra a decisão e a
[WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md) o plano: partes e páginas iniciais, uma
verificação que confere cada listagem citada contra o código a cada build, o PDF anexado a cada
versão e três capítulos novos sobre como este laboratório é conduzido com agentes. A mudança é
incremental, um pull request por tarefa, e todas as checagens passam depois de cada um. Cada
versão etiquetada é uma edição, arquivada com o código no Zenodo. `just book` gera o livro;
[`pubs/README.md`](pubs/README.md) descreve a cadeia. Uma edição gratuita no Leanpub é alimentada
pelo CI a partir dos mesmos capítulos ([`docs/LEANPUB_202610.md`](docs/LEANPUB_202610.md): o que o
workflow faz e os passos que só o autor pode dar); a
[ADR-0004](docs/ADRs/ADR-0004-book-channels-and-first-slice.md) registra os canais e o que a
primeira fatia mediu.

## Frentes de estudo

Cada linha é um trabalho separado. O que já foi integrado está no `main`; o resto aparece como pull
request ou issue aberta, com link para acompanhar.

| Frente | Pergunta | Onde | Situação |
|---|---|---|---|
| Curso | Como compilar, rodar e medir o WW3, e depois portar um kernel? | [`course/`](course/README.md), [`examples/`](examples/README.md), [`exercises/`](exercises/README.md) | 16 lições, integrado |
| Port do `W3SNL1` para Kokkos | Um kernel do WW3 roda na GPU com resultado idêntico bit a bit? | [`kokkos/`](kokkos/README.md), [lição 12](course/12-porting-a-kernel-w3snl1.md) | Idêntico em 3 backends; falta o replay dentro do WW3 |
| Benchmarks | Quanto um i9 e uma RTX 4090 entregam de fato para o WW3? | [`bench/`](bench/README.md), [`gpu/`](gpu/README.md), [lição 09](course/09-benchmark-profile-compile-run.md) | Ferramentas integradas |
| Prioridade do port | Qual rotina portar em seguida? | [#46](https://github.com/h0ffmann/ww3-gpu/pull/46), [`PORT_STATUS.md`](kokkos/PORT_STATUS.md) | Regra em revisão: portar pela ordem do tempo de parede medido. Um primeiro perfil coloca o `W3SDS4` em 67 % do tempo dos termos-fonte ([#45](https://github.com/h0ffmann/ww3-gpu/issues/45)) |
| Port com agentes | Como agentes de código podem portar quarenta rotinas sem que uma pessoa refaça as verificações? | [`AGENTS_KOKKOS`](docs/AGENTS_KOKKOS_202609.md), [lição 13](course/13-bulk-porting-with-agents.md), [#42](https://github.com/h0ffmann/ww3-gpu/issues/42) | Regras integradas; fila de tarefas e escada de paridade na #42 |
| Ferramentas para agentes | Quais frameworks de agentes e ferramentas de pesquisa servem a esse fluxo? | [#25](https://github.com/h0ffmann/ww3-gpu/pull/25) (NVIDIA NOOA), [#41](https://github.com/h0ffmann/ww3-gpu/pull/41) (Consensus, Antigravity, NotebookLM), [#38](https://github.com/h0ffmann/ww3-gpu/issues/38) | Avaliações em revisão |
| Prova bit a bit | O que dá para provar, e não só testar, sobre a tradução de Fortran para C++, e o port deve adotar uma linguagem de prova? | [`BITWISE_PROOF`](docs/BITWISE_PROOF_202610.md), [`docs/proof/`](docs/proof/README.md), [ADR-0001](docs/ADRs/ADR-0001-proof-language.md) | Decisão proposta: nenhuma linguagem de prova; varreduras exaustivas, diff de GIMPLE e consultas SMT. Seção 1 do `W3SNL1` varrida nas 2^32 entradas |
| Triton e vento de ML | Um kernel Triton do termo cumulativo do `W3SDS4` supera o do Kokkos, e o Fortran consegue chamá-lo? O vento do WeatherNext 3, do Google, melhora a previsão de ondas? | [#45](https://github.com/h0ffmann/ww3-gpu/issues/45), [`W3SDS4_TRITON_PLANO`](docs/W3SDS4_TRITON_PLANO_202610.pt.md) | Planejado, com o AIFS Single Wave do ECMWF como referência de ondas por ML; o plano de port e o peso entre CPU e GPU num H100 estão escritos |
| Port para uma H100 | O que exigiria um port completo para uma única H100? | [`KOKKOS_H100_PLAN`](docs/KOKKOS_H100_PLAN_202609.md) | Plano |
| WW4 e SWAN | O que substitui o WW3, e o que cobre a costa? | [lição 14](course/14-ww4-and-the-future.md), [lição 15](course/15-swan.md) | Integrado |
| O livro e a proposta | As lições compiladas em livro e a proposta do projeto | [`pubs/`](pubs/README.md), PDFs e arquivos Word em [`pdf/`](pdf/), [WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md) | Gerados pelo CI a cada merge; partes, verificação de listagens e capítulos novos planejados |

A proposta do projeto de graduação está em português em [`pubs/proposal/pt/`](pubs/proposal/pt/),
com PDF em [`pdf/proposal_pt.pdf`](pdf/proposal_pt.pdf). [`docs/GLOSSARY.md`](docs/GLOSSARY.md)
explica cada sigla, switch, rotina e ferramenta citada no repositório. Toda mudança não trivial é
desenhada antes de ser feita como uma *Wave Forecaster Improvement Proposal* (WFIP), ligada a uma
entrega prometida na proposta e com seu critério de pronto; [`docs/WFIPs/README.md`](docs/WFIPs/README.md)
é o índice e o relatório de progresso.

## Primeiros passos

É preciso ter [Nix](https://nixos.org) e [just](https://github.com/casey/just). Os compiladores vêm
do flake fixado, então não há mais nada para instalar.

```bash
git clone --recurse-submodules git@github.com:h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init   # sparse-checkout do nix-config (uma vez por clone)
just get              # clona o develop do NOAA-EMC/WW3 em ~/src/WW3
just rt               # compila com a switch do ww3_tp1.1 e roda esse regtest (~30 s)
just build            # recompila com a switch do laboratório (switches/switch_lab_shrd, física ST4)
just example01        # primeiro exemplo do curso: crescimento limitado por pista (~1 min)
just kokkos-test serial-debug   # compila e testa os kernels C++/Kokkos
```

Depois disso, comece por [`course/00-orientation.md`](course/00-orientation.md). `just` lista todas
as tarefas, e cada uma chama um script em `scripts/`. O toolchain, o fork do WW3 e os presets do
Kokkos estão descritos em [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md).

## O WW3 usa a minha GPU?

O WW3 oficial não tem suporte a GPU. O único port publicado (Ikuyajolu et al., GMD 2023) colocou
OpenACC num módulo só, o `W3SRCEMD`, e chegou a cerca de 1,3× contra 42 núcleos de CPU nas V100 do
Summit. A transferência de dados limitou o ganho, e o código não entrou no `NOAA-EMC/WW3`.

Este laboratório segue outro caminho: kernels em Kokkos que mantêm os dados na GPU e reproduzem o
Fortran bit a bit, portados na ordem do tempo de parede medido. A justificativa e o plano de
experimentos estão na [lição 09](course/09-benchmark-profile-compile-run.md).

## Como citar

Para citar o projeto como um todo, use o DOI conceitual
[10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351), que sempre aponta para a versão
mais recente. Para fixar exatamente o código usado, cite o DOI e o título da versão. A v0.1.1, a primeira com o título abaixo, é
[10.5281/zenodo.23233540](https://doi.org/10.5281/zenodo.23233540); a v0.1.0 é
[10.5281/zenodo.23221352](https://doi.org/10.5281/zenodo.23221352), com o título que tinha na época.

ABNT (NBR 6023):

> HOFFMANN, Matheus. **WW3 GPU Lab**: agentic coding and agentic research on a bit-for-bit GPU
> port of WAVEWATCH III. [S. l.]: Zenodo, 2026. DOI 10.5281/zenodo.23221351.
> Disponível em: https://doi.org/10.5281/zenodo.23221351.

BibTeX:

```bibtex
@software{hoffmann_ww3gpu,
  author    = {Hoffmann, Matheus},
  title     = {{WW3 GPU Lab: agentic coding and agentic research on a bit-for-bit GPU port of WAVEWATCH III}},
  year      = {2026},
  publisher = {Zenodo},
  doi       = {10.5281/zenodo.23221351},
  url       = {https://github.com/h0ffmann/ww3-gpu}
}
```

O botão **Cite this repository**, na lateral do GitHub, exporta APA e BibTeX a partir do
[`CITATION.cff`](CITATION.cff). Um projeto que dependa deste pode declarar a referência no próprio
`CITATION.cff`; o exemplo está na seção [How to cite](README.md#how-to-cite) do README em inglês. O
WW3 deve ser citado à parte, pelo manual do WAVEWATCH III Development Group da versão usada.
Cada depósito no Zenodo vai também para o Software Heritage, que arquiva ainda todo o histórico
do git a cada versão ([`swh-save.yml`](.github/workflows/swh-save.yml)).

## Trabalhos relacionados

O [marola](https://github.com/marola-dev/marola) ([marola.dev](https://marola.dev/)) é uma
plataforma aberta e sem fins lucrativos sobre mar e balneabilidade nas praias brasileiras, feita
com dados públicos, do mesmo autor com Bruno Valério. O mapa dele
ordena, hora a hora, as praias de Florianópolis, do Rio de Janeiro e de Salvador a
partir das previsões de mar e tempo do Open-Meteo e dos boletins oficiais de balneabilidade. Os
dados de onda dele já vêm do WAVEWATCH III, pelo GFS-Wave do NCEP. O próximo passo planejado é rodar
um modelo espectral de ondas detalhado para as próprias baías
([MIP-0052](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0052-wave-model-compute.md)),
e este repositório é a base disso: o registro do marola no Zenodo cita o WW3 GPU Lab como trabalho
relacionado `(v)`. Cite o marola pelo DOI conceitual, [10.5281/zenodo.23224155](https://doi.org/10.5281/zenodo.23224155), que sempre aponta
para a versão mais recente (a v0.2.0 é [10.5281/zenodo.23224156](https://doi.org/10.5281/zenodo.23224156)).

## Licença e marcas

O repositório é MIT, exceto os kernels traduzidos do WW3, que são obras derivadas dele e usam
`LGPL-3.0-or-later`. Cada arquivo informa a licença numa linha `SPDX-License-Identifier`. Os detalhes
estão na seção [Licensing](README.md#licensing).

WAVEWATCH III® é marca registrada e WAVEWATCH IV™ é marca do National Weather Service da NOAA. Os
nomes aparecem aqui só para identificar esses programas. Este é um projeto independente de
aprendizado, sem vínculo, patrocínio ou endosso da NOAA.

## Como contribuir

Veja [`CONTRIBUTING.md`](CONTRIBUTING.md). A contribuição mais útil é confirmar ou corrigir qualquer
afirmação marcada com `⚠`. Agentes de código leem antes o [`AGENTS.md`](AGENTS.md): as invariantes do
repositório, onde cada mudança entra e as checagens que a CI roda. Um padrão ou uma skill de outro
repositório entra pela skill [`sharingan`](.claude/skills/sharingan/SKILL.md), que fixa o commit,
lê a licença e mapeia cada conceito para o lugar dele aqui antes de escrever qualquer coisa.
