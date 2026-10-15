<div align="center">

<img src="site/static/favicon.svg" width="120" alt="" />

# ww3-gpu

<sub>o WW3 GPU Lab · desde 2026 · <a href="https://h0ffmann.github.io/ww3-gpu/">o site</a> · <a href="https://h0ffmann.github.io/ww3-gpu/book/">o livro</a> · <a href="RELEASES.md">versões</a> · <a href="#como-citar">citar</a> · <a href="README.md">english</a></sub>

[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.23221351-1682D4?style=flat-square)](https://doi.org/10.5281/zenodo.23221351)
[![versão](https://img.shields.io/github/v/tag/h0ffmann/ww3-gpu?style=flat-square&label=vers%C3%A3o&color=FF4F00)](RELEASES.md)
[![CI](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml/badge.svg)](https://github.com/h0ffmann/ww3-gpu/actions/workflows/ci.yml)
[![site](https://img.shields.io/badge/site-no%20ar-3FB950?style=flat-square)](https://h0ffmann.github.io/ww3-gpu/)
[![licença](https://img.shields.io/badge/licen%C3%A7a-MIT%20%2B%20kernels%20LGPL--3.0-1682D4?style=flat-square)](#licença)
[![read in english](https://img.shields.io/badge/read%20in-english-3FB950?style=flat-square)](README.md)

</div>

Um livro em construção sobre programação com agentes (*agentic coding*) e pesquisa com agentes
(*agentic research*), compilado a partir deste repositório e escrito em torno de um problema: levar
os trechos mais caros do WAVEWATCH III® (WW3), o modelo espectral de ondas de terceira geração da
NOAA, para a GPU sem mudar a resposta.

O repositório é a fonte do livro e a sua evidência. As 17 lições são os capítulos, gerados em PDF
a cada merge ([`pdf/ww3-lab-course.pdf`](pdf/ww3-lab-course.pdf)). O código que elas descrevem
está ao lado: um toolchain Fortran/MPI/NetCDF fixado com Nix que compila o WW3 e roda um teste de
regressão com um comando só, e um port para C++/Kokkos do termo de interações não lineares DIA
(`W3SNL1`), escrito com agentes de código, que reproduz a saída do Fortran bit a bit nos backends
Serial, OpenMP e CUDA. Numa RTX 4090, ele processa 1.000 pontos de mar em 0,047 ms, contra 24,96 ms
no serial ([`kokkos/PORT_STATUS.md`](kokkos/PORT_STATUS.md)). Cada afirmação traz a sua evidência
(`(v)` conferido, `⚠` não conferido) e cada número, o comando que o reproduz. O mesmo repositório é
a base de um projeto de graduação na Escola Politécnica da UFRJ, com coorientação no LabECO da
UFSC.

O público principal são cientistas: doutores, pós-doutorandos e pesquisadores independentes em
modelagem de ondas, métodos numéricos e HPC. O segundo leitor é o pesquisador ou engenheiro de
outra área que quer usar agentes de código em software científico sem perder a resposta. O
material técnico (lições, documentação, código) está em inglês; esta página resume o projeto em
português. Se o repositório for útil no seu trabalho, [cite-o](#como-citar).

## o livro

*Without Changing the Answer* (título provisório) segue a tradição dos livros técnicos
autopublicados a partir de um repositório público, como *Thinking with Types*, de Sandy Maguire,
e *Practical FP in Scala*, de Gabriel Volpe `(v)`
[MIP-0014 §4](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0014-marola-book.md)
do marola, lida em 2026-10-08. O assunto é um método: como agentes de código portam Fortran sob um
critério bit a bit e uma escada de validação (programação com agentes), e como planos, avaliações e
medições são escritos de modo que um agente possa ser cobrado por eles (pesquisa com agentes). As
regras em [`AGENTS.md`](AGENTS.md), os documentos de desenho em [`docs/WFIPs/`](docs/WFIPs/README.md),
as decisões em [`docs/ADRs/`](docs/ADRs/README.md) e o diário de pesquisa em
[`docs/log/`](docs/log/README.md) são capítulos à espera. A
[ADR-0003](docs/ADRs/ADR-0003-repository-is-a-book.md) registra a decisão e a
[WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md), o plano.

Cada versão etiquetada é uma edição, arquivada no Zenodo como livro ([`RELEASES.md`](RELEASES.md)).
Uma edição gratuita no Leanpub é alimentada pelo CI a partir dos mesmos capítulos
([`docs/LEANPUB_202610.md`](docs/LEANPUB_202610.md)).

```console
just book          # os capítulos em PDF (nix, o laboratório publisher do picasso)
just book-html     # os capítulos como páginas web, em build/site/book/
just leanpub       # os capítulos como manuscrito do Leanpub
just adrs          # uma linha por decisão
just wfip check    # os documentos de desenho e seus critérios de pronto
```

## o site

<https://h0ffmann.github.io/ww3-gpu/> é uma página inicial em Gleam + Lustre, pré-renderizada, sem
script. Cada fato que ela mostra vem dos arquivos deste repositório, lidos pelo `site/content`: os
capítulos, as decisões, os desenhos, as notas, o DOI e as pessoas. Os capítulos ficam em
[`/book/`](https://h0ffmann.github.io/ww3-gpu/book/), ao lado do
[PDF](https://h0ffmann.github.io/ww3-gpu/ww3-lab-course.pdf), da página de andamento (`/andamento/`)
e das decisões (`/adrs/`). O visual está em [`DESIGN.md`](DESIGN.md): um mar à noite, com três
luzes.

```console
just site          # os capítulos, depois a página inicial, em build/site/
just site-serve    # serve build/site em http://localhost:8000
```

<details><summary>Detalhes do ambiente</summary>

| Tipo | Programa |
| :---- | :-----: |
| Linguagem | Gleam (alvo JavaScript), Lustre |
| Empacotamento e testes | node, esbuild, gleeunit |
| Toolchain | o `labs/gleam-website` do picasso, pelo `flake.nix` (`nix develop .#site`) |
| Páginas do livro | pandoc (`scripts/build_html.sh`) |
| Publicação | `.github/workflows/pages.yml`, GitHub Pages |

</details>

## o segundo cérebro

As ideias em que este laboratório se apoia são notas em [`notes/`](notes/index.md): uma ideia por
nota, cada uma com a sua fonte e um status (`unverified` até a fonte ser conferida), ligadas entre
si, sob as regras do [`NOTES.md`](NOTES.md). O que foi tentado, inclusive o que deu errado, é o
diário: [`docs/log/`](docs/log/README.md), só por acréscimo. Onde fica cada tipo de memória está no
[`MEMORY.md`](MEMORY.md); a memória de longo prazo entre sessões e agentes é o
[ai-memory](https://github.com/akitaonrails/ai-memory), escolhido na
[ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md).

Parte dessa memória é publicada. As páginas do ai-memory que o dono marca com `publish` são
copiadas para [`memory/`](memory/README.md), que começa por [`HUMAN.md`](memory/HUMAN.md): quem é
o dono deste repositório e as ordens permanentes que deu, com as próprias palavras. Essas páginas são
pistas, não evidência; o que importa também está num arquivo daqui com o seu `(v)` ou `⚠`. Com o
plugin do ai-memory instalado no Claude Code (os passos estão no [`MEMORY.md`](MEMORY.md)),
carregue-as no seu próprio acervo depois de clonar e depois de cada pull que mude `memory/`. Elas
entram em `published/`, sem fixação e nunca como regra, então uma página vinda de um pull request
não vira uma ordem permanente no seu acervo.

```console
just notes          # as notas seguem as regras: cabeçalho, ids, links, o índice
just refs           # cada referência da proposta tem um veredito no seu REFERENCES.md
just memory import  # carrega memory/ no seu ai-memory; páginas iguais são puladas
just memory export  # dono: copia para memory/ as páginas do wiki marcadas com `publish`
just memory check   # memory/ só tem páginas publicadas: nada de sessão, log, e-mail ou caminho pessoal
```

## frentes de estudo

Cada linha é um trabalho separado. O que já foi integrado está no `main`; o resto aparece como pull
request ou issue aberta, com link para acompanhar.

| Frente | Pergunta | Onde | Situação |
|---|---|---|---|
| Curso | Como compilar, rodar e medir o WW3, e depois portar um kernel? | [`course/`](course/README.md), [`examples/`](examples/README.md), [`exercises/`](exercises/README.md) | 17 lições, integrado |
| Port do `W3SNL1` para Kokkos | Um kernel do WW3 roda na GPU com resultado idêntico bit a bit? | [`kokkos/`](kokkos/README.md), [lição 12](course/12-porting-a-kernel-w3snl1.md) | Idêntico em 3 backends; falta o replay dentro do WW3 |
| Benchmarks | Quanto um i9 e uma RTX 4090 entregam de fato para o WW3? | [`bench/`](bench/README.md), [`gpu/`](gpu/README.md), [lição 09](course/09-benchmark-profile-compile-run.md) | Ferramentas integradas |
| Prioridade do port | Qual rotina portar em seguida? | [ADR-0002](docs/ADRs/ADR-0002-port-order-wall-time.md), [`PORT_STATUS.md`](kokkos/PORT_STATUS.md) | Portar pela ordem do tempo de parede medido. Um primeiro perfil coloca o `W3SDS4` em 67 % do tempo dos termos-fonte ([#45](https://github.com/h0ffmann/ww3-gpu/issues/45)) |
| Port com agentes | Como agentes de código podem portar quarenta rotinas sem que uma pessoa refaça as verificações? | [`AGENTS_KOKKOS`](docs/AGENTS_KOKKOS_202609.md), [lição 13](course/13-bulk-porting-with-agents.md), [#42](https://github.com/h0ffmann/ww3-gpu/issues/42) | Regras integradas; fila de tarefas e escada de paridade na #42 |
| Ferramentas para agentes | Quais frameworks de agentes e ferramentas de pesquisa servem a esse fluxo? | [#25](https://github.com/h0ffmann/ww3-gpu/pull/25), [#41](https://github.com/h0ffmann/ww3-gpu/pull/41), [#38](https://github.com/h0ffmann/ww3-gpu/issues/38), [ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md) | Avaliações em revisão; ai-memory escolhido |
| Prova bit a bit | O que dá para provar, e não só testar, sobre a tradução de Fortran para C++? | [`BITWISE_PROOF`](docs/BITWISE_PROOF_202610.md), [`docs/proof/`](docs/proof/README.md), [ADR-0001](docs/ADRs/ADR-0001-proof-language.md) | Nenhuma linguagem de prova; varreduras exaustivas, diff de GIMPLE e consultas SMT. Seção 1 do `W3SNL1` varrida nas 2^32 entradas |
| Triton e vento de ML | Um kernel Triton do termo cumulativo do `W3SDS4` supera o do Kokkos? O vento do WeatherNext 3 melhora a previsão de ondas? | [#45](https://github.com/h0ffmann/ww3-gpu/issues/45), [`W3SDS4_TRITON_PLANO`](docs/W3SDS4_TRITON_PLANO_202610.pt.md) | Planejado |
| Port para uma H100 | O que exigiria um port completo para uma única H100? | [`KOKKOS_H100_PLAN`](docs/KOKKOS_H100_PLAN_202609.md) | Plano |
| WW4 e SWAN | O que substitui o WW3, e o que cobre a costa? | [lição 14](course/14-ww4-and-the-future.md), [lição 15](course/15-swan.md) | Integrado |
| O livro e a proposta | As lições compiladas em livro e a proposta do projeto | [`pubs/`](pubs/README.md), PDFs e arquivos Word em [`pdf/`](pdf/), [WFIP-0002](docs/WFIPs/WFIP-0002-lab-as-a-book.md) | Gerados pelo CI a cada merge |

A proposta do projeto de graduação está em português em [`pubs/proposal/pt/`](pubs/proposal/pt/),
com PDF em [`pdf/proposal_pt.pdf`](pdf/proposal_pt.pdf); a checagem de cada referência dela está em
[`pubs/proposal/REFERENCES.md`](pubs/proposal/REFERENCES.md). [`docs/GLOSSARY.md`](docs/GLOSSARY.md)
explica cada sigla, switch, rotina e ferramenta citada no repositório.

## primeiros passos

É preciso ter [Nix](https://nixos.org) e [just](https://github.com/casey/just). Os compiladores vêm
do flake fixado, então não há mais nada para instalar.

```console
git clone --recurse-submodules git@github.com:h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init             # sparse-checkout do nix-config (uma vez por clone)
just get                        # clona o develop do NOAA-EMC/WW3 em ~/src/WW3
just rt                         # compila com a switch do ww3_tp1.1 e roda esse regtest (~30 s)
just build                      # recompila com a switch do laboratório (física ST4)
just example01                  # primeiro exemplo do curso: crescimento limitado por pista (~1 min)
just kokkos-test serial-debug   # compila e testa os kernels C++/Kokkos
```

Depois disso, comece por [`course/00-orientation.md`](course/00-orientation.md). `just` lista todas
as tarefas, e cada uma chama um script em `scripts/`. O toolchain, o fork do WW3 e os presets do
Kokkos estão em [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md). Para casos costeiros, `just swan` instala
o SWAN.

## duas coisas para saber antes de investir

O WAVEWATCH IV™ (WW4) existe, e o WW3 tem fim programado. O
[NOAA-EMC/WW4](https://github.com/NOAA-EMC/WW4) é uma reescrita do zero, sem compatibilidade com o
WW3. Em 2026-10-07 ele não tinha física nem versão. O plano, com o compromisso de deixar de dar
suporte ao WW3 quando o WW4 amadurecer, está no
[NCEP Office Note 525](https://doi.org/10.25923/h7j3-1h25). Os detalhes estão na
[lição 14](course/14-ww4-and-the-future.md). O SWAN é a outra metade: o WW3 ao largo e o SWAN perto
da costa ([lição 15](course/15-swan.md)).

## o WW3 usa a minha GPU?

O WW3 oficial não tem suporte a GPU. O único port publicado
([Ikuyajolu et al., GMD 2023](https://gmd.copernicus.org/articles/16/1445/2023/)) colocou OpenACC
num módulo só, o `W3SRCEMD`, e chegou a cerca de 1,3× contra 42 núcleos de CPU nas V100 do Summit.
A transferência de dados limitou o ganho, e o código não entrou no `NOAA-EMC/WW3`. Este laboratório
segue outro caminho: kernels em Kokkos que mantêm os dados na GPU e reproduzem o Fortran bit a bit,
portados na ordem do tempo de parede medido ([lição 09](course/09-benchmark-profile-compile-run.md)).

Há três jeitos de rodar o WAVEWATCH aqui (`scripts/hoff.sh`). O código dos modelos vem só de forks
pessoais: o Fortran do WW3 pelo submódulo `WW3/` (h0ffmann/WW3, branch `hoff/kokkos-snl1`) e o C++
do WW4 pelo submódulo `WW4/` (h0ffmann/WW4, `develop`). Este repositório guarda só a reescrita em
Kokkos.

![ww3-gpu: três jeitos de rodar o WAVEWATCH. --ww3 roda o Fortran do WW3 do fork; --hoff liga o mesmo fork à reescrita em Kokkos deste repositório e compara os dois; --ww4 compila o WAVEWATCH IV do seu fork e para com código 5 quando falta portar uma parte que o caso usa](docs/figures/three-ways.svg)

```bash
just smoke             # smoke-ww3, depois smoke-hoff: regtest ww3_ts1, segundos
just smoke-ww3         # --ww3: o WW3 como está
just smoke-hoff        # --hoff: o WW3 com o DIA em Kokkos (W3SNL1), comparado com o Fortran
just full-ww3          # o caso caro: bench_small, 9 520 pontos de mar, minutos
just full-hoff         # o mesmo com o DIA em Kokkos
just smoke-ww4         # --ww4: o WAVEWATCH IV; código 5 enquanto faltar portar uma parte do caso
just full-ww4          # o mesmo no bench_small; `just ww4-parts full` lista as partes
just port-score        # quanto do WW3 roda em Kokkos, e as rotinas em ordem de tempo
```

O `--hoff` roda na CPU (Kokkos OpenMP) por padrão, e é comparado par a par com o `--ww3`. O
`just smoke-hoff gpu` roda em CUDA, só com uma pessoa supervisionando: ele pergunta no terminal
(regra rígida do AGENTS.md). Uma GPU NVIDIA cujo driver não responde para a execução com código 4
e a causa. Em 14 de outubro de
2026 os dois casos passaram em OpenMP; a rodada na GPU ainda não foi feita. Hoje o `--ww4` para
com código 5 nos dois casos: o `develop` do WW4 tem o laço de tempo e ainda nenhum termo fonte nem
solver de propagação
([`docs/PORT_ASSESSMENT_202610.md`](docs/PORT_ASSESSMENT_202610.md)).

## estrutura

A tabela completa está na seção [structure](README.md#structure) do README em inglês. Em resumo:
`course/` (os capítulos), `kokkos/` (os kernels), `bench/` e `gpu/` (as medições), `proof/` (a
prova), `docs/` (planos, decisões, desenhos, o diário), `notes/` (o segundo cérebro), `site/` (a
página inicial em Gleam) e `pubs/` (o livro e a proposta).

## convenções

- `⚠` marca uma afirmação não conferida. Confira antes de usar.
- `(v)` marca uma afirmação conferida numa fonte buscada durante o trabalho.
- Os arquivos de entrada usam a interface de namelist (`.nml`), não os `.inp` de formato fixo.
- Um commit traz o trailer `Tested:` e nenhum registro de custo
  ([ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md)).

## ponytail, desligado por padrão

O [ponytail](https://github.com/DietrichGebert/ponytail) é um plugin do Claude Code (MIT) cujas
regras levam o agente à menor mudança que funciona. Ele está habilitado aqui, fixado em `v5.2.0`, e
**desligado** por padrão (`PONYTAIL_DEFAULT_MODE=off` no [`.claude/settings.json`](.claude/settings.json)):
as regras valem para a sessão inteira, não podem ser limitadas a alguns arquivos ou linguagens, e
puxam contra "traduzir, não melhorar", a regra dos kernels portados ([`AGENTS.md`](AGENTS.md)).

- Ligue para uma sessão quando for trabalhar em C++ ao redor do porte (testes, o harness de
  benchmark, CMake): `/ponytail` (ou `/ponytail lite`, `/ponytail ultra`). Desligue com
  `/ponytail off` ou "stop ponytail".
- Em `kokkos/src/` uma rotina portada reproduz a aritmética do Fortran. O "menor mudança" do
  ponytail nunca autoriza ali uma refatoração, uma vetorização ou uma simplificação.
- O mapa do código dele está desligado (`PONYTAIL_MAP=0`). Ele lista só arquivos Python e JS, nunca
  C++, CUDA, Fortran ou Markdown, então aqui apontaria para as ferramentas em vez do código.
- Os comandos avulsos funcionam em qualquer modo: `/ponytail:ponytail-review` (uma mudança),
  `/ponytail:ponytail-audit` (o repositório), `/ponytail:ponytail-debt`, `/ponytail:ponytail-help`.

## autores relacionados

- **Guimarães P.V.** ([ORCID 0000-0002-1686-4722](https://orcid.org/0000-0002-1686-4722)),
  Departamento de Engenharia Mecânica da UFSC (LabECO): o coorientador do projeto de que este
  repositório é a base, e colaborador (Supervisor) no registro do Zenodo.
- **Fabio Akita** ([akitaonrails](https://github.com/akitaonrails)), lendário dev brasileiro
  ("legendary br dev", nas palavras do dono do repositório, 2026-10-14): o autor do
  [ai-memory](https://github.com/akitaonrails/ai-memory) (MIT), a memória de longo prazo que este
  repositório adota ([ADR-0009](docs/ADRs/ADR-0009-ww3-gpu-adopts-ai-memory-from-fabio-akita.md)).

## trabalhos relacionados

O [marola](https://github.com/marola-dev/marola) ([marola.dev](https://marola.dev/)) é uma
plataforma aberta e sem fins lucrativos sobre mar e balneabilidade nas praias brasileiras, do mesmo
autor com os membros do marola. Os dados de onda dele já vêm do WAVEWATCH III, pelo GFS-Wave do NCEP.
O próximo passo planejado é rodar um modelo espectral de ondas detalhado para as próprias baías
([MIP-0052](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0052-wave-model-compute.md)),
e este repositório é a base disso. Cite o marola pelo DOI conceitual,
[10.5281/zenodo.23224155](https://doi.org/10.5281/zenodo.23224155).

As obras irmãs de que este repositório toma a forma, todas do mesmo autor:
[picasso](https://doi.org/10.5281/zenodo.23311208) (o toolchain e os laboratórios de artigo e de
site), [*Closed Source: The God that Failed*](https://doi.org/10.5281/zenodo.23333656) (a disciplina
de citação e de artigo), [*Nashkell: Rhizomatic experiments*](https://doi.org/10.5281/zenodo.23356739)
(um livro aberto com site em Gleam) e [h0ffmann](https://doi.org/10.5281/zenodo.23352630) (as
notas, o site e as regras de tradução). Cada ligação do registro no Zenodo, e o que aponta de volta
para ele, está em [`RELATED_WORK.md`](RELATED_WORK.md).

## como citar

Para citar o projeto como um todo, use o DOI conceitual
[10.5281/zenodo.23221351](https://doi.org/10.5281/zenodo.23221351), que sempre aponta para a versão
mais recente. Para fixar exatamente o texto e o código lidos, cite o DOI da versão
([`RELEASES.md`](RELEASES.md) lista cada um).

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
`CITATION.cff`; o exemplo está na seção [how to cite](README.md#how-to-cite) do README em inglês. O
WW3 deve ser citado à parte, pelo manual do WAVEWATCH III Development Group da versão usada.
Cada depósito no Zenodo vai também para o Software Heritage, que arquiva todo o histórico do git a
cada versão ([`swh-save.yml`](.github/workflows/swh-save.yml)).

## licença

| O quê | De quem | Termos |
|---|---|---|
| Este repositório | Matheus Hoffmann | MIT ([`LICENSE`](LICENSE)) |
| Os kernels traduzidos do WW3 (lista na seção [licensing](README.md#licensing)) | obras derivadas do WW3 | `LGPL-3.0-or-later`, informada na linha `SPDX-License-Identifier` de cada arquivo |
| Skills de terceiros (`humanizar`, `humanizer`, `ponytail*`, `sharingan`) | seus autores | as licenças deles, fixadas em [`.claude/skills/skills.lock`](.claude/skills/skills.lock) |
| ai-memory, usado e não copiado | Fabio Akita | MIT |

## marcas

WAVEWATCH III® é marca registrada e WAVEWATCH IV™ é marca do National Weather Service da NOAA. Os
nomes aparecem aqui só para identificar esses programas. Este é um projeto independente de
aprendizado, sem vínculo, patrocínio ou endosso da NOAA.

## como contribuir

Veja [`CONTRIBUTING.md`](CONTRIBUTING.md). A contribuição mais útil é confirmar ou corrigir qualquer
afirmação marcada com `⚠`. Agentes de código leem antes o [`AGENTS.md`](AGENTS.md) e depois o
[`MEMORY.md`](MEMORY.md). Um padrão ou uma skill de outro repositório entra pela skill
[`sharingan`](.claude/skills/sharingan/SKILL.md), que fixa o commit e lê a licença antes de tudo.
