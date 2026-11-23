/* analises_vivo.c - vivacidade dos vregs da BIR (conjuntos ESPARSOS por bloco, ponto fixo por fila de trabalho). Ver analises.h.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.2.0 - M7b F2: conjuntos esparsos (vetor de vregs em ordem crescente) no lugar do bitset blocos x vregs, e fila de trabalho (so recalcula o bloco cujo sucessor mudou).
 *                    Mesmos conjuntos (ponto fixo minimo): o bitset custava 650 ms numa funcao de 23 mil blocos e 109 mil vregs (chamador com muito inline) e este leva poucos ms.
 *            0.1.0 - M7a F1: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises_interno.h"

/* Area de trabalho de um bloco: vivo[v] = 1 se o vreg esta vivo agora; na_lista evita repetir o vreg em lista (que guarda todos os que ja entraram). */
typedef struct { uint8_t *vivo, *na_lista; uint32_t *lista; uint32_t n; ConjuntoVivo entra, sai; } Trabalho;

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static int CompararVreg(const void *x, const void *y)
{
    uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y;

    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Conjuntos de vivos costumam ter poucos vregs: insercao ate VIVO_INSERCAO_MAX, qsort acima. */
#define VIVO_INSERCAO_MAX 24u

static void OrdenarVregs(uint32_t *v, uint32_t n)
{
    if (n > VIVO_INSERCAO_MAX) {
        qsort(v, n, sizeof *v, CompararVreg);
        return;
    }
    for (uint32_t i = 1; i < n; i++) {
        uint32_t x = v[i], j = i;

        for (; j > 0 && v[j - 1] > x; j--)
            v[j] = v[j - 1];
        v[j] = x;
    }
}

static void Adicionar(Trabalho *t, uint32_t v)
{
    t->vivo[v] = 1;
    if (!t->na_lista[v]) {
        t->na_lista[v] = 1;
        t->lista[t->n++] = v;
    }
}

static void UnirConjunto(Trabalho *t, const ConjuntoVivo *s)
{
    for (uint32_t k = 0; k < s->n; k++)
        Adicionar(t, s->v[k]);
}

static void UsarOperando(const AnalisesBir *a, Trabalho *t, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < a->nvregs)
        Adicionar(t, o->id);
}

/* Passo para tras de uma instrucao: tira o que ela define, une a entrada do bloco frio (*_OV) e poe o que ela usa. */
static void PassoParaTras(const AnalisesBir *a, const BirInstr *i, Trabalho *t)
{
    if (i->dst != BIR_SEM_VREG && i->dst < a->nvregs)
        t->vivo[i->dst] = 0;
    if (EhContaOv(i->op) && i->alvo[0] < a->nblocos)
        UnirConjunto(t, &a->vivo_entra[i->alvo[0]]);
    UsarOperando(a, t, &i->a);
    UsarOperando(a, t, &i->b);
    for (uint32_t k = 0; k < i->nargs; k++)
        UsarOperando(a, t, &i->args[k]);
}

/* Une os vivo_entra dos sucessores do terminador (SALTA, DESVIA). */
static void UnirSucessoresDoTerminador(const AnalisesBir *a, const BirBloco *blk, Trabalho *t)
{
    const BirInstr *i = &blk->instrs[blk->n - 1];

    if (i->op != BIR_SALTA && i->op != BIR_DESVIA)
        return;
    if (i->alvo[0] < a->nblocos)
        UnirConjunto(t, &a->vivo_entra[i->alvo[0]]);
    if (i->op == BIR_DESVIA && i->alvo[1] < a->nblocos)
        UnirConjunto(t, &a->vivo_entra[i->alvo[1]]);
}

/* Os vregs vivos agora, em ordem crescente, num vetor da arena; zerar = limpa a area de trabalho. */
static ConjuntoVivo Coletar(AnalisesBir *a, Trabalho *t, bool zerar)
{
    ConjuntoVivo s = {0};

    for (uint32_t k = 0; k < t->n; k++)
        s.n += t->vivo[t->lista[k]];
    if (s.n > 0) {
        s.v = AnalisesVetor32(a, s.n);
        for (uint32_t k = 0, w = 0; k < t->n; k++) {
            if (t->vivo[t->lista[k]])
                s.v[w++] = t->lista[k];
        }
        OrdenarVregs(s.v, s.n);
    }
    if (zerar) {
        for (uint32_t k = 0; k < t->n; k++)
            t->vivo[t->lista[k]] = t->na_lista[t->lista[k]] = 0;
        t->n = 0;
    }
    return s;
}

static bool MesmoConjunto(const ConjuntoVivo *x, const ConjuntoVivo *y)
{
    return x->n == y->n && (x->n == 0 || memcmp(x->v, y->v, (size_t)x->n * sizeof *x->v) == 0);
}

/* Recalcula entrada e saida do bloco b (em t->entra e t->sai) a partir dos vivo_entra atuais dos sucessores. */
static void RecalcularBloco(AnalisesBir *a, const BirFuncao *f, uint32_t b, Trabalho *t)
{
    const BirBloco *blk = &f->blocos[b];

    if (blk->n > 0)
        UnirSucessoresDoTerminador(a, blk, t);
    t->sai = Coletar(a, t, false);
    for (uint32_t k = blk->n; k > 0; k--)
        PassoParaTras(a, &blk->instrs[k - 1], t);
    t->entra = Coletar(a, t, true);
}

/* Fila de trabalho circular de blocos (cada bloco no maximo uma vez: em_fila). */
typedef struct { uint32_t *v; uint8_t *em_fila; uint32_t cap, ini, n; } Fila;

static void Empurrar(Fila *q, uint32_t b)
{
    if (q->em_fila[b])
        return;
    q->em_fila[b] = 1;
    q->v[(q->ini + q->n++) % q->cap] = b;
}

static uint32_t Retirar(Fila *q)
{
    uint32_t b = q->v[q->ini];

    q->ini = (q->ini + 1) % q->cap;
    q->n--;
    q->em_fila[b] = 0;
    return b;
}

void AnalisesCalcularVivo(AnalisesBir *a, const BirFuncao *f)
{
    Trabalho t = {.vivo = ArenaAlocar(&a->arena, (size_t)a->nvregs + 1), .na_lista = ArenaAlocar(&a->arena, (size_t)a->nvregs + 1),
                  .lista = AnalisesVetor32(a, a->nvregs)};
    Fila q = {.v = AnalisesVetor32(a, a->nblocos), .em_fila = ArenaAlocar(&a->arena, (size_t)a->nblocos + 1), .cap = a->nblocos + 1};

    a->vivo_entra = ArenaAlocar(&a->arena, ((size_t)a->nblocos + 1) * sizeof *a->vivo_entra);
    a->vivo_sai = ArenaAlocar(&a->arena, ((size_t)a->nblocos + 1) * sizeof *a->vivo_sai);
    for (uint32_t i = a->nrpo; i > 0; i--)
        Empurrar(&q, a->rpo[i - 1]);
    while (q.n > 0) {
        uint32_t b = Retirar(&q);

        RecalcularBloco(a, f, b, &t);
        a->vivo_sai[b] = t.sai;
        if (MesmoConjunto(&t.entra, &a->vivo_entra[b]))
            continue;
        a->vivo_entra[b] = t.entra;
        for (uint32_t k = a->pred_ini[b]; k < a->pred_ini[b + 1]; k++) {
            if (a->pos_rpo[a->pred[k]] != BIR_SEM_BLOCO)
                Empurrar(&q, a->pred[k]);
        }
    }
}

bool AnalisesVivoConfere(AnalisesBir *a, const BirFuncao *f, uint32_t b)
{
    Trabalho t = {.vivo = ArenaAlocar(&a->arena, (size_t)a->nvregs + 1), .na_lista = ArenaAlocar(&a->arena, (size_t)a->nvregs + 1),
                  .lista = AnalisesVetor32(a, a->nvregs)};

    RecalcularBloco(a, f, b, &t);
    return MesmoConjunto(&t.entra, &a->vivo_entra[b]) && MesmoConjunto(&t.sai, &a->vivo_sai[b]);
}

static bool TemVreg(const ConjuntoVivo *s, uint32_t vreg)
{
    uint32_t lo = 0, hi = s->n;

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (s->v[meio] == vreg)
            return true;
        if (s->v[meio] < vreg)
            lo = meio + 1;
        else
            hi = meio;
    }
    return false;
}

bool AnalisesVivoEntra(const AnalisesBir *a, uint32_t bloco, uint32_t vreg)
{
    return bloco < a->nblocos && vreg < a->nvregs && TemVreg(&a->vivo_entra[bloco], vreg);
}

bool AnalisesVivoSai(const AnalisesBir *a, uint32_t bloco, uint32_t vreg)
{
    return bloco < a->nblocos && vreg < a->nvregs && TemVreg(&a->vivo_sai[bloco], vreg);
}
