/* lower_mapa_vistas.c - lowering de keys, values e clear de map (IA1-L9e, K12). Sem IrOp nova: os pares densos do map (IR_MAPA_ITEM, a mesma leitura do `for k, v in m`),
 * IR_LISTA_CRIA, IR_ANEXA, IR_MAPA_REMOVE e IR_TAMANHO_TEXTO.
 *   keys / values   lista nova (IR_LISTA_CRIA, cap = len do map, no alocador do argumento); laco 0..<len acrescenta a chave (op_aux 0) ou o valor (op_aux 1) do par i.
 *                   Copia rasa e na ordem de iteracao (insercao; um remove poe o ultimo par no lugar do removido).
 *   clear(&m)       laco de len voltas que tira o ULTIMO par (indice len - 1 - volta) por IR_MAPA_REMOVE: como o ultimo ocupa o proprio lugar nao ha reordenacao nem
 *                   troca de par, e o map fica com len 0, a capacidade e o alocador de antes (insert funciona depois).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9e: primeira versao.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_lista.h"

#define INT_VALOR(n) LwValorInt((n), TIPO_INT)

/* O map nunca e copiado: ^map entra pelo valor do ponteiro; map enderecavel, pelo endereco (a mesma regra do for). */
static Valor BaseDoMapa(LowerCtx *c, const Expr *e)
{
    if (TipoClasseDe(&c->sem->tipos, e->tipo) == TC_PONTEIRO) return LwExpr(c, e);
    return LwEnderecavel(c, e) ? LwEndereco(c, e) : LwExpr(c, e);
}

/* O map de um map ou ^map. */
static TipoId MapaDoTipo(const LowerCtx *c, TipoId t)
{
    return TipoClasseDe(&c->sem->tipos, t) == TC_PONTEIRO ? (TipoId)TipoDe(&c->sem->tipos, t)->a : t;
}

/* IR_MAPA_ITEM op_aux 0 (chave) ou 1 (valor) do par denso `indice`. */
static Valor ItemDoMapa(const LoCtx *k, uint8_t op_aux, TipoId tipo, Valor base, Valor indice)
{
    IrInstr *i = LwNovaInstr(k->c, IR_MAPA_ITEM, k->pos);
    i->op_aux = op_aux;
    i->tipo = tipo;
    i->a = base;
    i->b = indice;
    i->destino = LwNovoTemp(k->c);
    return LwValorTemp(i->destino, tipo);
}

/* keys(m, alloc = A) e values(m, alloc = A) -> [dynamic]K / [dynamic]V. */
Valor LwVistasDoMapa(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    bool chaves = e->ref_id == EMB_KEYS;
    Valor base = BaseDoMapa(c, LlArgumento(e, 0));
    Valor n = LlTamanho(&k, base);
    TipoId elemento = (TipoId)TipoDe(&c->sem->tipos, e->tipo)->a;
    IrInstr *cria = LwNovaInstr(c, IR_LISTA_CRIA, e->pos_op);
    cria->destino = LwNovoTemp(c);
    cria->tipo = e->tipo;
    cria->a = LwAlocador(c, e);
    cria->b = n;
    int32_t lista = LoLocal(&k, e->tipo, LwValorTemp(cria->destino, e->tipo));
    IrInstr *endereco = LwNovaInstr(c, IR_UNARIO, e->pos_op);
    endereco->destino = LwNovoTemp(c);
    endereco->tipo = TipoPonteiro(&c->sem->tipos, e->tipo);
    endereco->op_aux = OP_ENDERECO;
    endereco->a = LwValorLocal(lista, e->tipo);
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    Valor item = ItemDoMapa(&k, chaves ? 0 : 1, elemento, base, volta);
    IrInstr *anexa = LwNovaInstr(c, IR_ANEXA, e->pos_op);
    anexa->tipo = elemento;
    anexa->a = LwValorTemp(endereco->destino, endereco->tipo);
    anexa->b = item;
    LlSaiDoLaco(&k, externo);
    return LwValorLocal(lista, e->tipo);
}

/* clear(&m): tira o ultimo par len vezes. */
Valor LwClearMapa(LowerCtx *c, const Expr *e)
{
    LoCtx k = {c, e->pos_op};
    IrBloco *externo;
    Valor mapa = BaseDoMapa(c, LlArgumento(e, 0));
    const TipoInfo *tipo_mapa = TipoDe(&c->sem->tipos, MapaDoTipo(c, LlArgumento(e, 0)->tipo));
    Valor n = LlTamanho(&k, mapa);
    Valor volta = LlEntraLaco(&k, INT_VALOR(0), n, &externo);
    Valor ultimo = LlMenos(&k, LlMenos(&k, n, INT_VALOR(1)), volta);
    Valor chave = ItemDoMapa(&k, 0, (TipoId)tipo_mapa->a, mapa, ultimo);
    IrInstr *tira = LwNovaInstr(c, IR_MAPA_REMOVE, e->pos_op);
    tira->tipo = (TipoId)tipo_mapa->b;
    tira->a = mapa;
    tira->b = chave;
    LlSaiDoLaco(&k, externo);
    return LwValorNenhum();
}
