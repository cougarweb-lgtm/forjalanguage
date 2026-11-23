/* bir_construir_fluxo.c - controle de fluxo da IR na BIR: se, enquanto, para, pare e continua (blocos, DESVIA e SALTA).
 * Versao: 0.2.2
 * Changelog: 0.2.2 - M3 F9b-3: decisoes extraidas de BirRegraSe e IniciarPara.
 *            0.2.1 - M2 P10b: so a assinatura nova de BirClasseDoTipo (recebe os Tipos).
 *            0.2.0 - M2 P9: o 'para' estende o limite e o inicio pelo sinal do tipo (EXT_Z em u8 u16 u32) e a variavel do laco recebe TRUNC em toda classe
 *                    menor que 64 bits.
 *            0.1.0 - backend x64 P6: 'para' igual ao emit_c_fluxo.c (EmitCabecalhoPara): fim lido uma vez, contador de 64 bits (sem sinal em uint),
 *                    ativo = cnt <= fim (< com faixa_excl), passo "cnt == ultimo ? sai : cnt + 1", continue vai ao passo; 'enquanto' recalcula cond no topo.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj>
 */
#include "nativo/bir/bir_interno.h"

/* Condicao do se/enquanto como vreg bool (um imediato, como em 'while true', vira CONST: o DESVIA le sempre um vreg). */
static bool CondicaoEmVreg(BirCtx *c, const IrInstr *i, BirOperando *cond)
{
    return BirOperandoDe(c, i, &i->a, cond) && BirOperandoVreg(c, *cond, cond);
}

/* O ramo senao (se houver): o bloco `outro` vira o corrente e o corpo do senao salta ao fim. */
static bool RamoSenao(BirCtx *c, const IrInstr *i, BirRotulo *fim, uint32_t outro)
{
    if (i->senao.primeira == NULL)
        return true;
    c->cur = outro;
    if (!BirConstruirBloco(c, &i->senao))
        return false;
    BirRotuloSalta(c, fim);
    return true;
}

bool BirRegraSe(BirCtx *c, const IrInstr *i)
{
    BirRotulo fim = {BIR_SEM_BLOCO};
    BirOperando cond;
    bool tem_senao = i->senao.primeira != NULL;
    uint32_t entao, outro;

    if (!BirConstruirBloco(c, &i->cond) || !CondicaoEmVreg(c, i, &cond))
        return false;
    entao = BirNovoBloco(c, false);
    outro = tem_senao ? BirNovoBloco(c, false) : BirRotuloAlvo(c, &fim);
    BirDesvia(c, cond, entao, outro);
    c->cur = entao;
    if (!BirConstruirBloco(c, &i->corpo))
        return false;
    BirRotuloSalta(c, &fim);
    if (!RamoSenao(c, i, &fim, outro))
        return false;
    BirRotuloColoca(c, &fim);
    return true;
}

bool BirRegraEnquanto(BirCtx *c, const IrInstr *i)
{
    BirRotulo topo = {BIR_SEM_BLOCO}, fim = {BIR_SEM_BLOCO};
    BirOperando cond;
    uint32_t corpo;

    BirRotuloSalta(c, &topo);
    c->cur = BirRotuloAlvo(c, &topo);
    if (!BirConstruirBloco(c, &i->cond) || !CondicaoEmVreg(c, i, &cond))
        return false;
    corpo = BirNovoBloco(c, false);
    BirDesvia(c, cond, corpo, BirRotuloAlvo(c, &fim));
    c->cur = corpo;
    BirEntrarLaco(c, &topo, &fim);
    if (!BirConstruirBloco(c, &i->corpo))
        return false;
    BirSairLaco(c);
    BirRotuloSalta(c, &topo);
    BirRotuloColoca(c, &fim);
    return true;
}

/* ---- para ---- */

/* cnt e o contador de 64 bits; fim e o limite lido uma vez; ultimo = o valor em que o laco termina (fim, ou fim - 1 com faixa_excl). */
typedef struct { uint32_t cnt, fim, ultimo, corpo; BirRotulo passo, saida; } BirPara;

/* Operando da IR -> vreg novo de 64 bits: as classes curtas estendem pelo sinal do tipo; int e uint so copiam (o sinal do laco vem do tipo do contador). */
static bool Vreg64(BirCtx *c, const IrInstr *i, const Valor *v, uint32_t *vreg)
{
    BirOperando o;

    if (!BirOperandoDe(c, i, v, &o))
        return false;
    *vreg = BirNovoVreg(c, BC_I64);
    if (o.tipo == BO_IMED || o.classe == BC_I64) {
        o.classe = BC_I64;
        BirMover(c, *vreg, o);
        return true;
    }
    BirEmitirPedido(c, (BirPedido){.op = TipoEhSemSinal(v->tipo) ? BIR_EXT_Z : BIR_EXT_S, .dst = *vreg, .a = o});
    return true;
}

/* A condicao de entrada: cnt < fim na faixa exclusiva, cnt <= fim na inclusiva, na ordem do tipo. */
static uint8_t CondicaoDoPara(const IrInstr *i)
{
    bool sem_sinal = TipoEhSemSinal(i->tipo);

    if (i->faixa_excl)
        return sem_sinal ? BCOND_LT_U : BCOND_LT_S;
    return sem_sinal ? BCOND_LE_U : BCOND_LE_S;
}

static bool IniciarPara(BirCtx *c, const IrInstr *i, BirPara *p)
{
    uint8_t cond = CondicaoDoPara(i);
    uint32_t ativo;

    if (!Vreg64(c, i, &i->b, &p->fim) || !Vreg64(c, i, &i->a, &p->cnt))
        return false;
    p->ultimo = p->fim;
    if (i->faixa_excl)
        p->ultimo = BirContaNova(c, BC_I64, (BirPedido){.op = BIR_SUB, .a = OpVreg(p->fim, BC_I64), .b = OpImed(1, BC_I64)});
    ativo = BirContaNova(c, BC_I8, (BirPedido){.op = BIR_CMP, .aux = cond, .a = OpVreg(p->cnt, BC_I64), .b = OpVreg(p->fim, BC_I64)});
    p->corpo = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(ativo, BC_I8), p->corpo, BirRotuloAlvo(c, &p->saida));
    return true;
}

/* A variavel do laco recebe o contador a cada volta (mexer nela no corpo nao muda o laco). */
static bool CorpoDoPara(BirCtx *c, const IrInstr *i, BirPara *p)
{
    uint8_t classe;
    uint32_t variavel;

    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe) || !BirVregDeclarar(c, i->destino, classe, &variavel))
        return false;
    c->cur = p->corpo;
    BirEmitirPedido(c, (BirPedido){.op = classe < BC_I64 ? BIR_TRUNC : BIR_COPIA, .dst = variavel, .a = OpVreg(p->cnt, BC_I64)});
    BirEntrarLaco(c, &p->passo, &p->saida);
    if (!BirConstruirBloco(c, &i->corpo))
        return false;
    BirSairLaco(c);
    return true;
}

/* Passo: se cnt == ultimo o laco acaba (sem somar, entao fim = maximo do tipo nao estoura); senao cnt + 1 e volta ao corpo. */
static void PassoDoPara(BirCtx *c, BirPara *p)
{
    uint32_t acabou, soma;

    BirRotuloSalta(c, &p->passo);
    BirRotuloColoca(c, &p->passo);
    if (c->cur == BIR_SEM_BLOCO)
        return;
    acabou = BirContaNova(c, BC_I8, (BirPedido){.op = BIR_CMP, .aux = BCOND_EQ, .a = OpVreg(p->cnt, BC_I64), .b = OpVreg(p->ultimo, BC_I64)});
    soma = BirNovoBloco(c, false);
    BirDesvia(c, OpVreg(acabou, BC_I8), BirRotuloAlvo(c, &p->saida), soma);
    c->cur = soma;
    BirEmitirPedido(c, (BirPedido){.op = BIR_ADD, .dst = p->cnt, .a = OpVreg(p->cnt, BC_I64), .b = OpImed(1, BC_I64)});
    BirSalta(c, p->corpo);
}

bool BirRegraPara(BirCtx *c, const IrInstr *i)
{
    BirPara p = {.passo = {BIR_SEM_BLOCO}, .saida = {BIR_SEM_BLOCO}};
    uint8_t classe;

    if (!BirClasseDoTipo(c->e->tipos, i->tipo, &classe))
        return BirFalhaTipo(c, i, "-", i->tipo);
    if (!IniciarPara(c, i, &p) || !CorpoDoPara(c, i, &p))
        return false;
    PassoDoPara(c, &p);
    BirRotuloColoca(c, &p.saida);
    return true;
}

bool BirRegraPare(BirCtx *c, const IrInstr *i)
{
    (void)i;
    if (c->nlacos == 0)
        return BirFalhaInconsistente(c, "pare fora de laco");
    BirSalta(c, BirRotuloAlvo(c, c->lacos[c->nlacos - 1].pare));
    return true;
}

bool BirRegraContinua(BirCtx *c, const IrInstr *i)
{
    (void)i;
    if (c->nlacos == 0)
        return BirFalhaInconsistente(c, "continua fora de laco");
    BirSalta(c, BirRotuloAlvo(c, c->lacos[c->nlacos - 1].continua));
    return true;
}
