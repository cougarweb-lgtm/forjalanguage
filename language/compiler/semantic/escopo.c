/* escopo.c - pilha de locais com busca por nome em O(1).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Fase 5c: primeira versao (o prototipo guardava o local dentro do simbolo global).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/escopo.h"

void EscopoIniciar(Escopo *e, Arena *arena, uint32_t qtd_nomes)
{
    VecIniciar(&e->pilha, sizeof(LocalInfo));
    e->cabeca = ArenaAlocar(arena, (size_t)qtd_nomes * sizeof *e->cabeca);
}

const LocalInfo *EscopoBuscar(const Escopo *e, int32_t nome)
{
    int32_t topo = e->cabeca[nome];
    return topo == 0 ? NULL : (const LocalInfo *)VecEm(&e->pilha, (size_t)topo - 1);
}

void EscopoEmpurrar(Escopo *e, const LocalInfo *local)
{
    LocalInfo *novo = VecEmpurrar(&e->pilha);
    *novo = *local;
    novo->anterior = e->cabeca[local->nome];
    e->cabeca[local->nome] = (int32_t)e->pilha.n;
}

void EscopoRecuar(Escopo *e, uint32_t altura)
{
    while (e->pilha.n > altura) {
        const LocalInfo *topo = VecEm(&e->pilha, e->pilha.n - 1);
        e->cabeca[topo->nome] = topo->anterior;
        e->pilha.n--;
    }
}

void EscopoLiberar(Escopo *e)
{
    VecLiberar(&e->pilha);
}
