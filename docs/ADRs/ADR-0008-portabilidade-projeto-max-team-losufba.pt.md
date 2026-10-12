# ADR-0008: O trabalho passa do projeto pessoal (Max) para a organização Team da colaboração com o LOSUFBA pelo repositório; threads, memória e rotinas não migram, e o projeto só muda de conta quando o Team tiver Projects

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-11 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, com um agente |
| **Scope** | O que do projeto Claude "Wave Forecaster", hoje na conta pessoal Max do responsável, pode ser levado para a organização Claude no plano Team usada na colaboração com o LOSUFBA, e como se trabalha nas duas enquanto o Team não tem Projects. Não trata de onde o código mora (isso é o ADR-0006, ainda em revisão no [PR #96](https://github.com/h0ffmann/ww3-gpu/pull/96)) nem de custo de assentos |
| **Related** | [ADR-0005](ADR-0005-claude-project-settings-in-repo.md) (o retrato das configurações do projeto em [`.claude/project/`](../../.claude/project/README.md), que é a base desta portabilidade); [lição 19](../../course/19-two-agents-one-issue-tracker.md) (dois projetos que conversam por issues, o mesmo padrão usado aqui entre contas); [`AGENTS.md`](../../AGENTS.md) e [`CLAUDE.md`](../../CLAUDE.md) (as regras que já viajam com o repositório) |

## Contexto

O responsável trabalha neste repositório a partir de um projeto do Claude Code ("Projects") na
sua conta pessoal, plano Max. O projeto coordena threads em paralelo, cada uma uma sessão na
nuvem, com instruções, memória, arquivos, rotinas e repositórios em comum. A colaboração com o
LOSUFBA usa outra conta, numa organização no plano Team. O nome LOSUFBA está como o responsável
o escreveu; a expansão da sigla e quem administra a organização Team não foram conferidos ⚠.

O que a documentação da Anthropic diz hoje, conferido em 2026-10-11 `(v)`:

- **Projects não existe no Team.** "Projects are in public beta on Pro and Max plans [...]. They
  aren't available on Team or Enterprise plans yet."
  ([Projects](https://code.claude.com/docs/en/claude-projects), nota de abertura). A ausência na
  organização Team não é uma configuração que o administrador possa ligar.
- **Um projeto é de uma pessoa só.** "A project belongs to one user. You can't share a project or
  its threads with another user [...]. There are no organization-level controls for projects
  during the beta." ([Projects](https://code.claude.com/docs/en/claude-projects), "Limitations").
- **Threads não saem do projeto.** "You can't move or copy a thread to another project, or move
  it out to stand alone." O caminho inverso existe: **Move to project** traz uma sessão na nuvem
  para dentro de um projeto (mesma página).
- **Apagar um projeto apaga threads, memória e arquivos**; branches e PRs no GitHub ficam
  ([Projects](https://code.claude.com/docs/en/claude-projects), "Pause, archive, or delete a
  project"). Arquivar só esconde.
- **Sessões na nuvem existem no Team.** "Cloud sessions are available on Pro, Max, and Team plans,
  and for Enterprise users with premium seats or Chat + Claude Code seats."
  ([Cloud](https://code.claude.com/docs/en/claude-code-on-the-web)). No Team, uma sessão pode ser
  compartilhada com visibilidade **Team**, para os membros da organização; no Max, só **Public**
  (mesma página, "Share sessions"). Um Owner pode desligar as sessões na nuvem pela política
  `allow_remote_sessions`, e o **Quick setup** do GitHub vem desligado no Team.
- **Rotinas existem no Team, mas são da conta.** "Routines are available on Pro, Max, Team, and
  Enterprise plans." e "Routines belong to your individual claude.ai account. They are not shared
  with teammates" ([Routines](https://code.claude.com/docs/en/routines)). Um Owner do Team pode
  desligá-las para todos.
- **Segredos de rede não existem no Team.** No ambiente da nuvem, "Team and Enterprise plans don't
  have them yet"; variáveis de ambiente ficam visíveis a quem usa o ambiente
  ([Cloud](https://code.claude.com/docs/en/claude-code-on-the-web), "Security and isolation").
- **O mais próximo de um projeto compartilhado no Team é o Claude Tag**, o Claude nos canais do
  Slack, só em Team e Enterprise, configurado por um Owner, cobrado por uso de um saldo da
  organização ([Claude Tag](https://claude.com/docs/claude-tag/overview)). A página de Projects o
  descreve como o contraponto de equipe de um projeto, que "is yours alone".
- Os "Projects" do chat do claude.ai, pastas de conhecimento, são outro produto e existem em todos
  os planos; não estão em discussão aqui.

O que o projeto tem e o que acontece com cada parte, pelo retrato do ADR-0005 e pelas fontes acima:

| Parte | Onde vive | Vai para o Team? | Como |
|---|---|---|---|
| Regras para agentes: `AGENTS.md`, `CLAUDE.md`, `.claude/settings.json` (hooks, permissões), skills, o agente `revisor-proposta`, comandos | git | sim | já vai: toda sessão na nuvem lê o `CLAUDE.md` e as skills do clone, e os subagentes de `.claude/agents/` "are picked up automatically" `(v)` [Cloud](https://code.claude.com/docs/en/claude-code-on-the-web) |
| ADRs, WFIPs, planos, lições, o livro | git | sim | já vai |
| Retrato das configurações do projeto (`.claude/project/`) | git | sim | é o roteiro de restauração; seu README diz como recriar o projeto à mão |
| Instruções do projeto | serviço | sim | estão vazias `(v)` [`project.json`](../../.claude/project/project.json), lido em 2026-10-09; se ganharem texto, ele é copiado do retrato |
| Prompts das rotinas | serviço, com cópia em `.claude/project/routines/` | o texto sim, a rotina não | recria-se a rotina na conta Team a partir do arquivo; a rotina antiga continua na conta pessoal |
| Memória do projeto | serviço | não | está desligada nas configurações da conta do responsável, então não há o que levar; o que merecer durar vai para `AGENTS.md`, uma skill ou um documento (ADR-0005, item 4) |
| Threads e seus históricos | serviço | não | não se movem nem se copiam; o que delas importa já está em PRs, commits e documentos |
| Arquivos do projeto (`/mnt/project-files`) | serviço | não | o que for referência entra no repositório; o resto fica |
| Artefatos publicados | conta | não | o painel de andamento vai para o GitHub Pages ([PR #99](https://github.com/h0ffmann/ww3-gpu/pull/99)) |
| Conectores (Gmail, Calendar, Drive, Docs, GitHub) | conta | não | cada um é um login OAuth; reconecta-se na conta Team |
| Ambiente da nuvem | conta | não | recria-se; hoje é o "Default" com rede "Trusted" e sem script no serviço, o que o hook `SessionStart` do repositório supre |
| Acesso ao GitHub | conta e GitHub App | não | cada membro do Team conecta o próprio GitHub; o Claude GitHub App já está instalado em `h0ffmann/ww3-gpu` ⚠ (inferido de os PRs desta conta funcionarem) |

## Decisão

**O repositório é o veículo da portabilidade. Tudo o que um colaborador do LOSUFBA precisa para
trabalhar com agentes neste repositório vive em git e funciona em qualquer sessão Claude Code,
de qualquer plano. O projeto pessoal continua sendo onde o responsável coordena o trabalho, e só
é recriado na organização Team quando o Team tiver Projects.**

1. Nenhuma regra de trabalho mora só no projeto. Instruções, preferências e lições que valem para
   o LOSUFBA entram em `AGENTS.md`, `CLAUDE.md`, uma skill ou um documento, por PR. As instruções
   do projeto continuam vazias, e a memória, desligada.
2. Na organização Team, os colaboradores usam sessões na nuvem, a CLI ou o app de desktop no mesmo
   repositório, com os mesmos hooks e gates. O ponto de encontro entre as duas contas são issues e
   PRs no GitHub, como entre os dois projetos da lição 19. Sessões que o grupo precisa ver são
   compartilhadas com visibilidade **Team**.
3. Cada rotina roda numa conta só. Uma rotina que o grupo precise na conta Team é recriada lá a
   partir do prompt em `.claude/project/routines/`, e a da conta pessoal é desligada no mesmo dia,
   para que nenhuma issue seja respondida duas vezes nem nenhum PR de dependências saia em dobro.
4. Antes de qualquer troca de conta, os arquivos do projeto que servem de referência entram no
   repositório, e os artefatos que outras pessoas leem vão para o GitHub Pages.
5. Quando Projects chegar ao Team, cria-se o projeto na organização seguindo
   `.claude/project/README.md`, atualiza-se o retrato com o novo `read_on`, e o projeto pessoal é
   arquivado, não apagado, enquanto alguém ainda consultar suas threads.
6. Nada pago é ligado na organização Team por causa desta decisão (Claude Tag, créditos de uso,
   assentos) sem confirmação explícita do responsável e do Owner da organização.

## Alternativas consideradas

| Opção | Por que não |
|---|---|
| Compartilhar o projeto pessoal com os colaboradores | Impossível: um projeto é de um usuário só e não tem compartilhamento `(v)`. |
| Dar aos colaboradores acesso à conta Max | Uma conta pessoal é de uma pessoa; tudo o que ela faz no GitHub e nos conectores sai em nome do responsável `(v)` [Routines](https://code.claude.com/docs/en/routines), e compartilhar credenciais contraria os termos de uso ⚠ (não conferido no texto dos termos). |
| Mover todo o trabalho para o Team agora e abandonar o projeto | Perde-se a coordenação de threads, a visão geral e as rotinas ligadas ao projeto, sem ganho para os colaboradores, que já recebem pelo repositório tudo o que é portátil. |
| Adotar o Claude Tag no Slack como substituto do projeto | É o produto de equipe que mais se parece com um projeto, mas exige um workspace Slack pareado, um Owner e um saldo de uso da organização; não foi avaliado para este grupo e é custo novo. Fica como opção a revisitar. |
| Exportar memória, threads e arquivos por script | Não há exportação nem API fora de uma sessão do próprio projeto (ADR-0005, "Alternatives considered"), e a memória está desligada. |
| Esperar o Team ter Projects e não fazer nada até lá | Os colaboradores ficariam sem as regras e gates enquanto isso, quando o repositório já os entrega hoje. |

## Consequências

- Um colaborador do LOSUFBA que clona o repositório recebe as mesmas regras, hooks, skills e o
  revisor da proposta que as threads do projeto recebem, sem depender da conta pessoal.
- O responsável continua sendo o único que vê as threads do projeto. O que o grupo precisa saber
  sobre elas chega por PR, issue ou pelo painel de andamento.
- Regras que hoje um agente poderia guardar na memória do projeto passam a exigir um PR. É mais
  lento, e é o que a regra de evidência do repositório já pede.
- Duas contas podem disparar o mesmo tipo de automação; o item 3 da decisão é o que evita
  respostas em dobro, e depende de disciplina, não de uma verificação automática.
- A migração futura do projeto é um procedimento à mão, como no ADR-0005, e o retrato pode estar
  desatualizado no dia em que for usado; `read_on` diz de quando ele é.

## Revisitar quando

- A documentação de Projects deixar de dizer que o recurso não está no Team ou no Enterprise.
- Projects ganhar compartilhamento, controles de organização, exportação ou transferência entre
  contas.
- O Team ganhar segredos de rede no ambiente da nuvem, o que muda como uma rotina guarda chaves.
- O grupo decidir usar Slack, o que torna o Claude Tag uma alternativa a avaliar com custo medido.
- O ADR-0006 ([PR #96](https://github.com/h0ffmann/ww3-gpu/pull/96)) for aceito e parte das regras
  sair deste repositório para um submódulo.
