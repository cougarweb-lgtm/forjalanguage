/* numeracao_fatos.c - tabela de fatos do passe `numeracao` (ver numeracao_interno.h): inserir, buscar, invalidar por versao e desfazer na ordem inversa.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M7b R3 rodada 2: NumTemp/NumPreparar (memoria de trabalho reaproveitada entre funcoes); NumFatosLiberar so no fim do programa.
 *            0.1.0 - M7b R3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/numeracao_interno.h"

static void *Crescer(void *p, size_t novo, size_t tam)
{
    void *r = realloc(p, novo * tam);

    if (r == NULL)
        FalhaInterna("numeracao: memoria esgotada");
    return r;
}

void NumVetorAcrescentar(NumVetor *vetor, uint32_t valor)
{
    if (vetor->n == vetor->cap) {
        vetor->cap = vetor->cap == 0 ? 64u : vetor->cap * 2u;
        vetor->v = Crescer(vetor->v, vetor->cap, sizeof(uint32_t));
    }
    ((uint32_t *)vetor->v)[vetor->n++] = valor;
}

uint32_t *NumTemp(NumCtx *c, size_t palavras, int preenchimento)
{
    uint32_t *p;

    if (c->temp_usado + palavras > c->temp_cap)
        FalhaInterna("numeracao: memoria de trabalho insuficiente (%zu + %zu > %zu)", c->temp_usado, palavras, c->temp_cap);
    p = c->temp + c->temp_usado;
    c->temp_usado += palavras;
    memset(p, preenchimento, palavras * sizeof *p);
    return p;
}

void NumPreparar(NumCtx *c, size_t palavras)
{
    if (palavras > c->temp_cap) {
        free(c->temp);
        c->temp_cap = palavras + palavras / 2u;
        c->temp = Crescer(/* lint-ok: iaf-nome-generico temp e o nome do campo (memoria de trabalho) */NULL, c->temp_cap, sizeof *c->temp);
    }
    c->temp_usado = 0;
    c->nfatos = 0;
    c->cargas.n = c->verlog.n = c->mortos.n = 0;
    c->seq = 0;
    c->n = (NumContagem){0};
}

void NumFatosIniciar(NumCtx *c, uint32_t ninstrs)
{
    uint32_t baldes = 64u;

    while (baldes < ninstrs && baldes < (1u << 22))
        baldes <<= 1;
    c->mascara = baldes - 1u;
    c->balde = NumTemp(c, baldes, 0xFF);
    c->ver = NumTemp(c, (size_t)c->nvregs + 1u, 0);
    c->cp = NumTemp(c, (size_t)c->nvregs + 1u, 0xFF);
}

/* Fim do programa: devolve tudo (a memoria de trabalho e os vetores vivem o programa inteiro). */
void NumFatosLiberar(NumCtx *c)
{
    free(c->fatos);
    free(c->temp);
    free(c->cargas.v);
    free(c->verlog.v);
    free(c->mortos.v);
}

NumMarca NumMarcar(const NumCtx *c)
{
    return (NumMarca){c->nfatos, c->cargas.n, c->verlog.n, c->mortos.n};
}

/* Desfaz o ultimo fato: devolve o balde (ou o fato de copia do vreg) ao anterior. */
static void DesfazerFato(NumCtx *c, uint32_t k)
{
    const NumFato *f = &c->fatos[k];

    if (f->copia)
        c->cp[f->dep[0]] = f->prox;
    else
        c->balde[f->hash & c->mascara] = f->prox;
}

/* versoes = false: so descarta os fatos e as cargas lembradas (usado no 1o *_OV do bloco: o que nasce depois nao vale no bloco frio); as versoes e as mortes ficam (so invalidam mais). */
void NumRestaurar(NumCtx *c, NumMarca marca, bool versoes)
{
    while (c->nfatos > marca.nfatos)
        DesfazerFato(c, --c->nfatos);
    c->cargas.n = marca.ncargas;
    if (!versoes)
        return;
    while (c->verlog.n > marca.nverlog)
        c->ver[((uint32_t *)c->verlog.v)[--c->verlog.n]]--;
    while (c->mortos.n > marca.nmortos)
        c->fatos[((uint32_t *)c->mortos.v)[--c->mortos.n]].morta = 0;
}

void NumDefinir(NumCtx *c, uint32_t vreg)
{
    c->ver[vreg]++;
    NumVetorAcrescentar(&c->verlog, vreg);
}

bool NumFatoValido(const NumCtx *c, const NumFato *fato, uint32_t ebb)
{
    if (fato->morta || (!fato->global && fato->ebb != ebb))
        return false;
    for (uint32_t k = 0; k < 3; k++) {
        if (fato->dep[k] != NUM_SEM && c->ver[fato->dep[k]] != fato->ver[k])
            return false;
    }
    return true;
}

static bool ChavesIguais(const NumChave *x, const NumChave *y)
{
    return x->op == y->op && x->aux == y->aux && x->classe == y->classe && x->ta == y->ta && x->tb == y->tb && x->ca == y->ca && x->cb == y->cb && x->va == y->va &&
           x->vb == y->vb && x->desloc == y->desloc;
}

static uint32_t Misturar(uint64_t h, uint64_t v)
{
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return (uint32_t)(h ^ (h >> 32));
}

static uint32_t HashDa(const NumChave *k)
{
    uint64_t h = k->op | ((uint64_t)k->aux << 8) | ((uint64_t)k->classe << 16) | ((uint64_t)k->ta << 24) | ((uint64_t)k->tb << 32) | ((uint64_t)k->ca << 40) | ((uint64_t)k->cb << 48);

    h = Misturar(h, (uint64_t)k->va);
    h = Misturar(h, (uint64_t)k->vb);
    return Misturar(h, (uint64_t)(uint32_t)k->desloc);
}

/* O fato mais novo e valido da chave, ou NUM_SEM. Visita no maximo NUM_CADEIA_MAX nos. */
uint32_t NumBuscar(const NumCtx *c, const NumChave *chave, uint32_t ebb)
{
    uint32_t k = c->balde[HashDa(chave) & c->mascara];

    for (uint32_t visitados = 0; k != NUM_SEM && visitados < NUM_CADEIA_MAX; visitados++) {
        const NumFato *f = &c->fatos[k];

        if (!f->copia && ChavesIguais(&f->chave, chave) && NumFatoValido(c, f, ebb))
            return k;
        k = f->prox;
    }
    return NUM_SEM;
}

/* Fato novo no topo da pilha; fato de copia entra em cp[d] (d = dep[0]), os demais no balde da chave. Carga entra tambem na lista de cargas da EBB. */
void NumInserir(NumCtx *c, NumFato fato)
{
    if (c->nfatos == c->capfatos) {
        c->capfatos = c->capfatos == 0 ? 256u : c->capfatos * 2u;
        c->fatos = Crescer(c->fatos, c->capfatos, sizeof *c->fatos);
    }
    fato.morta = 0;
    if (fato.copia) {
        fato.prox = c->cp[fato.dep[0]];
        c->cp[fato.dep[0]] = c->nfatos;
    } else {
        fato.hash = HashDa(&fato.chave);
        fato.prox = c->balde[fato.hash & c->mascara];
        c->balde[fato.hash & c->mascara] = c->nfatos;
    }
    if (!fato.copia && fato.chave.op == BIR_CARREGA)
        NumVetorAcrescentar(&c->cargas, c->nfatos);
    c->fatos[c->nfatos++] = fato;
}

void NumMatar(NumCtx *c, uint32_t fato)
{
    if (c->fatos[fato].morta)
        return;
    c->fatos[fato].morta = 1;
    NumVetorAcrescentar(&c->mortos, fato);
}
