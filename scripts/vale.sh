#!/usr/bin/env bash
# vale — lint this repo's English Markdown for AI-writing tells (issue #36).
#
#   just vale            # the gate CI runs: .vale.ini, error-level rules only, fails on a hit
#   just vale --report   # every ai-tells rule as suggestions (.vale-report.ini), never fails
#   just vale --ste      # the ASD-STE100 checks (.vale-ste.ini, .vale/styles/STE), never fails
#   just vale --ptc      # Português Técnico Controlado on the pt-BR files (.vale-ptc.ini), never fails
#   just vale FILE...    # only those files; other --flags go to vale (--output=line)
#   just vale --self-test
#
# Out of scope: the submodules, .claude/ (vendored skills quote the tells as examples) and its
# copies in .omp/agents/,
# pubs/ (the proposal has its own reviewer and house style) and Portuguese files
# (*.pt.md, *.pt-BR.md), which the English rules would misread.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

scope() {
  git ls-files '*.md' | grep -v -E '^(WW3|WW4|picasso|bend-lang|\.claude|\.omp/agents|\.specify|pubs|\.vale)/' \
    | grep -v -E '\.pt(-BR)?\.md$'
}

# The Portuguese Markdown, pubs/ included: --ptc reports and never gates, so the proposal's
# own reviewer is not bypassed.
ptc_scope() {
  git ls-files '*.md' | grep -v -E '^(WW3|WW4|picasso|bend-lang|chaceon|\.claude|\.omp/agents|\.specify|\.vale)/' \
    | grep -E '\.pt(-BR)?\.md$|(^|/)pt/|-pt-BR/'
}

if [[ "${1:-}" == --self-test ]]; then
  files="$(scope)"
  grep -q '^README.md$' <<<"$files"
  grep -q '^docs/GLOSSARY.md$' <<<"$files"
  if grep -E '^(pubs|\.claude)/|\.pt(-BR)?\.md$' <<<"$files"; then exit 1; fi
  if command -v vale >/dev/null; then
    # Each STE rule fires on its own sample and stays silent on the gate's config.
    t="$(mktemp -d)"; trap 'rm -rf "$t"' EXIT
    printf '%s\n' 'Prior to the run, utilize the shim to set up the build after building it.' '' \
      'The files were written by the shim. It should be noted that the run would have stopped.' '' \
      'One two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty more words here now done again.' '' \
      'A. B. C. D. E. F. G.' >"$t/s.md"
    got="$(vale --config=.vale-ste.ini --no-exit --output=line "$t/s.md" | cut -d: -f4 | sort -u | tr '\n' ' ')"
    want='STE.IngForm STE.ParagraphLength STE.PassiveVoice STE.PhrasalVerbs STE.SentenceLength STE.Tenses STE.Words STE.Wordy '
    [[ "$got" == "$want" ]] || { echo "vale.sh self-test: STE rules fired '$got', want '$want'" >&2; exit 1; }
    vale --config=.vale.ini "$t/s.md" >/dev/null || { echo "vale.sh self-test: an STE rule fails the gate" >&2; exit 1; }
    printf '%s\n' 'Faz-se a validação e o mesmo foi apagado. Far-se-á o backup.' '' \
      'Deve-se rodar o script, gerando o relatório. O processo deveria parar, uma vez que caiu.' '' \
      'Realize a validação. Utilize o ficheiro da infra-estrutura com 1.5 s e 10MB, um simples teste.' '' \
      'Além disso, a solução potencializa tudo.' '' \
      'Um dois três quatro cinco seis sete oito nove dez onze doze treze catorze quinze dezesseis dezessete dezoito dezenove vinte vinte um vinte dois vinte três e mais palavras aqui agora.' '' \
      'A. B. C. D. E. F. G.' >"$t/p.pt.md"
    got="$(vale --config=.vale-ptc.ini --no-exit --output=line "$t/p.pt.md" | cut -d: -f4 | sort -u | tr '\n' ' ')"
    want='PTC.Adjetivo PTC.Conectores PTC.Correferencia PTC.FormaVerbal PTC.Formato PTC.Lexico PTC.Mesoclise PTC.Modalidade PTC.Ortografia PTC.Ruido PTC.SeApassivador PTC.TamanhoFrase PTC.TamanhoParagrafo PTC.VerboSuporte '
    [[ "$got" == "$want" ]] || { echo "vale.sh self-test: PTC rules fired '$got', want '$want'" >&2; exit 1; }
    grep -q '^README.pt-BR.md$' < <(ptc_scope)
  fi
  echo "vale.sh self-test: ok ($(wc -l <<<"$files") files in scope)"
  exit 0
fi

config=.vale.ini; opts=(); files=(); ptc=0
for arg in "$@"; do
  case "$arg" in
    --report) config=.vale-report.ini; opts+=(--no-exit) ;;
    --ste) config=.vale-ste.ini; opts+=(--no-exit) ;;
    --ptc) config=.vale-ptc.ini; opts+=(--no-exit); ptc=1 ;;
    -*) opts+=("$arg") ;;
    *) files+=("$arg") ;;
  esac
done
command -v vale >/dev/null || { echo "vale not on PATH (https://vale.sh/docs/install)" >&2; exit 2; }

vale --config="$config" sync >/dev/null
if ((!${#files[@]})); then
  if ((ptc)); then mapfile -t files < <(ptc_scope); else mapfile -t files < <(scope); fi
fi
vale --config="$config" "${opts[@]}" "${files[@]}" </dev/null
