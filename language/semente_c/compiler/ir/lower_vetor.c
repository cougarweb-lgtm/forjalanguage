/* lower_vetor.c - lowering da conta elemento a elemento em [N]T (v1.11 L12a, A04): a + b, a - b, a * b, a / b, a * k, k * a, a / k, -a, += e -=.
 * Sem IrOp nova: N <= 16 desenrola (por elemento, na ordem 0..N-1: IR_ELEMENTO de cada operando [N]T + IR_BINARIO/IR_UNARIO; no fim um IR_ARRAY);
 * N > 16 vira um IR_PARA 0..<N que grava cada elemento num local [N]T (IR_ENDERECO_ELEMENTO + IR_GRAVA_PONTEIRO). O indice e provado (op_aux 1).
 * Divisao inteira por zero, estouro (R0002 em dev) e unidade seguem a regra do escalar: e o proprio IR_BINARIO de cada elemento.
 * Interface (lower_interno.h): LwContaVetor (usado tambem por '+=' e '-=' em lower_stmt.c), LwBinarioVetor e LwUnarioVetor (lower_expr.c).
 * v1.11 L12b: LwSwizzle (v.xy, v.zyx, c.bgra: rvalue [k]T, indice constante provado, IR_ARRAY); v.x (1 letra) sai como EX_INDICE da semantica.
 * v1.11 L13: LwPorElemento (o desenrolar/laco por elemento, sem saber a conta; usado por LwContaVetor e por lower_matematica.c).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - Linguagem v1.11 L13: LwPorElemento (PorElemento); Desenrolada/EmLaco/ElementoDoResultado deixam de conhecer ContaVetor.
 *            0.2.0 - Linguagem v1.11 L12b: LwSwizzle.
 *            0.1.0 - Linguagem v1.11 L12a: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

enum { VETOR_DESENROLA_MAX = 16 };

static bool EhVetor(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t) == TC_ARRAY;
}

/* O elemento de [N]T; um escalar e o proprio elemento. */
static TipoId ElementoDe(const LowerCtx *c, TipoId t)
{
    return EhVetor(c, t) ? (TipoId)TipoDe(&c->sem->tipos, t)->a : t;
}

/* Elemento 'indice' de um operando [N]T (IR_ELEMENTO, indice provado); o escalar (ou V_NENHUM do '-a') vale como esta. */
Valor LwElementoDoOperando(LowerCtx *c, Valor v, Valor indice, uint32_t pos)
{
    if (v.tipo_no == V_NENHUM || !EhVetor(c, v.tipo)) return v;
    IrInstr *i = LwNovaInstr(c, IR_ELEMENTO, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = ElementoDe(c, v.tipo);
    i->a = v;
    i->b = indice;
    i->op_aux = 1;
    return LwValorTemp(i->destino, i->tipo);
}

/* O elemento 'indice' do resultado: a conta (dados = ContaVetor) dos elementos 'indice' dos operandos, no tipo do elemento do resultado. */
static Valor ElementoDoResultado(LowerCtx *c, const void *dados, Valor indice)
{
    const ContaVetor *conta = dados;
    Valor a = LwElementoDoOperando(c, conta->a, indice, conta->pos);
    Valor b = LwElementoDoOperando(c, conta->b, indice, conta->pos);
    IrInstr *i = LwNovaInstr(c, conta->op == OP_NEGAR ? IR_UNARIO : IR_BINARIO, conta->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = ElementoDe(c, conta->tipo);
    i->op_aux = conta->op;
    i->a = a;
    i->b = b;
    return LwValorTemp(i->destino, i->tipo);
}

/* N pequeno: um grupo de instrucoes por elemento, em ordem; o IR_ARRAY junta os resultados. */
static Valor Desenrolada(LowerCtx *c, const PorElemento *por, uint32_t n)
{
    Valor *elementos = LwArgs(c, n);
    for (uint32_t k = 0; k < n; k++) elementos[k] = por->elemento(c, por->dados, LwValorInt((int64_t)k, TIPO_INT));
    IrInstr *i = LwNovaInstr(c, IR_ARRAY, por->pos);
    i->destino = LwNovoTemp(c);
    i->tipo = por->tipo;
    i->args = elementos;
    i->nargs = n;
    return LwValorTemp(i->destino, i->tipo);
}

/* resultado[indice] = valor, pelo endereco do elemento do local. */
static void GravarElemento(LowerCtx *c, Valor resultado, Valor indice, Valor valor, uint32_t pos)
{
    TipoId elemento = ElementoDe(c, resultado.tipo);
    IrInstr *base = LwNovaInstr(c, IR_UNARIO, pos);
    base->destino = LwNovoTemp(c);
    base->tipo = TipoPonteiro(&c->sem->tipos, resultado.tipo);
    base->op_aux = OP_ENDERECO;
    base->a = resultado;
    IrInstr *alvo = LwNovaInstr(c, IR_ENDERECO_ELEMENTO, pos);
    alvo->destino = LwNovoTemp(c);
    alvo->tipo = TipoPonteiro(&c->sem->tipos, elemento);
    alvo->a = LwValorTemp(base->destino, base->tipo);
    alvo->b = indice;
    alvo->op_aux = 1;
    IrInstr *grava = LwNovaInstr(c, IR_GRAVA_PONTEIRO, pos);
    grava->tipo = elemento;
    grava->a = LwValorTemp(alvo->destino, alvo->tipo);
    grava->b = valor;
    grava->nome = -1;
}

/* N grande: local [N]T zerado e um laco 0..<N com o mesmo grupo de instrucoes, uma vez. */
static Valor EmLaco(LowerCtx *c, const PorElemento *por, uint32_t n)
{
    IrInstr *def = LwNovaInstr(c, IR_LOCAL, por->pos);
    def->destino = LwNovoLocal(c);
    def->tipo = por->tipo;
    def->mutavel = true;
    Valor resultado = LwValorLocal(def->destino, por->tipo);
    int32_t contador = LwNovoLocal(c);
    IrInstr *laco = LwNovaInstr(c, IR_PARA, por->pos);
    laco->destino = contador;
    laco->tipo = TIPO_INT;
    laco->a = LwValorInt(0, TIPO_INT);
    laco->b = LwValorInt((int64_t)n, TIPO_INT);
    laco->faixa_excl = true;
    IrBloco *externo = LwTrocarBloco(c, &laco->corpo);
    Valor indice = LwValorLocal(contador, TIPO_INT);
    GravarElemento(c, resultado, indice, por->elemento(c, por->dados, indice), por->pos);
    LwTrocarBloco(c, externo);
    return resultado;
}

Valor LwPorElemento(LowerCtx *c, const PorElemento *por)
{
    uint32_t n = TipoDe(&c->sem->tipos, por->tipo)->b;
    if (n <= VETOR_DESENROLA_MAX) return Desenrolada(c, por, n);
    return EmLaco(c, por, n);
}

Valor LwContaVetor(LowerCtx *c, const ContaVetor *conta)
{
    PorElemento por = {.tipo = conta->tipo, .pos = conta->pos, .elemento = ElementoDoResultado, .dados = conta};
    return LwPorElemento(c, &por);
}

/* + - * / com resultado [N]T: operandos na ordem do fonte, depois a conta. Matriz x vetor (resultado [R]T) ja saiu em LwBinarioV19. */
bool LwBinarioVetor(LowerCtx *c, const Expr *e, Valor *saida)
{
    if (!EhVetor(c, e->tipo)) return false;
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    ContaVetor conta = {.op = e->op, .tipo = e->tipo, .pos = e->pos_op, .a = a, .b = b};
    *saida = LwContaVetor(c, &conta);
    return true;
}

/* -a com a [N]T. */
bool LwUnarioVetor(LowerCtx *c, const Expr *e, Valor *saida)
{
    if (e->op != OP_NEGAR || !EhVetor(c, e->tipo)) return false;
    ContaVetor conta = {.op = OP_NEGAR, .tipo = e->tipo, .pos = e->pos_op, .a = LwExpr(c, e->a), .b = LwValorNenhum()};
    *saida = LwContaVetor(c, &conta);
    return true;
}

/* v.xy, v.zyx, c.bgra (v1.11 L12b): a base e lida uma vez; cada letra e um IR_ELEMENTO de indice constante (provado: a semantica
 * conferiu cada letra < N, sem R0006), na ordem escrita; o IR_ARRAY junta. ref_id = (k << 8) | posicao da letra j nos bits 2j..2j+1. */
Valor LwSwizzle(LowerCtx *c, const Expr *e)
{
    uint32_t letras = (uint32_t)e->ref_id >> SWIZZLE_DESLOCA_CONTA;
    Valor base = LwBaseIndexada(c, e->a);
    Valor *elementos = LwArgs(c, letras);
    for (uint32_t j = 0; j < letras; j++) {
        IrInstr *i = LwNovaInstr(c, IR_ELEMENTO, e->pos_op);
        i->destino = LwNovoTemp(c);
        i->tipo = ElementoDe(c, e->tipo);
        i->a = base;
        i->b = LwValorInt((e->ref_id >> (SWIZZLE_BITS_POR_LETRA * j)) & SWIZZLE_MASCARA_LETRA, TIPO_INT);
        i->op_aux = 1;
        elementos[j] = LwValorTemp(i->destino, i->tipo);
    }
    IrInstr *junta = LwNovaInstr(c, IR_ARRAY, e->pos_op);
    junta->destino = LwNovoTemp(c);
    junta->tipo = e->tipo;
    junta->args = elementos;
    junta->nargs = letras;
    return LwValorTemp(junta->destino, junta->tipo);
}
