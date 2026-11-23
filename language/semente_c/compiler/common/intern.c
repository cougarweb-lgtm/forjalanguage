/* intern.c - tabela de nomes internados.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M2 P17c: InternerAchar (mesma sondagem do Internar, sem criar o nome).
 *            0.1.0 - Fase 5a.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/intern.h"

#include <stdlib.h>

#define INTERN_TABELA_INICIAL 1024

static uint32_t HashBytes(const char *s, uint32_t tam)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < tam; i++) h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h;
}

static void CresceTabela(Interner *in)
{
    uint32_t cap = in->tabela_cap ? in->tabela_cap * 2 : INTERN_TABELA_INICIAL;
    InternSlot *tabela = malloc((size_t)cap * sizeof(InternSlot));
    if (tabela == NULL) FalhaInterna("memoria esgotada na tabela de nomes (%u slots)", cap);
    memset(tabela, 0xFF, (size_t)cap * sizeof(InternSlot));
    for (size_t i = 0; i < in->nomes.n; i++) {
        const Nome *nome = VecEm(&in->nomes, i);
        uint32_t slot = nome->hash & (cap - 1);
        while (tabela[slot].idx >= 0) slot = (slot + 1) & (cap - 1);
        tabela[slot].idx = (int32_t)i;
        tabela[slot].hash = nome->hash;
    }
    free(in->tabela);
    in->tabela = tabela;
    in->tabela_cap = cap;
}

void InternerIniciar(Interner *in, Arena *arena)
{
    in->arena = arena;
    VecIniciar(&in->nomes, sizeof(Nome));
    in->tabela = NULL;
    in->tabela_cap = 0;
}

static int32_t NovoNome(Interner *in, InternSlot *slot, const char *s, uint32_t tam, uint32_t hash)
{
    Nome *nome = VecEmpurrar(&in->nomes);
    nome->texto = ArenaCopiarTexto(in->arena, s, tam);
    nome->tam = tam;
    nome->hash = hash;
    slot->idx = (int32_t)(in->nomes.n - 1);
    slot->hash = hash;
    return slot->idx;
}

int32_t Internar(Interner *in, const char *s, uint32_t tam)
{
    uint32_t hash = HashBytes(s, tam);
    if ((in->nomes.n + 1) * 2 > in->tabela_cap) CresceTabela(in);
    uint32_t slot = hash & (in->tabela_cap - 1);
    while (in->tabela[slot].idx >= 0) {
        if (in->tabela[slot].hash == hash) {
            const Nome *nome = VecEm(&in->nomes, (size_t)in->tabela[slot].idx);
            if (nome->tam == tam && memcmp(nome->texto, s, tam) == 0) return in->tabela[slot].idx;
        }
        slot = (slot + 1) & (in->tabela_cap - 1);
    }
    return NovoNome(in, &in->tabela[slot], s, tam, hash);
}

int32_t InternerAchar(const Interner *in, const char *s, uint32_t tam)
{
    if (in->tabela_cap == 0) return -1;
    uint32_t hash = HashBytes(s, tam);
    uint32_t slot = hash & (in->tabela_cap - 1);
    while (in->tabela[slot].idx >= 0) {
        if (in->tabela[slot].hash == hash) {
            const Nome *nome = VecEm(&in->nomes, (size_t)in->tabela[slot].idx);
            if (nome->tam == tam && memcmp(nome->texto, s, tam) == 0) return in->tabela[slot].idx;
        }
        slot = (slot + 1) & (in->tabela_cap - 1);
    }
    return -1;
}

const char *InternerTexto(const Interner *in, int32_t id)
{
    return ((const Nome *)VecEm(&in->nomes, (size_t)id))->texto;
}

uint32_t InternerTam(const Interner *in, int32_t id)
{
    return ((const Nome *)VecEm(&in->nomes, (size_t)id))->tam;
}

void InternerLiberar(Interner *in)
{
    VecLiberar(&in->nomes);
    free(in->tabela);
    in->tabela = NULL;
    in->tabela_cap = 0;
}
