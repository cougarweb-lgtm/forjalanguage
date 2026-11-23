/* libmem.h - contabilidade de memoria por biblioteca estatica em DEV (M6d, D-M6d-2/3/4): o mapa slot -> lib, o redirecionamento da familia malloc/calloc/realloc/free/strdup/_strdup
 * que o codigo de cada lib contada referencia para os involucros forja_libmem_<fn>_<slot> do runtime dev, e a tabela FLM1 que o runtime le.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M6d P93h (e): LibmemEntrada {estaticas, fins, qtd_fins}; LibmemMontar(entrada, arena, saida).
 *            0.2.0 - M6d P92e: LibmemMontar recebe os fins `ends`; TerceirosLibmem.fins; a tabela FLM1 preenche off_fins.
 *            0.1.0 - M6d P92c: LibmemMontar, LibmemRedirecionar e LibmemTabela (off_fins = 0: os fins `ends` entram no P92e).
 * Verificacao: construir_rapido.py && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_LIBMEM_H
#define FORJA_NATIVO_OBJ_LIBMEM_H

#include <stdint.h>

#include "common/arena.h"
#include "common/vec.h"
#include "nativo/obj/objeto.h"

#define FORJA_LIBMEM_SLOTS 16u

/* O mapa que o ligador usa em dev. qtd_slots == 0: nada e contado (release, DLL, programa sem lib estatica contada). Os textos e slot_do_arquivo moram na arena de quem montou. */
typedef struct {
    uint32_t qtd_slots;
    uint32_t qtd_arquivos;
    const int8_t *slot_do_arquivo;               /* por EstaticaMembro.arquivo; -1 = o arquivo nao conta */
    const char *nome[FORJA_LIBMEM_SLOTS];        /* o nome do 'link' como o .fj escreveu */
    const char *arquivo[FORJA_LIBMEM_SLOTS];     /* so o nome do arquivo (libz.a) */
    const char *fins[FORJA_LIBMEM_SLOTS];        /* M6d P92e: os simbolos `ends` que o arquivo do slot define: "a\0b\0\0" (NUL duplo fecha); NULL = a lib nao declara `ends` */
    uint32_t excedentes;                       /* arquivos alem do slot 16: nao contados; primeiro_excedente e o link do 1o */
    const char *primeiro_excedente;
} TerceirosLibmem;

/* Entrada de LibmemMontar (M6d P93h): estaticas = EstaticaArquivo do resolvedor, na ordem; fins = os simbolos C dos foreign `ends` declarados (qtd_fins deles). */
typedef struct {
    const Vec *estaticas;
    const char *const *fins;
    uint32_t qtd_fins;
} LibmemEntrada;

/* Monta o mapa a partir de entrada->estaticas. Conta quem tem link, nao e coberto pelo ligador (c, m, msvcrt...) e cabe nos 16 slots; o mesmo arquivo
 * repetido (mesmo ponteiro de bytes) reaproveita o slot. M6d P92e: cada fim vai para o slot do arquivo contado que o DEFINE (pelo indice do ar); fim que nenhum arquivo contado define
 * (proc de DLL, lib nao contada) e ignorado. */
void LibmemMontar(const LibmemEntrada *entrada, Arena *arena, TerceirosLibmem *saida);

/* Troca, nas relocacoes das secoes de arquivo contado, o simbolo indefinido da familia por forja_libmem_<fn>_<slot> (e __imp_forja_libmem_... com a celula criada). inicios[m] = indice da
 * 1a secao do membro m na uniao; membros = EstaticaMembro. Simbolo definido por algum membro nao e tocado. */
void LibmemRedirecionar(Objeto *o, const Vec *membros, const Vec *inicios, const TerceirosLibmem *lm);

/* Sintetiza a secao .rdata$libmem (alinhamento 4) com o simbolo global forja_libmem_tabela: "FLM1", qtd, {off_nome, off_arquivo, off_fins = 0} por slot e os textos. */
void LibmemTabela(Objeto *o, const TerceirosLibmem *lm);

#endif
