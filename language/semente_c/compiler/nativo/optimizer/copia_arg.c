/* copia_arg.c - passe `copia_arg` (M7a): ver copia_arg.h.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7a: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <abs>
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/optimizer/copia_arg.h"

#define SEM_RAIZ UINT32_MAX
#define NAO_ACHOU UINT32_MAX
#define VOLTAS_MAX 8u /* cada volta desfaz um elo da cadeia de copias; cadeia maior fica como esta */

/* Fatos da funcao numa volta: definicoes e usos por vreg, a raiz (slot de que o vreg e endereco, direto ou somado a indice), o slot que escapou e as instrucoes apagadas. */
typedef struct {
    uint32_t nvregs, nslots;
    uint32_t *ndefs, *usos, *raiz, *def_b, *def_i;
    uint8_t *escapou;
    uint32_t *inicio;   /* inicio[b] = indice global da 1a instrucao do bloco b */
    uint8_t *morto;     /* por instrucao (indice global): a copia/end_slot que esta volta apagou */
} Fatos;

static bool DefineVreg(const BirInstr *i)
{
    if (i->dst == BIR_SEM_VREG)
        return false;
    return i->op != BIR_SALTA && i->op != BIR_DESVIA && i->op != BIR_RETORNA && i->op != BIR_ARMADILHA && i->op != BIR_GRAVA && i->op != BIR_COPIA_MEM &&
           i->op != BIR_ZERA_MEM;
}

static bool EhVreg(const BirOperando *o, uint32_t id)
{
    return o->tipo == BO_VREG && o->id == id;
}

/* ---- o chamado so le o parametro? ---- */

/* O proc (BirFuncao) do simbolo; NULL se o simbolo nao e funcao gerada. */
static const BirFuncao *AcharFuncao(const BirPrograma *p, uint32_t simbolo)
{
    if (simbolo >= p->nsimbolos || p->simbolos[simbolo].tipo != BS_FUNCAO)
        return NULL;
    for (uint32_t k = 0; k < p->nfuncoes; k++) {
        if (p->funcoes[k].simbolo == simbolo && p->funcoes[k].nblocos > 0)
            return &p->funcoes[k];
    }
    return NULL;
}

/* A instrucao usa o vreg k de forma que pode gravar na memoria dele ou deixar o endereco escapar? So `carrega [k+d]` e a fonte de `copia_mem` leem apenas. */
static bool UsaParamAlemDeLer(const BirInstr *i, uint32_t k)
{
    if (DefineVreg(i) && i->dst == k)
        return true;
    if (EhVreg(&i->a, k) && i->op != BIR_CARREGA)
        return true;
    if (EhVreg(&i->b, k) && i->op != BIR_COPIA_MEM)
        return true;
    for (uint32_t m = 0; m < i->nargs; m++) {
        if (EhVreg(&i->args[m], k))
            return true;
    }
    return false;
}

static bool ParamSoLido(const BirFuncao *g, uint32_t k)
{
    for (uint32_t b = 0; b < g->nblocos; b++) {
        for (uint32_t m = 0; m < g->blocos[b].n; m++) {
            if (UsaParamAlemDeLer(&g->blocos[b].instrs[m], k))
                return false;
        }
    }
    return true;
}

/* O argumento k deste CHAMA e agregado por valor para um proc do programa que so le o parametro correspondente? */
static bool ChamadoSoLe(const BirPrograma *p, const BirInstr *i, uint32_t k)
{
    const BirOperando *arg = &i->args[k];
    const BirFuncao *g;

    if (i->op != BIR_CHAMA || arg->tam == 0 || arg->sysv != NULL)
        return false;
    g = AcharFuncao(p, i->simbolo);
    if (g == NULL || g->param_tam == NULL || g->nparams != i->nargs || k >= g->nparams || g->param_tam[k] != arg->tam)
        return false;
    return ParamSoLido(g, k);
}

/* ---- fatos ---- */

static void ContarUso(Fatos *t, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < t->nvregs)
        t->usos[o->id]++;
}

static void Contar(Fatos *t, const BirFuncao *f)
{
    for (uint32_t k = 0; k < f->nparams && k < t->nvregs; k++)
        t->ndefs[k]++;
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t m = 0; m < f->blocos[b].n; m++) {
            const BirInstr *i = &f->blocos[b].instrs[m];

            if (DefineVreg(i) && i->dst < t->nvregs) {
                t->ndefs[i->dst]++;
                t->def_b[i->dst] = b;
                t->def_i[i->dst] = m;
            }
            ContarUso(t, &i->a);
            ContarUso(t, &i->b);
            for (uint32_t q = 0; q < i->nargs; q++)
                ContarUso(t, &i->args[q]);
        }
    }
}

static uint32_t RaizDe(const Fatos *t, const BirOperando *o)
{
    return o->tipo == BO_VREG && o->id < t->nvregs ? t->raiz[o->id] : SEM_RAIZ;
}

/* A raiz que a instrucao da ao dst (vreg de definicao unica): END_SLOT, ou COPIA/ADD/SUB de um vreg com raiz. */
static uint32_t RaizDefinida(const Fatos *t, const BirInstr *i)
{
    uint32_t ra;

    if (!DefineVreg(i) || i->dst >= t->nvregs || t->ndefs[i->dst] != 1)
        return SEM_RAIZ;
    if (i->op == BIR_END_SLOT)
        return i->a.tipo == BO_IMED && i->a.imed >= 0 && (uint64_t)i->a.imed < t->nslots ? (uint32_t)i->a.imed : SEM_RAIZ;
    ra = RaizDe(t, &i->a);
    if (i->op == BIR_COPIA || i->op == BIR_SUB)
        return ra;
    if (i->op == BIR_ADD)
        return ra != SEM_RAIZ ? ra : RaizDe(t, &i->b);
    return SEM_RAIZ;
}

/* Da raiz ao dst da instrucao se ele ainda nao tem; true se mudou. */
static bool PropagarRaiz(Fatos *t, const BirInstr *i)
{
    uint32_t r = RaizDefinida(t, i);

    if (r == SEM_RAIZ || t->raiz[i->dst] != SEM_RAIZ)
        return false;
    t->raiz[i->dst] = r;
    return true;
}

/* Ponto fixo: a raiz so cresce (vreg de definicao unica; a ordem dos blocos nao importa). */
static void CalcularRaiz(Fatos *t, const BirFuncao *f)
{
    bool mudou = true;

    while (mudou) {
        mudou = false;
        for (uint32_t b = 0; b < f->nblocos; b++) {
            for (uint32_t m = 0; m < f->blocos[b].n; m++)
                mudou |= PropagarRaiz(t, &f->blocos[b].instrs[m]);
        }
    }
}

/* ---- escape do endereco de slot ---- */

static void MarcarEscape(Fatos *t, const BirOperando *o)
{
    uint32_t r = RaizDe(t, o);

    if (r != SEM_RAIZ)
        t->escapou[r] = 1;
}

/* memcpy/memset do runtime nao guardam o ponteiro: nao escapa (a gravacao que fazem entra na checagem entre a copia e o chama). */
static bool EhRuntimeDeCopia(const BirPrograma *p, uint32_t simbolo)
{
    const BirSimbolo *s;

    if (simbolo >= p->nsimbolos)
        return false;
    s = &p->simbolos[simbolo];
    if (s->tipo != BS_RUNTIME || s->nome == NULL)
        return false;
    return (p->nomes_mem.copia != NULL && strcmp(s->nome, p->nomes_mem.copia) == 0) || (p->nomes_mem.zera != NULL && strcmp(s->nome, p->nomes_mem.zera) == 0);
}

static void EscapaChama(Fatos *t, const BirPrograma *p, const BirInstr *i)
{
    bool runtime = EhRuntimeDeCopia(p, i->simbolo);

    MarcarEscape(t, &i->a);
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (!runtime && !ChamadoSoLe(p, i, k))
            MarcarEscape(t, &i->args[k]);
    }
}

/* COPIA/ADD/SUB so derivam o endereco quando o dst e de definicao unica e o ponteiro entra uma vez (SUB: so na esquerda). */
static void EscapaDerivacao(Fatos *t, const BirInstr *i)
{
    bool dst_unico = DefineVreg(i) && i->dst < t->nvregs && t->ndefs[i->dst] == 1;
    bool a_raiz = RaizDe(t, &i->a) != SEM_RAIZ, b_raiz = RaizDe(t, &i->b) != SEM_RAIZ;
    bool ok = dst_unico && !(a_raiz && b_raiz) && !(i->op == BIR_SUB && b_raiz);

    if (ok)
        return;
    MarcarEscape(t, &i->a);
    MarcarEscape(t, &i->b);
}

static void EscapaInstr(Fatos *t, const BirPrograma *p, const BirInstr *i)
{
    switch (i->op) {
    case BIR_CARREGA:
    case BIR_COPIA_MEM:
    case BIR_ZERA_MEM:
        return;
    case BIR_GRAVA:
        MarcarEscape(t, &i->b);
        return;
    case BIR_END_SLOT:
        if (t->ndefs[i->dst] != 1 && i->a.tipo == BO_IMED && i->a.imed >= 0 && (uint64_t)i->a.imed < t->nslots)
            t->escapou[i->a.imed] = 1;
        return;
    case BIR_COPIA:
    case BIR_ADD:
    case BIR_SUB:
        EscapaDerivacao(t, i);
        return;
    case BIR_CHAMA:
        EscapaChama(t, p, i);
        return;
    default:
        MarcarEscape(t, &i->a);
        MarcarEscape(t, &i->b);
        for (uint32_t k = 0; k < i->nargs; k++)
            MarcarEscape(t, &i->args[k]);
    }
}

/* ---- reserva ---- */

static bool Reservar(Fatos *t, const BirFuncao *f)
{
    uint32_t total = 0;

    t->nvregs = f->nvregs;
    t->nslots = f->nslots;
    t->ndefs = calloc(f->nvregs + 1u, sizeof *t->ndefs);
    t->usos = calloc(f->nvregs + 1u, sizeof *t->usos);
    t->raiz = malloc((f->nvregs + 1u) * sizeof *t->raiz);
    t->def_b = calloc(f->nvregs + 1u, sizeof *t->def_b);
    t->def_i = calloc(f->nvregs + 1u, sizeof *t->def_i);
    t->escapou = calloc(f->nslots + 1u, 1);
    t->inicio = calloc(f->nblocos + 1u, sizeof *t->inicio);
    for (uint32_t b = 0; b < f->nblocos && t->inicio != NULL; b++) {
        t->inicio[b] = total;
        total += f->blocos[b].n;
    }
    t->morto = calloc(total + 1u, 1);
    if (t->raiz != NULL) {
        for (uint32_t v = 0; v < f->nvregs; v++)
            t->raiz[v] = SEM_RAIZ;
    }
    return t->ndefs && t->usos && t->raiz && t->def_b && t->def_i && t->escapou && t->inicio && t->morto;
}

static void Liberar(Fatos *t)
{
    free(t->ndefs);
    free(t->usos);
    free(t->raiz);
    free(t->def_b);
    free(t->def_i);
    free(t->escapou);
    free(t->inicio);
    free(t->morto);
}

static bool Preparar(Fatos *t, const BirPrograma *p, const BirFuncao *f)
{
    if (!Reservar(t, f))
        return false;
    Contar(t, f);
    CalcularRaiz(t, f);
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t m = 0; m < f->blocos[b].n; m++)
            EscapaInstr(t, p, &f->blocos[b].instrs[m]);
    }
    return true;
}

/* ---- a transformacao ---- */

static bool Morto(const Fatos *t, uint32_t b, uint32_t m)
{
    return t->morto[t->inicio[b] + m] != 0;
}

/* Posicao de instrucao: bloco b, indice c no bloco. */
typedef struct { uint32_t b, c; } Pos;

/* Programa, funcao e fatos da volta. */
typedef struct { const BirPrograma *p; BirFuncao *f; Fatos *t; } Rodada;

/* A copia `copia_mem [d], [origem]` mais proxima antes do chama, no mesmo bloco. */
static uint32_t AcharCopia(const Fatos *t, const BirBloco *bl, Pos chama, uint32_t d)
{
    for (uint32_t j = chama.c; j-- > 0;) {
        const BirInstr *i = &bl->instrs[j];

        if (!Morto(t, chama.b, j) && i->op == BIR_COPIA_MEM && EhVreg(&i->a, d))
            return j;
    }
    return NAO_ACHOU;
}

/* A instrucao grava (ou pode gravar, no caso de chamada que recebe o endereco) no slot raiz? */
static bool GravaNaRaiz(const Fatos *t, const BirInstr *i, uint32_t raiz)
{
    if (i->op == BIR_GRAVA || i->op == BIR_COPIA_MEM || i->op == BIR_ZERA_MEM)
        return RaizDe(t, &i->a) == raiz;
    if (i->op != BIR_CHAMA && i->op != BIR_CHAMA_IND)
        return false;
    if (RaizDe(t, &i->a) == raiz || RaizDe(t, &i->b) == raiz)
        return true;
    for (uint32_t k = 0; k < i->nargs; k++) {
        if (RaizDe(t, &i->args[k]) == raiz)
            return true;
    }
    return false;
}

/* Entre a copia (indice j) e o chama: o vreg da origem nao e redefinido e nada grava no slot dela. */
static bool OrigemIntacta(const Fatos *t, const BirBloco *bl, Pos chama, uint32_t j)
{
    uint32_t origem = bl->instrs[j].b.id;
    uint32_t raiz = t->raiz[origem];

    for (uint32_t m = j + 1; m < chama.c; m++) {
        const BirInstr *i = &bl->instrs[m];

        if (Morto(t, chama.b, m))
            continue;
        if ((DefineVreg(i) && i->dst == origem) || GravaNaRaiz(t, i, raiz))
            return false;
    }
    return true;
}

/* d e o end_slot de um temp que so serve de destino da copia e de argumento deste chama? */
static bool TempPrivado(const Fatos *t, const BirFuncao *f, uint32_t d)
{
    const BirInstr *def;

    if (d >= t->nvregs || t->ndefs[d] != 1 || t->usos[d] != 2)
        return false;
    def = &f->blocos[t->def_b[d]].instrs[t->def_i[d]];
    return def->op == BIR_END_SLOT;
}

/* A origem da copia tem raiz em slot local que nao escapou e nao e o proprio temp? */
static bool OrigemLocal(const Fatos *t, const BirInstr *copia, uint32_t d)
{
    uint32_t raiz;

    if (copia->b.tipo != BO_VREG || copia->desloc <= 0)
        return false;
    raiz = RaizDe(t, &copia->b);
    return raiz != SEM_RAIZ && !t->escapou[raiz] && raiz != t->raiz[d];
}

static bool TentarArg(const Rodada *v, Pos pos, uint32_t k)
{
    Fatos *t = v->t;
    BirBloco *bl = &v->f->blocos[pos.b];
    BirInstr *chama = &bl->instrs[pos.c];
    const BirOperando *arg = &chama->args[k];
    uint32_t d = arg->id, j;
    const BirInstr *copia;

    if (arg->tipo != BO_VREG || arg->tam == 0 || !TempPrivado(t, v->f, d) || !ChamadoSoLe(v->p, chama, k))
        return false;
    j = AcharCopia(t, bl, pos, d);
    if (j == NAO_ACHOU)
        return false;
    copia = &bl->instrs[j];
    if (copia->desloc != (int32_t)arg->tam || !OrigemLocal(t, copia, d) || !OrigemIntacta(t, bl, pos, j))
        return false;
    chama->args[k].id = copia->b.id;
    t->morto[t->inicio[pos.b] + j] = 1;
    t->morto[t->inicio[t->def_b[d]] + t->def_i[d]] = 1;
    return true;
}

static bool TentarChama(const Rodada *v, Pos pos)
{
    const BirInstr *chama = &v->f->blocos[pos.b].instrs[pos.c];
    bool mudou = false;

    if (chama->op != BIR_CHAMA)
        return false;
    for (uint32_t k = 0; k < chama->nargs; k++)
        mudou |= TentarArg(v, pos, k);
    return mudou;
}

/* Tira do bloco as instrucoes marcadas, mantendo a ordem. */
static void Compactar(BirFuncao *f, const Fatos *t)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        BirBloco *bl = &f->blocos[b];
        uint32_t w = 0;

        for (uint32_t m = 0; m < bl->n; m++) {
            if (!Morto(t, b, m))
                bl->instrs[w++] = bl->instrs[m];
        }
        bl->n = w;
    }
}

/* Uma volta: fatos novos, tenta todo chama, compacta. true se mudou algo. */
static bool Volta(const BirPrograma *p, BirFuncao *f)
{
    Fatos t;
    Rodada r = {p, f, &t};
    bool mudou = false;

    memset(&t, 0, sizeof t);
    if (!Preparar(&t, p, f)) {
        Liberar(&t);
        return false;
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t c = 0; c < f->blocos[b].n; c++)
            mudou |= TentarChama(&r, (Pos){b, c});
    }
    if (mudou)
        Compactar(f, &t);
    Liberar(&t);
    return mudou;
}

static bool TemCopiaMem(const BirFuncao *f)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t m = 0; m < f->blocos[b].n; m++) {
            if (f->blocos[b].instrs[m].op == BIR_COPIA_MEM)
                return true;
        }
    }
    return false;
}

void OtimCopiaArgAplicar(const BirPrograma *programa, BirFuncao *funcao)
{
    if (funcao->nblocos == 0 || !TemCopiaMem(funcao))
        return;
    for (uint32_t volta = 0; volta < VOLTAS_MAX; volta++) {
        if (!Volta(programa, funcao))
            return;
    }
}
