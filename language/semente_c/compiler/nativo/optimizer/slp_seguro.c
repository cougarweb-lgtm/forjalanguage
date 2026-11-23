/* slp_seguro.c - passe `slp`: as conferencias que decidem se o grupo pode virar vetor sem mudar nada observavel (ver slp.h).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - S4a: deslocamentos em int64.
 *            0.1.0- S3 (D0703): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * O grupo vira: carga vetorial + conta vetorial na posicao da ULTIMA instrucao da arvore (newpos) e gravacao vetorial na posicao da ULTIMA gravacao (S). Tudo que andou (cargas para
 * depois, gravacoes para depois) tem de poder andar: nada entre elas pode chamar, trapar, ou tocar memoria que possa ser a mesma (regra conservadora: so dois slots ou simbolos
 * diferentes, ou o mesmo slot/simbolo com faixas de bytes disjuntas, sao provados sem alias; ponteiro de base distinta sempre pode). Uma carga do grupo que vem DEPOIS de uma
 * gravacao do grupo sobre a mesma memoria possivel e recusada: o vetor leria antes de gravar.
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/slp_interno.h"

static bool Falhar(SlpPlano *pl, const char *motivo)
{
    if (pl->motivo == NULL)
        pl->motivo = motivo;
    return false;
}

static uint32_t TamanhoDaClasse(uint8_t classe)
{
    static const uint8_t BYTES[] = {0, 1, 2, 4, 8, 4, 8, 16}; /* BC_NADA, I8, I16, I32, I64, F32, F64, V128 */

    return classe < sizeof BYTES ? BYTES[classe] : 0u;
}

static bool PodeSobrepor(const SlpAcesso *x, const SlpAcesso *y)
{
    if (SlpMesmaChave(&x->chave, &y->chave))
        return x->desloc < y->desloc + (int64_t)y->tam && y->desloc < x->desloc + (int64_t)x->tam;
    return x->chave.tipo == SB_VREG || y->chave.tipo == SB_VREG;
}

static SlpAcesso AcessoDe(const SlpCtx *c, uint32_t pos, bool escrita)
{
    const BirInstr *i = &c->f->blocos[c->bloco].instrs[pos];

    SlpChave chave = SlpChaveBase(c, i->a.id, pos);

    return (SlpAcesso){pos, chave, (int64_t)i->desloc + chave.off, TamanhoDaClasse(escrita ? i->b.classe : i->classe), escrita};
}

/* Toda definicao da arvore tem so leituras da arvore: nenhum intermediario, carga ou valor gravado e usado fora. */
static bool ContagemFecha(const SlpCtx *c, SlpPlano *pl)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];

    for (uint32_t k = 0; k < pl->nsomem; k++) {
        uint32_t v = blk->instrs[pl->somem[k]].dst, defs = 0, lidos = 0;

        for (uint32_t j = 0; j < pl->nsomem; j++)
            defs += blk->instrs[pl->somem[j]].dst == v ? 1u : 0u;
        for (uint32_t j = 0; j < pl->nlidos; j++)
            lidos += pl->lidos[j] == v ? 1u : 0u;
        if (c->ndefs[v] != defs || c->nleit[v] != lidos)
            return Falhar(pl, "uso_externo");
    }
    return true;
}

/* Vreg lido de fora (base de acesso, valor difundido): nenhuma definicao dele depois da que vale e ate a gravacao do vetor. */
static bool PendentesIguais(const SlpCtx *c, SlpPlano *pl, uint32_t ultima_loja)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];

    for (uint32_t k = 0; k < pl->npend; k++) {
        uint32_t desde = pl->pend[k].def == SLP_SEM ? 0u : pl->pend[k].def + 1u;

        for (uint32_t q = desde; q <= ultima_loja; q++) {
            if (blk->instrs[q].dst == pl->pend[k].vreg)
                return Falhar(pl, "valor_redefinido");
        }
    }
    return true;
}

/* Reune os acessos do grupo: as cargas da arvore (que so se movem para depois) e as gravacoes. */
static uint32_t ReunirAcessos(const SlpCtx *c, const SlpPlano *pl, SlpAcesso *acessos)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < pl->nsomem; k++) {
        if (c->f->blocos[c->bloco].instrs[pl->somem[k]].op == BIR_CARREGA)
            acessos[n++] = AcessoDe(c, pl->somem[k], false);
    }
    for (uint32_t l = 0; l < pl->lanes; l++)
        acessos[n++] = AcessoDe(c, pl->loja[l], true);
    return n;
}

/* Dentro do grupo: carga que vem depois de uma gravacao com sobreposicao possivel mudaria de valor. */
static bool OrdemInternaPreservada(const SlpAcesso *acessos, uint32_t n)
{
    for (uint32_t x = 0; x < n; x++) {
        for (uint32_t y = 0; y < n; y++) {
            if (!acessos[x].escrita && acessos[y].escrita && acessos[x].pos > acessos[y].pos && PodeSobrepor(&acessos[x], &acessos[y]))
                return false;
        }
    }
    return true;
}

/* A instrucao de fora do grupo, entre as que andaram, pode ser cruzada?
 * LIMITE: parametros 5 (contexto, instrucao, posicao e a lista de acessos do grupo com a contagem). */
static bool PodeCruzar(const SlpCtx *c, const BirInstr *i, uint32_t q, const SlpAcesso *acessos, uint32_t n)
{
    SlpAcesso o;

    switch (i->op) {
    case BIR_CHAMA: case BIR_CHAMA_IND: case BIR_ARMADILHA:
    case BIR_ADD_OV: case BIR_SUB_OV: case BIR_MUL_OV: case BIR_NEG_OV:
    case BIR_COPIA_MEM: case BIR_ZERA_MEM:
        return false;
    case BIR_GRAVA: case BIR_CARREGA:
        o = AcessoDe(c, q, i->op == BIR_GRAVA);
        for (uint32_t k = 0; k < n; k++) {
            if ((o.escrita || acessos[k].escrita) && PodeSobrepor(&o, &acessos[k]))
                return false;
        }
        return true;
    default:
        return true;
    }
}

/* Nada entre as instrucoes que andaram impede o movimento (chama, trap, alias).
 * LIMITE: parametros 6 (contexto, plano, os dois limites do intervalo e a lista de acessos com a contagem). */
static bool IntervaloLivre(const SlpCtx *c, SlpPlano *pl, uint32_t baixo, uint32_t alto, const SlpAcesso *acessos, uint32_t n)
{
    const BirBloco *blk = &c->f->blocos[c->bloco];
    uint8_t *do_grupo = calloc((size_t)blk->n + 1u, 1);
    bool ok = true;

    if (do_grupo == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t k = 0; k < pl->nsomem; k++)
        do_grupo[pl->somem[k]] = 1;
    for (uint32_t l = 0; l < pl->lanes; l++)
        do_grupo[pl->loja[l]] = 1;
    for (uint32_t q = baixo; q <= alto && ok; q++)
        ok = do_grupo[q] != 0 || PodeCruzar(c, &blk->instrs[q], q, acessos, n);
    free(do_grupo);
    return ok || Falhar(pl, "chama_trap_ou_alias");
}

static bool TemCargaVetorial(const SlpPlano *pl)
{
    for (uint32_t k = 0; k < pl->nno; k++) {
        if (pl->no[k].tipo == SN_CARGA)
            return true;
    }
    return false;
}

/* Todas as conferencias do plano, na ordem; false com o motivo em pl->motivo.
 * LIMITE: cognitiva 16 (alvo 15): duas varreduras curtas (ultima gravacao e ultima conta) seguidas das conferencias; partir criaria helper de uso unico. */
bool SlpConferirPlano(const SlpCtx *c, SlpPlano *pl)
{
    SlpAcesso acessos[SLP_ACESSOS_MAX];
    uint32_t n, ultima_loja = 0, ultima_conta = 0, baixo = UINT32_MAX;

    if (pl->nsomem == 0 || !TemCargaVetorial(pl))
        return Falhar(pl, "sem_carga_vetorial");
    if (!ContagemFecha(c, pl))
        return false;
    for (uint32_t l = 0; l < pl->lanes; l++) {
        ultima_loja = pl->loja[l] > ultima_loja ? pl->loja[l] : ultima_loja;
        baixo = pl->loja[l] < baixo ? pl->loja[l] : baixo;
    }
    for (uint32_t k = 0; k < pl->nsomem; k++) {
        ultima_conta = pl->somem[k] > ultima_conta ? pl->somem[k] : ultima_conta;
        baixo = pl->somem[k] < baixo ? pl->somem[k] : baixo;
    }
    if (ultima_conta >= ultima_loja)
        return Falhar(pl, "ordem");
    if (!PendentesIguais(c, pl, ultima_loja))
        return false;
    n = ReunirAcessos(c, pl, acessos);
    if (!OrdemInternaPreservada(acessos, n))
        return Falhar(pl, "alias");
    return IntervaloLivre(c, pl, baixo, ultima_loja, acessos, n);
}
