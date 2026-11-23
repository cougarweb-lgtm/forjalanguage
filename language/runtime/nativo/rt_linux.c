/*
 * rt_linux.c - unidade unica do runtime nativo da Forja para Linux x86-64 (M5 P26d), o espelho de rt_win.c.
 *
 * Versao: 0.4.0
 * Changelog:
 *   0.4.0 - M8 H2: inclui rt_reload_linux.c (stubs do std:reload: unsupported_target) depois de rt_os_linux.c.
 *   0.3.0 - M5c-b P57: inclui rt_fio_linux.c (o RtFio por thread da variante libc: principal, ponte e liberacao) depois de rt_aloc.c.
 *   0.2.0 - M5 P43: inclui rt_fio.h (RtFio, o estado do runtime; no Linux um fio unico, sem thread ainda) depois de rt_salto.c.
 *   0.1.1 - IA1-L6: inclui rt_os_linux.c (sleep_ms e time_ns do std:os) depois de rt_proc.c.
 *   0.1.0 - M5 P26d: primeira versao. rt_sys_linux.c no lugar de rt_sys_win.c, mais os argumentos/ambiente (rt_proc_linux.c) e o arquivo (rt_arquivo_linux.c); std:proc (rt_proc.c) e
 *           std:arquivo (rt_arquivo.c) entram aqui mesmo (o Linux nao tem exes antigos a preservar, ao contrario do rt_win.o). rt_linha.c (linha de comando do Windows) nao entra.
 *           O heap (dlmalloc) e o Ryu ficam em rt_linux_comum.o (rt_comum_linux.c), igual nas duas variantes: elas saem com -DFORJA_RT_SEM_RYU e referencias indefinidas a eles.
 *
 * Um gcc so, sem ld -r: este arquivo inclui os .c, que nao compilam sozinhos. O MESMO gcc MinGW do Windows, com a convencao Win64 (D-M5-2); o objeto e COFF e o ligador interno o le.
 * Compilar: gcc -std=c11 -O2 -ffreestanding -nostdlib -fno-builtin -fno-tree-loop-distribute-patterns -mno-stack-arg-probe -fno-stack-protector -fno-ident
 *   -fno-asynchronous-unwind-tables -fno-unwind-tables -Wframe-larger-than=4000 -Wall -Wextra -Werror -DRT_ALVO_LINUX -DFORJA_RT_SEM_RYU -I <language>\third [-DFORJA_DEV]
 *   -c rt_linux.c -o rt_linux_dev.o
 * Saida esperada: .text, o simbolo forja_programa (fornecido pelo codigo gerado) e os do objeto comum (sys_heap_*; o d2fixed/d2exp saiu no Ryu P4); sem .pdata/.xdata/.idata
 * e sem simbolo __imp_.
 */
#ifndef RT_ALVO_LINUX
#error "rt_linux.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#include "rt_sys_linux.c"
#include "rt_mem.c"
#include "rt_proc_linux.c"
#include "rt_arquivo_linux.c"
#include "rt_saida.c"
#include "rt_salto.c"
#include "rt_fio.h"
#include "rt_nativo.c"
#include "rt_teste.c"
/* rt_real.c usa rt_print_linha, RtCausa e forja_falha de rt_nativo.c: vem depois dele. */
#ifndef FORJA_RT_SEM_RYU
#include "rt_terceiros.c"
#endif
#include "rt_real.c"
/* rt_aloc.c usa RtCausa, rt_sem_relatorio e forja_falha de rt_nativo.c; em dev rt_rastreio.c (cabecalho de 48 B, R0009) vem ANTES dele. */
#ifdef FORJA_DEV
#include "rt_rastreio.c"
#endif
#include "rt_aloc.c"
/* rt_fio_linux.c usa FORJA_FRAME_BYTES (rt_aloc.c), sys_* e memset (rt_mem.c). */
#include "rt_fio_linux.c"
/* rt_formata.c usa rt_print_linha, rt_fmt_f (rt_real.c), forja_mem_alocar e forja_falha_alocacao (rt_aloc.c): vem depois deles. */
#include "rt_formata.c"
/* rt_mapa.c e rt_soa.c usam RT_ALINHA, rt_frame_epoca, rt_aloc_lista e forja_lista_viva de rt_aloc.c: vem depois dele. */
#include "rt_mapa.c"
#include "rt_soa.c"
/* std:proc e std:arquivo usam forja_falha, forja_falha_alocacao e rt_saida_descarregar (globais) e sys_*: vem por ultimo. */
#include "rt_proc.c"
#include "rt_os_linux.c"
#include "rt_reload_linux.c"
#include "rt_arquivo.c"
