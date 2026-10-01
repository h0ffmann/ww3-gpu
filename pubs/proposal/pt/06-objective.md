<!-- revisado à mão em 2026-10-01 (PT-BR). scripts/translate_md.py só sobrescreve este arquivo se ../en/06-objective.md mudar ou com --force. -->
# OBJETIVO

O objetivo geral é reduzir, de forma medida e reprodutível, o tempo de execução do ciclo operacional do WAVEWATCH III mantido pelo LabECO/UFSC para a ReNOMO, sem que os resultados se afastem dos atuais além das tolerâncias acordadas com o laboratório. As otimizações de compilação e de configuração serão esgotadas antes de qualquer reescrita de código, e o mesmo critério, ganho medido e resultados dentro das tolerâncias, decidirá se *kernels* em C++/Kokkos executados em uma GPU NVIDIA H100 passam a fazer parte da configuração operacional.

Os objetivos específicos são:

1. Registrar e congelar a configuração operacional de cada caso (versão do código, *switches*, *namelists*, grade, forçantes, saídas e *hardware*) e construir um *benchmark* reprodutível da execução de referência, com uma métrica definida: o tempo de execução por hora de previsão.
2. Levantar o perfil de desempenho da execução de referência por rotina e por fase (termos de fonte, propagação, comunicação, entrada e saída), com um e com vários processos MPI.
3. Quantificar o ganho das etapas de compilação, de configuração e de refatoração em Fortran, cada uma acompanhada da verificação de concordância com a referência, e entregar ao laboratório a melhor configuração na forma de um *build* documentado.
4. Construir a infraestrutura de validação que o WW3 não tem: um comparador campo a campo com tolerâncias versionadas e testes unitários por rotina com entradas capturadas do caso operacional, integrados à matriz de regressão do modelo.
5. Reescrever em C++/Kokkos, na ordem indicada pelo perfil, as rotinas que continuarem dominantes, validá-las contra o Fortran original em CPU, medir o ganho em GPU (H100) e decidir, com base no ganho e na concordância dos resultados, se elas entram na configuração operacional.
6. Publicar ferramentas, resultados e recomendações no repositório do projeto, de forma que o laboratório possa repetir as medições, respeitada a política de divulgação descrita na metodologia.
