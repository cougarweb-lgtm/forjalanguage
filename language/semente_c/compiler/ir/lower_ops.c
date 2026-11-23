/* lower_ops.c - pecas de IR dos embutidos de aritmetica com volta e de bits (IA1-L9a, IA1-L9b): binario, nao, ~, conversao, transmute,
 * local, atribuicao e "se". Mesmo molde de lower_matematica.c (que as guarda estaticas, so para min/max/clamp/abs).
 * Interface (lower_ops.h): LoSemSinal, LoLargura, LoMascara, LoBinario, LoNao, LoBitNao, LoConverte, LoTransmuta, LoLocal, LoAtribui, LoEntraSe, LoSaiDoSe.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9a/L9b: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_ops.h"

enum { BITS_POR_INT64 = 64 };

TipoId LoSemSinal(TipoId tipo)
{
    switch (tipo) {
    case TIPO_I8: return TIPO_U8;
    case TIPO_I16: return TIPO_U16;
    case TIPO_I32: return TIPO_U32;
    case TIPO_INT: return TIPO_UINT;
    default: return tipo;
    }
}

uint32_t LoLargura(const LoCtx *k, TipoId tipo)
{
    return TipoLarguraBits(&k->c->sem->tipos, tipo);
}

Valor LoMascara(const LoCtx *k, TipoId tipo_sem_sinal, uint64_t bits)
{
    uint32_t largura = LoLargura(k, tipo_sem_sinal);
    if (largura < BITS_POR_INT64) bits &= (UINT64_C(1) << largura) - 1;
    return LwValorInt((int64_t)bits, tipo_sem_sinal);
}

static Valor InstrComTemp(const LoCtx *k, IrOp op, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(k->c, op, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = tipo;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, tipo);
}

Valor LoBinario(const LoCtx *k, uint8_t operador, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(k->c, IR_BINARIO, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = tipo;
    i->op_aux = operador;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, tipo);
}

static Valor Unario(const LoCtx *k, uint8_t operador, TipoId tipo, Valor a)
{
    IrInstr *i = LwNovaInstr(k->c, IR_UNARIO, k->pos);
    i->destino = LwNovoTemp(k->c);
    i->tipo = tipo;
    i->op_aux = operador;
    i->a = a;
    return LwValorTemp(i->destino, tipo);
}

Valor LoNao(const LoCtx *k, Valor a)
{
    return Unario(k, OP_NAO, TIPO_BOOL, a);
}

Valor LoBitNao(const LoCtx *k, Valor a)
{
    return Unario(k, OP_BIT_NAO, a.tipo, a);
}

/* Constante (V_INT): so muda o tipo, sem instrucao; os chamadores so convertem constante que cabe no alvo (valor >= 0). */
Valor LoConverte(const LoCtx *k, TipoId alvo, Valor a)
{
    if (a.tipo == alvo) return a;
    if (a.tipo_no == V_INT) return LwValorInt(a.inteiro, alvo);
    return InstrComTemp(k, IR_CONVERSAO, alvo, a, LwValorNenhum());
}

/* Constante (V_INT) para sem sinal: os mesmos bits na largura do alvo (-1 de i32 vira 0xFFFFFFFF em u32). */
Valor LoTransmuta(const LoCtx *k, TipoId alvo, Valor a)
{
    if (a.tipo == alvo) return a;
    if (a.tipo_no == V_INT && TipoEhSemSinal(alvo)) return LoMascara(k, alvo, (uint64_t)a.inteiro);
    return InstrComTemp(k, IR_TRANSMUTA, alvo, a, LwValorNenhum());
}

int32_t LoLocal(const LoCtx *k, TipoId tipo, Valor inicial)
{
    IrInstr *def = LwNovaInstr(k->c, IR_LOCAL, k->pos);
    def->destino = LwNovoLocal(k->c);
    def->tipo = tipo;
    def->mutavel = true;
    def->a = inicial;
    return def->destino;
}

Valor LoEmLocal(const LoCtx *k, Valor v)
{
    if (v.tipo_no != V_INT) return v;
    return LwValorLocal(LoLocal(k, v.tipo, v), v.tipo);
}

void LoAtribui(const LoCtx *k, int32_t local, TipoId tipo, Valor valor)
{
    IrInstr *i = LwNovaInstr(k->c, IR_ATRIBUICAO, k->pos);
    i->destino = local;
    i->tipo = tipo;
    i->a = valor;
}

IrBloco *LoEntraSe(const LoCtx *k, Valor condicao)
{
    IrInstr *se = LwNovaInstr(k->c, IR_SE, k->pos);
    se->a = condicao;
    return LwTrocarBloco(k->c, &se->corpo);
}

void LoSaiDoSe(const LoCtx *k, IrBloco *externo)
{
    LwTrocarBloco(k->c, externo);
}
