/*
 * rt_mem.c - memoria e string do runtime nativo da Forja (P2a).
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - memcpy e memmove copiam por vetores de 16 bytes (movdqu): 32 bytes por volta, 1 passo de 16 e a cauda de
 *           1 a 15 bytes por UMA copia de 16 sobreposta ao fim (bytes so abaixo de 16). memmove le a cauda (para
 *           frente) ou o comeco (para tras) antes de gravar. memset, memcmp e strlen seguem iguais.
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
typedef RT_VETOR_MEM_TIPO VetorMem;

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *pd = (unsigned char *)dst;
    const unsigned char *ps = (const unsigned char *)src;
    size_t restante = n;

    /* Menos de 16 bytes nao enche um vetor: bytes. */
    if (n < sizeof(VetorMem)) {
        while (restante > 0) {
            *pd++ = *ps++;
            restante--;
        }
        return dst;
    }
    /* 32 bytes por volta: as 2 cargas antes das 2 gravacoes (o contrato do C proibe sobreposicao). */
    while (restante >= 2 * sizeof(VetorMem)) {
        VetorMem primeiro = *(const VetorMem *)ps;
        VetorMem segundo = *(const VetorMem *)(ps + sizeof(VetorMem));
        *(VetorMem *)pd = primeiro;
        *(VetorMem *)(pd + sizeof(VetorMem)) = segundo;
        pd += 2 * sizeof(VetorMem);
        ps += 2 * sizeof(VetorMem);
        restante -= 2 * sizeof(VetorMem);
    }
    if (restante >= sizeof(VetorMem)) {
        *(VetorMem *)pd = *(const VetorMem *)ps;
        pd += sizeof(VetorMem);
        ps += sizeof(VetorMem);
        restante -= sizeof(VetorMem);
    }
    /* Cauda de 1 a 15 bytes: UMA copia de 16 encostada no fim, sobreposta ao que ja foi copiado (n >= 16 garante o inicio dentro do bloco). */
    if (restante > 0) {
        *(VetorMem *)(pd + restante - sizeof(VetorMem)) = *(const VetorMem *)(ps + restante - sizeof(VetorMem));
    }
    return dst;
}

/* LIMITE: linhas memmove tem 3 caminhos (menos de 16 bytes, para frente, para tras) cuja ordem de carga e gravacao e o que a torna correta
 * com sobreposicao; separar em helpers de uso unico esconderia essa ordem (e a prova esta em tests/nativo/rt/teste_rt_mem.c).
 * LIMITE: cognitiva mesmos 3 caminhos; o conferidor le UMA metrica por marca, entao a de cima nao cobre esta. */
void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *pd = (unsigned char *)dst;
    const unsigned char *ps = (const unsigned char *)src;
    size_t restante = n;

    if (pd == ps) {
        return dst;
    }
    if (n < sizeof(VetorMem)) {
        /* Menos de 16 bytes: bytes, na direcao que nao sobrescreve o que falta ler. */
        if (pd < ps) {
            while (restante > 0) {
                *pd++ = *ps++;
                restante--;
            }
        } else {
            while (restante > 0) {
                restante--;
                pd[restante] = ps[restante];
            }
        }
        return dst;
    }
    if (pd < ps) {
        /* Destino antes da origem: copia para frente. A cauda (os ultimos 16 bytes) e lida ANTES da
         * 1a gravacao: uma copia sobreposta ao fim leria bytes que as gravacoes anteriores ja trocaram.
         * Cada volta le 32 bytes antes de gravar, e a gravacao fica abaixo de ps+32, ja lido. */
        VetorMem cauda = *(const VetorMem *)(ps + n - sizeof(VetorMem));
        while (restante >= 2 * sizeof(VetorMem)) {
            VetorMem primeiro = *(const VetorMem *)ps;
            VetorMem segundo = *(const VetorMem *)(ps + sizeof(VetorMem));
            *(VetorMem *)pd = primeiro;
            *(VetorMem *)(pd + sizeof(VetorMem)) = segundo;
            pd += 2 * sizeof(VetorMem);
            ps += 2 * sizeof(VetorMem);
            restante -= 2 * sizeof(VetorMem);
        }
        if (restante >= sizeof(VetorMem)) {
            *(VetorMem *)pd = *(const VetorMem *)ps;
            pd += sizeof(VetorMem);
            ps += sizeof(VetorMem);
            restante -= sizeof(VetorMem);
        }
        /* O fim guardado cobre os 0 a 15 bytes que faltam (e regrava os ultimos 16 com o valor original). */
        *(VetorMem *)((unsigned char *)dst + n - sizeof(VetorMem)) = cauda;
        return dst;
    }
    /* Destino depois da origem: copia de tras para frente, 32 bytes por volta (2 cargas antes das 2 gravacoes;
     * a gravacao fica acima de ps+restante-32, e as leituras seguintes ficam todas abaixo disso). O comeco
     * (os primeiros 16 bytes) e lido antes de tudo e gravado por ultimo. */
    {
        VetorMem cabeca = *(const VetorMem *)ps;
        while (restante >= 2 * sizeof(VetorMem)) {
            VetorMem alto = *(const VetorMem *)(ps + restante - sizeof(VetorMem));
            VetorMem baixo = *(const VetorMem *)(ps + restante - 2 * sizeof(VetorMem));
            *(VetorMem *)(pd + restante - sizeof(VetorMem)) = alto;
            *(VetorMem *)(pd + restante - 2 * sizeof(VetorMem)) = baixo;
            restante -= 2 * sizeof(VetorMem);
        }
        if (restante >= sizeof(VetorMem)) {
            *(VetorMem *)(pd + restante - sizeof(VetorMem)) = *(const VetorMem *)(ps + restante - sizeof(VetorMem));
        }
        *(VetorMem *)pd = cabeca;
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
