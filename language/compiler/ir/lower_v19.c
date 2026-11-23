/* lower_v19.c - lowering dos tipos da Linguagem v1.9 (hoje so bit_set): literal, diferenca, insert/remove/has, len e for.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.9 P11: map em LwEmbutidoV19 (LwEmbutidoMapa), len de map (IR_TAMANHO_TEXTO) e for de map (LwForMapa); saem os provisorios.
 *                    Linguagem v1.9 P8: literal {} de soa, len de soa (TamanhoDaSoa) e for sobre soa (LwForSoa, lower_soa.c).
 *                    Linguagem v1.9 P6: matrix em IR_MATRIZ / IR_MATRIZ_ELEMENTO / IR_MATRIZ_END_ELEMENTO (literal, contas, -m, transpose, m[i, j], &m[i, j]).
 *                    Linguagem v1.9 P4: bit_set em IR_CONJUNTO (literal, a - b, insert, remove, has, len, for item in s).
 *                    soa (P8) e map (P11) acrescentam os ramos deles.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

static TipoClasse ClasseDe(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t);
}

/* Instrucao IR_CONJUNTO; destino em temporario novo (ou -1 com TIPO_VAZIO). Chamar depois de calcular os operandos. */
static Valor Conjunto(LowerCtx *c, uint8_t op_aux, uint32_t pos, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(c, IR_CONJUNTO, pos);
    i->op_aux = op_aux;
    i->tipo = tipo;
    i->a = a;
    i->b = b;
    if (tipo == TIPO_VAZIO) return LwValorNenhum();
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, tipo);
}

/* Instrucao IR_MATRIZ; destino em temporario novo. Chamar depois de calcular os operandos. */
static Valor Matriz(LowerCtx *c, uint8_t op_aux, uint32_t pos, TipoId tipo, Valor a, Valor b)
{
    IrInstr *i = LwNovaInstr(c, IR_MATRIZ, pos);
    i->op_aux = op_aux;
    i->tipo = tipo;
    i->a = a;
    i->b = b;
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, tipo);
}

/* O argumento do parametro 'indice' (gravado pela semantica em Arg.indice), ou NULL. */
static const Expr *ArgumentoDe(const Expr *e, int32_t indice)
{
    for (uint32_t k = 0; k < e->nargs; k++) {
        if (e->args[k].indice == indice) return e->args[k].valor;
    }
    return NULL;
}

/* Classe do tipo, ou do apontado quando e ponteiro. */
static TipoClasse ClasseSemPonteiro(const LowerCtx *c, TipoId t)
{
    if (ClasseDe(c, t) == TC_PONTEIRO) t = TipoDe(&c->sem->tipos, t)->a;
    return ClasseDe(c, t);
}

/* bit_set ou ^bit_set. */
static bool EhBitSet(const LowerCtx *c, TipoId t)
{
    return ClasseSemPonteiro(c, t) == TC_BIT_SET;
}

/* map ou ^map. */
static bool EhMapa(const LowerCtx *c, TipoId t)
{
    return ClasseSemPonteiro(c, t) == TC_MAPA;
}

bool LwEhV19(const LowerCtx *c, TipoId t)
{
    if (ClasseDe(c, t) == TC_PONTEIRO) t = TipoDe(&c->sem->tipos, t)->a;
    return ClasseDe(c, t) >= TC_MAPA;
}

/* matrix {..}: nargs == 0 = zero; senao IR_MATRIZ op 8 com os R*C elementos na ordem do fonte (por linhas). */
static Valor LiteralMatriz(LowerCtx *c, const Expr *e)
{
    if (e->nargs == 0) return LwZeroComposto(c, e->tipo, e->pos_op);
    Valor *elementos = LwArgs(c, e->nargs);
    for (uint32_t k = 0; k < e->nargs; k++) elementos[k] = LwExpr(c, e->args[k].valor);
    IrInstr *i = LwNovaInstr(c, IR_MATRIZ, e->pos_op);
    i->op_aux = 8;
    i->tipo = e->tipo;
    i->args = elementos;
    i->nargs = e->nargs;
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, e->tipo);
}

/* {A, B, x}: as constantes viram uma mascara; cada variavel soma o seu bit (op 7), na ordem do fonte. */
Valor LwLiteralV19(LowerCtx *c, const Expr *e)
{
    if (ClasseDe(c, e->tipo) == TC_MATRIZ) return LiteralMatriz(c, e);
    if (ClasseDe(c, e->tipo) == TC_SOA) return LwZeroComposto(c, e->tipo, e->pos_op); /* soa[N]S: so {} (a semantica recusa o resto) */
    if (ClasseDe(c, e->tipo) != TC_BIT_SET)
        FalhaInterna("lowering: literal de tipo %u sem lowering (offset %u)", (unsigned)e->tipo, (unsigned)e->pos_op);
    uint64_t mascara = 0;
    Valor *variaveis = LwArgs(c, e->nargs);
    uint32_t qtd = 0;
    for (uint32_t k = 0; k < e->nargs; k++) {
        Valor v = LwExpr(c, e->args[k].valor);
        if (v.tipo_no == V_INT) mascara |= UINT64_C(1) << v.inteiro;
        else variaveis[qtd++] = v;
    }
    Valor resultado = LwValorInt((int64_t)mascara, e->tipo);
    for (uint32_t k = 0; k < qtd; k++) resultado = Conjunto(c, 7, e->pos_op, e->tipo, resultado, variaveis[k]);
    return resultado;
}

/* Conta com algum operando matrix: + e - (0, 1), * matriz x matriz (2), x escalar (3), escalar x matriz (4), x vetor (5). */
static Valor BinarioMatriz(LowerCtx *c, const Expr *e, TipoClasse ca, TipoClasse cb)
{
    uint8_t op_aux = e->op == OP_SOMA ? 0 : 1;
    if (e->op == OP_MUL) {
        if (ca == TC_MATRIZ && cb == TC_MATRIZ) op_aux = 2;
        else if (ca != TC_MATRIZ) op_aux = 4;
        else op_aux = cb == TC_ARRAY ? 5 : 3;
    }
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    return Matriz(c, op_aux, e->pos_op, e->tipo, a, b);
}

/* a - b de bit_set e toda conta com operando matrix; | & == != de bit_set seguem o caminho de IR_BINARIO. */
bool LwBinarioV19(LowerCtx *c, const Expr *e, Valor *saida)
{
    TipoClasse ca = ClasseDe(c, e->a->tipo), cb = ClasseDe(c, e->b->tipo);
    if (ca == TC_MATRIZ || cb == TC_MATRIZ) {
        *saida = BinarioMatriz(c, e, ca, cb);
        return true;
    }
    if (e->op != OP_SUB || ClasseDe(c, e->tipo) != TC_BIT_SET) return false;
    Valor a = LwExpr(c, e->a);
    Valor b = LwExpr(c, e->b);
    *saida = Conjunto(c, 2, e->pos_op, e->tipo, a, b);
    return true;
}

/* -m: IR_MATRIZ op 6. */
Valor LwMatrizUnaria(LowerCtx *c, const Expr *e, uint8_t op_aux)
{
    Valor a = LwExpr(c, e->a);
    return Matriz(c, op_aux, e->pos, e->tipo, a, LwValorNenhum());
}

/* m[i, j]: base, i, j (a ordem do fonte); a matriz sai por valor (ou ^matriz). */
Valor LwIndiceMatriz(LowerCtx *c, const Expr *e)
{
    Valor base = LwBaseIndexada(c, e->a);
    Valor linha = LwIndiceInt(c, e->b);
    Valor coluna = LwIndiceInt(c, e->args[0].valor);
    IrInstr *i = LwNovaInstr(c, IR_MATRIZ_ELEMENTO, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = e->tipo;
    i->a = base;
    i->b = linha;
    i->args = LwArgs(c, 1);
    i->args[0] = coluna;
    i->nargs = 1;
    return LwValorTemp(i->destino, i->tipo);
}

/* &m[i, j] e o alvo de m[i, j] = v: a base de campo ou de ^p precisa ser o proprio objeto (endereco), nao uma copia. */
Valor LwEnderecoMatriz(LowerCtx *c, const Expr *e)
{
    bool pelo_endereco = e->a->tipo_no != EX_NOME && ClasseDe(c, e->a->tipo) == TC_MATRIZ && LwEnderecavel(c, e->a);
    Valor base = pelo_endereco ? LwEndereco(c, e->a) : LwBaseIndexada(c, e->a);
    Valor linha = LwIndiceInt(c, e->b);
    Valor coluna = LwIndiceInt(c, e->args[0].valor);
    IrInstr *i = LwNovaInstr(c, IR_MATRIZ_END_ELEMENTO, e->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = TipoPonteiro(&c->sem->tipos, e->tipo);
    i->a = base;
    i->b = linha;
    i->args = LwArgs(c, 1);
    i->args[0] = coluna;
    i->nargs = 1;
    return LwValorTemp(i->destino, i->tipo);
}

/* insert(&s, key = m), remove(&s, key = m) e has(s, key = m); argumentos na ordem dos parametros. transpose(m): IR_MATRIZ op 7. */
Valor LwEmbutidoV19(LowerCtx *c, const Expr *e)
{
    if (e->ref_id == EMB_TRANSPOSE) {
        Valor origem = LwExpr(c, ArgumentoDe(e, 0));
        return Matriz(c, 7, e->pos_op, e->tipo, origem, LwValorNenhum());
    }
    const Expr *colecao = ArgumentoDe(e, 0);
    const Expr *chave = ArgumentoDe(e, 1);
    if (colecao != NULL && chave != NULL && EhMapa(c, colecao->tipo)) return LwEmbutidoMapa(c, e);
    if (colecao == NULL || chave == NULL || e->ref_id == EMB_GET || e->ref_id == EMB_GET_PTR || e->ref_id == EMB_TRANSPOSE ||
        !EhBitSet(c, colecao->tipo))
        FalhaInterna("lowering: embutido %d invalido para a colecao (offset %u)", e->ref_id, (unsigned)e->pos_op);
    Valor s = LwExpr(c, colecao);
    Valor m = LwExpr(c, chave);
    if (e->ref_id == EMB_HAS) return Conjunto(c, 3, e->pos_op, TIPO_BOOL, s, m);
    return Conjunto(c, e->ref_id == EMB_INSERT ? 4 : 5, e->pos_op, TIPO_VAZIO, s, m);
}

/* len de soa: soa[N]S e N (o argumento nao e avaliado); soa dinamica le o campo len (IR_TAMANHO_TEXTO). */
static Valor TamanhoDaSoa(LowerCtx *c, const Expr *arg, TipoId soa)
{
    TipoId colunas = (TipoId)TipoDe(&c->sem->tipos, soa)->a;
    if (ClasseDe(c, colunas) == TC_ARRAY) return LwValorInt((int64_t)TipoDe(&c->sem->tipos, colunas)->b, TIPO_INT);
    Valor origem = LwExpr(c, arg);
    IrInstr *i = LwNovaInstr(c, IR_TAMANHO_TEXTO, arg->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_INT;
    i->a = origem;
    return LwValorTemp(i->destino, TIPO_INT);
}

/* len de map (ou ^map): o campo len (IR_TAMANHO_TEXTO; o emissor escreve ".len" ou "->len" pelo TC_PONTEIRO). */
static Valor TamanhoDoMapa(LowerCtx *c, const Expr *arg)
{
    Valor origem = LwExpr(c, arg);
    IrInstr *i = LwNovaInstr(c, IR_TAMANHO_TEXTO, arg->pos_op);
    i->destino = LwNovoTemp(c);
    i->tipo = TIPO_INT;
    i->a = origem;
    return LwValorTemp(i->destino, TIPO_INT);
}

/* len(s): quantos membros. */
Valor LwTamanhoV19(LowerCtx *c, const Expr *arg)
{
    TipoId t = arg->tipo;
    if (ClasseDe(c, t) == TC_PONTEIRO) t = TipoDe(&c->sem->tipos, t)->a;
    if (ClasseDe(c, t) == TC_SOA) return TamanhoDaSoa(c, arg, t);
    if (ClasseDe(c, t) == TC_MAPA) return TamanhoDoMapa(c, arg);
    if (ClasseDe(c, arg->tipo) != TC_BIT_SET)
        FalhaInterna("lowering: len de tipo %u sem lowering (offset %u)", (unsigned)arg->tipo, (unsigned)arg->pos_op);
    return Conjunto(c, 6, arg->pos_op, TIPO_INT, LwExpr(c, arg), LwValorNenhum());
}

/* for item in s: sobre uma foto tirada na entrada; contador 0..<N e, a cada volta, tem(foto, contador) -> item = membro. */
void LwStmtForV19(LowerCtx *c, const Stmt *s)
{
    TipoId colecao = s->expr->tipo;
    if (ClasseDe(c, colecao) == TC_SOA) {
        LwForSoa(c, s);
        return;
    }
    if (ClasseSemPonteiro(c, colecao) == TC_MAPA) {
        LwForMapa(c, s);
        return;
    }
    if (ClasseDe(c, colecao) != TC_BIT_SET)
        LwFalhaStmt(c, s, "for de colecao v1.9 sem lowering");
    TipoId conjunto = s->expr->tipo;
    Valor base = LwExpr(c, s->expr);
    IrInstr *foto = LwNovaInstr(c, IR_LOCAL, s->pos);
    foto->destino = LwNovoLocal(c);
    foto->tipo = conjunto;
    foto->a = base;
    foto->mutavel = false;
    int32_t contador = LwNovoLocal(c);
    IrInstr *laco = LwNovaInstr(c, IR_PARA, s->pos);
    laco->destino = contador;
    laco->tipo = TIPO_INT;
    laco->a = LwValorInt(0, TIPO_INT);
    laco->b = LwValorInt((int64_t)TipoDe(&c->sem->tipos, conjunto)->b, TIPO_INT);
    laco->faixa_excl = true;
    IrBloco *externo = LwTrocarBloco(c, &laco->corpo);
    IrInstr *se = LwNovaInstr(c, IR_SE, s->pos);
    LwTrocarBloco(c, &se->cond);
    se->a = Conjunto(c, 3, s->pos, TIPO_BOOL, LwValorLocal(foto->destino, conjunto), LwValorLocal(contador, TIPO_INT));
    LwTrocarBloco(c, &se->corpo);
    Valor membro = Conjunto(c, 8, s->pos, s->tipo_local, LwValorNenhum(), LwValorLocal(contador, TIPO_INT));
    IrInstr *item = LwNovaInstr(c, IR_LOCAL, s->pos);
    item->destino = s->local_id;
    item->tipo = s->tipo_local;
    item->a = membro;
    item->mutavel = false;
    LwBloco(c, s->corpo, true);
    LwTrocarBloco(c, externo);
}
