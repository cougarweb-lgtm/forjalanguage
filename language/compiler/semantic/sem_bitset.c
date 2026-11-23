/* sem_bitset.c - bit_set[E] (Linguagem v1.9): tipo, literal, | & - == !=, insert/remove/has, for e len (len em sem_call.c).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.9 P3: ResolverBitSet, SemLiteralBitSet, SemContaBitSet, SemComparacaoBitSet, SemChamarBitSet e SemForBitSet (E0319).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <stdio.h>

enum { BIT_SET_MEMBROS_MAX = 64 };

/* bit_set[E]: E precisa ser enum sem valor explicito (o bit de cada membro e a posicao dele) com ate 64 membros. */
bool ResolverBitSet(Semantica *s, TipoRef *t, TipoId *base)
{
    char tipo[96];
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    TipoId alvo = t->alvo->tipo;
    if (TipoClasseDe(&s->tipos, alvo) != TC_ENUM) {
        SemDescrever(s, alvo, tipo, sizeof tipo);
        return SemErro(s, E0319, t->alvo->pos, "bit_set[E] pede um enum (recebeu %s)", tipo);
    }
    const Decl *d = s->globais[TipoDe(&s->tipos, alvo)->a].decl;
    if (d->op == 1) {
        return SemErro(s, E0319, t->alvo->pos,
                       "bit_set de enum com valor explicito nao existe: o bit de cada membro e a posicao dele (tire os '= valor')");
    }
    if (d->nmembros > BIT_SET_MEMBROS_MAX) {
        return SemErro(s, E0319, t->alvo->pos, "bit_set de enum com %u membros: o maximo e 64", (unsigned)d->nmembros);
    }
    *base = TipoBitSet(&s->tipos, alvo, d->nmembros);
    return true;
}

/* {Cor.a, Cor.b} e {} com o tipo bit_set vindo do contexto; membro repetido vale (o bit ja esta ligado). */
bool SemLiteralBitSet(Semantica *s, Expr *e, TipoId ctx)
{
    TipoId enumerado = (TipoId)TipoDe(&s->tipos, ctx)->a;
    for (uint32_t i = 0; i < e->nargs; i++) {
        Expr *v = e->args[i].valor;
        if (!SemChecar(s, v, enumerado) || !SemCoagir(s, v, enumerado, v->pos)) return false;
    }
    e->tipo = ctx;
    return true;
}

/* a | b, a & b e a - b entre bit_set do mesmo enum; qualquer outra conta e recusada. */
bool SemContaBitSet(Semantica *s, Expr *e)
{
    TipoId ta = e->a->tipo, tb = e->b->tipo;
    char texto_a[96], texto_b[96];
    if (TipoClasseDe(&s->tipos, ta) != TC_BIT_SET || TipoClasseDe(&s->tipos, tb) != TC_BIT_SET) {
        return SemErro(s, E0319, e->pos_op, "bit_set so opera com bit_set do mesmo enum");
    }
    if (ta != tb) {
        SemDescrever(s, ta, texto_a, sizeof texto_a);
        SemDescrever(s, tb, texto_b, sizeof texto_b);
        return SemErro(s, E0319, e->pos_op, "operandos de bit_set de enums diferentes (%s e %s)", texto_a, texto_b);
    }
    if (e->op != OP_BIT_OU && e->op != OP_BIT_E && e->op != OP_SUB) {
        return SemErro(s, E0319, e->pos_op, "bit_set tem | (uniao), & (intersecao) e - (diferenca)");
    }
    e->tipo = ta;
    return true;
}

/* == e != entre bit_set do mesmo enum; ordem e outros tipos sao recusados. */
bool SemComparacaoBitSet(Semantica *s, Expr *e)
{
    if (e->a->tipo != e->b->tipo) return SemErro(s, E0319, e->pos_op, "bit_set so se compara com bit_set do mesmo enum");
    if (e->op == OP_IGUAL || e->op == OP_DIFERENTE) {
        e->tipo = TIPO_BOOL;
        return true;
    }
    return SemErro(s, E0319, e->pos_op, "bit_set nao tem ordem: compare com == ou !=");
}

/* insert/remove pedem ^bit_set (e var quando e '&x'); insert sem value. */
static bool ConferirAlteracao(Semantica *s, Expr *e, const char *nome, Expr *colecao)
{
    char motivo[64];
    if (TipoClasseDe(&s->tipos, colecao->tipo) != TC_PONTEIRO) {
        return SemErro(s, E0319, colecao->pos, "%s recebe o endereco do bit_set: %s(&s, key = Cor.x)", nome, nome);
    }
    if (colecao->tipo_no == EX_UNARIO && colecao->op == OP_ENDERECO) {
        snprintf(motivo, sizeof motivo, "%s altera o bit_set: declare com var", nome);
        if (!SemLvalorMutavelDe(s, colecao->a, motivo)) return false;
    }
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].indice == 2) {
            return SemErro(s, E0319, e->args[i].valor->pos, "bit_set guarda so a chave: insert(&s, key = Cor.x)");
        }
    }
    return true;
}

/* insert(&s, key = m), remove(&s, key = m) e has(s, key = m); get e get_ptr nao existem em bit_set. */
bool SemChamarBitSet(Semantica *s, Expr *e, int32_t embutido, Expr *colecao)
{
    const char *nome = SemNomeTexto(s, e->nome);
    bool por_ponteiro = TipoClasseDe(&s->tipos, colecao->tipo) == TC_PONTEIRO;
    if (embutido == EMB_GET || embutido == EMB_GET_PTR) {
        return SemErro(s, E0319, colecao->pos, "bit_set nao tem get: use has(s, key = Cor.x)");
    }
    if (embutido == EMB_HAS && por_ponteiro) return SemErro(s, E0319, colecao->pos, "has recebe o bit_set: has(s, key = Cor.x)");
    if (embutido != EMB_HAS && !ConferirAlteracao(s, e, nome, colecao)) return false;
    TipoId bit_set = por_ponteiro ? (TipoId)TipoDe(&s->tipos, colecao->tipo)->a : colecao->tipo;
    TipoId enumerado = (TipoId)TipoDe(&s->tipos, bit_set)->a;
    for (uint32_t i = 0; i < e->nargs; i++) {
        Expr *chave = e->args[i].valor;
        if (e->args[i].indice != 1) continue;
        if (!SemChecar(s, chave, enumerado) || !SemCoagir(s, chave, enumerado, chave->pos)) return false;
    }
    e->tipo = embutido == EMB_HAS ? TIPO_BOOL : TIPO_VAZIO;
    return true;
}

/* for item in s: percorre os membros na ordem do enum; sem &item, sem indice e sem ^. */
bool SemForBitSet(Semantica *s, Stmt *st)
{
    uint32_t pos = SemPosDepoisDaPalavra(s, st->pos);
    if (st->item_ref) return SemErro(s, E0319, pos, "for &item nao existe em bit_set: o membro e um valor");
    if (st->nome_indice >= 0) return SemErro(s, E0319, pos, "for de bit_set nao tem indice: for item in s");
    if (TipoClasseDe(&s->tipos, st->expr->tipo) == TC_PONTEIRO) {
        return SemErro(s, E0319, st->expr->pos, "for percorre o bit_set: for item in s (sem ^)");
    }
    uint32_t altura = EscopoAltura(&s->escopo);
    st->tipo_local = (TipoId)TipoDe(&s->tipos, st->expr->tipo)->a;
    NovoLocal item = {st->nome, pos, st->tipo_local, LOC_FOR, 0};
    bool ok = SemDeclararLocal(s, &item, &st->local_id) && CorpoDeLaco(s, st->corpo);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}
