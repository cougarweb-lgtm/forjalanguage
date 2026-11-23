/* bir_construir_opcional.c - IR -> BIR: ?T, ?^T, ?cstr, union e tupla (IR_EMBRULHA, IR_OPC_TEM, IR_OPC_VALOR, IR_TUPLA, IR_TUPLA_ELEMENTO) e o none de ?T.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M2 P15a: primeira versao. As mesmas decisoes de layout do backend C (layout/): ?T de valor = {bool tem; T v}; ?^T e ?cstr = o proprio ponteiro (nulo = none, classe
 *                    BC_I64, sem slot); union = {u32 tag; union interna} com a variante no offset do membro 1; tupla = struct dos elementos. ?T, union e tupla sao agregados (BirEhAgregado):
 *                    vivem num slot e viajam como endereco; as cinco operacoes viram GRAVA/CARREGA/COPIA_MEM/CMP sobre ele. Nenhum op novo na BIR.
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/opcional
 *
 * Semantica (ir.h 0.14.0): EMBRULHA de ?T grava tem = 1 e o valor; de union grava a tag (b) e o valor na variante (o resto do slot fica sem definir, como o padding da struct);
 * OPC_VALOR nao confere se tem valor; TUPLA_ELEMENTO le o membro k (constante) da tupla. O none de ?T (V_NULO) e um slot zerado ({tem = 0}).
 */
#include "nativo/bir/bir_interno.h"

/* ?^T e ?cstr: o ponteiro e o proprio opcional (classe BC_I64, nulo = none). */
bool BirEhPonteiroOpcional(const Tipos *tipos, TipoId tipo)
{
    return TipoClasseDe(tipos, tipo) == TC_ANULAVEL || TipoOpcionalNicho(tipos, tipo);
}

/* none de ?T: slot novo e zerado; o vreg do endereco sai em *endereco. */
bool BirNuloAgregado(BirCtx *c, TipoId tipo, uint32_t *endereco)
{
    BirSlot forma;

    if (!BirForma(c, tipo, &forma))
        return false;
    *endereco = BirEmitirEndSlot(c, BirNovoSlot(c, forma.tam, forma.alinhamento), BirNovoVreg(c, BC_I64));
    BirZeraBloco(c, *endereco, forma.tam);
    return true;
}

static int32_t OffsetDo(BirCtx *c, TipoId tipo, uint32_t membro)
{
    return (int32_t)LayoutOffsetCampo(BirLayouts(c), tipo, membro);
}

/* ---- IR_EMBRULHA ---- */

/* ?cstr e ?^T: temp = o ponteiro. */
static bool EmbrulhaPonteiro(BirCtx *c, const IrInstr *i)
{
    BirOperando valor;
    uint32_t dst;

    if (!BirOperandoDe(c, i, &i->a, &valor) || !BirDefinirDestino(c, BC_I64, &dst))
        return false;
    BirMover(c, dst, valor);
    return true;
}

/* ?T: tem = 1 e o valor no membro v. */
static bool EmbrulhaOpcional(BirCtx *c, const IrInstr *i)
{
    BirAlvoMem alvo = {.offset = (uint32_t)OffsetDo(c, i->tipo, OPC_MEMBRO_VALOR), .tipo = (TipoId)TipoDe(c->e->tipos, i->tipo)->a};

    if (!BirSlotDoDestino(c, i, &alvo.base))
        return false;
    BirEmitirGrava(c, alvo.base, OffsetDo(c, i->tipo, OPC_MEMBRO_TEM), OpImed(1, BC_I8));
    return BirGravarEm(c, i, &alvo, &i->a);
}

/* union: a tag (b) no membro 0 e o valor na variante dessa tag. */
static bool EmbrulhaUniao(BirCtx *c, const IrInstr *i)
{
    const IrUniao *u = LayoutUniao(c->e->tipos, c->e->ir, i->tipo);
    uint32_t tag = (uint32_t)i->b.inteiro;
    BirAlvoMem alvo = {.offset = (uint32_t)OffsetDo(c, i->tipo, UNIAO_MEMBRO_VARIANTE)};

    if (u == NULL || tag >= u->nvariantes)
        return BirFalhaInconsistente(c, "IR_EMBRULHA de union com tag fora das variantes");
    alvo.tipo = u->variantes[tag];
    if (!BirSlotDoDestino(c, i, &alvo.base))
        return false;
    BirEmitirGrava(c, alvo.base, OffsetDo(c, i->tipo, UNIAO_MEMBRO_TAG), OpImed(tag, BC_I32));
    return BirGravarEm(c, i, &alvo, &i->a);
}

bool BirRegraEmbrulha(BirCtx *c, const IrInstr *i)
{
    if (TipoClasseDe(c->e->tipos, i->tipo) == TC_UNIAO)
        return EmbrulhaUniao(c, i);
    if (BirEhPonteiroOpcional(c->e->tipos, i->tipo))
        return EmbrulhaPonteiro(c, i);
    if (TipoClasseDe(c->e->tipos, i->tipo) != TC_OPCIONAL)
        return BirFalhaTipo(c, i, "-", i->tipo);
    return EmbrulhaOpcional(c, i);
}

/* ---- IR_OPC_TEM ---- */

/* ?^T e ?cstr: temp(bool) = ponteiro != 0 (op_aux 0) ou == 0 (op_aux 1). */
static bool TemPonteiro(BirCtx *c, const IrInstr *i)
{
    BirOperando ponteiro;
    uint32_t dst;

    if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirOperandoVreg(c, ponteiro, &ponteiro) || !BirDefinirDestino(c, BC_I8, &dst))
        return false;
    BirEmitirPedido(c, (BirPedido){.op = BIR_CMP, .aux = i->op_aux != 0 ? BCOND_EQ : BCOND_NE, .dst = dst, .a = ponteiro, .b = OpImed(0, BC_I64)});
    return true;
}

/* ?T: temp(bool) = o membro tem (op_aux 0) ou o contrario (op_aux 1, XOR #1 sobre o byte 0/1). */
static bool TemOpcional(BirCtx *c, const IrInstr *i)
{
    uint32_t base, dst, tem;

    if (!BirEnderecoDe(c, i, &i->a, &base) || !BirDefinirDestino(c, BC_I8, &dst))
        return false;
    tem = i->op_aux != 0 ? BirNovoVreg(c, BC_I8) : dst;
    BirEmitirCarrega(c, tem, base, OffsetDo(c, i->a.tipo, OPC_MEMBRO_TEM));
    if (tem != dst)
        BirEmitirPedido(c, (BirPedido){.op = BIR_XOR, .dst = dst, .a = OpVreg(tem, BC_I8), .b = OpImed(1, BC_I8)});
    return true;
}

bool BirRegraOpcTem(BirCtx *c, const IrInstr *i)
{
    if (BirEhPonteiroOpcional(c->e->tipos, i->a.tipo))
        return TemPonteiro(c, i);
    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_OPCIONAL)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    return TemOpcional(c, i);
}

/* ---- IR_OPC_VALOR ---- */

bool BirRegraOpcValor(BirCtx *c, const IrInstr *i)
{
    BirCampoInfo valor;
    BirOperando ponteiro;
    uint32_t base, dst;

    if (BirEhPonteiroOpcional(c->e->tipos, i->a.tipo)) {
        if (!BirOperandoDe(c, i, &i->a, &ponteiro) || !BirDefinirDestino(c, BC_I64, &dst))
            return false;
        BirMover(c, dst, ponteiro);
        return true;
    }
    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_OPCIONAL)
        return BirFalhaTipo(c, i, "-", i->a.tipo);
    if (!BirEnderecoDe(c, i, &i->a, &base))
        return false;
    valor = (BirCampoInfo){.offset = (uint32_t)OffsetDo(c, i->a.tipo, OPC_MEMBRO_VALOR), .tipo = (TipoId)TipoDe(c->e->tipos, i->a.tipo)->a};
    return BirTempDoCampo(c, i, base, &valor);
}

/* ---- IR_TUPLA e IR_TUPLA_ELEMENTO ---- */

bool BirRegraTupla(BirCtx *c, const IrInstr *i)
{
    uint32_t base;

    if (TipoClasseDe(c->e->tipos, i->tipo) != TC_TUPLA || i->nargs != TipoTuplaQtd(c->e->tipos, i->tipo))
        return BirFalhaInconsistente(c, "IR_TUPLA com valores diferentes dos elementos do tipo");
    if (!BirSlotDoDestino(c, i, &base))
        return false;
    for (uint32_t k = 0; k < i->nargs; k++) {
        BirAlvoMem alvo = {.base = base, .offset = (uint32_t)OffsetDo(c, i->tipo, k), .tipo = TipoTuplaElemento(c->e->tipos, i->tipo, k)};

        if (!BirGravarEm(c, i, &alvo, &i->args[k]))
            return false;
    }
    return true;
}

bool BirRegraTuplaElemento(BirCtx *c, const IrInstr *i)
{
    uint32_t k = (uint32_t)i->b.inteiro;
    BirCampoInfo elemento;
    uint32_t base;

    if (TipoClasseDe(c->e->tipos, i->a.tipo) != TC_TUPLA || k >= TipoTuplaQtd(c->e->tipos, i->a.tipo))
        return BirFalhaInconsistente(c, "IR_TUPLA_ELEMENTO com indice fora da tupla");
    if (!BirEnderecoDe(c, i, &i->a, &base))
        return false;
    elemento = (BirCampoInfo){.offset = (uint32_t)OffsetDo(c, i->a.tipo, k), .tipo = TipoTuplaElemento(c->e->tipos, i->a.tipo, k)};
    return BirTempDoCampo(c, i, base, &elemento);
}
