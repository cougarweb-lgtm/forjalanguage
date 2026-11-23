/*
 * rt_mem.c - memoria e string do runtime nativo da Forja (P2a).
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - memcpy, memmove e memset copiam/preenchem por palavras de 8 bytes com cauda
 *           de bytes (M3). memcmp e strlen seguem iguais.
 *   0.1.0 - primeira versao: memcpy, memmove, memset, memcmp e strlen.
 *
 * O gcc pode trocar um laco destes por chamada ao proprio nome; as flags do rt_win.o
 * (-ffreestanding, -fno-tree-loop-distribute-patterns) impedem a troca, e o .o nao pode
 * depender de libc. Incluido por rt_win.c; nao compila sozinho.
 *
 * PalavraMem: acesso de 8 bytes sem alinhamento (definido em rt_compat.h, RT_PALAVRA_MEM_TIPO).
 * Em x86-64 o acesso nao alinhado e legal e custa quase o mesmo que o alinhado; may_alias
 * evita UB de aliasing ao reler bytes como palavras.
 */
#include <stddef.h>
#include "rt_compat.h"

typedef RT_PALAVRA_MEM_TIPO PalavraMem;

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *pd = (unsigned char *)dst;
    const unsigned char *ps = (const unsigned char *)src;

    /* Palavras primeiro: cada uma le 8 bytes e grava 8 bytes; o contrato do C proibe sobreposicao. */
    while (n >= sizeof(PalavraMem)) {
        *(PalavraMem *)pd = *(const PalavraMem *)ps;
        pd += sizeof(PalavraMem);
        ps += sizeof(PalavraMem);
        n -= sizeof(PalavraMem);
    }
    /* Cauda de 0 a 7 bytes, que nao cabe numa palavra inteira. */
    while (n > 0) {
        *pd++ = *ps++;
        n--;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *pd = (unsigned char *)dst;
    const unsigned char *ps = (const unsigned char *)src;
    size_t restante = n;

    if (pd == ps) {
        return dst;
    }
    if (pd < ps) {
        /* Destino antes da origem: copia para frente. Cada gravacao em pd+i..pd+i+7 fica abaixo
         * de ps+i+8, onde comeca a proxima leitura, entao nenhum byte ainda nao lido e sobrescrito. */
        while (restante >= sizeof(PalavraMem)) {
            *(PalavraMem *)pd = *(const PalavraMem *)ps;
            pd += sizeof(PalavraMem);
            ps += sizeof(PalavraMem);
            restante -= sizeof(PalavraMem);
        }
        while (restante > 0) {
            *pd++ = *ps++;
            restante--;
        }
        return dst;
    }
    /* Destino depois da origem: copia de tras para frente. Palavras do fim primeiro; cada uma
     * grava acima de ps+restante, e as leituras seguintes ficam todas abaixo disso. */
    while (restante >= sizeof(PalavraMem)) {
        restante -= sizeof(PalavraMem);
        *(PalavraMem *)(pd + restante) = *(const PalavraMem *)(ps + restante);
    }
    /* Sobram os primeiros n%8 bytes; tambem de tras para frente, pelo mesmo motivo. */
    while (restante > 0) {
        restante--;
        pd[restante] = ps[restante];
    }
    return dst;
}

void *memset(void *dst, int valor, size_t n)
{
    unsigned char *pd = (unsigned char *)dst;
    PalavraMem padrao = (unsigned char)valor * 0x0101010101010101ULL;

    /* O valor repetido em 8 bytes permite gravar 8 de uma vez; a cauda sai byte a byte. */
    while (n >= sizeof(PalavraMem)) {
        *(PalavraMem *)pd = padrao;
        pd += sizeof(PalavraMem);
        n -= sizeof(PalavraMem);
    }
    while (n > 0) {
        *pd++ = (unsigned char)valor;
        n--;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i;

    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i])
            return pa[i] < pb[i] ? -1 : 1;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}
