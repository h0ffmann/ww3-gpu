<!-- revisado à mão em 2026-09-18 (PT-BR). scripts/translate_md.py só sobrescreve este arquivo se ../en/06-objective.md mudar ou com --force. -->
# OBJETIVO

O objetivo geral é reduzir, de forma medida e reprodutível, o tempo de execução do ciclo operacional do WAVEWATCH III mantido pelo LabECO/UFSC no âmbito do ReNOMO, sem alterar seus resultados científicos além das tolerâncias acordadas com o laboratório, esgotando as otimizações de configuração e compilação antes de reescrever código, e decidir com o mesmo critério, ganho medido e ausência de regressão, se *kernels* em C++/Kokkos executados em uma GPU NVIDIA H100 entram na configuração operacional.

Os objetivos específicos são:

1. Congelar e documentar a configuração operacional (revisão do código, *switches*, *namelists*, grade, forçante, saídas, hardware) e construir um *benchmark* reprodutível da execução de referência, com métrica definida (tempo de execução por hora de previsão).
2. Produzir um perfil da execução de referência por rotina e por fase (termos de fonte, propagação, comunicação, entrada e saída), com um e com vários processos MPI.
3. Quantificar o ganho das etapas de compilação, configuração e refatoração em Fortran, cada uma com sua evidência de paridade em relação à referência, e entregar ao laboratório a melhor configuração como um *build* documentado.
4. Construir a infraestrutura de validação que o WW3 não tem: um comparador por campo com tolerâncias versionadas e testes unitários por rotina com entradas capturadas, integrados à matriz de regressão do modelo.
5. Reescrever em C++/Kokkos, na ordem do perfil, os *kernels* que permanecerem dominantes, validá-los contra o Fortran original em CPU, medir seu ganho em GPU (H100) e decidir, com base em ganho e paridade, se entram na configuração operacional.
6. Publicar ferramentas, resultados e recomendação no repositório do projeto, em forma que o laboratório possa reexecutar, respeitada a política de divulgação descrita na metodologia.
