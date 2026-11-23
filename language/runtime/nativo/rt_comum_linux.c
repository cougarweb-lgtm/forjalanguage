/*
 * rt_comum_linux.c - a parte do runtime Linux que e igual nas variantes dev e release: o heap (dlmalloc) e o Ryu (M5 P26d).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P26d: primeira versao. O construir.bat compila esta unidade UMA vez, em rt_linux_comum.o, e o forja.exe a embute uma vez (o mesmo desenho do rt_win_ryu.o): heap e Ryu nao
 *           dependem de -DFORJA_DEV, e repetir os ~140 KB deles nas duas variantes estouraria o limite de 2 MiB do forja.exe. As variantes (rt_linux.c com -DFORJA_RT_SEM_RYU) ficam com
 *           referencias indefinidas a eles (sys_heap_*; o d2fixed/d2exp saiu no Ryu P4); o ligador junta os dois objetos.
 *
 * Referencias indefinidas que o ligador resolve no runtime da variante: forja_falha (ABORT do dlmalloc), forja_panic (malloc do envelope do Ryu), memcpy e memset.
 * Compilar: as mesmas flags do rt_linux.c (sem -DFORJA_DEV e sem -DFORJA_RT_SEM_RYU), -DRT_ALVO_LINUX e -I <language>\third.
 */
#ifndef RT_ALVO_LINUX
#error "rt_comum_linux.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#include "rt_heap_linux.c"
#include "rt_terceiros.c"
