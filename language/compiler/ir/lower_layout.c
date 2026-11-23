/* lower_layout.c - size_of, align_of, offset_of e static_assert no lowering (Linguagem v1.11 L9, O10).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.11 L9: primeira versao. O layout dos tipos (LayoutCalcular) sai UMA vez, depois das structs e antes das constantes
 *                    e dos corpos, so se a semantica viu um dos quatro (Semantica.usa_layout); size_of/align_of/offset_of viram literal int;
 *                    static_assert falso e E0329 com a msg na causa, na posicao do static_assert.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

void LwLayoutCalcular(LowerCtx *c)
{
    if (!c->sem->usa_layout) return;
    LayoutCalcular(&c->sem->tipos, c->prog, c->arena, &c->layouts);
}

/* size_of(T) = tamanho, align_of(T) = alinhamento, offset_of(Struct.campo) = deslocamento do campo (o da struct em C).
 * false com E0328: o tipo nao tem forma em C (a semantica ja recusa os que conhece; este e o fecho). */
bool LwLayoutValor(LowerCtx *c, const Expr *e, int64_t *valor)
{
    const Expr *no = e->args[0].valor;
    if (e->ref_id == EMB_OFFSET_OF) {
        *valor = (int64_t)LayoutOffsetCampo(&c->layouts, no->a->tipo, (uint32_t)no->ref_id);
        return true;
    }
    const LayoutTipo *lt = &c->layouts.tipos[no->tipo];
    if (lt->tam == 0) {
        if (!c->sem->diag->tem) DiagRelatar(c->sem->diag, E0328, no->pos, "o tipo nao tem forma em C e nao tem tamanho");
        return false;
    }
    *valor = e->ref_id == EMB_SIZE_OF ? (int64_t)lt->tam : (int64_t)lt->ali;
    return true;
}

/* Texto do msg = "..." do static_assert (parametro de indice 1); NULL sem msg ou sem texto constante. */
static const char *MensagemDoAssert(LowerCtx *c, const Expr *e)
{
    Valor v;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].indice != 1) continue;
        if (LwDobrar(c, e->args[i].valor, &v) && v.tipo_no == V_TEXTO) return InternerTexto(c->sem->nomes, v.id);
    }
    return NULL;
}

/* static_assert(cond, msg = "..."): dobra a condicao; falsa ou nao constante para a compilacao (E0329). */
void LwStaticAssert(LowerCtx *c, const Expr *e)
{
    Valor cond;
    const Expr *condicao = e->args[0].valor;
    if (!LwDobrar(c, condicao, &cond)) {
        if (!c->sem->diag->tem) DiagRelatar(c->sem->diag, E0329, condicao->pos, "a condicao do static_assert nao e constante");
        return;
    }
    if (cond.inteiro != 0) return;
    const char *mensagem = MensagemDoAssert(c, e);
    DiagRelatar(c->sem->diag, E0329, e->pos, "static_assert falhou: %s", mensagem != NULL ? mensagem : "a condicao e falsa");
}

/* static_assert de topo (D_ASSERT), na ordem do fonte, depois das constantes. */
void LwAssertsDeTopo(LowerCtx *c)
{
    for (const Decl *d = c->sem->programa->primeira; d != NULL && !c->sem->diag->tem; d = d->prox) {
        if (d->tipo_no == D_ASSERT) LwStaticAssert(c, d->init);
    }
}

/* Chamada de embutido de layout (despachada por LwChamada): static_assert nao tem valor; os outros tres viram um int literal. */
Valor LwLayoutChamada(LowerCtx *c, const Expr *e)
{
    int64_t valor = 0;
    if (e->ref_id == EMB_STATIC_ASSERT) {
        LwStaticAssert(c, e);
        return LwValorNenhum();
    }
    LwLayoutValor(c, e, &valor);
    return LwValorInt(valor, TIPO_INT);
}
