/* lower_ir.c - construcao da IR no lowering: instrucoes, temporarios, locais sinteticos e operandos.
 * Versao: 0.1.2
 * Changelog: 0.1.2 - Linguagem v1.3 P6: LwValorTexto(nome, tipo) (str ou cstr).
 *            0.1.1 - Linguagem v1.3: LwValorNulo.
 *            0.1.0 - Fase 5d: primeira versao (contrato em lower_interno.h).
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#include "ir/lower_interno.h"

void *LwAlocar(LowerCtx *c, size_t n)
{
    return ArenaAlocar(c->arena, n);
}

IrInstr *LwNovaInstr(LowerCtx *c, IrOp op, uint32_t pos)
{
    IrInstr *i = LwAlocar(c, sizeof *i);
    i->op = (uint8_t)op;
    i->pos = pos;
    i->destino = -1;
    i->nome = -1;
    if (c->atual->ultima == NULL) c->atual->primeira = i;
    else c->atual->ultima->prox = i;
    c->atual->ultima = i;
    return i;
}

int32_t LwNovoTemp(LowerCtx *c)
{
    return (int32_t)c->ntemps++;
}

int32_t LwNovoLocal(LowerCtx *c)
{
    return (int32_t)c->prox_local++;
}

IrBloco *LwTrocarBloco(LowerCtx *c, IrBloco *novo)
{
    IrBloco *anterior = c->atual;
    c->atual = novo;
    return anterior;
}

Valor LwValorTemp(int32_t id, TipoId tipo)
{
    return (Valor){.tipo_no = V_TEMP, .tipo = tipo, .id = id};
}

Valor LwValorLocal(int32_t id, TipoId tipo)
{
    return (Valor){.tipo_no = V_LOCAL, .tipo = tipo, .id = id};
}

Valor LwValorInt(int64_t v, TipoId tipo)
{
    return (Valor){.tipo_no = V_INT, .tipo = tipo, .inteiro = v};
}

Valor LwValorReal(double v, TipoId tipo)
{
    return (Valor){.tipo_no = V_REAL, .tipo = tipo, .real = v};
}

Valor LwValorBool(bool v)
{
    return (Valor){.tipo_no = V_BOOL, .tipo = TIPO_BOOL, .inteiro = v ? 1 : 0};
}

Valor LwValorTexto(int32_t nome_internado, TipoId tipo)
{
    return (Valor){.tipo_no = V_TEXTO, .tipo = tipo, .id = nome_internado};
}

Valor LwValorNulo(TipoId tipo)
{
    return (Valor){.tipo_no = V_NULO, .tipo = tipo};
}

Valor LwValorNenhum(void)
{
    return (Valor){.tipo_no = V_NENHUM, .tipo = TIPO_VAZIO};
}

Valor *LwArgs(LowerCtx *c, uint32_t n)
{
    return n == 0 ? NULL : LwAlocar(c, n * sizeof(Valor));
}
