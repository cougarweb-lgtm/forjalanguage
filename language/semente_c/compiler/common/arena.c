/* arena.c - alocador por blocos que dobram de 16 KiB ate 8 MiB (bloco maior sob demanda).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S4c: o 1o bloco de 16 KiB de cada arena fica guardado ao liberar (primeiro_livre) e entra ZERADO na arena seguinte: ArenaLiberar zera so o trecho que a arena usou (Arena.usado_primeiro) e o
 *                    ArenaAlocar nao precisa de memset nesse bloco. O calloc de 16 KiB (e depois o malloc/free) por arena de analise/alocador x 8 passes x 20 mil funcoes do corpus_200000 custava ~15 ms.
 *                    Blocos seguintes de ate 64 KiB vem de malloc e o ArenaAlocar zera o que entrega; maiores seguem calloc. Mesma semantica (zerado, alinhado em 8, qualquer n).
 *            0.2.0 - M4/P24c: o 1o bloco passa de 8 MiB para 16 KiB e cada bloco novo dobra ate 8 MiB. A arena por funcao do X64Gerar (19675 por
 *                    corpus_200000) pedia 8 MiB por funcao: malloc/free de bloco grande vira VirtualAlloc/VirtualFree (ZwFreeVirtualMemory = 45% do
 *                    tempo amostrado no perfil do P24b). Mesma semantica (zerado, alinhado em 8, qualquer n); exes gerados byte a byte iguais.
 *            0.1.0 - Fase 5a: reaproveita o ArenaAlloc do prototipo; OOM vira FalhaInterna.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/arena.h"

#include <stdlib.h>

#define ARENA_BLOCO_INICIAL ((size_t)16 << 10)
#define ARENA_BLOCO_MAXIMO ((size_t)8 << 20)
#define ARENA_BLOCO_ZERO_POR_USO ((size_t)64 << 10)

struct ArenaBloco { ArenaBloco *anterior; size_t cap; };

/* Um bloco inicial (16 KiB) ZERADO por inteiro, guardado entre o ArenaLiberar e a proxima arena; por thread (o compilador e de uma thread so, mas a regra nao depende disso). */
static _Thread_local ArenaBloco *primeiro_livre;

/* O 1o bloco da arena (anterior == NULL) com o tamanho inicial e o que fica zerado enquanto ocioso: nele o ArenaAlocar nao faz memset. */
static bool EhPrimeiroZerado(const ArenaBloco *b)
{
    return b->anterior == NULL && b->cap == ARENA_BLOCO_INICIAL;
}

/* Tamanho do proximo bloco: o dobro do anterior, entre o inicial e o maximo; n maior que isso pede um bloco sob medida. */
static size_t CapDoProximoBloco(const Arena *a, size_t n)
{
    size_t cap = a->topo == NULL ? ARENA_BLOCO_INICIAL : a->topo->cap * 2;

    if (cap > ARENA_BLOCO_MAXIMO) cap = ARENA_BLOCO_MAXIMO;
    return n + sizeof(ArenaBloco) > cap ? n + sizeof(ArenaBloco) : cap;
}

static void NovoBloco(Arena *a, size_t n)
{
    size_t cap = CapDoProximoBloco(a, n);
    ArenaBloco *novo;

    if (a->topo != NULL && EhPrimeiroZerado(a->topo))
        a->usado_primeiro = a->usado; /* quanto do 1o bloco foi usado: e so isso que o ArenaLiberar zera */
    if (a->topo == NULL && cap == ARENA_BLOCO_INICIAL && primeiro_livre != NULL) {
        novo = primeiro_livre;
        primeiro_livre = NULL;
    } else {
        /* bloco pequeno: malloc e o ArenaAlocar zera o que entrega; o 1o de 16 KiB e os grandes vem zerados do calloc */
        novo = cap <= ARENA_BLOCO_ZERO_POR_USO && !(a->topo == NULL && cap == ARENA_BLOCO_INICIAL) ? malloc(cap) : calloc(1, cap);
    }
    if (novo == NULL) FalhaInterna("memoria esgotada pedindo %zu bytes na arena", n);
    novo->anterior = a->topo;
    novo->cap = cap;
    a->topo = novo;
    a->usado = sizeof(ArenaBloco);
}

void *ArenaAlocar(Arena *a, size_t n)
{
    n = (n + 7) & ~(size_t)7;
    if (a->topo == NULL || a->usado + n > a->topo->cap) NovoBloco(a, n);
    void *p = (char *)a->topo + a->usado;
    a->usado += n;
    if (a->topo->cap <= ARENA_BLOCO_ZERO_POR_USO && !EhPrimeiroZerado(a->topo)) memset(p, 0, n);
    return p;
}

char *ArenaCopiarTexto(Arena *a, const char *s, size_t n)
{
    char *copia = ArenaAlocar(a, n + 1);
    memcpy(copia, s, n);
    return copia;
}

void ArenaLiberar(Arena *a)
{
    size_t usado_primeiro = a->topo != NULL && a->topo->anterior == NULL ? a->usado : a->usado_primeiro;

    while (a->topo != NULL) {
        ArenaBloco *anterior = a->topo->anterior;

        if (EhPrimeiroZerado(a->topo) && primeiro_livre == NULL) {
            memset((char *)a->topo + sizeof(ArenaBloco), 0, usado_primeiro - sizeof(ArenaBloco));
            primeiro_livre = a->topo;
        } else {
            free(a->topo);
        }
        a->topo = anterior;
    }
    a->usado = 0;
    a->usado_primeiro = 0;
}
