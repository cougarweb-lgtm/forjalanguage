/* slp_arvore.c - passe `slp`: contagens por vreg e a montagem do grafo vetorial a partir das gravacoes (ver slp.h).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - S4b: EmpacotarCarga recusa a carga contigua so se o sroa escalarizaria o slot (c->escalariza); carga repetida livre.
 *            0.1.1 - S4a: SlpSlotEscalarizavel (carga de slot pequeno recusa: slot_escalarizavel); deslocamentos em int64.
 *            0.1.0- S3 (D0703): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * A BIR nao e SSA: um vreg pode ser regravado (o acumulador de `m = m * a` e `v = 0; v = v + p0; v = v + p1`). Por isso cada leitura e ligada a definicao que vale NA POSICAO dela (a ultima
 * antes dela no bloco, SlpDefNoBloco) e cada instrucao da arvore so entra se essa definicao tem uma unica leitura e nenhuma leitura fora do grupo (conferido em slp_seguro.c por contagem).
 * As lanes sao percorridas juntas (Empacotar): em cada passo as 4 (ou 2) leituras tem de ser da mesma forma: a mesma operacao, cargas contiguas, ou o mesmo valor difundido.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/slp_interno.h"

typedef enum { LF_CONST, LF_EXT, LF_CARGA, LF_OP } LaneTipo;

/* O que uma leitura de uma lane resolve: constante, valor de fora da arvore, carga ou operacao do bloco. */
typedef struct { uint8_t tipo; bool estavel; uint32_t pos, vreg; BirOperando valor; } Folha;

/* Um operando e a posicao da instrucao que o le (a definicao que vale e a ultima antes dela). */
typedef struct { BirOperando op; uint32_t upos; } Faixa;

static uint32_t Empacotar(const SlpCtx *c, SlpPlano *pl, const Faixa *fx);

/* ---- contagens ---- */

static void ContarOperando(SlpCtx *c, const BirOperando *o)
{
    if (o->tipo == BO_VREG && o->id < c->f->nvregs)
        c->nleit[o->id]++;
}

void SlpContar(SlpCtx *c)
{
    size_t n = (size_t)c->f->nvregs + 1u;

    SlpLiberar(c);
    c->ndefs = calloc(4u * n, sizeof *c->ndefs);
    if (c->ndefs == NULL)
        FalhaInterna("otim: memoria esgotada");
    c->nleit = c->ndefs + n;
    c->def_b = c->ndefs + 2u * n;
    c->def_k = c->ndefs + 3u * n;
    for (uint32_t b = 0; b < c->f->nblocos; b++) {
        for (uint32_t k = 0; k < c->f->blocos[b].n; k++) {
            const BirInstr *i = &c->f->blocos[b].instrs[k];

            if (i->dst != BIR_SEM_VREG && i->dst < c->f->nvregs) {
                c->ndefs[i->dst]++;
                c->def_b[i->dst] = b;
                c->def_k[i->dst] = k;
            }
            ContarOperando(c, &i->a);
            ContarOperando(c, &i->b);
            for (uint32_t a = 0; a < i->nargs; a++)
                ContarOperando(c, &i->args[a]);
        }
    }
}

void SlpLiberar(SlpCtx *c)
{
    free(c->ndefs);
    c->ndefs = c->nleit = c->def_b = c->def_k = NULL;
}

/* ---- definicao que vale numa posicao, base de acesso, vreg estavel ---- */

uint32_t SlpDefNoBloco(const SlpCtx *c, uint32_t antes, uint32_t vreg)
{
    const BirBloco *b = &c->f->blocos[c->bloco];

    for (uint32_t k = antes; k > 0; k--) {
        if (b->instrs[k - 1].dst == vreg)
            return k - 1;
    }
    return SLP_SEM;
}

/* O proximo passo da cadeia de endereco de x (definicao unica): copia de vreg ou `add vreg, imm`. true com a origem e o deslocamento somado; so vale se a definicao vem antes de `pos` (no bloco) ou esta em outro bloco.
 * LIMITE: parametros 5 (duas saidas por ponteiro: origem e deslocamento; uma struct de uso unico so as repassaria). */
static bool PassoDoEndereco(const SlpCtx *c, uint32_t x, uint32_t pos, uint32_t *origem, int32_t *soma)
{
    const BirInstr *d;

    if (x >= c->f->nvregs || c->ndefs[x] != 1 || (c->def_b[x] == c->bloco && c->def_k[x] >= pos))
        return false;
    d = &c->f->blocos[c->def_b[x]].instrs[c->def_k[x]];
    if (d->classe != BC_I64 || d->a.tipo != BO_VREG)
        return false;
    if (d->op == BIR_COPIA) {
        *origem = d->a.id;
        *soma = 0;
        return true;
    }
    if (d->op != BIR_ADD || d->b.tipo != BO_IMED || d->b.imed < -65536 || d->b.imed > 65536)
        return false;
    *origem = d->a.id;
    *soma = (int32_t)d->b.imed;
    return true;
}

/* Base do acesso (slot, simbolo ou vreg com a definicao que vale) e o deslocamento que a cadeia de copia/add imm somou.
 * LIMITE: cognitiva 19 (alvo 15): um laco de ate 8 passos com tres formas de parada (slot, simbolo, fim da cadeia); partir criaria helper de uso unico. */
SlpChave SlpChaveBase(const SlpCtx *c, uint32_t vreg, uint32_t pos)
{
    int32_t off = 0;

    for (uint32_t volta = 0; volta < 8u; volta++) {
        uint32_t origem = 0;
        int32_t soma = 0;
        const BirInstr *d;

        if (vreg < c->f->nvregs && c->ndefs[vreg] == 1) {
            d = &c->f->blocos[c->def_b[vreg]].instrs[c->def_k[vreg]];
            if (d->op == BIR_END_SLOT && d->a.tipo == BO_IMED)
                return (SlpChave){SB_SLOT, (uint32_t)d->a.imed, 0, off};
            if (d->op == BIR_END_SIMBOLO && d->a.tipo == BO_SIMBOLO)
                return (SlpChave){SB_SIMBOLO, d->a.id, 0, off};
        }
        if (!PassoDoEndereco(c, vreg, pos, &origem, &soma))
            break;
        if (c->def_b[vreg] != c->bloco && origem < c->f->nvregs && c->ndefs[origem] > 1)
            break; /* a origem vale o que valia no outro bloco: so se nunca muda */
        pos = c->def_b[vreg] == c->bloco ? c->def_k[vreg] : pos;
        off += soma;
        vreg = origem;
    }
    return (SlpChave){SB_VREG, vreg, SlpDefNoBloco(c, pos, vreg), off};
}

bool SlpMesmaChave(const SlpChave *x, const SlpChave *y)
{
    return x->tipo == y->tipo && x->id == y->id && (x->tipo != SB_VREG || x->def == y->def);
}

bool SlpSlotEscalarizavel(const SlpCtx *c, const SlpChave *chave)
{
    return chave->tipo == SB_SLOT && chave->id < c->f->nslots && c->f->slots[chave->id].tam <= SLP_SLOT_PEQUENO_BYTES;
}

/* Valor que nunca muda depois da definicao unica, que esta fora do bloco (ou parametro nunca regravado): as leituras de qualquer lane veem o mesmo valor. */
static bool Estavel(const SlpCtx *c, uint32_t v)
{
    if (v >= c->f->nvregs)
        return false;
    return (v < c->f->nparams && c->ndefs[v] == 0) || (c->ndefs[v] == 1 && c->def_b[v] != c->bloco);
}

/* Segue copias de vreg estavel para estavel: v334 = copia v318 (as lanes de `m * a` leem o mesmo valor por vregs diferentes). */
static uint32_t Canonico(const SlpCtx *c, uint32_t v)
{
    for (uint32_t volta = 0; volta < 8u && Estavel(c, v) && c->ndefs[v] == 1; volta++) {
        const BirInstr *d = &c->f->blocos[c->def_b[v]].instrs[c->def_k[v]];

        if (d->op != BIR_COPIA || d->a.tipo != BO_VREG || d->a.classe != d->classe || !Estavel(c, d->a.id))
            break;
        v = d->a.id;
    }
    return v;
}

/* ---- plano ---- */

static bool Falhar(SlpPlano *pl, const char *motivo)
{
    if (pl->motivo == NULL)
        pl->motivo = motivo;
    return false;
}

static bool AdicionarPendente(SlpPlano *pl, uint32_t vreg, uint32_t def)
{
    for (uint32_t k = 0; k < pl->npend; k++) {
        if (pl->pend[k].vreg == vreg && pl->pend[k].def == def)
            return true;
    }
    if (pl->npend == SLP_PEND_MAX)
        return Falhar(pl, "grande");
    pl->pend[pl->npend++] = (SlpPendente){vreg, def};
    return true;
}

static bool Registrar(SlpPlano *pl, uint32_t pos, uint32_t vreg)
{
    for (uint32_t k = 0; k < pl->nsomem; k++) {
        if (pl->somem[k] == pos)
            return Falhar(pl, "valor_lido_duas_vezes");
    }
    if (pl->nsomem == SLP_SOMEM_MAX || pl->nlidos == SLP_SOMEM_MAX)
        return Falhar(pl, "grande");
    pl->somem[pl->nsomem++] = pos;
    pl->lidos[pl->nlidos++] = vreg;
    return true;
}

static uint32_t NovoNo(SlpPlano *pl, SlpNo no)
{
    if (pl->nno == SLP_NOS_MAX) {
        Falhar(pl, "grande");
        return SLP_SEM;
    }
    pl->no[pl->nno] = no;
    return pl->nno++;
}

/* ---- leitura de uma lane ---- */

/* A instrucao e de um tipo que a arvore sabe substituir e com a classe do grupo? */
static bool InstrucaoDaArvore(const BirInstr *i, uint8_t classe)
{
    if (i->classe != classe || i->nargs != 0 || i->a.tam != 0 || i->b.tam != 0)
        return false;
    switch (i->op) {
    case BIR_COPIA: return i->a.tipo == BO_VREG || i->a.tipo == BO_IMED;
    case BIR_CONST: return i->a.tipo == BO_IMED;
    case BIR_CARREGA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_DIV: return true;
    default: return false;
    }
}

/* Valor lido que nao e da arvore: vreg estavel (pela copia canonica) ou vreg que o grupo exige igual de ponta a ponta.
 * LIMITE: parametros 5 (contexto, plano, o par vreg/definicao e a folha de saida). */
static bool FolhaExterna(const SlpCtx *c, SlpPlano *pl, uint32_t vreg, uint32_t def, Folha *fo)
{
    uint32_t canonico = def == SLP_SEM ? Canonico(c, vreg) : vreg;

    if (def == SLP_SEM && Estavel(c, canonico)) {
        *fo = (Folha){.tipo = LF_EXT, .estavel = true, .pos = SLP_SEM, .vreg = canonico};
        return true;
    }
    *fo = (Folha){.tipo = LF_EXT, .pos = def, .vreg = vreg};
    return AdicionarPendente(pl, vreg, def);
}

/* Resolve o operando de uma lane (seguindo COPIA) ate constante, valor de fora, carga ou operacao. As instrucoes passadas entram em pl->somem.
 * LIMITE: cognitiva 21 (alvo 15): o laco de copias com os 5 desfechos (imediato, operando invalido, fora do bloco, fora da arvore, no); cada um e um retorno curto.
 * LIMITE: parametros 5 (contexto, plano, operando, posicao do leitor e a folha de saida). */
static bool ResolverLane(const SlpCtx *c, SlpPlano *pl, BirOperando o, uint32_t upos, Folha *fo)
{
    for (uint32_t volta = 0; volta < 16u; volta++) {
        const BirInstr *i;
        uint32_t d;

        if (o.tipo == BO_IMED && o.classe == pl->classe) {
            *fo = (Folha){.tipo = LF_CONST, .valor = o};
            return true;
        }
        if (o.tipo != BO_VREG || o.classe != pl->classe)
            return Falhar(pl, "operando");
        d = SlpDefNoBloco(c, upos, o.id);
        if (d == SLP_SEM)
            return FolhaExterna(c, pl, o.id, SLP_SEM, fo);
        i = &c->f->blocos[c->bloco].instrs[d];
        if (!InstrucaoDaArvore(i, pl->classe))
            return FolhaExterna(c, pl, o.id, d, fo);
        if (!Registrar(pl, d, o.id))
            return false;
        if (i->op == BIR_COPIA) {
            o = i->a;
            upos = d;
            continue;
        }
        if (i->op == BIR_CONST)
            *fo = (Folha){.tipo = LF_CONST, .valor = i->a};
        else
            *fo = (Folha){.tipo = i->op == BIR_CARREGA ? LF_CARGA : LF_OP, .pos = d};
        return true;
    }
    return Falhar(pl, "copias");
}

/* ---- empacotar ---- */

static uint32_t EmpacotarOperacao(const SlpCtx *c, SlpPlano *pl, const Folha *fo)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];
    Faixa fa[SLP_LANES_MAX], fb[SLP_LANES_MAX];
    uint32_t a, b;

    for (uint32_t l = 0; l < pl->lanes; l++) {
        const BirInstr *i = &blk->instrs[fo[l].pos];

        if (i->op != blk->instrs[fo[0].pos].op) {
            Falhar(pl, "operacao_mista");
            return SLP_SEM;
        }
        fa[l] = (Faixa){i->a, fo[l].pos};
        fb[l] = (Faixa){i->b, fo[l].pos};
    }
    a = Empacotar(c, pl, fa);
    b = a == SLP_SEM ? SLP_SEM : Empacotar(c, pl, fb);
    if (b == SLP_SEM)
        return SLP_SEM;
    return NovoNo(pl, (SlpNo){.tipo = SN_OPERACAO, .op = blk->instrs[fo[0].pos].op, .filho = {a, b}});
}

/* Cargas das lanes: contiguas em ordem (carga vetorial) ou todas do mesmo endereco (uma carga escalar difundida); mesma base nas lanes. */
static uint32_t EmpacotarCarga(const SlpCtx *c, SlpPlano *pl, const Folha *fo)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];
    const BirInstr *i0 = &blk->instrs[fo[0].pos];
    SlpChave k0 = SlpChaveBase(c, i0->a.id, fo[0].pos);
    int64_t passo = pl->classe == BC_F32 ? 4 : 8;
    int64_t desloc0 = (int64_t)i0->desloc + k0.off;
    bool contiguas = true, repetidas = true;

    for (uint32_t l = 0; l < pl->lanes; l++) {
        const BirInstr *i = &blk->instrs[fo[l].pos];
        SlpChave k = SlpChaveBase(c, i->a.id, fo[l].pos);

        if (i->a.tipo != BO_VREG || !SlpMesmaChave(&k0, &k)) {
            Falhar(pl, "bases_diferentes");
            return SLP_SEM;
        }
        contiguas = contiguas && (int64_t)i->desloc + k.off == desloc0 + passo * (int64_t)l;
        repetidas = repetidas && (int64_t)i->desloc + k.off == desloc0;
    }
    if (contiguas && k0.tipo == SB_SLOT && k0.id < c->f->nslots && c->escalariza != NULL && c->escalariza[k0.id]) { /* veredito real do sroa; carga repetida (escalar + splat) segue livre */
        Falhar(pl, "slot_escalarizavel");
        return SLP_SEM;
    }
    if (!contiguas && !repetidas) {
        Falhar(pl, "lanes_nao_contiguas");
        return SLP_SEM;
    }
    if (k0.tipo == SB_VREG && !AdicionarPendente(pl, k0.id, k0.def))
        return SLP_SEM;
    return NovoNo(pl, (SlpNo){.tipo = contiguas ? SN_CARGA : SN_CARGA_REPETIDA, .modelo = fo[0].pos});
}

static uint64_t BitsDaConstante(const BirOperando *o, uint8_t classe)
{
    return classe == BC_F32 ? (uint64_t)(uint32_t)o->imed : (uint64_t)o->imed;
}

static uint32_t EmpacotarConstante(SlpPlano *pl, const Folha *fo)
{
    for (uint32_t l = 1; l < pl->lanes; l++) {
        if (BitsDaConstante(&fo[l].valor, pl->classe) != BitsDaConstante(&fo[0].valor, pl->classe)) {
            Falhar(pl, "constantes_diferentes");
            return SLP_SEM;
        }
    }
    return NovoNo(pl, (SlpNo){.tipo = SN_CONSTANTE, .valor = fo[0].valor});
}

static uint32_t EmpacotarExterno(SlpPlano *pl, const Folha *fo)
{
    for (uint32_t l = 1; l < pl->lanes; l++) {
        if (fo[l].vreg != fo[0].vreg || fo[l].pos != fo[0].pos || fo[l].estavel != fo[0].estavel) {
            Falhar(pl, "valores_diferentes");
            return SLP_SEM;
        }
    }
    return NovoNo(pl, (SlpNo){.tipo = SN_VREG, .valor = {.tipo = BO_VREG, .classe = pl->classe, .id = fo[0].vreg}});
}

/* Todas as lanes leem o MESMO vreg e a mesma definicao dele dentro do bloco (um escalar calculado antes, `s * v`): difunde-se esse vreg; a definicao fica onde esta. */
static uint32_t EmpacotarMesmaDefinicao(const SlpCtx *c, SlpPlano *pl, const Faixa *fx)
{
    uint32_t def = SLP_SEM;

    for (uint32_t l = 0; l < pl->lanes; l++) {
        if (fx[l].op.tipo != BO_VREG || fx[l].op.classe != pl->classe || fx[l].op.id != fx[0].op.id)
            return SLP_SEM;
        if (l == 0)
            def = SlpDefNoBloco(c, fx[0].upos, fx[0].op.id);
        if (def == SLP_SEM || SlpDefNoBloco(c, fx[l].upos, fx[l].op.id) != def)
            return SLP_SEM;
    }
    if (!AdicionarPendente(pl, fx[0].op.id, def))
        return SLP_SEM;
    return NovoNo(pl, (SlpNo){.tipo = SN_VREG, .valor = fx[0].op});
}

static uint32_t Empacotar(const SlpCtx *c, SlpPlano *pl, const Faixa *fx)
{
    Folha fo[SLP_LANES_MAX] = {{0}};
    uint32_t mesmo = EmpacotarMesmaDefinicao(c, pl, fx);

    if (mesmo != SLP_SEM || pl->motivo != NULL)
        return mesmo;
    for (uint32_t l = 0; l < pl->lanes; l++) {
        if (!ResolverLane(c, pl, fx[l].op, fx[l].upos, &fo[l]))
            return SLP_SEM;
        if (fo[l].tipo != fo[0].tipo) {
            Falhar(pl, "folhas_mistas");
            return SLP_SEM;
        }
    }
    switch (fo[0].tipo) {
    case LF_OP: return EmpacotarOperacao(c, pl, fo);
    case LF_CARGA: return EmpacotarCarga(c, pl, fo);
    case LF_CONST: return EmpacotarConstante(pl, fo);
    default: return EmpacotarExterno(pl, fo);
    }
}

bool SlpMontarArvore(const SlpCtx *c, SlpPlano *pl)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];
    Faixa raizes[SLP_LANES_MAX];

    for (uint32_t l = 0; l < pl->lanes; l++)
        raizes[l] = (Faixa){blk->instrs[pl->loja[l]].b, pl->loja[l]};
    pl->raiz = Empacotar(c, pl, raizes);
    return pl->raiz != SLP_SEM;
}
