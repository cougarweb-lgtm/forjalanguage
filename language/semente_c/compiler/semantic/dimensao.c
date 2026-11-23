/* dimensao.c - tabela de dimensoes de unidades (termos esparsos internados por hash).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Fase 5c: ideia do prototipo (dimensao = vetor de expoentes, DimId por inteiro) com termos esparsos.
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/dimensao.h"

#include <stdlib.h>

enum { CAP_INICIAL = 64 };

static uint32_t HashTermos(const DimTermo *x, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) {
        h = (h ^ x[i].base) * 16777619u;
        h = (h ^ (uint16_t)x[i].expoente) * 16777619u;
    }
    return h;
}

static bool TermosIguais(const DimTabela *t, DimId id, const DimTermo *x, uint32_t n)
{
    const DimItem *it = VecEm(&t->itens, id);
    return it->qtd == n && (n == 0 || memcmp(VecEm(&t->termos, it->inicio), x, n * sizeof *x) == 0);
}

static DimId Acrescentar(DimTabela *t, const DimTermo *x, uint32_t n)
{
    DimItem *it = VecEmpurrar(&t->itens);
    it->inicio = (uint32_t)t->termos.n;
    it->qtd = n;
    it->nome = -1;
    for (uint32_t i = 0; i < n; i++) *(DimTermo *)VecEmpurrar(&t->termos) = x[i];
    return (DimId)(t->itens.n - 1);
}

static uint32_t *AlocarTabela(uint32_t cap)
{
    uint32_t *tabela = calloc(cap, sizeof *tabela);
    if (tabela == NULL) FalhaInterna("memoria esgotada na tabela de dimensoes");
    return tabela;
}

static void Crescer(DimTabela *t)
{
    uint32_t cap = t->cap * 2;
    uint32_t *nova = AlocarTabela(cap);
    for (size_t id = 0; id < t->itens.n; id++) {
        const DimItem *it = VecEm(&t->itens, id);
        uint32_t i = HashTermos(VecEm(&t->termos, it->inicio), it->qtd) & (cap - 1);
        while (nova[i] != 0) i = (i + 1) & (cap - 1);
        nova[i] = (uint32_t)id + 1;
    }
    free(t->tabela);
    t->tabela = nova;
    t->cap = cap;
}

/* Id da dimensao com esses termos (cria se for nova). 'x' nao pode apontar para t->termos. */
static DimId InternarTermos(DimTabela *t, const DimTermo *x, uint32_t n)
{
    uint32_t mascara = t->cap - 1, i = HashTermos(x, n) & mascara;
    while (t->tabela[i] != 0) {
        if (TermosIguais(t, t->tabela[i] - 1, x, n)) return t->tabela[i] - 1;
        i = (i + 1) & mascara;
    }
    DimId novo = Acrescentar(t, x, n);
    t->tabela[i] = novo + 1;
    if (t->itens.n * 2 > t->cap) Crescer(t);
    return novo;
}

void DimIniciar(DimTabela *t)
{
    VecIniciar(&t->itens, sizeof(DimItem));
    VecIniciar(&t->termos, sizeof(DimTermo));
    VecIniciar(&t->rascunho, sizeof(DimTermo));
    t->cap = CAP_INICIAL;
    t->tabela = AlocarTabela(t->cap);
    t->qtd_bases = 0;
    InternarTermos(t, NULL, 0);
}

DimId DimNovaBase(DimTabela *t, int32_t nome)
{
    DimTermo termo = {(uint16_t)t->qtd_bases++, 1};
    DimId d = InternarTermos(t, &termo, 1);
    DimNomear(t, d, nome);
    return d;
}

typedef struct { const DimTermo *a, *b; uint32_t na, nb, i, j; int sinal; } Mescla;

/* Consome o proximo termo (menor base) de a e/ou b; expoente zero some. */
static void MesclarProximo(DimTabela *t, Mescla *m)
{
    DimTermo r;
    bool tem_a = m->i < m->na, tem_b = m->j < m->nb;
    if (tem_a && (!tem_b || m->a[m->i].base < m->b[m->j].base)) {
        r = m->a[m->i++];
    } else if (tem_b && (!tem_a || m->b[m->j].base < m->a[m->i].base)) {
        r = m->b[m->j++];
        r.expoente = (int16_t)(r.expoente * m->sinal);
    } else {
        r = m->a[m->i++];
        r.expoente = (int16_t)(r.expoente + m->sinal * m->b[m->j++].expoente);
    }
    if (r.expoente != 0) *(DimTermo *)VecEmpurrar(&t->rascunho) = r;
}

DimId DimCombinar(DimTabela *t, DimId a, DimId b, int sinal)
{
    const DimItem *ia = VecEm(&t->itens, a), *ib = VecEm(&t->itens, b);
    Mescla m = {VecEm(&t->termos, ia->inicio), VecEm(&t->termos, ib->inicio), ia->qtd, ib->qtd, 0, 0, sinal};
    t->rascunho.n = 0;
    while (m.i < m.na || m.j < m.nb) MesclarProximo(t, &m);
    return InternarTermos(t, t->rascunho.dados, (uint32_t)t->rascunho.n);
}

void DimNomear(DimTabela *t, DimId d, int32_t nome)
{
    DimItem *it = VecEm(&t->itens, d);
    if (it->nome < 0 && d != DIM_ADIMENSIONAL) it->nome = nome;
}

int32_t DimNome(const DimTabela *t, DimId d)
{
    return ((const DimItem *)VecEm(&t->itens, d))->nome;
}

bool DimDeclarada(const DimTabela *t, DimId d)
{
    return d == DIM_ADIMENSIONAL || DimNome(t, d) >= 0;
}

void DimLiberar(DimTabela *t)
{
    free(t->tabela);
    VecLiberar(&t->itens);
    VecLiberar(&t->termos);
    VecLiberar(&t->rascunho);
    t->tabela = NULL;
}
