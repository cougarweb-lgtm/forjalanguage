/*
 * rt_curto.c - objeto rt_win_curto.o do runtime nativo da Forja: print de f32/f64 pela forma mais curta que volta igual (Ryu d2s/f2s, rt_real_curto.c) e eprint (rt_saida_erro.c). IA1-L5a.
 *
 * Versao: 0.3.0
 * Changelog: 0.3.0 - IA1-LX: objeto NEUTRO DE ALVO (liga no PE e no ELF); rt_os_win.c (por alvo) sai daqui para o rt_ryu.c. Se um dia precisar de codigo por alvo, partir para um objeto Linux separado.
 *   0.2.0 - IA1-L5b/L6: inclui rt_ordenar.c (forja_ordenar), rt_os_win.c (forja_proc_dormir_ms, forja_proc_tempo_ns): o objeto vira o dos recursos novos do runtime; o nome rt_win_curto.o fica.
 *   0.1.1 - IA1-L5a: DIGIT_TABLE renomeada neste objeto (o ligador recusa o mesmo simbolo em dois objetos: X0903 num programa que usa o Ryu e o curto).
 *   0.1.0 - IA1-L5a: primeira versao. Objeto proprio, e nao parte do rt_win.o nem do rt_win_ryu.o, porque o ligador leva as secoes de dado (.rdata) do objeto INTEIRAS e um literal ou funcao nova
 *           muda o gcc/a ordem delas (medido: +384 bytes de .text no rt_win.o e .rdata reordenado no rt_win_ryu.o): os exes dos programas que nao usam o recurso ficam byte a byte iguais
 *           (D-IA1-13). O construir.bat o compila UMA vez (flags de release do runtime, sem -DFORJA_DEV); o nativo.c o junta so quando o programa referencia simbolo dele
 *           (forja_fmt_f32_curto, forja_fmt_f64_curto, forja_fmt_imprimir_erro). Sem estado mutavel.
 *
 * Referencias indefinidas que o ligador resolve no runtime: forja_panic (malloc do envelope), forja_fmt_alinhado, forja_fmt_bytes, forja_fmt_buffer, rt_saida_stderr, memcpy/memset.
 * Compilar exige -I <language>\third (o d2s.c faz #include "ryu/ryu.h").
 */
#ifndef RT_ALVO_WIN
#error "rt_curto.c so compila para Windows: use -DRT_ALVO_WIN"
#endif

#define NDEBUG 1

#include <stddef.h>
#include <stdlib.h>
#include "forja_rt_nativo.h"

/* d2s() e f2s() (sem _buffered) alocam com malloc e nao sao usados pelo rt: se alguem os chamar, R0003 em vez de um simbolo solto (o mesmo envelope do rt_terceiros.c). */
static void *rt_curto_malloc(size_t n)
{
    (void)n;
    forja_panic("ryu: alocacao (malloc) nao existe no runtime nativo; use d2s_buffered_n/f2s_buffered_n", __FILE__, __LINE__);
}

#define malloc(n) rt_curto_malloc(n)

/* Historico: o d2fixed.c (rt_win_ryu.o) e o d2s.c definiam a mesma tabela DIGIT_TABLE (digit_table.h) e o ligador recusava o nome em dois objetos (X0903); o d2fixed saiu no Ryu P4, o cuidado fica. */
#define DIGIT_TABLE DIGIT_TABLE_CURTO

#include "rt_real_curto.c"
#include "rt_saida_erro.c"
/* IA1-L5b: o sort (forja_ordenar) mora neste mesmo objeto de "recursos novos": so entra quando o programa referencia simbolo dele. sleep_ms e time_ns (por alvo) moram no rt_ryu.c (Windows) e no rt_linux.c (Linux). */
#include "rt_ordenar.c"
