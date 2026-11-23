/*
 * rt_win.c - unidade unica do runtime nativo da Forja para Windows x64 (P2a).
 *
 * Versao: 0.8.0
 * Changelog:
 *   0.8.0 - M5 P43: inclui rt_fio.h (RtFio, o estado por thread) depois de rt_salto.c e rt_fio.c (slot TLS) depois de rt_nativo.c.
 *   0.7.0 - M3 F5: com -DFORJA_RT_SEM_RYU o rt_terceiros.c (Ryu) nao entra: as variantes do construir.bat ficam com referencias indefinidas ao Ryu, que vive em rt_ryu.c (rt_win_ryu.o). Sem a macro, igual a antes.
 *   0.6.0 - M2 P17b: inclui rt_mapa.c (forja_mapa de 56 B) e rt_soa.c (soa dinamica) depois de rt_formata.c; a unidade de teste tests\nativo\rt\rt_win_mapa.c some.
 *   0.5.0 - M2 P15b: inclui rt_formata.c (forja_fmt_*: f"...", format, format_c) depois de rt_aloc.c.
 *   0.4.0 - M2 P14b: inclui rt_rastreio.c (so FORJA_DEV) e rt_aloc.c (heap, frame, listas, Arena, Pool; R0007/R0008/R0009); a unidade de teste tests\nativo\rt\rt_win_aloc.c some.
 *   0.3.0 - M2 P12b: inclui rt_salto.c (salvar/saltar em asm, antes de rt_nativo.c, que guarda o ForjaSalto) e rt_teste.c (executor de testes, depois dele).
 *   0.2.0 - M2 P10b: inclui tambem rt_terceiros.c (envelope do Ryu) e rt_real.c (formatador e conversoes de real); o construir.bat compila
 *           esta unidade DUAS vezes (rt_win_dev.o com -DFORJA_DEV e rt_win_rel.o) e exige -I <language>\third.
 *   0.1.1 - o .o passa a importar 15 simbolos de kernel32 (console UTF-8 em rt_sys_win.c 0.2.0).
 *   0.1.0 - primeira versao: inclui os quatro .c (sys, mem, saida, nativo) numa so unidade.
 *
 * Um gcc so, sem ld -r: este arquivo inclui os .c, que nao compilam sozinhos.
 * Compilar: gcc -std=c11 -O2 -ffreestanding -nostdlib -fno-builtin -fno-tree-loop-distribute-patterns
 *   -mno-stack-arg-probe -fno-stack-protector -fno-ident -Wframe-larger-than=4000 -Wall -Wextra -Werror
 *   -DRT_ALVO_WIN -I <language>\third [-DFORJA_DEV] -c rt_win.c -o rt_win.o
 * Saida esperada: .text, os 15 __imp_ de kernel32 e o simbolo forja_programa (fornecido pelo codigo gerado).
 */
#ifndef RT_ALVO_WIN
#error "rt_win.c so compila para Windows: use -DRT_ALVO_WIN"
#endif

#include "rt_sys_win.c"
#include "rt_mem.c"
#include "rt_saida.c"
#include "rt_salto.c"
#include "rt_fio.h"
#include "rt_nativo.c"
/* rt_fio.c usa sys_*, memset (rt_mem.c) e rt_fio.h; fica depois de rt_nativo.c, que so o chama (forja_entrada) e le o fio pelo inline do rt_fio.h. */
#include "rt_fio.c"
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
/* rt_formata.c usa rt_print_linha, rt_fmt_f (rt_real.c), forja_mem_alocar e forja_falha_alocacao (rt_aloc.c): vem depois deles. */
#include "rt_formata.c"
/* rt_mapa.c e rt_soa.c usam RT_ALINHA, rt_frame_epoca, rt_aloc_lista e forja_lista_viva de rt_aloc.c: vem depois dele. */
#include "rt_mapa.c"
#include "rt_soa.c"
