/* buf.c - parte lenta do buffer (crescimento, numeros, formato).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - Fase 5i: BufAnexarU64 escreve dois digitos por divisao (tabela de pares).
 *            0.2.0 - Fase 5i: BufFormatar em 1 passada quando cabe na capacidade livre; BufAnexarI64.
 *            0.1.0 - Fase 5a: BufFormatar mede o tamanho antes (vsnprintf em 2 passadas).
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/buf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define BUF_CAP_INICIAL 4096

static void BufReservar(Buf *b, size_t extra)
{
    if (b->tam + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : BUF_CAP_INICIAL;
    while (cap < b->tam + extra + 1) cap *= 2;
    char *novo = realloc(b->dados, cap);
    if (novo == NULL) FalhaInterna("memoria esgotada no buffer (%zu bytes pedidos)", cap);
    b->dados = novo;
    b->cap = cap;
}

void BufReservarTotal(Buf *b, size_t total)
{
    if (total > b->tam) BufReservar(b, total - b->tam);
}

void BufAnexarLento(Buf *b, const char *s, size_t n)
{
    BufReservar(b, n);
    memcpy(b->dados + b->tam, s, n);
    b->tam += n;
    b->dados[b->tam] = '\0';
}

/* Os 100 pares "00".."99" lado a lado: dois digitos por divisao em vez de um. */
static const char PARES_DE_DIGITOS[] =
    "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263"
    "646566676869707172737475767778798081828384858687888990919293949596979899";

void BufAnexarU64(Buf *b, uint64_t v)
{
    char digitos[24];
    size_t i = sizeof digitos;
    while (v >= 100) {
        size_t par = (size_t)(v % 100) * 2;
        v /= 100;
        digitos[--i] = PARES_DE_DIGITOS[par + 1];
        digitos[--i] = PARES_DE_DIGITOS[par];
    }
    if (v >= 10) {
        digitos[--i] = PARES_DE_DIGITOS[v * 2 + 1];
        digitos[--i] = PARES_DE_DIGITOS[v * 2];
    } else {
        digitos[--i] = (char)('0' + v);
    }
    BufAnexarN(b, digitos + i, sizeof digitos - i);
}

void BufAnexarI64(Buf *b, int64_t v)
{
    if (v >= 0) {
        BufAnexarU64(b, (uint64_t)v);
        return;
    }
    BufAnexarN(b, "-", 1);
    BufAnexarU64(b, (uint64_t)0 - (uint64_t)v); /* vale tambem para INT64_MIN */
}

/* Uma passada no caso comum: formata direto na capacidade livre e so refaz (com a medida exata) se nao coube. */
void BufFormatar(Buf *b, const char *fmt, ...)
{
    va_list ap, copia;
    size_t livre = b->cap > b->tam ? b->cap - b->tam : 0;
    char *destino = b->dados != NULL ? b->dados + b->tam : NULL;

    va_start(ap, fmt);
    va_copy(copia, ap);
    int n = vsnprintf(destino, livre, fmt, copia);
    va_end(copia);
    if (n < 0) FalhaInterna("BufFormatar: formato invalido '%s'", fmt);
    if ((size_t)n >= livre || livre == 0) {
        BufReservar(b, (size_t)n);
        vsnprintf(b->dados + b->tam, (size_t)n + 1, fmt, ap);
    }
    va_end(ap);
    b->tam += (size_t)n;
}

void BufLiberar(Buf *b)
{
    free(b->dados);
    b->dados = NULL;
    b->tam = b->cap = 0;
}
