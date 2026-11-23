/* vec.c - vetor crescente.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Fase 5i: VecReservar/VecCrescer; VecEmpurrar passa a usar VecEmpurrarCru + memset.
 *            0.1.0 - Fase 5a.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/vec.h"

#include <stdlib.h>

#define VEC_CAP_INICIAL 64

void VecIniciar(Vec *v, size_t tam_elemento)
{
    v->dados = NULL;
    v->n = v->cap = 0;
    v->tam = tam_elemento;
}

void VecReservar(Vec *v, size_t cap)
{
    if (cap <= v->cap) return;
    void *novo = realloc(v->dados, cap * v->tam);
    if (novo == NULL) FalhaInterna("memoria esgotada no vetor (%zu elementos de %zu bytes)", cap, v->tam);
    v->dados = novo;
    v->cap = cap;
}

void VecCrescer(Vec *v)
{
    VecReservar(v, v->cap ? v->cap * 2 : VEC_CAP_INICIAL);
}

void *VecEmpurrar(Vec *v)
{
    void *slot = VecEmpurrarCru(v);
    memset(slot, 0, v->tam);
    return slot;
}

void VecLiberar(Vec *v)
{
    free(v->dados);
    v->dados = NULL;
    v->n = v->cap = 0;
}
