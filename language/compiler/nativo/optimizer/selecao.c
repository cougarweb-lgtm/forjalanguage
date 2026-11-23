/* selecao.c - passe `selecao` (R5 do SIMD): losango ou triangulo puro vira BIR_SELECIONA (ver selecao.h).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S1b (D0703): InstrucaoPura recusa BC_V128 (opaco).
 *            0.1.1 - R5: recusa cabeca com condicao de dois lados constantes (f5_expr_redundante_sccp regrediu +19,9% e +11 instr no laco). 0.1.0 - R5: primeira versao. Entra depois do `sroa` e antes do `cfg` (o `cfg` funde a juncao na cabeca e a `numeracao` ja ve o bloco grande); o passe remove os blocos dos braços.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Por cabeca (bloco que termina em CMP + DESVIA com o bool de uso unico): reconhece a forma (ReconhecerForma), simula cada braço com um mapa "vreg do braço -> operando final" (copia e
 * constante nao geram instrucao; o resto sobe para a cabeca com o dst renomeado para um vreg novo), monta um SELECIONA por vreg escrito, confere o plano inteiro e so entao altera a funcao
 * (tudo ou nada: nenhum vreg novo nem bloco muda antes da conferencia). Os SELECIONA escrevem os vregs de destino depois de todas as instrucoes que subiram, por isso o plano recusa
 * dois SELECIONA em que o destino de um e operando do outro (ou da comparacao).
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/cfg.h"
#include "nativo/optimizer/selecao.h"

#define BRACO_MAX 4u /* instrucoes (sem o SALTA) por braço */
#define PLANO_MAX (2u * BRACO_MAX)

/* vreg escrito no braço -> operando que ele vale no fim do braço. */
typedef struct { uint32_t n; uint32_t dst[BRACO_MAX]; BirOperando valor[BRACO_MAX]; } MapaBraco;

typedef struct {
    BirInstr sobe[PLANO_MAX];   /* instrucoes dos braços que sobem para a cabeca (dst = vreg novo) */
    BirInstr sel[PLANO_MAX];    /* os SELECIONA */
    uint8_t classe_novo[PLANO_MAX];
    uint32_t nsobe, nsel, nnovos, base_vreg;
    BirOperando cmp[2];         /* operandos da comparacao */
} Plano;

/* cabeca, os dois alvos do DESVIA, a juncao e quais alvos sao blocos de braço (o outro e a propria juncao: triangulo). */
typedef struct { uint32_t cabeca, alvo_t, alvo_f, juncao; bool braco_t, braco_f; } Forma;

typedef struct {
    BirPrograma *p;
    BirFuncao *f;
    uint32_t *entradas;   /* arestas que chegam em cada bloco (SALTA, DESVIA, frio dos *_OV) */
    uint32_t *usos;       /* leituras de cada vreg na funcao inteira */
    uint8_t *morto;       /* bloco de braço ja absorvido */
    uint32_t nvregs_ini;  /* nvregs quando a contagem de usos foi feita */
} Selecao;

/* ---- contagens ---- */

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

static void ContarUso(Selecao *s, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < s->nvregs_ini)
        s->usos[o->id]++;
}

static void ContarInstrucao(Selecao *s, const BirInstr *i)
{
    if (i->op == BIR_SALTA || i->op == BIR_DESVIA || EhContaOv(i->op))
        s->entradas[i->alvo[0]]++;
    if (i->op == BIR_DESVIA)
        s->entradas[i->alvo[1]]++;
    ContarUso(s, &i->a);
    ContarUso(s, &i->b);
    for (uint32_t k = 0; k < i->nargs; k++)
        ContarUso(s, &i->args[k]);
}

static bool Alocar(Selecao *s)
{
    s->nvregs_ini = s->f->nvregs;
    s->entradas = calloc((size_t)s->f->nblocos + 1u, sizeof *s->entradas);
    s->usos = calloc((size_t)s->f->nvregs + 1u, sizeof *s->usos);
    s->morto = calloc((size_t)s->f->nblocos + 1u, 1);
    return s->entradas != NULL && s->usos != NULL && s->morto != NULL;
}

static void Liberar(Selecao *s)
{
    free(s->entradas);
    free(s->usos);
    free(s->morto);
}

/* ---- forma ---- */

/* O bloco b e um braço da cabeca: entrada unica, quente, ate BRACO_MAX instrucoes e termina em SALTA (a juncao sai em *juncao). */
static bool EhBraco(const Selecao *s, uint32_t b, uint32_t cabeca, uint32_t *juncao)
{
    const BirBloco *k = &s->f->blocos[b];

    if (b == 0 || b == cabeca || s->entradas[b] != 1 || k->frio || k->n > BRACO_MAX + 1u || k->instrs[k->n - 1].op != BIR_SALTA)
        return false;
    *juncao = k->instrs[k->n - 1].alvo[0];
    return true;
}

static bool CabecaValida(const Selecao *s, uint32_t h)
{
    const BirBloco *b = &s->f->blocos[h];
    const BirInstr *desvia, *cmp;

    if (b->frio || b->n < 2 || s->morto[h])
        return false;
    desvia = &b->instrs[b->n - 1];
    cmp = &b->instrs[b->n - 2];
    return desvia->op == BIR_DESVIA && cmp->op == BIR_CMP && desvia->a.tipo == BO_VREG && cmp->dst == desvia->a.id && cmp->dst < s->nvregs_ini &&
           s->usos[cmp->dst] == 1 && desvia->alvo[0] != desvia->alvo[1];
}

static bool ReconhecerForma(const Selecao *s, uint32_t h, Forma *fm)
{
    const BirInstr *desvia;
    uint32_t jt = 0, jf = 0;

    if (!CabecaValida(s, h))
        return false;
    desvia = &s->f->blocos[h].instrs[s->f->blocos[h].n - 1];
    *fm = (Forma){.cabeca = h, .alvo_t = desvia->alvo[0], .alvo_f = desvia->alvo[1]};
    fm->braco_t = EhBraco(s, fm->alvo_t, h, &jt);
    fm->braco_f = EhBraco(s, fm->alvo_f, h, &jf);
    if (fm->braco_t && fm->braco_f) {
        fm->juncao = jt;
        return jt == jf && jt != fm->alvo_t && jt != fm->alvo_f && jt != h;
    }
    if (fm->braco_t) {
        fm->juncao = fm->alvo_f;
        return jt == fm->alvo_f && fm->alvo_f != h;
    }
    if (fm->braco_f) {
        fm->juncao = fm->alvo_t;
        return jf == fm->alvo_t && fm->alvo_t != h;
    }
    return false;
}

/* ---- braços ---- */

static bool OperandoSimples(const BirOperando *o)
{
    return (o->tipo == BO_VREG || o->tipo == BO_IMED) && o->tam == 0 && o->extensao == BE_NENHUMA;
}

/* Instrucao que pode subir para a cabeca: sem trap, sem memoria, sem chamada (real: so as contas que nao trapam). */
static bool InstrucaoPura(const BirFuncao *f, const BirInstr *i)
{
    bool real = BirClasseEhReal(i->classe);

    if (i->dst == BIR_SEM_VREG || i->dst >= f->nvregs || i->nargs != 0 || i->a.tam != 0 || i->b.tam != 0 || i->classe == BC_V128)
        return false; /* vetor e opaco: nao vira SELECIONA */
    switch (i->op) {
    case BIR_CONST: case BIR_COPIA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_NEG:
        return true;
    case BIR_AND: case BIR_OR: case BIR_XOR: case BIR_NOT: case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC:
        return !real;
    default:
        return false;
    }
}

/* Troca um operando vreg pelo valor que o braço ja lhe deu; imediato so onde a instrucao aceita (pode_imed). */
static bool Substituir(const MapaBraco *m, BirOperando *o, bool pode_imed)
{
    if (o->tipo != BO_VREG)
        return true;
    for (uint32_t k = 0; k < m->n; k++) {
        if (m->dst[k] != o->id)
            continue;
        if (m->valor[k].tipo == BO_IMED && !pode_imed)
            return false;
        *o = m->valor[k];
        return true;
    }
    return true;
}

static bool Registrar(MapaBraco *m, uint32_t dst, const BirOperando *valor)
{
    for (uint32_t k = 0; k < m->n; k++) {
        if (m->dst[k] == dst) {
            m->valor[k] = *valor;
            return true;
        }
    }
    if (m->n >= BRACO_MAX)
        return false;
    m->dst[m->n] = dst;
    m->valor[m->n++] = *valor;
    return true;
}

/* Copia e constante nao geram instrucao (o mapa guarda o operando); o resto sobe com o dst novo. */
static bool Simular(const Selecao *s, Plano *pl, MapaBraco *m, const BirInstr *i)
{
    BirInstr h = *i;
    BirOperando valor;

    if (!Substituir(m, &h.a, i->op == BIR_COPIA) || !Substituir(m, &h.b, true) || !OperandoSimples(&h.a) || (h.b.tipo != BO_NADA && h.b.tipo != BO_SIMBOLO && !OperandoSimples(&h.b)))
        return false;
    if (i->op == BIR_CONST || i->op == BIR_COPIA)
        return Registrar(m, i->dst, &h.a);
    if (pl->nsobe >= PLANO_MAX)
        return false;
    h.dst = pl->base_vreg + pl->nnovos;
    pl->classe_novo[pl->nnovos++] = s->f->classe_vreg[i->dst];
    pl->sobe[pl->nsobe++] = h;
    valor = (BirOperando){.tipo = BO_VREG, .classe = i->classe, .id = h.dst};
    return Registrar(m, i->dst, &valor);
}

static bool MontarBraco(const Selecao *s, Plano *pl, uint32_t bloco, MapaBraco *m)
{
    const BirBloco *b = &s->f->blocos[bloco];

    for (uint32_t k = 0; k + 1u < b->n; k++) {
        if (!InstrucaoPura(s->f, &b->instrs[k]) || !Simular(s, pl, m, &b->instrs[k]))
            return false;
    }
    return true;
}

/* ---- SELECIONA ---- */

static bool AcharValor(const MapaBraco *m, uint32_t dst, BirOperando *saida)
{
    for (uint32_t k = 0; k < m->n; k++) {
        if (m->dst[k] == dst) {
            *saida = m->valor[k];
            return true;
        }
    }
    return false;
}

/* O que MontarSelecoes precisa: a funcao, o plano, a comparacao e o que cada braço escreveu. */
typedef struct { const Selecao *s; Plano *pl; const BirInstr *cmp; const MapaBraco *mt, *mf; } Montagem;

static void AcrescentarSelecao(const Montagem *m, uint32_t dst)
{
    BirInstr *x = &m->pl->sel[m->pl->nsel++];
    BirOperando antigo = {.tipo = BO_VREG, .classe = m->s->f->classe_vreg[dst], .id = dst};

    *x = (BirInstr){.op = BIR_SELECIONA, .aux = m->cmp->aux, .classe = antigo.classe, .dst = dst, .a = antigo, .b = antigo, .args = m->pl->cmp, .nargs = 2, .pos = m->cmp->pos};
    AcharValor(m->mt, dst, &x->a);
    AcharValor(m->mf, dst, &x->b);
}

static void MontarSelecoes(const Montagem *m)
{
    BirOperando ignorado;

    for (uint32_t k = 0; k < m->mt->n; k++)
        AcrescentarSelecao(m, m->mt->dst[k]);
    for (uint32_t k = 0; k < m->mf->n; k++) {
        if (!AcharValor(m->mt, m->mf->dst[k], &ignorado))
            AcrescentarSelecao(m, m->mf->dst[k]);
    }
}

static bool LeVreg(const BirOperando *o, uint32_t vreg)
{
    return o->tipo == BO_VREG && o->id == vreg;
}

/* O SELECIONA x escreve um vreg que outro SELECIONA (ou, havendo varios, a comparacao) le: a ordem dos dois mudaria o resultado. */
static bool ConflitaComOutros(const Plano *pl, uint32_t x)
{
    uint32_t dst = pl->sel[x].dst;

    for (uint32_t y = 0; y < pl->nsel; y++) {
        if (y != x && (LeVreg(&pl->sel[y].a, dst) || LeVreg(&pl->sel[y].b, dst)))
            return true;
    }
    return pl->nsel > 1 && (LeVreg(&pl->cmp[0], dst) || LeVreg(&pl->cmp[1], dst));
}

/* Inteiro pede comparacao inteira, real pede o padrao minss/maxss (e portanto comparacao real). */
static bool PlanoAceitavel(const Plano *pl)
{
    bool maximo;
    const BirOperando *primeiro, *segundo;

    if (pl->nsel == 0)
        return false;
    for (uint32_t x = 0; x < pl->nsel; x++) {
        const BirInstr *i = &pl->sel[x];

        if (BirClasseEhReal(i->classe) != BirClasseEhReal(pl->cmp[0].classe) || ConflitaComOutros(pl, x))
            return false;
        if (BirClasseEhReal(i->classe) && !BirSelecionaMinMax(i, &maximo, &primeiro, &segundo))
            return false;
    }
    return true;
}

/* ---- aplicar ---- */

static void GarantirCapacidade(BirBloco *b, uint32_t n)
{
    BirInstr *novo;

    if (n <= b->cap)
        return;
    novo = realloc(b->instrs, (size_t)n * sizeof *novo);
    if (novo == NULL)
        FalhaInterna("otim: memoria esgotada");
    b->instrs = novo;
    b->cap = n;
}

static void CrescerVregs(Selecao *s, const Plano *pl)
{
    BirFuncao *f = s->f;
    uint8_t *classes = realloc(f->classe_vreg, (size_t)(f->nvregs + pl->nnovos + 1u) * sizeof *classes);

    if (classes == NULL)
        FalhaInterna("otim: memoria esgotada");
    memcpy(classes + f->nvregs, pl->classe_novo, pl->nnovos);
    f->classe_vreg = classes;
    f->nvregs += pl->nnovos;
}

static void MatarBraco(Selecao *s, uint32_t bloco)
{
    s->morto[bloco] = 1;
    s->entradas[bloco] = 0;
}

/* A cabeca perde CMP e DESVIA e ganha: instrucoes que subiram, SELECIONA, SALTA para a juncao. Os braços morrem. */
static void Aplicar(Selecao *s, Plano *pl, const Forma *fm)
{
    BirBloco *h = &s->f->blocos[fm->cabeca];
    BirInstr salta = h->instrs[h->n - 1];

    CrescerVregs(s, pl);
    GarantirCapacidade(h, h->n - 2u + pl->nsobe + pl->nsel + 1u);
    h->n -= 2u;
    for (uint32_t k = 0; k < pl->nsobe; k++)
        h->instrs[h->n++] = pl->sobe[k];
    for (uint32_t k = 0; k < pl->nsel; k++) {
        BirInstr *x = &h->instrs[h->n++];

        *x = pl->sel[k];
        x->args = ArenaAlocar(&s->p->arena, 2u * sizeof *x->args);
        memcpy(x->args, pl->cmp, 2u * sizeof *x->args);
    }
    salta.op = BIR_SALTA;
    salta.a = (BirOperando){0};
    salta.alvo[0] = fm->juncao;
    salta.alvo[1] = 0;
    h->instrs[h->n++] = salta;
    if (fm->braco_t)
        MatarBraco(s, fm->alvo_t);
    if (fm->braco_f)
        MatarBraco(s, fm->alvo_f);
    s->entradas[fm->juncao]--; /* T->J e F->J (ou H->J e braço->J) viram um H->J */
}

/* Simula os braços e monta o plano; false = a cabeca fica como esta (nada foi alterado). */
static bool MontarPlano(const Selecao *s, const Forma *fm, const BirInstr *cmp, Plano *pl)
{
    MapaBraco mt = {0}, mf = {0};

    pl->cmp[0] = cmp->a;
    pl->cmp[1] = cmp->b;
    if ((fm->braco_t && !MontarBraco(s, pl, fm->alvo_t, &mt)) || (fm->braco_f && !MontarBraco(s, pl, fm->alvo_f, &mf)))
        return false;
    MontarSelecoes(&(Montagem){.s = s, .pl = pl, .cmp = cmp, .mt = &mt, .mf = &mf});
    return PlanoAceitavel(pl);
}

/* Operando de valor conhecido na cabeca: imediato, ou vreg cuja definicao anterior ao CMP, no mesmo bloco, e uma CONST. */
static bool OperandoConstanteNaCabeca(const BirBloco *b, const BirOperando *o)
{
    if (o->tipo == BO_IMED)
        return true;
    if (o->tipo != BO_VREG)
        return false;
    for (uint32_t k = 0; k + 2 < b->n; k++)
        if (b->instrs[k].dst == o->id && b->instrs[k].op == BIR_CONST)
            return true;
    return false;
}

/* Condicao com os dois lados constantes ja tem desvio resolvivel; a selecao a deixaria como cmp+cmov de constantes (f5_expr_redundante_sccp: +11 instr no laco quente). */
static bool CondicaoConstante(const BirBloco *b, const BirInstr *cmp)
{
    return OperandoConstanteNaCabeca(b, &cmp->a) && OperandoConstanteNaCabeca(b, &cmp->b);
}

static bool TentarCabeca(Selecao *s, uint32_t h)
{
    Forma fm;
    const BirInstr *cmp;

    if (!ReconhecerForma(s, h, &fm))
        return false;
    cmp = &s->f->blocos[h].instrs[s->f->blocos[h].n - 2];
    if (CondicaoConstante(&s->f->blocos[h], cmp))
        return false;
    if (OperandoSimples(&cmp->a) && OperandoSimples(&cmp->b)) {
        Plano pl = {.base_vreg = s->f->nvregs};

        if (MontarPlano(s, &fm, cmp, &pl)) {
            Aplicar(s, &pl, &fm);
            return true;
        }
    }
    return false;
}

/* Fica com os blocos vivos na ordem de id. */
static void Compactar(Selecao *s)
{
    uint32_t *ordem = malloc((size_t)s->f->nblocos * sizeof *ordem), n = 0;

    if (ordem == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t b = 0; b < s->f->nblocos; b++) {
        if (!s->morto[b])
            ordem[n++] = b;
    }
    CfgAplicarOrdem(s->f, ordem, n);
    free(ordem);
}

static void SelecaoFuncao(BirPrograma *p, BirFuncao *f)
{
    Selecao s = {.p = p, .f = f};
    uint32_t convertidas = 0;

    if (!CfgElegivel(p, f) || !Alocar(&s)) {
        Liberar(&s);
        return;
    }
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t k = 0; k < f->blocos[b].n; k++)
            ContarInstrucao(&s, &f->blocos[b].instrs[k]);
    }
    for (uint32_t b = 0; b < f->nblocos; b++)
        convertidas += TentarCabeca(&s, b) ? 1u : 0u;
    if (convertidas > 0)
        Compactar(&s);
    Liberar(&s);
}

void OtimSelecaoAplicar(BirPrograma *programa, Buf *relato)
{
    (void)relato;
    for (uint32_t i = 0; i < programa->nfuncoes; i++)
        SelecaoFuncao(programa, &programa->funcoes[i]);
}
