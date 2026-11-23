/*
 * rt_ordenar.c - sort(fatia) do runtime nativo da Forja: forja_ordenar(ptr, len, ORDENA_*) (IA1-L5b, L-06).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - K17: inclui runtime/forja_rt_heap.h (heap_push e heap_pop: forja_heap_subir, forja_heap_retira_topo); mesmo objeto rt_win_curto.o.
 *   0.2.0 - K08b: inclui runtime/forja_rt_ordenar_chave.h (sort_by_key e binary_search: forja_ordc_*); mora no mesmo rt_win_curto.o, ligado quando o programa referencia um desses simbolos.
 *   0.1.0 - IA1-L5b: primeira versao. O codigo e o de runtime/forja_rt_ordenar.h, o MESMO texto que o backend C escreve no .c gerado (mesmo algoritmo = mesma ordem dos iguais nos dois
 *           backends). Aqui FORJA_ORDENAR_API fica vazio: as funcoes saem externas. Mora no objeto rt_win_curto.o (rt_curto.c), que o ligador so junta quando o programa referencia um simbolo dele
 *           (forja_ordenar): runtime novo em unidade propria (D-IA1-13). Sem libc, sem alocacao, sem referencia indefinida.
 *
 * Incluido por rt_curto.c; nao compila sozinho.
 */
#include <stdint.h>

#include "../forja_rt_ordenar.h"
#include "../forja_rt_ordenar_estavel.h"
#include "../forja_rt_ordenar_chave.h"
#include "../forja_rt_heap.h"
