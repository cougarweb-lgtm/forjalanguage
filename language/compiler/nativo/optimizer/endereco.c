/* endereco.c - passe `endereco` (R6a-3): recoloca a cadeia ADD(x, MUL(i, s)) [+ imm] na frente de cada CARREGA/GRAVA que a compartilhava (ver endereco.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - R6a-3 (D0704 fatia C): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Fases por funcao: (1) conta definicoes e usos por vreg; (2) para cada CARREGA/GRAVA cuja base t tem 2+ usos, todos de base, e e uma cadeia reconhecida, confere a estabilidade das folhas (i, x) do
 * ponto onde cada uma foi lida ate o acesso e anota um Plano; (3) remove o que ficou sem uso; (4) reconstroi os blocos com a cadeia nova colada na frente do acesso.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/bir/analises.h"
#include "nativo/optimizer/cfg.h"
#include "nativo/optimizer/endereco.h"

#define ENDERECO_BLOCOS_MAX 2048u /* acima disso a conferencia de caminhos (O(blocos) por acesso) nao compensa */

typedef struct { uint32_t b, j; } PosEnd;

/* Um acesso a recompor: t (soma), m (multiplicacao) e w (soma com imediato, se tem_w) sao copias das definicoes originais. */
typedef struct {
    uint32_t b, j;
    PosEnd pt, pm, pw;
    bool tem_w;
    BirInstr t, m, w;
} Plano;

typedef struct {
    BirFuncao *f;
    AnalisesBir an;
    uint32_t *defs, *usos, *usos_base, *def_b, *def_j, *inicio, *fila;
    uint8_t *morta, *marca_f, *marca_b;
    Plano *plano;
    uint32_t nplano, capplano;
} EndCtx;

static bool OperandoSimples(const BirOperando *o)
{
    return o->tam == 0 && o->extensao == BE_NENHUMA && o->sysv == NULL;
}

static bool SimplesI64(const BirOperando *o)
{
    return OperandoSimples(o) && o->classe == BC_I64;
}

static bool EhAcesso(const BirInstr *i)
{
    return i->op == BIR_CARREGA || i->op == BIR_GRAVA;
}

/* ---- usos (compartilhado com o licm) ---- */

static void SomarUso(uint32_t *vet, uint32_t n, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < n)
        vet[o->id]++;
}

void OtimEnderecoUsosCalcular(const BirFuncao *funcao, EnderecoUsos *u)
{
    u->n = funcao->nvregs;
    u->usos = calloc((size_t)u->n + 1u, sizeof *u->usos);
    u->usos_base = calloc((size_t)u->n + 1u, sizeof *u->usos_base);
    if (u->usos == NULL || u->usos_base == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t b = 0; b < funcao->nblocos; b++) {
        for (uint32_t j = 0; j < funcao->blocos[b].n; j++) {
            const BirInstr *i = &funcao->blocos[b].instrs[j];

            SomarUso(u->usos, u->n, &i->a);
            SomarUso(u->usos, u->n, &i->b);
            for (uint32_t k = 0; k < i->nargs; k++)
                SomarUso(u->usos, u->n, &i->args[k]);
            if (EhAcesso(i) && OperandoSimples(&i->a) && !(i->op == BIR_GRAVA && i->b.tipo == BO_VREG && i->a.tipo == BO_VREG && i->b.id == i->a.id))
                SomarUso(u->usos_base, u->n, &i->a);
        }
    }
}

void OtimEnderecoUsosLiberar(EnderecoUsos *u)
{
    free(u->usos);
    free(u->usos_base);
    u->usos = u->usos_base = NULL;
}

bool OtimEnderecoSomaFundivel(const EnderecoUsos *u, const BirInstr *soma)
{
    const BirOperando *imed = soma->b.tipo == BO_IMED ? &soma->b : &soma->a;
    const BirOperando *outro = imed == &soma->b ? &soma->a : &soma->b;

    if (soma->op != BIR_ADD || soma->classe != BC_I64 || !SimplesI64(imed) || !SimplesI64(outro) || imed->tipo != BO_IMED || outro->tipo != BO_VREG)
        return false;
    if (imed->imed < INT32_MIN || imed->imed > INT32_MAX || soma->dst >= u->n)
        return false;
    return u->usos[soma->dst] == 1 && u->usos_base[soma->dst] == 1;
}

/* ---- preparo do contexto ---- */

static void *Zerado(size_t n, size_t tam)
{
    void *p = calloc(n + 1u, tam);

    if (p == NULL)
        FalhaInterna("otim: memoria esgotada");
    return p;
}

/* Definicoes (parametro conta uma), posicao da definicao, usos e o indice global de cada instrucao. */
static void ContarFuncao(EndCtx *c)
{
    BirFuncao *f = c->f;
    uint32_t total = 0;

    for (uint32_t v = 0; v < f->nparams && v < f->nvregs; v++)
        c->defs[v]++;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        c->inicio[b] = total;
        total += f->blocos[b].n;
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            uint32_t d = f->blocos[b].instrs[j].dst;

            if (d == BIR_SEM_VREG || d >= f->nvregs)
                continue;
            c->defs[d]++;
            c->def_b[d] = b;
            c->def_j[d] = j;
        }
    }
    c->inicio[f->nblocos] = total;
    c->morta = Zerado(total, 1);
}

static void PrepararContexto(EndCtx *c)
{
    const BirFuncao *f = c->f;
    EnderecoUsos u;

    c->defs = Zerado(f->nvregs, sizeof *c->defs);
    c->def_b = Zerado(f->nvregs, sizeof *c->def_b);
    c->def_j = Zerado(f->nvregs, sizeof *c->def_j);
    c->inicio = Zerado((size_t)f->nblocos + 1u, sizeof *c->inicio);
    c->fila = Zerado(f->nblocos, sizeof *c->fila);
    c->marca_f = Zerado(f->nblocos, 1);
    c->marca_b = Zerado(f->nblocos, 1);
    OtimEnderecoUsosCalcular(f, &u);
    c->usos = u.usos;
    c->usos_base = u.usos_base;
    ContarFuncao(c);
    AnalisesCalcularLacosApenas(f, &c->an);
}

static void LiberarContexto(EndCtx *c)
{
    free(c->defs); free(c->usos); free(c->usos_base); free(c->def_b); free(c->def_j); free(c->inicio); free(c->fila);
    free(c->morta); free(c->marca_f); free(c->marca_b); free(c->plano);
    AnalisesLiberar(&c->an);
}

/* ---- reconhecimento da cadeia ---- */

static const BirInstr *DefUnica(const EndCtx *c, uint32_t v, PosEnd *pos)
{
    if (v >= c->f->nvregs || v < c->f->nparams || c->defs[v] != 1)
        return NULL;
    pos->b = c->def_b[v];
    pos->j = c->def_j[v];
    return &c->f->blocos[pos->b].instrs[pos->j];
}

static bool OpI64Vreg(const BirInstr *i, uint8_t op)
{
    return i != NULL && i->op == op && i->classe == BC_I64 && i->nargs == 0 && i->dst != BIR_SEM_VREG && SimplesI64(&i->a) && SimplesI64(&i->b);
}

/* MUL v = i * {1,2,4,8} (imediato em qualquer lado, i vreg). */
static bool CasarMul(const EndCtx *c, uint32_t v, Plano *p)
{
    const BirInstr *di = DefUnica(c, v, &p->pm);
    const BirOperando *imed, *ind;

    if (!OpI64Vreg(di, BIR_MUL))
        return false;
    imed = di->b.tipo == BO_IMED ? &di->b : &di->a;
    ind = imed == &di->b ? &di->a : &di->b;
    if (imed->tipo != BO_IMED || ind->tipo != BO_VREG || ind->id == v)
        return false;
    if (imed->imed != 1 && imed->imed != 2 && imed->imed != 4 && imed->imed != 8)
        return false;
    p->m = *di;
    return true;
}

/* ADD v = x + imm32 (imediato em qualquer lado, x vreg). */
static bool CasarSomaImediata(const EndCtx *c, uint32_t v, Plano *p)
{
    const BirInstr *di = DefUnica(c, v, &p->pw);
    const BirOperando *imed, *x;

    if (!OpI64Vreg(di, BIR_ADD))
        return false;
    imed = di->b.tipo == BO_IMED ? &di->b : &di->a;
    x = imed == &di->b ? &di->a : &di->b;
    if (imed->tipo != BO_IMED || x->tipo != BO_VREG || x->id == v || imed->imed < INT32_MIN || imed->imed > INT32_MAX)
        return false;
    p->w = *di;
    return true;
}

/* t = ADD(m, b) com m = MUL(i, s); b = w (ADD x imm) ou um vreg qualquer. Preenche t, m e, se houver, w. */
static bool CasarCadeia(const EndCtx *c, uint32_t t, Plano *p)
{
    const BirInstr *di = DefUnica(c, t, &p->pt);
    const BirOperando *outro;

    if (!OpI64Vreg(di, BIR_ADD) || di->a.tipo != BO_VREG || di->b.tipo != BO_VREG || di->dst == di->a.id || di->dst == di->b.id)
        return false;
    if (CasarMul(c, di->a.id, p))
        outro = &di->b;
    else if (CasarMul(c, di->b.id, p))
        outro = &di->a;
    else
        return false;
    if (outro->id == p->m.dst)
        return false;
    p->t = *di;
    p->tem_w = CasarSomaImediata(c, outro->id, p);
    return true;
}

/* ---- estabilidade das folhas ---- */

/* v e definido nas instrucoes de.j .. ate-1 do bloco de.b? */
static bool DefNoBloco(const EndCtx *c, uint32_t v, PosEnd de, uint32_t ate)
{
    const BirBloco *blk = &c->f->blocos[de.b];

    for (uint32_t j = de.j; j < ate && j < blk->n; j++) {
        if (blk->instrs[j].dst == v)
            return true;
    }
    return false;
}

/* Busca em largura pelo grafo: marca, n sementes ja marcadas em fila[0..n), bloco a nao atravessar e o sentido (adiante = sucessores, senao predecessores). */
typedef struct { uint8_t *marca; uint32_t n, evitar; bool adiante; } Busca;

static void Alcancar(const EndCtx *c, Busca bu)
{
    const AnalisesBir *an = &c->an;

    for (uint32_t k = 0; k < bu.n; k++) {
        uint32_t b = c->fila[k];
        const uint32_t *viz = bu.adiante ? &an->suc[an->suc_ini[b]] : &an->pred[an->pred_ini[b]];
        uint32_t cnt = bu.adiante ? an->suc_ini[b + 1u] - an->suc_ini[b] : an->pred_ini[b + 1u] - an->pred_ini[b];

        for (uint32_t m = 0; m < cnt; m++) {
            if (viz[m] == bu.evitar || bu.marca[viz[m]])
                continue;
            bu.marca[viz[m]] = 1;
            c->fila[bu.n++] = viz[m];
        }
    }
}

/* Algum bloco dos caminhos db -> ub que nao repassam por db (ub inteiro incluso) define v? */
static bool DefNaRegiao(const EndCtx *c, uint32_t v, uint32_t db, uint32_t ub)
{
    uint32_t nb = c->f->nblocos;

    memset(c->marca_f, 0, nb);
    memset(c->marca_b, 0, nb);
    c->fila[0] = db;
    Alcancar(c, (Busca){.marca = c->marca_f, .n = 1, .evitar = db, .adiante = true});
    c->fila[0] = ub;
    c->marca_b[ub] = 1;
    Alcancar(c, (Busca){.marca = c->marca_b, .n = 1, .evitar = db, .adiante = false});
    for (uint32_t x = 0; x < nb; x++) {
        if (c->marca_f[x] && c->marca_b[x] && DefNoBloco(c, v, (PosEnd){x, 0}, UINT32_MAX))
            return true;
    }
    return false;
}

/* v tem o mesmo valor no acesso `uso` que tinha na instrucao de definicao `d` (a que o leu)? Sem definicao propria (parametro nunca reescrito) vale sempre. */
static bool Estavel(const EndCtx *c, uint32_t v, PosEnd d, PosEnd uso)
{
    if (c->defs[v] <= (v < c->f->nparams ? 1u : 0u))
        return true;
    if (d.b == uso.b)
        return d.j < uso.j && !DefNoBloco(c, v, (PosEnd){d.b, d.j + 1u}, uso.j);
    if (!AnalisesDomina(&c->an, d.b, uso.b) || DefNoBloco(c, v, (PosEnd){d.b, d.j + 1u}, UINT32_MAX))
        return false;
    return !DefNaRegiao(c, v, d.b, uso.b);
}

/* A folha (vreg) de uma instrucao `op vreg, imm`: o operando que nao e imediato. */
static uint32_t FolhaDe(const BirInstr *i)
{
    return i->b.tipo == BO_IMED ? i->a.id : i->b.id;
}

/* A definicao de t roda antes do acesso em todo caminho (uso antes da definicao no bloco seria valor da volta anterior). */
static bool DefinicaoDominaAcesso(const EndCtx *c, PosEnd d, PosEnd uso)
{
    return d.b == uso.b ? d.j < uso.j : AnalisesDomina(&c->an, d.b, uso.b);
}

/* i (do MUL) e x (do ADD imediato, ou o outro operando de t) valem no acesso o que valiam onde foram lidos. */
static bool CadeiaEstavel(const EndCtx *c, const Plano *p, PosEnd uso)
{
    uint32_t outro;

    if (!DefinicaoDominaAcesso(c, p->pt, uso) || !Estavel(c, FolhaDe(&p->m), p->pm, uso))
        return false;
    if (p->tem_w)
        return Estavel(c, FolhaDe(&p->w), p->pw, uso);
    outro = p->t.a.id == p->m.dst ? p->t.b.id : p->t.a.id;
    return Estavel(c, outro, p->pt, uso);
}

/* ---- plano ---- */

static void AcrescentarPlano(EndCtx *c, const Plano *p)
{
    if (c->nplano == c->capplano) {
        c->capplano = c->capplano == 0 ? 16u : c->capplano * 2u;
        c->plano = realloc(c->plano, (size_t)c->capplano * sizeof *c->plano);
        if (c->plano == NULL)
            FalhaInterna("otim: memoria esgotada");
    }
    c->plano[c->nplano++] = *p;
}

/* Base com 2+ usos, todos de base de acesso (o isel nao funde temporario compartilhado). */
static bool BaseCompartilhada(const EndCtx *c, const BirInstr *acesso)
{
    uint32_t t = acesso->a.id;

    return acesso->a.tipo == BO_VREG && SimplesI64(&acesso->a) && t < c->f->nvregs && c->usos_base[t] >= 2 && c->usos_base[t] == c->usos[t];
}

static void ColetarPlano(EndCtx *c)
{
    const BirFuncao *f = c->f;

    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            const BirInstr *i = &f->blocos[b].instrs[j];
            Plano p;

            if (!EhAcesso(i) || !BaseCompartilhada(c, i))
                continue;
            memset(&p, 0, sizeof p);
            if (!CasarCadeia(c, i->a.id, &p) || !CadeiaEstavel(c, &p, (PosEnd){b, j}))
                continue;
            p.b = b;
            p.j = j;
            AcrescentarPlano(c, &p);
        }
    }
}

/* ---- aplicacao ---- */

static void MarcarMorta(EndCtx *c, PosEnd pos, const BirInstr *def)
{
    uint32_t g = c->inicio[pos.b] + pos.j;

    if (c->morta[g])
        return;
    c->morta[g] = 1;
    if (def->a.tipo == BO_VREG && def->a.id < c->f->nvregs)
        c->usos[def->a.id]--;
    if (def->b.tipo == BO_VREG && def->b.id < c->f->nvregs)
        c->usos[def->b.id]--;
}

/* Cada acesso recomposto deixa de usar t; o que ficar sem uso morre em cascata (t, depois m e w). Devolve quantas instrucoes morreram. */
static uint32_t MarcarMortas(EndCtx *c)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < c->nplano; k++)
        c->usos[c->plano[k].t.dst]--;
    for (uint32_t k = 0; k < c->nplano; k++) {
        if (c->usos[c->plano[k].t.dst] == 0)
            MarcarMorta(c, c->plano[k].pt, &c->plano[k].t);
    }
    for (uint32_t k = 0; k < c->nplano; k++) {
        if (c->usos[c->plano[k].m.dst] == 0)
            MarcarMorta(c, c->plano[k].pm, &c->plano[k].m);
        if (c->plano[k].tem_w && c->usos[c->plano[k].w.dst] == 0)
            MarcarMorta(c, c->plano[k].pw, &c->plano[k].w);
    }
    for (uint32_t g = 0; g < c->inicio[c->f->nblocos]; g++)
        n += c->morta[g];
    return n;
}

static void TrocarVreg(BirInstr *i, uint32_t de, uint32_t para)
{
    if (i->a.tipo == BO_VREG && i->a.id == de)
        i->a.id = para;
    if (i->b.tipo == BO_VREG && i->b.id == de)
        i->b.id = para;
}

/* Escreve a cadeia nova (w', m', t') em saida e devolve quantas escreveu; *prox e o proximo vreg livre; *vt = o endereco novo. */
static uint32_t EmitirCadeia(const Plano *p, BirInstr *saida, uint32_t *prox, uint32_t *vt)
{
    uint32_t n = 0, vm, vw = BIR_SEM_VREG;

    if (p->tem_w) {
        vw = (*prox)++;
        saida[n] = p->w;
        saida[n++].dst = vw;
    }
    vm = (*prox)++;
    saida[n] = p->m;
    saida[n++].dst = vm;
    *vt = (*prox)++;
    saida[n] = p->t;
    TrocarVreg(&saida[n], p->m.dst, vm);
    if (p->tem_w)
        TrocarVreg(&saida[n], p->w.dst, vw);
    saida[n++].dst = *vt;
    return n;
}

static void NovosVregs(BirFuncao *f, uint32_t n)
{
    uint8_t *classes = realloc(f->classe_vreg, ((size_t)f->nvregs + n + 1u) * sizeof *classes);

    if (classes == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < n; k++)
        classes[f->nvregs + k] = BC_I64;
    f->classe_vreg = classes;
}

/* Reconstroi o bloco b: sem as mortas, com a cadeia nova na frente de cada acesso do plano a partir de *k. */
static void ReconstruirBloco(EndCtx *c, uint32_t b, uint32_t *k, uint32_t *prox)
{
    BirBloco *blk = &c->f->blocos[b];
    uint32_t nb = 0;
    BirInstr *novo = malloc(((size_t)blk->n + 3u * (c->nplano - *k) + 1u) * sizeof *novo);

    if (novo == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t j = 0; j < blk->n; j++) {
        if (c->morta[c->inicio[b] + j])
            continue;
        if (*k < c->nplano && c->plano[*k].b == b && c->plano[*k].j == j) {
            uint32_t vt;

            nb += EmitirCadeia(&c->plano[*k], &novo[nb], prox, &vt);
            novo[nb] = blk->instrs[j];
            novo[nb++].a.id = vt;
            (*k)++;
            continue;
        }
        novo[nb++] = blk->instrs[j];
    }
    free(blk->instrs);
    blk->instrs = novo;
    blk->n = nb;
    blk->cap = nb + 1u;
}

static uint32_t AplicarPlano(EndCtx *c, uint32_t *removidas)
{
    BirFuncao *f = c->f;
    uint32_t k = 0, prox = f->nvregs, novos = 0;

    for (uint32_t i = 0; i < c->nplano; i++)
        novos += c->plano[i].tem_w ? 3u : 2u;
    *removidas = MarcarMortas(c);
    NovosVregs(f, novos);
    for (uint32_t b = 0; b < f->nblocos; b++)
        ReconstruirBloco(c, b, &k, &prox);
    f->nvregs = prox;
    return c->nplano;
}

static uint32_t EnderecoFuncao(const BirPrograma *programa, BirFuncao *f, uint32_t *removidas)
{
    EndCtx c = {.f = f};
    uint32_t n = 0;

    if (!CfgElegivel(programa, f) || f->nblocos > ENDERECO_BLOCOS_MAX)
        return 0;
    PrepararContexto(&c);
    ColetarPlano(&c);
    if (c.nplano > 0)
        n = AplicarPlano(&c, removidas);
    LiberarContexto(&c);
    return n;
}

void OtimEnderecoAplicar(BirPrograma *programa, Buf *relato)
{
    for (uint32_t i = 0; i < programa->nfuncoes; i++) {
        BirFuncao *f = &programa->funcoes[i];
        uint32_t removidas = 0, n = EnderecoFuncao(programa, f, &removidas);

        if (n > 0 && relato != NULL)
            BufFormatar(relato, "endereco %s: %u cadeias recolocadas, %u instrucoes removidas\n", programa->simbolos[f->simbolo].nome, (unsigned)n, (unsigned)removidas);
    }
}
