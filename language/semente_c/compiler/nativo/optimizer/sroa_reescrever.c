/* sroa_reescrever.c - passe `sroa` (M7b F3), 2a metade: troca no lugar cada acesso a slot escalarizado pelos vregs dos campos (a decisao esta em sroa.c; ver sroa.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b F3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe sroa
 *
 * Nenhuma instrucao muda de ordem: cada acesso vira 1 instrucao (CARREGA -> COPIA, GRAVA -> COPIA/CONST) ou, nas operacoes de bloco, uma por campo, no lugar da original e com o pos e o ir
 * dela. CARREGA mantem o dst (o validador acha a guarda do DIV pelo vreg). COPIA_MEM contra memoria externa vira CARREGA/GRAVA por campo pela mesma base, em ordem crescente de offset.
 * END_SLOT e as COPIA/ADD que so serviam de endereco do slot saem.
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/optimizer/sroa_interno.h"

typedef struct { const SroaSlot *info; const SroaRaiz *raiz; uint32_t nvregs; } Contexto;

typedef struct { BirInstr *v; uint32_t n, cap; } Saida;

/* A instrucao nova: op, classe, dst, operandos e deslocamento; pos e ir vem da original. */
typedef struct { uint8_t op, classe; uint32_t dst; BirOperando a, b; int32_t desloc; } Nova;

static const SroaRaiz *RaizViva(const Contexto *c, const BirOperando *o)
{
    const SroaRaiz *r;

    if (o->tipo != BO_VREG || o->id >= c->nvregs)
        return NULL;
    r = &c->raiz[o->id];
    return r->slot != SROA_SEM && c->info[r->slot].motivo == SM_OK ? r : NULL;
}

static bool DefineEndereco(const Contexto *c, const BirInstr *i)
{
    const SroaRaiz *r;

    if (i->op != BIR_END_SLOT && i->op != BIR_COPIA && i->op != BIR_ADD)
        return false;
    if (i->dst >= c->nvregs)
        return false;
    r = &c->raiz[i->dst];
    return r->slot != SROA_SEM && c->info[r->slot].motivo == SM_OK;
}

/* A instrucao precisa ser trocada ou apagada? */
static bool Mexe(const Contexto *c, const BirInstr *i)
{
    switch (i->op) {
    case BIR_CARREGA: case BIR_GRAVA: case BIR_ZERA_MEM:
        return RaizViva(c, &i->a) != NULL;
    case BIR_COPIA_MEM:
        return RaizViva(c, &i->a) != NULL || RaizViva(c, &i->b) != NULL;
    default:
        return DefineEndereco(c, i);
    }
}

static BirInstr *Reservar(Saida *s)
{
    if (s->n == s->cap) {
        void *novo;

        s->cap = s->cap == 0 ? 8u : s->cap * 2u;
        novo = realloc(s->v, (size_t)s->cap * sizeof *s->v);
        if (novo == NULL)
            FalhaInterna("sroa: memoria esgotada");
        s->v = novo;
    }
    return &s->v[s->n++];
}

static void Emitir(Saida *s, const BirInstr *orig, const Nova *n)
{
    BirInstr *i = Reservar(s);

    *i = (BirInstr){.op = n->op, .classe = n->classe, .dst = n->dst, .a = n->a, .b = n->b, .simbolo = BIR_SEM_SIMBOLO, .alvo = {BIR_SEM_BLOCO, BIR_SEM_BLOCO},
                    .pos = orig->pos, .ir = orig->ir, .desloc = n->desloc};
}

static BirOperando Vreg(uint32_t id, uint8_t classe)
{
    return (BirOperando){.tipo = BO_VREG, .classe = classe, .id = id};
}

/* O campo do slot que comeca em off (exato); a decisao de sroa.c garante que existe. */
static const SroaCampo *CampoEm(const Contexto *c, uint32_t slot, int32_t off)
{
    const SroaSlot *s = &c->info[slot];

    for (uint32_t k = 0; k < s->nf; k++) {
        if (s->campo[k].off == off)
            return &s->campo[k];
    }
    FalhaInterna("sroa: campo ausente no slot %u, offset %d", (unsigned)slot, (int)off);
}

static void EmitirCarrega(const Contexto *c, Saida *s, const BirInstr *i)
{
    const SroaRaiz *r = RaizViva(c, &i->a);
    const SroaCampo *f = CampoEm(c, r->slot, r->off + i->desloc);
    Nova n = {.op = BIR_COPIA, .classe = i->classe, .dst = i->dst, .a = Vreg(f->vreg, i->classe)};

    Emitir(s, i, &n);
}

static void EmitirGrava(const Contexto *c, Saida *s, const BirInstr *i)
{
    const SroaRaiz *r = RaizViva(c, &i->a);
    const SroaCampo *f = CampoEm(c, r->slot, r->off + i->desloc);
    Nova n = {.op = i->b.tipo == BO_IMED ? BIR_CONST : BIR_COPIA, .classe = i->classe, .dst = f->vreg, .a = i->b};

    Emitir(s, i, &n);
}

/* Indices em campo[] [ini, fim) dos campos de um slot dentro de [off, off+n). */
typedef struct { uint32_t ini, fim; } FaixaCampos;

static FaixaCampos FaixaDeCampos(const SroaSlot *s, int32_t off, uint32_t n)
{
    FaixaCampos faixa = {0, 0};

    for (uint32_t k = 0; k < s->nf; k++) {
        if (s->campo[k].off < off)
            faixa.ini = k + 1;
        if (s->campo[k].off + (int32_t)s->campo[k].tam <= off + (int32_t)n)
            faixa.fim = k + 1;
    }
    return faixa;
}

static void EmitirZera(const Contexto *c, Saida *s, const BirInstr *i)
{
    const SroaRaiz *r = RaizViva(c, &i->a);
    FaixaCampos faixa = FaixaDeCampos(&c->info[r->slot], r->off, (uint32_t)i->desloc);

    for (uint32_t k = faixa.ini; k < faixa.fim; k++) {
        const SroaCampo *f = &c->info[r->slot].campo[k];
        Nova n = {.op = BIR_CONST, .classe = f->classe, .dst = f->vreg, .a = {.tipo = BO_IMED, .classe = f->classe}};

        Emitir(s, i, &n);
    }
}

/* Um campo da copia: campo f do lado escalarizado. dst_vivo: o escalarizado e o destino da copia. O outro lado e outro slot escalarizado (rb != NULL) ou memoria externa (base). */
typedef struct { const SroaCampo *f; int32_t desloc; const SroaRaiz *rb; const BirOperando *base; bool dst_vivo; int32_t off_b; } LadoCopia;

static void EmitirCampoCopiado(const Contexto *c, Saida *s, const BirInstr *i, const LadoCopia *l)
{
    const SroaCampo *f = l->f;
    Nova n = {.classe = f->classe};

    if (l->rb != NULL) {
        const SroaCampo *g = CampoEm(c, l->rb->slot, l->off_b + l->desloc);

        n.op = BIR_COPIA;
        n.dst = l->dst_vivo ? f->vreg : g->vreg;
        n.a = Vreg(l->dst_vivo ? g->vreg : f->vreg, f->classe);
    } else if (l->dst_vivo) {
        n.op = BIR_CARREGA;
        n.dst = f->vreg;
        n.a = *l->base;
        n.desloc = l->desloc;
    } else {
        n.op = BIR_GRAVA;
        n.dst = BIR_SEM_VREG;
        n.a = *l->base;
        n.b = Vreg(f->vreg, f->classe);
        n.desloc = l->desloc;
    }
    Emitir(s, i, &n);
}

static void EmitirCopiaMem(const Contexto *c, Saida *s, const BirInstr *i)
{
    const SroaRaiz *ra = RaizViva(c, &i->a), *rb = RaizViva(c, &i->b);
    const SroaRaiz *dono = ra != NULL ? ra : rb;
    FaixaCampos faixa = FaixaDeCampos(&c->info[dono->slot], dono->off, (uint32_t)i->desloc);
    LadoCopia l = {.dst_vivo = ra != NULL};

    l.rb = ra != NULL && rb != NULL ? rb : NULL;
    l.base = ra != NULL ? &i->b : &i->a;
    l.off_b = rb != NULL ? rb->off : 0;
    for (uint32_t k = faixa.ini; k < faixa.fim; k++) {
        l.f =&c->info[dono->slot].campo[k];
        l.desloc = l.f->off - dono->off;
        EmitirCampoCopiado(c, s, i, &l);
    }
}

static void Trocar(const Contexto *c, Saida *s, const BirInstr *i)
{
    switch (i->op) {
    case BIR_CARREGA: EmitirCarrega(c, s, i); break;
    case BIR_GRAVA: EmitirGrava(c, s, i); break;
    case BIR_ZERA_MEM: EmitirZera(c, s, i); break;
    case BIR_COPIA_MEM: EmitirCopiaMem(c, s, i); break;
    default: break; /* END_SLOT, COPIA e ADD de endereco: saem */
    }
}

static void ReescreverBloco(const Contexto *c, BirBloco *b)
{
    Saida s = {0};
    bool mexe = false;

    for (uint32_t j = 0; j < b->n && !mexe; j++)
        mexe = Mexe(c, &b->instrs[j]);
    if (!mexe)
        return;
    for (uint32_t j = 0; j < b->n; j++) {
        if (Mexe(c, &b->instrs[j]))
            Trocar(c, &s, &b->instrs[j]);
        else
            *Reservar(&s) = b->instrs[j];
    }
    free(b->instrs);
    b->instrs = s.v;
    b->n = s.n;
    b->cap = s.cap;
}

void SroaReescrever(BirFuncao *f, const SroaPlano *plano)
{
    const SroaSlot *info = plano->info;
    Contexto c = {.info = info, .raiz = plano->raiz, .nvregs = plano->nvregs_antigo};
    uint8_t *classes = realloc(f->classe_vreg, (size_t)plano->nvregs_novo * sizeof *classes);

    if (classes == NULL)
        FalhaInterna("sroa: memoria esgotada");
    f->classe_vreg = classes;
    for (uint32_t k = 0; k < f->nslots; k++) {
        if (info[k].motivo != SM_OK)
            continue;
        for (uint32_t m = 0; m < info[k].nf; m++)
            classes[info[k].campo[m].vreg] = info[k].campo[m].classe;
    }
    f->nvregs = plano->nvregs_novo;
    for (uint32_t b = 0; b < f->nblocos; b++)
        ReescreverBloco(&c, &f->blocos[b]);
}
