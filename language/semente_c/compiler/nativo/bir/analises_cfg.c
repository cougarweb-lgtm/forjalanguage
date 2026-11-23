/* analises_cfg.c - grafo de fluxo da BIR: sucessores, predecessores, RPO e dominadores (Cooper-Harvey-Kennedy). Ver analises.h.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.2.0 - M7b F2: NumerarDominadores (dom_pre/dom_tam) e AnalisesDomina em O(1) no lugar da subida pelo idom (320 ms numa funcao de 23 mil blocos).
 *            0.1.0 - M7a F1: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#include <string.h>

#include "nativo/bir/analises_interno.h"

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

/* Contexto da montagem dos sucessores: marca[para] = de + 1 evita a aresta repetida; as arestas entram em suc em ordem de bloco, entao as de um bloco ficam contiguas. */
typedef struct { uint32_t nblocos, n; uint32_t *marca, *suc; } MontagemArestas;

static void NovaAresta(MontagemArestas *m, uint32_t de, uint32_t para)
{
    if (para >= m->nblocos || m->marca[para] == de + 1)
        return;
    m->marca[para] = de + 1;
    m->suc[m->n++] = para;
}

/* Arestas do bloco na ordem das instrucoes; o terminador vem por ultimo (DESVIA: alvo[0] e depois alvo[1]). */
static void ArestasDoBloco(MontagemArestas *m, const BirFuncao *f, uint32_t b)
{
    const BirBloco *blk = &f->blocos[b];

    for (uint32_t k = 0; k < blk->n; k++) {
        const BirInstr *i = &blk->instrs[k];

        if (EhContaOv(i->op) || i->op == BIR_SALTA || i->op == BIR_DESVIA)
            NovaAresta(m, b, i->alvo[0]);
        if (i->op == BIR_DESVIA)
            NovaAresta(m, b, i->alvo[1]);
    }
}

/* ini[b+1] = ini[b] + cont[b]. */
static void SomarPrefixos(uint32_t *ini, const uint32_t *cont, uint32_t n)
{
    ini[0] = 0;
    for (uint32_t b = 0; b < n; b++)
        ini[b + 1] = ini[b] + cont[b];
}

/* Predecessores a partir dos sucessores: percorre b em ordem crescente, entao cada lista sai em ordem de id. */
static void MontarPredecessores(AnalisesBir *a)
{
    uint32_t *cont = AnalisesVetor32(a, a->nblocos);

    for (uint32_t k = 0; k < a->suc_ini[a->nblocos]; k++)
        cont[a->suc[k]]++;
    a->pred_ini = AnalisesVetor32(a, (size_t)a->nblocos + 1);
    SomarPrefixos(a->pred_ini, cont, a->nblocos);
    a->pred = AnalisesVetor32(a, a->pred_ini[a->nblocos]);
    memset(cont, 0, ((size_t)a->nblocos + 1) * sizeof *cont);
    for (uint32_t b = 0; b < a->nblocos; b++) {
        for (uint32_t k = a->suc_ini[b]; k < a->suc_ini[b + 1]; k++)
            a->pred[a->pred_ini[a->suc[k]] + cont[a->suc[k]]++] = b;
    }
}

void AnalisesMontarArestas(AnalisesBir *a, const BirFuncao *f)
{
    MontagemArestas m = {.nblocos = a->nblocos, .marca = AnalisesVetor32(a, a->nblocos)};
    size_t total = 0;

    for (uint32_t b = 0; b < a->nblocos; b++)
        total += f->blocos[b].n;
    a->suc_ini = AnalisesVetor32(a, (size_t)a->nblocos + 1);
    m.suc = a->suc = AnalisesVetor32(a, 2 * total); /* cada instrucao da no maximo 2 arestas (o DESVIA) */
    for (uint32_t b = 0; b < a->nblocos; b++) {
        a->suc_ini[b] = m.n;
        ArestasDoBloco(&m, f, b);
    }
    a->suc_ini[a->nblocos] = m.n;
    MontarPredecessores(a);
}

/* Estado da DFS iterativa: visitado[b], prox[b] = proximo indice de suc a olhar, pilha de blocos abertos. */
typedef struct { uint32_t *visitado, *prox, *pilha; } PilhaDfs;

/* DFS a partir da entrada; devolve quantos blocos saiu e a pos-ordem em ordem[]. */
static uint32_t PosOrdem(const AnalisesBir *a, uint32_t *ordem, PilhaDfs *d)
{
    uint32_t topo = 1, n = 0;

    for (uint32_t b = 0; b < a->nblocos; b++)
        d->prox[b] = a->suc_ini[b];
    d->pilha[0] = 0;
    d->visitado[0] = 1;
    while (topo > 0) {
        uint32_t b = d->pilha[topo - 1];

        if (d->prox[b] == a->suc_ini[b + 1]) {
            ordem[n++] = b;
            topo--;
            continue;
        }
        uint32_t s = a->suc[d->prox[b]++];

        if (!d->visitado[s]) {
            d->visitado[s] = 1;
            d->pilha[topo++] = s;
        }
    }
    return n;
}

void AnalisesCalcularRpo(AnalisesBir *a)
{
    uint32_t *ordem = AnalisesVetor32(a, a->nblocos);
    PilhaDfs d = {AnalisesVetor32(a, a->nblocos), AnalisesVetor32(a, a->nblocos), AnalisesVetor32(a, a->nblocos)};

    a->rpo = AnalisesVetor32(a, a->nblocos);
    a->pos_rpo = AnalisesVetor32(a, a->nblocos);
    for (uint32_t b = 0; b < a->nblocos; b++)
        a->pos_rpo[b] = BIR_SEM_BLOCO;
    if (a->nblocos == 0)
        return;
    a->nrpo = PosOrdem(a, ordem, &d);
    for (uint32_t i = 0; i < a->nrpo; i++) {
        a->rpo[i] = ordem[a->nrpo - 1 - i];
        a->pos_rpo[a->rpo[i]] = i;
    }
}

/* Ponto de encontro de x e y na arvore de dominadores (os ja processados); sobe pelo de maior indice de RPO. */
static uint32_t Intersectar(const AnalisesBir *a, uint32_t x, uint32_t y)
{
    while (x != y) {
        while (a->pos_rpo[x] > a->pos_rpo[y])
            x = a->idom[x];
        while (a->pos_rpo[y] > a->pos_rpo[x])
            y = a->idom[y];
    }
    return x;
}

/* idom candidato de b: intersecao dos predecessores alcancaveis que ja tem idom. */
static uint32_t IdomCandidato(const AnalisesBir *a, uint32_t b)
{
    uint32_t novo = BIR_SEM_BLOCO;

    for (uint32_t k = a->pred_ini[b]; k < a->pred_ini[b + 1]; k++) {
        uint32_t p = a->pred[k];

        if (a->pos_rpo[p] == BIR_SEM_BLOCO || a->idom[p] == BIR_SEM_BLOCO)
            continue;
        novo = novo == BIR_SEM_BLOCO ? p : Intersectar(a, novo, p);
    }
    return novo;
}

/* Numera a arvore de dominadores para que "x domina y" seja uma comparacao: a subarvore de x ocupa dom_pre[x] .. dom_pre[x] + dom_tam[x] - 1. No RPO o idom vem antes do filho:
 * tamanhos somando de tras para frente, depois cada filho toma a proxima faixa livre do pai. */
static void NumerarDominadores(AnalisesBir *a)
{
    uint32_t *proximo = AnalisesVetor32(a, a->nblocos);

    a->dom_pre = AnalisesVetor32(a, a->nblocos);
    a->dom_tam = AnalisesVetor32(a, a->nblocos);
    for (uint32_t i = 0; i < a->nrpo; i++)
        a->dom_tam[a->rpo[i]] = 1;
    for (uint32_t i = a->nrpo; i-- > 1;)
        a->dom_tam[a->idom[a->rpo[i]]] += a->dom_tam[a->rpo[i]];
    if (a->nrpo > 0)
        proximo[0] = 1;
    for (uint32_t i = 1; i < a->nrpo; i++) {
        uint32_t b = a->rpo[i], pai = a->idom[b];

        a->dom_pre[b] = proximo[pai];
        proximo[pai] += a->dom_tam[b];
        proximo[b] = a->dom_pre[b] + 1;
    }
}

void AnalisesCalcularIdom(AnalisesBir *a)
{
    bool mudou = a->nrpo > 0;

    a->idom = AnalisesVetor32(a, a->nblocos);
    for (uint32_t b = 0; b < a->nblocos; b++)
        a->idom[b] = BIR_SEM_BLOCO;
    if (a->nrpo > 0)
        a->idom[0] = 0;
    while (mudou) {
        mudou = false;
        for (uint32_t i = 1; i < a->nrpo; i++) {
            uint32_t novo = IdomCandidato(a, a->rpo[i]);

            if (novo != a->idom[a->rpo[i]]) {
                a->idom[a->rpo[i]] = novo;
                mudou = true;
            }
        }
    }
    NumerarDominadores(a);
}

bool AnalisesDomina(const AnalisesBir *a, uint32_t x, uint32_t y)
{
    if (x >= a->nblocos || y >= a->nblocos || a->pos_rpo[y] == BIR_SEM_BLOCO || a->pos_rpo[x] == BIR_SEM_BLOCO)
        return false;
    return a->dom_pre[y] >= a->dom_pre[x] && a->dom_pre[y] < a->dom_pre[x] + a->dom_tam[x];
}
