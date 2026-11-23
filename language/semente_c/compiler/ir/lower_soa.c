/* lower_soa.c - lowering de soa[N]S e soa[dynamic]S (Linguagem v1.9): a[i], a[i].campo, &a[i].campo, a.campo (coluna) e for.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.9 P8: IR_SOA_ELEMENTO, IR_SOA_CAMPO, IR_SOA_END_CAMPO, IR_SOA_COLUNA e o for sobre soa (4.5).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

static TipoClasse ClasseDe(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t);
}

/* Instrucao com destino em temporario novo; chamar depois de calcular os operandos. */
static IrInstr *InstrComTemp(LowerCtx *c, IrOp op, uint32_t pos, TipoId tipo)
{
    IrInstr *i = LwNovaInstr(c, op, pos);
    i->destino = LwNovoTemp(c);
    i->tipo = tipo;
    return i;
}

/* A soa de um nome pelo proprio nome; de uma cadeia lvalor (campo, p^) pelo endereco, para nao copiar as colunas nem gravar numa copia. */
static Valor BaseDaSoa(LowerCtx *c, const Expr *base)
{
    if (base->tipo_no != EX_NOME && LwEnderecavel(c, base)) return LwEndereco(c, base);
    return LwBaseIndexada(c, base);
}

/* a[i] lido inteiro: copia o elemento (gather). */
Valor LwIndiceSoa(LowerCtx *c, const Expr *e)
{
    Valor base = BaseDaSoa(c, e->a);
    Valor indice = LwIndiceInt(c, e->b);
    IrInstr *i = InstrComTemp(c, IR_SOA_ELEMENTO, e->pos_op, e->tipo);
    i->a = base;
    i->b = indice;
    return LwValorTemp(i->destino, i->tipo);
}

/* a[i].campo (e->a e EX_INDICE de soa) ou a.campo (e->a e a propria soa: a coluna). */
bool LwCampoDeSoa(const LowerCtx *c, const Expr *e)
{
    if (e->ref != REF_CAMPO) return false;
    if (e->a->tipo_no == EX_INDICE && ClasseDe(c, e->a->a->tipo) == TC_SOA) return true;
    return ClasseDe(c, e->a->tipo) == TC_SOA;
}

/* a[i].campo lido: IR_SOA_CAMPO; a.campo: IR_SOA_COLUNA (fatia []T da coluna, pelo endereco da soa). */
Valor LwCampoSoa(LowerCtx *c, const Expr *e)
{
    if (e->a->tipo_no == EX_INDICE && ClasseDe(c, e->a->a->tipo) == TC_SOA) {
        Valor base = BaseDaSoa(c, e->a->a);
        Valor indice = LwIndiceInt(c, e->a->b);
        IrInstr *i = InstrComTemp(c, IR_SOA_CAMPO, e->pos_op, e->tipo);
        i->a = base;
        i->b = indice;
        i->nome = e->nome;
        return LwValorTemp(i->destino, i->tipo);
    }
    Valor soa = LwEndereco(c, e->a);
    IrInstr *i = InstrComTemp(c, IR_SOA_COLUNA, e->pos_op, e->tipo);
    i->a = soa;
    i->nome = e->nome;
    return LwValorTemp(i->destino, i->tipo);
}

/* &a[i].campo e o ponteiro da gravacao a[i].campo = v. */
Valor LwEnderecoCampoSoa(LowerCtx *c, const Expr *e)
{
    Valor base = BaseDaSoa(c, e->a->a);
    Valor indice = LwIndiceInt(c, e->a->b);
    IrInstr *i = InstrComTemp(c, IR_SOA_END_CAMPO, e->pos_op, TipoPonteiro(&c->sem->tipos, e->tipo));
    i->a = base;
    i->b = indice;
    i->nome = e->nome;
    return LwValorTemp(i->destino, i->tipo);
}

/* O soa[N]S ou soa[dynamic]S de uma soa ou ^soa. */
static TipoId SoaDoTipo(const LowerCtx *c, TipoId t)
{
    return ClasseDe(c, t) == TC_PONTEIRO ? (TipoId)TipoDe(&c->sem->tipos, t)->a : t;
}

/* Base e tamanho do for: pelo endereco quando tem (o laco le a soa viva); o tamanho da dinamica e lido UMA vez. */
static Valor BaseDoForSoa(LowerCtx *c, const Expr *e, Valor *tamanho)
{
    TipoId soa = SoaDoTipo(c, e->tipo);
    Valor base = LwEnderecavel(c, e) ? LwEndereco(c, e) : LwExpr(c, e);
    TipoId colunas = (TipoId)TipoDe(&c->sem->tipos, soa)->a;
    if (ClasseDe(c, colunas) == TC_ARRAY) {
        *tamanho = LwValorInt((int64_t)TipoDe(&c->sem->tipos, colunas)->b, TIPO_INT);
        return base;
    }
    IrInstr *t = InstrComTemp(c, IR_TAMANHO_TEXTO, e->pos_op, TIPO_INT);
    t->a = base;
    *tamanho = LwValorTemp(t->destino, TIPO_INT);
    return base;
}

/* for item in a / for i, item in a: laco 0..<n; a cada volta item = a[contador] (copia, nunca por referencia). */
void LwForSoa(LowerCtx *c, const Stmt *s)
{
    Valor n;
    Valor base = BaseDoForSoa(c, s->expr, &n);
    int32_t contador = s->local_indice >= 0 ? s->local_indice : LwNovoLocal(c);
    IrInstr *laco = LwNovaInstr(c, IR_PARA, s->pos);
    laco->destino = contador;
    laco->tipo = TIPO_INT;
    laco->a = LwValorInt(0, TIPO_INT);
    laco->b = n;
    laco->faixa_excl = true;
    IrBloco *externo = LwTrocarBloco(c, &laco->corpo);
    IrInstr *elemento = InstrComTemp(c, IR_SOA_ELEMENTO, s->pos, s->tipo_local);
    elemento->a = base;
    elemento->b = LwValorLocal(contador, TIPO_INT);
    IrInstr *item = LwNovaInstr(c, IR_LOCAL, s->pos);
    item->destino = s->local_id;
    item->tipo = s->tipo_local;
    item->a = LwValorTemp(elemento->destino, elemento->tipo);
    item->mutavel = false;
    LwBloco(c, s->corpo, true);
    LwTrocarBloco(c, externo);
}
