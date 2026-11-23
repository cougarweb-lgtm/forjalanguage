/*
 * rt_linux_es.c - unidade do handle e do sistema de arquivos do std:file, da entrada padrao do std:os e (P28b-2) do processo filho no runtime Linux x86-64 (M5 P28b-1), o espelho de rt_es_win.c.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - D-G3-L2b: inclui tambem rt_es_trava_linux.c (file.lock) e rt_processo_espera_linux.c (pid, wait_any, peak_memory, cpu_times).
 *   0.2.0 - M5 P28b-2: inclui tambem rt_linha.c (divide a linha com aspas do Windows em argv) e rt_processo_linux.c (forja_processo_*); o objeto passa a ligar tambem quando o programa referencia forja_processo_*.
 *   0.1.0 - M5 P28b-1: primeira versao. Inclui rt_es_linux.c (forja_fs_* e forja_stdin_*). O construir.bat compila esta unidade UMA vez, em rt_linux_es.o, com FLAGS_RTL (as do rt_linux.c sem -DFORJA_DEV):
 *           o objeto e igual em dev e release e so entra na ligacao do programa que referencia forja_fs_* ou forja_stdin_* (nativo.c LerEs e UsarRyuDoPrograma, como o rt_win_es.o).
 *
 * Referencias indefinidas que o ligador resolve: sys_heap_alocar e sys_heap_liberar (rt_linux_comum.o) e rt_saida_descarregar (o runtime da variante). Nenhum simbolo __imp_, nenhuma libc.
 * Compilar: as flags do rt_linux.c (ver la) sem -DFORJA_DEV, -c rt_linux_es.c -o rt_linux_es.o. Verificacao: python -I tests\nativo\linux\auditar_runtime.py
 */
#ifndef RT_ALVO_LINUX
#error "rt_linux_es.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#include "rt_es_linux.c"
#include "rt_es_trava_linux.c"
#include "rt_linha.c"
#include "rt_processo_linux.c"
#include "rt_processo_espera_linux.c"
