/* vec.h - vetor crescente de elementos de tamanho fixo (tokens, simbolos, listas de AST).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - Fase 5i: VecReservar, VecCrescer e VecEmpurrarCru (push inline sem memset).
 *            0.1.0 - Fase 5a: substitui os "cap/len" soltos do Ctx do prototipo.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_VEC_H
#define FORJA_VEC_H

#include "common/base.h"

typedef struct { void *dados; size_t n, cap, tam; } Vec; /* tam = bytes por elemento */

void VecIniciar(Vec *v, size_t tam_elemento);

/* Abre um slot zerado no fim e devolve o ponteiro dele. O ponteiro vale ate o proximo VecEmpurrar. */
void *VecEmpurrar(Vec *v);

/* Garante capacidade para 'cap' elementos de uma vez (so reserva; nao toca as paginas nem muda n). */
void VecReservar(Vec *v, size_t cap);

/* Dobra a capacidade (ou abre a inicial). Parte lenta de VecEmpurrarCru. */
void VecCrescer(Vec *v);

/* Como VecEmpurrar, mas o slot NAO e zerado: quem chama grava todos os campos que vai ler (laco quente do lexer). */
static inline void *VecEmpurrarCru(Vec *v)
{
    if (v->n == v->cap) VecCrescer(v);
    return (char *)v->dados + v->n++ * v->tam;
}

/* Elemento i (sem checagem de limite; quem chama garante i < v->n). */
static inline void *VecEm(const Vec *v, size_t i)
{
    return (char *)v->dados + i * v->tam;
}

void VecLiberar(Vec *v);

#endif
