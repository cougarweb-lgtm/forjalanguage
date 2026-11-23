/* checagem.c - passe `checagem` (M7a F6): ver checagem.h.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - S4c (tempo de build): funcao sem bloco terminado em CMP GE_U + DESVIA sai antes de reservar e calcular os fatos (mesma BIR de saida).
 *            0.2.0 - M7a F6b: indice afim (add/mul de provados e constantes >= 0, I64, sem overflow). 0.1.0 - M7a F6: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/optimizer/checagem.h"

/* Fatos sobre a funcao, calculados uma vez: definicoes e usos por vreg, referencias por bloco e o intervalo [0, maximo] provado de cada vreg de indice. */
typedef struct {
    uint32_t nvregs, nblocos;
    uint32_t *ndefs, *usos, *refs;
    uint32_t *def_b[2], *def_i[2];   /* [0] = 1a definicao, [1] = ultima (iguais com 1 definicao) */
    int64_t *maximo;
    uint8_t *provado;
} Fatos;

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static bool DefineVreg(const BirInstr *i)
{
    if (i->dst == BIR_SEM_VREG)
        return false;
    return i->op != BIR_SALTA && i->op != BIR_DESVIA && i->op != BIR_RETORNA && i->op != BIR_ARMADILHA && i->op != BIR_GRAVA && i->op != BIR_COPIA_MEM &&
           i->op != BIR_ZERA_MEM;
}

static void Usa(Fatos *t, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < t->nvregs)
        t->usos[o->id]++;
}

static void ContarDef(Fatos *t, const BirInstr *i, uint32_t b, uint32_t k)
{
    if (!DefineVreg(i) || i->dst >= t->nvregs)
        return;
    if (t->ndefs[i->dst]++ == 0) {
        t->def_b[0][i->dst] = b;
        t->def_i[0][i->dst] = k;
    }
    t->def_b[1][i->dst] = b;
    t->def_i[1][i->dst] = k;
}

/* refs[bloco]++ para cada bloco que a instrucao nomeia como alvo. */
static void MarcarAlvos(uint32_t *refs, const BirInstr *i)
{
    if (EhContaOv(i->op) || i->op == BIR_SALTA || i->op == BIR_DESVIA)
        refs[i->alvo[0]]++;
    if (i->op == BIR_DESVIA)
        refs[i->alvo[1]]++;
}

/* Troca cada alvo da instrucao pelo numero novo do bloco. */
static void RenumerarAlvos(BirInstr *i, const uint32_t *novo)
{
    if (EhContaOv(i->op) || i->op == BIR_SALTA || i->op == BIR_DESVIA)
        i->alvo[0] = novo[i->alvo[0]];
    if (i->op == BIR_DESVIA)
        i->alvo[1] = novo[i->alvo[1]];
}

static void Contar(Fatos *t, const BirFuncao *f)
{
    for (uint32_t k = 0; k < f->nparams && k < t->nvregs; k++)
        t->ndefs[k]++;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++) {
            const BirInstr *i = &f->blocos[b].instrs[k];

            ContarDef(t, i, b, k);
            MarcarAlvos(t->refs, i);
            Usa(t, &i->a);
            Usa(t, &i->b);
            for (uint32_t m = 0; m < i->nargs; m++)
                Usa(t, &i->args[m]);
        }
    }
}

static bool ReservarFatos(Fatos *t, const BirFuncao *f)
{
    t->nvregs = f->nvregs;
    t->nblocos = f->nblocos;
    t->ndefs = calloc(f->nvregs, sizeof *t->ndefs);
    t->usos = calloc(f->nvregs, sizeof *t->usos);
    t->refs = calloc(f->nblocos, sizeof *t->refs);
    for (uint32_t m = 0; m < 2; m++) {
        t->def_b[m] = calloc(f->nvregs, sizeof **t->def_b);
        t->def_i[m] = calloc(f->nvregs, sizeof **t->def_i);
    }
    t->maximo = calloc(f->nvregs, sizeof *t->maximo);
    t->provado = calloc(f->nvregs, 1);
    return t->ndefs && t->usos && t->refs && t->def_b[0] && t->def_i[0] && t->def_b[1] && t->def_i[1] && t->maximo && t->provado;
}

static void LiberarFatos(Fatos *t)
{
    free(t->ndefs);
    free(t->usos);
    free(t->refs);
    for (uint32_t m = 0; m < 2; m++) {
        free(t->def_b[m]);
        free(t->def_i[m]);
    }
    free(t->maximo);
    free(t->provado);
}

/* A instrucao que define v (1a ou ultima definicao). */
static const BirInstr *DefDe(const Fatos *t, const BirFuncao *f, uint32_t v, uint32_t qual)
{
    return &f->blocos[t->def_b[qual][v]].instrs[t->def_i[qual][v]];
}

static bool EhImed(const BirOperando *o, int64_t valor)
{
    return o->tipo == BO_IMED && o->imed == valor;
}

/* O laco reconhecido: v = contador, vk = vreg do limite, corpo = bloco do corpo. */
typedef struct { uint32_t v, vk, corpo; } Laco;

/* vreg de definicao unica `sub a, #1`; devolve a (o vreg do limite) em *base. */
static bool EhMenosUm(const Fatos *t, const BirFuncao *f, uint32_t v, uint32_t *base)
{
    const BirInstr *d;

    if (v >= t->nvregs || t->ndefs[v] != 1)
        return false;
    d = DefDe(t, f, v, 0);
    if (d->op != BIR_SUB || d->a.tipo != BO_VREG || !EhImed(&d->b, 1))
        return false;
    *base = d->a.id;
    return true;
}

/* O bloco da verificacao de saida: termina em `desvia (cmp.eq V, vL), saida, soma`, com vL = vK - 1. */
static bool VerificaSaida(const Fatos *t, const BirFuncao *f, uint32_t verifica, const Laco *laco)
{
    const BirBloco *blk = &f->blocos[verifica];
    const BirInstr *d, *c;
    uint32_t base = 0;

    if (blk->n < 2)
        return false;
    d = &blk->instrs[blk->n - 1];
    c = &blk->instrs[blk->n - 2];
    if (d->op != BIR_DESVIA || c->op != BIR_CMP || c->aux != BCOND_EQ || c->dst != d->a.id || c->a.tipo != BO_VREG || c->a.id != laco->v ||
        c->b.tipo != BO_VREG)
        return false;
    return EhMenosUm(t, f, c->b.id, &base) && base == laco->vk;
}

/* O bloco `soma` e `V = V + 1; salta corpo`, alcancado so pela verificacao de saida; devolve o bloco da verificacao em *verifica. */
static bool BlocoSoma(const Fatos *t, const BirFuncao *f, const Laco *laco, uint32_t *verifica)
{
    uint32_t v = laco->v, corpo = laco->corpo, soma = t->def_b[1][v];
    const BirBloco *blk = &f->blocos[soma];

    if (t->refs[soma] != 1 || t->refs[corpo] != 2 || blk->n != 2)
        return false;
    if (blk->instrs[0].op != BIR_ADD || blk->instrs[0].dst != v || blk->instrs[0].a.tipo != BO_VREG || blk->instrs[0].a.id != v ||
        !EhImed(&blk->instrs[0].b, 1) || blk->instrs[1].op != BIR_SALTA || blk->instrs[1].alvo[0] != corpo)
        return false;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        const BirBloco *outro = &f->blocos[b];

        if (outro->n > 0 && outro->instrs[outro->n - 1].op == BIR_DESVIA && outro->instrs[outro->n - 1].alvo[1] == soma) {
            *verifica = b;
            return true;
        }
    }
    return false;
}

/* b termina em `desvia (cmp.lt_s V, vK), corpo, saida` com V = const S (S >= 0) e vK = const K definidos de uma vez; marca V em [S, K-1].
 * INVARIANTE (revisao Opus M7a, P1): a prova nao olha o fluxo; vale porque o lowering so le V dentro do corpo do laco. Todo passe que leve uma copia/conta de V
 * (ou do que herda dela) para fora do laco (propagacao de copia, LICM, reducao de forca: M7b) deve antes exigir que a definicao seja dominada pelo corpo. */
static void TentaLacoContado(Fatos *t, const BirFuncao *f, uint32_t b)
{
    const BirBloco *blk = &f->blocos[b];
    const BirInstr *d, *c, *s, *k, *soma;
    uint32_t v, vk, verifica;
    Laco laco;

    if (blk->n < 2)
        return;
    d = &blk->instrs[blk->n - 1];
    c = &blk->instrs[blk->n - 2];
    if (d->op != BIR_DESVIA || c->op != BIR_CMP || c->aux != BCOND_LT_S || c->dst != d->a.id || c->a.tipo != BO_VREG || c->b.tipo != BO_VREG ||
        c->a.classe != BC_I64)
        return;
    v = c->a.id;
    vk = c->b.id;
    if (v >= t->nvregs || vk >= t->nvregs || t->ndefs[v] != 2 || t->ndefs[vk] != 1)
        return;
    s = DefDe(t, f, v, 0);
    soma = DefDe(t, f, v, 1);
    k = DefDe(t, f, vk, 0);
    if (s->op != BIR_CONST || t->def_b[0][v] != b || s->a.imed < 0 || k->op != BIR_CONST || soma->op != BIR_ADD)
        return;
    if (k->a.imed <= s->a.imed) /* laco morto (K <= S): sem intervalo a provar; tambem evita K - 1 estourar com K = INT64_MIN */
        return;
    laco = (Laco){.v = v, .vk = vk, .corpo = d->alvo[0]};
    if (!BlocoSoma(t, f, &laco, &verifica) || !VerificaSaida(t, f, verifica, &laco))
        return;
    t->provado[v] = 1;
    t->maximo[v] = k->a.imed - 1;
}

/* A copia do usuario (`x = copia V`, definicao unica) herda o intervalo de V. */
static void HerdaCopias(Fatos *t, const BirFuncao *f)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++) {
            const BirInstr *i = &f->blocos[b].instrs[k];

            if (i->op != BIR_COPIA || i->dst >= t->nvregs || t->ndefs[i->dst] != 1 || i->a.tipo != BO_VREG || i->a.id >= t->nvregs ||
                !t->provado[i->a.id])
                continue;
            t->provado[i->dst] = 1;
            t->maximo[i->dst] = t->maximo[i->a.id];
        }
    }
}

#define LIMITE_AFIM ((int64_t)1 << 31) /* operando acima disso nao entra na conta: o resultado (soma ou produto) cabe em 63 bits */

/* Maximo de um operando nao negativo: vreg provado (maximo >= 0) ou imediato >= 0; ambos <= LIMITE_AFIM. */
static bool MaximoDe(const Fatos *t, const BirOperando *o, int64_t *max)
{
    if (o->tipo == BO_IMED)
        *max = o->imed;
    else if (o->tipo == BO_VREG && o->id < t->nvregs && t->provado[o->id])
        *max = t->maximo[o->id];
    else
        return false;
    return *max >= 0 && *max <= LIMITE_AFIM;
}

/* dst de definicao unica `add a, b` ou `mul a, b` (I64) com a e b em [0, max]: dst fica em [0, max da conta]. Forma afim i*C + k com inducoes constantes. */
static bool ProvaAfim(Fatos *t, const BirInstr *i)
{
    int64_t ma, mb;

    if ((i->op != BIR_ADD && i->op != BIR_MUL) || i->classe != BC_I64 || i->dst >= t->nvregs || t->ndefs[i->dst] != 1 || t->provado[i->dst])
        return false;
    if (!MaximoDe(t, &i->a, &ma) || !MaximoDe(t, &i->b, &mb))
        return false;
    t->provado[i->dst] = 1;
    t->maximo[i->dst] = i->op == BIR_ADD ? ma + mb : ma * mb;
    return true;
}

/* A copia do usuario e a conta afim sobre vregs provados propagam o intervalo; repete ate nao mudar (a ordem dos blocos nao e a do fluxo). */
static void PropagaAfim(Fatos *t, const BirFuncao *f)
{
    bool mudou = true;

    while (mudou) {
        mudou = false;
        for (uint32_t b = 0; b < f->nblocos; b++) {
            for (uint32_t k = 0; k < f->blocos[b].n; k++)
                mudou |= ProvaAfim(t, &f->blocos[b].instrs[k]);
        }
    }
}

/* O bloco acaba em `cmp.ge_u X, #N; desvia c, frio, segue` com X provado em [0, maximo] e maximo < N, c so usado ali? Vira `salta segue`. */
static bool TiraChecagem(const Fatos *t, BirFuncao *f, uint32_t b)
{
    BirBloco *blk = &f->blocos[b];
    BirInstr *d, *c;

    if (blk->n < 2)
        return false;
    d = &blk->instrs[blk->n - 1];
    c = &blk->instrs[blk->n - 2];
    if (d->op != BIR_DESVIA || c->op != BIR_CMP || c->aux != BCOND_GE_U || c->dst != d->a.id || c->dst >= t->nvregs || t->usos[c->dst] != 1 ||
        t->ndefs[c->dst] != 1 || c->a.tipo != BO_VREG || c->b.tipo != BO_IMED || !f->blocos[d->alvo[0]].frio)
        return false;
    if (c->a.id >= t->nvregs || !t->provado[c->a.id] || t->maximo[c->a.id] >= c->b.imed)
        return false;
    c->op = BIR_SALTA;
    c->alvo[0] = d->alvo[1];
    c->dst = BIR_SEM_VREG;
    c->a = (BirOperando){0};
    c->b = (BirOperando){0};
    blk->n--;
    return true;
}

/* O bloco b acaba em `salta s` e s so tem b como predecessor: s entra no fim de b e fica vazio (RemoveBlocosMortos o tira). Sem isso o corpo do laco
 * perde a vizinhanca com o cabecalho e o x64 emite um jmp a mais por volta. */
static bool MesclaSegue(const Fatos *t, BirFuncao *f, uint32_t b)
{
    BirBloco *blk = &f->blocos[b];
    uint32_t s = blk->instrs[blk->n - 1].alvo[0];
    BirBloco *seg = &f->blocos[s];

    if (s == 0 || s == b || seg->frio || seg->n == 0 || t->refs[s] != 1)
        return false;
    if (blk->n - 1 + seg->n > blk->cap) {
        BirInstr *novo = realloc(blk->instrs, (size_t)(blk->n - 1 + seg->n) * sizeof *novo);

        if (novo == NULL)
            return false;
        blk->instrs = novo;
        blk->cap = blk->n - 1 + seg->n;
    }
    memcpy(&blk->instrs[blk->n - 1], seg->instrs, (size_t)seg->n * sizeof *seg->instrs);
    blk->n = blk->n - 1 + seg->n;
    seg->n = 0;
    return true;
}

/* Tira da funcao os blocos frios que ficaram sem predecessor e os mesclados (vazios), e renumera os alvos.
 * LIMITE: cognitiva 17 (alvo 15): 3 passadas lineares sobre os blocos (contar, compactar, renumerar) que so fazem sentido juntas, sem aninhamento fundo. */
static void RemoveBlocosMortos(const Fatos *t, BirFuncao *f)
{
    uint32_t *novo = malloc(f->nblocos * sizeof *novo);
    uint32_t qtd = 0;
    uint32_t *refs = calloc(f->nblocos, sizeof *refs);

    if (novo == NULL || refs == NULL) {
        free(novo);
        free(refs);
        return;
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++)
            MarcarAlvos(refs, &f->blocos[b].instrs[k]);
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        bool some = b != 0 && (f->blocos[b].n == 0 || (f->blocos[b].frio && refs[b] == 0 && t->refs[b] != 0));
        novo[b] = some ? BIR_SEM_BLOCO : qtd;
        if (some)
            free(f->blocos[b].instrs);
        else
            f->blocos[qtd++] = f->blocos[b];
    }
    for (uint32_t b = 0; b < qtd; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++)
            RenumerarAlvos(&f->blocos[b].instrs[k], novo);
    }
    f->nblocos = qtd;
    free(novo);
    free(refs);
}

void OtimChecagemAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    Fatos t = {0};
    uint32_t tiradas = 0;
    bool candidata = false;

    /* so TiraChecagem muda a BIR (as outras etapas so enchem Fatos): sem bloco que termine em CMP GE_U + DESVIA nada sai e os 8 vetores por vreg nao se calculam */
    for (uint32_t b = 0; b < funcao->nblocos && !candidata; b++) {
        const BirBloco *blk = &funcao->blocos[b];

        candidata = blk->n >= 2 && blk->instrs[blk->n - 1].op == BIR_DESVIA && blk->instrs[blk->n - 2].op == BIR_CMP && blk->instrs[blk->n - 2].aux == BCOND_GE_U;
    }
    if (!candidata)
        return;
    if (!programa->checagem || funcao->nblocos == 0 || !ReservarFatos(&t, funcao)) {
        LiberarFatos(&t);
        return;
    }
    Contar(&t, funcao);
    for (uint32_t b = 0; b < funcao->nblocos; b++)
        TentaLacoContado(&t, funcao, b);
    HerdaCopias(&t, funcao);
    PropagaAfim(&t, funcao);
    for (uint32_t b = 0; b < funcao->nblocos; b++) {
        while (funcao->blocos[b].n > 0 && TiraChecagem(&t, funcao, b)) {
            tiradas++;
            if (!MesclaSegue(&t, funcao, b))
                break;
        }
    }
    if (tiradas > 0)
        RemoveBlocosMortos(&t, funcao);
    LiberarFatos(&t);
}
