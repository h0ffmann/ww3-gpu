# Escadas de evidência {#sec:ladders}

Cada projeto dá nome aos próprios degraus. Um degrau num registro não é a mesma evidência que um
degrau no outro registro.

| Degrau                      | ww3-gpu                                                    | chaceon                                                     |
|:-----------------------|:-------------------------------------|:-------------------------------------|
| O código existe             | *translated*: 316 rotinas, 26,2 % das linhas de Fortran     | *translated*: o Rust existe, sem cobertura completa         |
| Uma execução chama o código | *ported*: 72 rotinas, 99,3 % do tempo de execução medido    | *routine*: toda chamada capturada é idêntica bit a bit no host e em SPIR-V |
| Execuções inteiras idênticas, CPU | *regtested*: 14 rotinas, 48,6 % do tempo de execução  | *host*: execuções inteiras idênticas por `cmp`; 10.080 linhas |
| Idêntico por uma API de GPU | nenhum                                                     | *spirv*: os mesmos arquivos a partir de SPIR-V no Vulkan de CPU; 5.189 linhas |
| Idêntico numa GPU real      | *gpu*: nenhum                                              | *cuda*: nenhum                                              |

: Degraus de evidência. As contagens do ww3-gpu vêm de `scripts/port_score.py --write` (perfil
`bench_small`, 120 × 80 pontos, 12 h, uma thread, gprof, 2026-10-14); as contagens do chaceon vêm
de `just port-summary`. {#tbl:ladders}
