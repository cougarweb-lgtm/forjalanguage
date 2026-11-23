/* lower_texto_bytes.c - lowering de clone(str), from_bytes e as_bytes (IA1-L9e, T10). Sem IrOp nova.
 *   clone(s, alloc = A) e from_bytes(b, alloc = A)   a semantica ja trocou o 1o argumento por f"{x}" de uma parte so (x = s, ou b visto como str por um transmute
 *                    sintetico), entao e o IR_FORMATA do format: aloca len bytes no alocador, copia e devolve o str novo (dono; o chamador da delete).
 *   as_bytes(s)      IR_TRANSMUTA str -> []u8: a mesma {ptr, len}, sem copia e sem dono (so leitura).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

Valor LwTextoCopia(LowerCtx *c, const Expr *e)
{
    IrInstr *i;
    LwFormata(c, e->args[0].valor, 1, LwAlocador(c, e), &i);
    return LwValorTemp(i->destino, i->tipo);
}

Valor LwAsBytes(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    return LoTransmuta(&k, e->tipo, LwExpr(c, LlArgumento(e, 0)));
}
