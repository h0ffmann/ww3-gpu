# O que este texto não afirma {#sec:limits}

1. Nenhum dos dois projetos rodou o modelo inteiro numa GPU real. O PTX do chaceon nunca rodou. O
   ww3-gpu rodou um kernel de benchmark da DIA numa RTX 4090 [@diagpuv2].
2. Este texto não afirma ganho de velocidade. As regras do ww3-gpu marcam os tempos registrados
   [@noaarecord] como "regtest pending", e o chaceon não tem tempos medidos.
3. Numa GPU, a identidade de bits também exige que o `expf` e o `logf` do dispositivo devolvam os
   bits da glibc. O ww3-gpu manda os kernels para cópias de dispositivo das funções da glibc 2.44.
   Essas cópias são iguais às funções do host em toda entrada conferida. Nenhuma execução em
   dispositivo comparou as cópias ainda. Até uma execução comparar, uma tolerância fixa julga uma
   execução em GPU.
4. As duas referências são linhas diferentes do WW3 (NOAA-EMC e MetOcean). Por isso, a comparação
   entre os dois portes é por nome de rotina, e não por texto-fonte. Os dois registros contêm 85
   rotinas.
