# Prefácio {.unnumbered}

Este projeto existe para apoiar o Ondas & Ventos [@ondasventos], um sistema operacional de
previsão de ondas da Universidade Federal de Santa Catarina (UFSC). O LabECO da UFSC roda o WW3 de
forma operacional para a ReNOMO, a Rede Nacional de Observação e Monitoramento Oceânico
[@renomo]. A ReNOMO começou em 2023, com a chamada CNPq/MCTI/Finep 062/2022. Mais de 100
pesquisadores de 31 instituições brasileiras participam da rede [@renomo].

O sistema precisa rodar o modelo de ondas a cada ciclo de previsão, e o tempo de execução do WW3
limita o ciclo. O sistema também planeja forçar o modelo de ondas com mais de um modelo de vento:
o IFS do ECMWF, o GFS do NCEP e o WeatherNext do Google ⚠. Cada modelo de vento acrescenta uma
execução completa do WW3, e por isso um conjunto de modelos de vento multiplica o custo do ciclo.
Um WW3 mais rápido em GPU, com os mesmos bits, torna essas execuções viáveis sem mudança na
previsão. O ww3-gpu e o chaceon trabalham para chegar a esse WW3, e este texto não afirma ganho de
velocidade.
