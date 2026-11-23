/* buf.h - buffer de texto crescente (saida C gerada, mensagens).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Fase 5i: BufAnexarI64.
 *            0.1.0 - Fase 5a: sem retorno bool (memoria esgotada = FalhaInterna); BufFormatar sem limite fixo.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_BUF_H
#define FORJA_BUF_H

#include "common/base.h"

typedef struct { char *dados; size_t tam, cap; } Buf;

void BufAnexarLento(Buf *b, const char *s, size_t n);
void BufReservarTotal(Buf *b, size_t total); /* garante capacidade para 'total' bytes de uma vez (evita realloc em cascata) */
void BufAnexarU64(Buf *b, uint64_t v);
void BufAnexarI64(Buf *b, int64_t v);
void BufFormatar(Buf *b, const char *fmt, ...) __attribute__((format(gnu_printf, 2, 3)));
void BufLiberar(Buf *b);

/* Caminho rapido inline: com literal, o strlen e o memcpy viram constantes. */
static inline void BufAnexarN(Buf *b, const char *s, size_t n)
{
    if (b->tam + n + 1 > b->cap) {
        BufAnexarLento(b, s, n);
        return;
    }
    memcpy(b->dados + b->tam, s, n);
    b->tam += n;
    b->dados[b->tam] = '\0';
}

static inline void BufAnexar(Buf *b, const char *s)
{
    BufAnexarN(b, s, strlen(s));
}

#endif
