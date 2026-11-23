/* lower_texto_builder.c - lowering do builder de texto (IA1-L9f, T11): to_str(&buf) e format(f"...", into = &buf). Sem IrOp nova.
 *   to_str(&buf)               IR_REDIMENSIONA op_aux REDIM_PARA_STR: o temp(str) recebe os bytes do builder SEM copia (o bloco vira a str dona, no alocador do builder, com NUL depois do
 *                              ultimo byte) e a lista volta a vazia (cap 0), com o mesmo alocador, pronta para outro texto (so o backend x64: bir_construir_aloca.c).
 *   format(f"...", into = &buf) versao simples: IR_FORMATA modo 1 num bloco temporario do heap, IR_ANEXA_FATIA dos bytes no builder (a mesma que o append de str usa) e IR_LIBERA do
 *                              bloco. O texto sai identico ao do format; o custo e uma alocacao e uma copia por chamada (escrever direto no builder pede um IR_FORMATA novo: pendencia).
 *   append(&buf, item = "texto") nao passa por aqui: a semantica ja o troca por items e o lowering do append o faz IR_ANEXA_FATIA.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - to_str aceita builder de qualquer alocador (so comentario; o lowering nao muda).
 *            0.1.0 - IA1-L9f: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

#define ALOCADOR_DO_HEAP 1 /* o mesmo valor de ALOCADOR_HEAP de lower_memoria.c e FORJA_ALOC_HEAP do runtime */
#define MODO_FORMATA_STR 1 /* op_aux do IR_FORMATA que devolve str */
#define LIBERA_TEXTO 2     /* op_aux do IR_LIBERA de str */

Valor LwToStr(LowerCtx *c, const Expr *e)
{
    Valor lista = LwExpr(c, LlArgumento(e, 0));
    IrInstr *i = LwNovaInstr(c, IR_REDIMENSIONA, e->pos_op);
    i->op_aux = REDIM_PARA_STR;
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_STR;
    i->a = lista;
    return LwValorTemp(i->destino, TIPO_STR);
}

Valor LwFormatoInto(LowerCtx *c, const Expr *e)
{
    IrInstr *formata;
    LwFormata(c, e->args[0].valor, MODO_FORMATA_STR, LwValorInt(ALOCADOR_DO_HEAP, TIPO_U32), &formata);
    Valor texto = LwValorTemp(formata->destino, TIPO_STR);
    Valor lista = LwExpr(c, e->args[1].valor);
    IrInstr *anexa = LwNovaInstr(c, IR_ANEXA_FATIA, e->pos_op);
    anexa->tipo = TIPO_U8;
    anexa->a = lista;
    anexa->b = texto;
    IrInstr *libera = LwNovaInstr(c, IR_LIBERA, e->pos_op);
    libera->op_aux = LIBERA_TEXTO;
    libera->a = texto;
    return LwValorNenhum();
}
