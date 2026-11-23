/* slp_emitir.c - passe `slp`: troca o grupo escalar pelas instrucoes vetoriais (ver slp.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - S3 (D0703): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * As instrucoes que o plano apaga somem; no lugar da ULTIMA delas entra a sequencia vetorial (uma instrucao por no do grafo, na ordem em que o grafo foi montado: filho antes do pai)
 * e no lugar da ULTIMA gravacao entra a gravacao vetorial; as outras gravacoes somem. Todo vreg do vetor e novo (BC_V128 ou o escalar da carga repetida e da constante difundida).
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/slp_interno.h"

#define SEQ_MAX (2u * SLP_NOS_MAX)

typedef struct {
    BirInstr seq[SEQ_MAX];
    uint32_t nseq;
    uint32_t vet[SLP_NOS_MAX]; /* vreg V128 de cada no */
    uint32_t proximo;          /* proximo vreg novo */
    uint8_t forma;
} Sequencia;

static BirOperando OpVreg(uint32_t id, uint8_t classe)
{
    return (BirOperando){.tipo = BO_VREG, .classe = classe, .id = id};
}

static BirInstr NovaInstr(uint8_t op, uint8_t classe, uint32_t dst, const BirInstr *molde)
{
    BirInstr i;

    memset(&i, 0, sizeof i);
    i.op = op;
    i.classe = classe;
    i.dst = dst;
    i.pos = molde->pos;
    i.ir = molde->ir;
    i.simbolo = BIR_SEM_SIMBOLO;
    i.alvo[0] = BIR_SEM_BLOCO;
    i.alvo[1] = BIR_SEM_BLOCO;
    return i;
}

static void Acrescentar(Sequencia *s, BirInstr i)
{
    s->seq[s->nseq++] = i;
}

/* Quantos vregs novos o plano pede: um por no, e dois nos nos que primeiro carregam ou criam o escalar para difundir. */
static uint32_t VregsNovos(const SlpPlano *pl)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < pl->nno; k++)
        n += (pl->no[k].tipo == SN_CARGA_REPETIDA || pl->no[k].tipo == SN_CONSTANTE) ? 2u : 1u;
    return n;
}

/* A classe de cada vreg novo e posta por NovoVreg. */
static void CrescerVregs(BirFuncao *f, uint32_t novos)
{
    uint8_t *classes = realloc(f->classe_vreg, (size_t)(f->nvregs + novos + 1u) * sizeof *classes);

    if (classes == NULL)
        FalhaInterna("otim: memoria esgotada");
    f->classe_vreg = classes;
}

static uint32_t NovoVreg(BirFuncao *f, Sequencia *s, uint8_t classe)
{
    f->classe_vreg[s->proximo] = classe;
    return s->proximo++;
}

/* Escalar que vai ser difundido: a carga repetida (copia da carga da lane 0) ou a constante; devolve o vreg escalar.
 * LIMITE: parametros 6 (estado da emissao: funcao, sequencia, plano, no, instrucoes e molde; so duas funcoes o usam). */
static uint32_t EscalarDoNo(BirFuncao *f, Sequencia *s, const SlpPlano *pl, const SlpNo *no, const BirInstr *instrs, const BirInstr *molde)
{
    uint32_t v = NovoVreg(f, s, pl->classe);

    if (no->tipo == SN_CARGA_REPETIDA) {
        BirInstr i = instrs[no->modelo];

        i.dst = v;
        Acrescentar(s, i);
    } else {
        BirInstr i = NovaInstr(BIR_CONST, pl->classe, v, molde);

        i.a = no->valor;
        Acrescentar(s, i);
    }
    return v;
}

/* Emite a instrucao vetorial de um no do grafo.
 * LIMITE: parametros 6 (o mesmo estado da emissao de EscalarDoNo mais o indice do no). */
static void EmitirNo(BirFuncao *f, Sequencia *s, const SlpPlano *pl, uint32_t k, const BirInstr *instrs, const BirInstr *molde)
{
    const SlpNo *no = &pl->no[k];
    uint32_t dst = 0;
    BirInstr i;

    if (no->tipo == SN_CARGA) {
        dst = NovoVreg(f, s, BC_V128);
        i = instrs[no->modelo];
        i.classe = BC_V128;
        i.dst = dst;
        i.aux = s->forma;
    } else if (no->tipo == SN_OPERACAO) {
        dst = NovoVreg(f, s, BC_V128);
        i = NovaInstr(no->op, BC_V128, dst, molde);
        i.a = OpVreg(s->vet[no->filho[0]], BC_V128);
        i.b = OpVreg(s->vet[no->filho[1]], BC_V128);
        i.aux = s->forma;
    } else {
        BirOperando origem = no->valor;

        if (no->tipo != SN_VREG)
            origem = OpVreg(EscalarDoNo(f, s, pl, no, instrs, molde), pl->classe);
        dst = NovoVreg(f, s, BC_V128);
        i = NovaInstr(BIR_SPLAT, BC_V128, dst, molde);
        i.a = origem;
        i.aux = s->forma;
    }
    Acrescentar(s, i);
    s->vet[k] = dst;
}

/* A ultima gravacao do vetor: copia da gravacao da lane 0 com o vetor raiz como valor. */
static BirInstr GravacaoVetorial(const Sequencia *s, const SlpPlano *pl, const BirInstr *instrs)
{
    BirInstr g = instrs[pl->loja[0]];

    g.classe = BC_V128;
    g.b = OpVreg(s->vet[pl->raiz], BC_V128);
    g.aux = s->forma;
    return g;
}

/* Monta o bloco novo: sem o que o plano apaga, com a sequencia vetorial e a gravacao vetorial.
 * LIMITE: parametros 5 (bloco, plano, sequencia e as duas posicoes de insercao). */
static void ReescreverBloco(BirBloco *blk, const SlpPlano *pl, const Sequencia *s, uint32_t pos_conta, uint32_t pos_loja)
{
    uint8_t *apaga = calloc((size_t)blk->n + 1u, 1);
    BirInstr *novo = malloc(((size_t)blk->n + SEQ_MAX + 1u) * sizeof *novo);
    BirInstr grava;
    uint32_t n = 0;

    if (apaga == NULL || novo == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < pl->nsomem; k++)
        apaga[pl->somem[k]] = 1;
    for (uint32_t l = 0; l < pl->lanes; l++)
        apaga[pl->loja[l]] = 1;
    grava = GravacaoVetorial(s, pl, blk->instrs);
    for (uint32_t q = 0; q < blk->n; q++) {
        if (q == pos_conta) {
            memcpy(novo + n, s->seq, s->nseq * sizeof *novo);
            n += s->nseq;
        } else if (q == pos_loja) {
            novo[n++] = grava;
        } else if (!apaga[q]) {
            novo[n++] = blk->instrs[q];
        }
    }
    if (n > blk->cap) {
        BirInstr *maior = realloc(blk->instrs, (size_t)n * sizeof *maior);

        if (maior == NULL)
            FalhaInterna("otim: memoria esgotada");
        blk->instrs = maior;
        blk->cap = n;
    }
    memcpy(blk->instrs, novo, (size_t)n * sizeof *novo);
    blk->n = n;
    free(apaga);
    free(novo);
}

void SlpAplicarPlano(SlpCtx *c, const SlpPlano *pl)
{
    BirFuncao *f = c->f;
    BirBloco *blk = &f->blocos[c->bloco];
    Sequencia *s = calloc(1, sizeof *s);
    uint32_t pos_conta = 0, pos_loja = 0;
    BirInstr molde;

    if (s == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < pl->nsomem; k++)
        pos_conta = pl->somem[k] > pos_conta ? pl->somem[k] : pos_conta;
    for (uint32_t l = 0; l < pl->lanes; l++)
        pos_loja = pl->loja[l] > pos_loja ? pl->loja[l] : pos_loja;
    molde = blk->instrs[pos_loja];
    s->forma = pl->classe == BC_F32 ? BVEC_4XF32 : BVEC_2XF64;
    s->proximo = f->nvregs;
    CrescerVregs(f, VregsNovos(pl));
    for (uint32_t k = 0; k < pl->nno; k++)
        EmitirNo(f, s, pl, k, blk->instrs, &molde);
    f->nvregs += VregsNovos(pl);
    ReescreverBloco(blk, pl, s, pos_conta, pos_loja);
    free(s);
}
