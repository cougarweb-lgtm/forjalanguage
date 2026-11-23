/* lower_mapa.c - lowering de map[K]V (Linguagem v1.9): insert, get, get_ptr, has, remove e o for k, v in m.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.9 P11: IR_MAPA_INSERE, IR_MAPA_ACHA, IR_MAPA_PONTEIRO, IR_MAPA_TEM, IR_MAPA_REMOVE e IR_MAPA_ITEM (4.7).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

static TipoClasse ClasseDe(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t);
}

/* O argumento ligado ao parametro 'indice' (a ordem no vetor e a do fonte). */
static const Expr *ArgumentoDe(const Expr *e, int32_t indice)
{
    for (uint32_t k = 0; k < e->nargs; k++) {
        if (e->args[k].indice == indice) return e->args[k].valor;
    }
    return NULL;
}

/* O map de um map ou ^map. */
static TipoId MapaDoTipo(const LowerCtx *c, TipoId t)
{
    return ClasseDe(c, t) == TC_PONTEIRO ? (TipoId)TipoDe(&c->sem->tipos, t)->a : t;
}

/* Instrucao do map (m, chave ja calculados); destino em temporario novo quando o resultado existe. */
static Valor InstrucaoDoMapa(LowerCtx *c, IrOp op, const Expr *e, TipoId tipo, Valor m, Valor chave)
{
    IrInstr *i = LwNovaInstr(c, op, e->pos_op);
    i->tipo = tipo;
    i->a = m;
    i->b = chave;
    if (op == IR_MAPA_INSERE || op == IR_MAPA_REMOVE) return LwValorNenhum();
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, tipo);
}

/* insert(&m, key = k, value = v): m, k, v nessa ordem; o valor vai em args[0]. */
static Valor InsereNoMapa(LowerCtx *c, const Expr *e, Valor m, Valor chave)
{
    Valor valor = LwExpr(c, ArgumentoDe(e, 2));
    IrInstr *i = LwNovaInstr(c, IR_MAPA_INSERE, e->pos_op);
    i->tipo = (TipoId)TipoDe(&c->sem->tipos, MapaDoTipo(c, ArgumentoDe(e, 0)->tipo))->b;
    i->a = m;
    i->b = chave;
    i->args = LwArgs(c, 1);
    i->args[0] = valor;
    i->nargs = 1;
    return LwValorNenhum();
}

/* insert, get, get_ptr, has e remove sobre map; argumentos na ordem dos parametros (1o, key, value). */
Valor LwEmbutidoMapa(LowerCtx *c, const Expr *e)
{
    const Expr *colecao = ArgumentoDe(e, 0);
    Valor m = LwExpr(c, colecao);
    Valor chave = LwExpr(c, ArgumentoDe(e, 1));
    TipoId valor = (TipoId)TipoDe(&c->sem->tipos, MapaDoTipo(c, colecao->tipo))->b;
    switch (e->ref_id) {
    case EMB_INSERT: return InsereNoMapa(c, e, m, chave);
    case EMB_GET: return InstrucaoDoMapa(c, IR_MAPA_ACHA, e, e->tipo, m, chave);
    case EMB_GET_PTR: return InstrucaoDoMapa(c, IR_MAPA_PONTEIRO, e, e->tipo, m, chave);
    case EMB_HAS: return InstrucaoDoMapa(c, IR_MAPA_TEM, e, TIPO_BOOL, m, chave);
    case EMB_REMOVE: return InstrucaoDoMapa(c, IR_MAPA_REMOVE, e, valor, m, chave);
    default: FalhaInterna("lowering: embutido %d invalido para map (offset %u)", e->ref_id, (unsigned)e->pos_op);
    }
}

/* IR_MAPA_ITEM op_aux 0 (chave), 1 (valor) ou 2 (endereco do valor) do par denso 'contador'. */
static Valor ItemDoMapa(LowerCtx *c, uint8_t op_aux, uint32_t pos, TipoId tipo, Valor base, int32_t contador)
{
    IrInstr *i = LwNovaInstr(c, IR_MAPA_ITEM, pos);
    i->op_aux = op_aux;
    i->tipo = tipo;
    i->a = base;
    i->b = LwValorLocal(contador, TIPO_INT);
    i->destino = LwNovoTemp(c);
    return LwValorTemp(i->destino, tipo);
}

/* IR_LOCAL imutavel do prologo do corpo. */
static void LocalDoFor(LowerCtx *c, uint32_t pos, int32_t destino, TipoId tipo, Valor inicial)
{
    IrInstr *l = LwNovaInstr(c, IR_LOCAL, pos);
    l->destino = destino;
    l->tipo = tipo;
    l->a = inicial;
    l->mutavel = false;
}

/* O map percorrido nunca e copiado: ^map entra pelo valor do ponteiro; map enderecavel, pelo endereco. */
static Valor BaseDoForMapa(LowerCtx *c, const Expr *e)
{
    if (ClasseDe(c, e->tipo) == TC_PONTEIRO) return LwExpr(c, e);
    return LwEnderecavel(c, e) ? LwEndereco(c, e) : LwExpr(c, e);
}

/* for k, v in m / for k, &v in m: n lido UMA vez antes do laco; a cada volta k = chave[i], v = valor[i] (ou &valor[i]). */
void LwForMapa(LowerCtx *c, const Stmt *s)
{
    Valor base = BaseDoForMapa(c, s->expr);
    IrInstr *tamanho = LwNovaInstr(c, IR_TAMANHO_TEXTO, s->pos);
    tamanho->destino = LwNovoTemp(c);
    tamanho->tipo = TIPO_INT;
    tamanho->a = base;
    int32_t contador = LwNovoLocal(c);
    IrInstr *laco = LwNovaInstr(c, IR_PARA, s->pos);
    laco->destino = contador;
    laco->tipo = TIPO_INT;
    laco->a = LwValorInt(0, TIPO_INT);
    laco->b = LwValorTemp(tamanho->destino, TIPO_INT);
    laco->faixa_excl = true;
    IrBloco *externo = LwTrocarBloco(c, &laco->corpo);
    TipoId chave = (TipoId)TipoDe(&c->sem->tipos, MapaDoTipo(c, s->expr->tipo))->a;
    LocalDoFor(c, s->pos, s->local_indice, chave, ItemDoMapa(c, 0, s->pos, chave, base, contador));
    Valor item = ItemDoMapa(c, s->item_ref ? 2 : 1, s->pos, s->tipo_local, base, contador);
    LocalDoFor(c, s->pos, s->local_id, s->tipo_local, item);
    LwBloco(c, s->corpo, true);
    LwTrocarBloco(c, externo);
}
