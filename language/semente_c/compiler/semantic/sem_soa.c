/* sem_soa.c - soa[N]S e soa[dynamic]S (Linguagem v1.9): tipo, literal, a[i], a.campo (coluna) e for (E0320).
 * Versao: 0.1.2
 * Changelog: 0.1.2 - IA1-b07: o E0301 de indice nao inteiro leva a forma da ocorrencia (SemErroDoInteiro).
 *            0.1.1 - Linguagem v1.11 L5: E0201 de campo da soa com candidatos "parecido" (sem_parecido.c).
 *            0.1.0 - Linguagem v1.9 P7: ResolverSoa, SemLiteralSoa, SemIndiceSoa, SemColunaDaSoa e SemForSoa (3.7).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* Struct do elemento da soa (a soa guarda [N]S ou [dynamic]S em .a, e o array/lista guarda S em .a). */
static TipoId ElementoDaSoa(const Semantica *s, TipoId soa)
{
    return (TipoId)TipoDe(&s->tipos, (TipoId)TipoDe(&s->tipos, soa)->a)->a;
}

/* S precisa ser struct da Forja, nao opaca, com ao menos 1 campo e sem campo dono. */
static bool ConferirStructDaSoa(Semantica *s, const TipoRef *ref, TipoId elemento)
{
    char tipo[96];
    if (TipoClasseDe(&s->tipos, elemento) != TC_STRUCT) {
        SemDescrever(s, elemento, tipo, sizeof tipo);
        return SemErro(s, E0320, ref->pos, "soa guarda struct da Forja (recebeu %s)", tipo);
    }
    const Decl *d = s->globais[TipoDe(&s->tipos, elemento)->a].decl;
    if (d->ncampos == 0 || SemTipoEhOpaco(s, elemento)) {
        return SemErro(s, E0320, ref->pos, "soa de '%s' nao existe: a struct precisa de ao menos 1 campo visivel", SemNomeTexto(s, d->nome));
    }
    if (d->contem_dono) {
        return SemErro(s, E0320, ref->pos,
                       "soa de '%s' nao existe: a struct tem campo dono de memoria (lista, map ou soa dinamica)", SemNomeTexto(s, d->nome));
    }
    return true;
}

/* soa[N]S e soa[dynamic]S; soa[]S nao existe. */
bool ResolverSoa(Semantica *s, TipoRef *t, TipoId *base)
{
    if (t->alvo->forma == TR_FATIA) {
        return SemErro(s, E0320, t->alvo->pos, "soa[]S nao existe: use soa[N]S ou soa[dynamic]S");
    }
    if (!SemResolverTipoRef(s, t->alvo)) return false;
    TipoId colunas = t->alvo->tipo;
    if (!ConferirStructDaSoa(s, t->alvo->alvo, (TipoId)TipoDe(&s->tipos, colunas)->a)) return false;
    *base = TipoSoa(&s->tipos, colunas);
    return true;
}

/* {} em soa[N]S e o valor zero; soa[dynamic]S nasce de make; elementos no literal nao existem. */
bool SemLiteralSoa(Semantica *s, Expr *e, TipoId ctx)
{
    if (TipoSoaDinamica(&s->tipos, ctx)) {
        return SemErro(s, E0320, e->pos, "soa[dynamic]S nasce de make: make(soa[dynamic]S, alloc = heap)");
    }
    if (e->nargs > 0) {
        return SemErro(s, E0320, e->pos, "soa nao tem literal com elementos: comece com {} e grave a[i] = S{...}");
    }
    e->tipo = ctx;
    return true;
}

/* a[i]: o indice e inteiro e o resultado e a struct S (gather); ^soa nao se indexa direto. */
bool SemIndiceSoa(Semantica *s, Expr *e)
{
    if (TipoClasseDe(&s->tipos, e->a->tipo) == TC_PONTEIRO) {
        return SemErro(s, E0320, e->pos_op, "indexe a soa apontada com p^[i]");
    }
    if (!SemChecar(s, e->b, TIPO_INT)) return false;
    if (!TipoEhInteiro(e->b->tipo)) return SemErroDoInteiro(s, e->b, "o indice");
    e->tipo = ElementoDaSoa(s, e->a->tipo);
    return true;
}

/* a.campo: a fatia []T da coluna; como a fatia empresta o campo para alterar, a raiz pede var. */
bool SemColunaDaSoa(Semantica *s, Expr *e)
{
    const Decl *d = s->globais[TipoDe(&s->tipos, ElementoDaSoa(s, e->a->tipo))->a].decl;
    int32_t idx = SemAcharCampo(d, e->nome);
    if (idx < 0) {
        SemParecido sp;
        SemParecidoIniciar(s, E0201, e->pos_op, e->nome, &sp);
        SemParecidosDeCampos(s, d, &sp);
        return SemErroParecido(s, &sp, "campo '%s' nao existe na struct '%s' da soa", SemNomeTexto(s, e->nome), SemNomeTexto(s, d->nome));
    }
    if (!SemLvalorMutavelDe(s, e->a, "a coluna da soa empresta os campos para alterar: declare com var")) return false;
    e->ref = REF_CAMPO;
    e->ref_id = idx;
    e->tipo = TipoFatia(&s->tipos, d->campos[idx].tipo.tipo);
    return true;
}

/* for item in a / for i, item in a: item e uma copia de S; sem &item e sem ^. */
bool SemForSoa(Semantica *s, Stmt *st)
{
    uint32_t pos = SemPosDepoisDaPalavra(s, st->pos);
    if (st->item_ref) {
        return SemErro(s, E0320, pos, "for &item nao existe em soa: use for i, item in a e grave a[i].campo");
    }
    if (TipoClasseDe(&s->tipos, st->expr->tipo) == TC_PONTEIRO) {
        return SemErro(s, E0320, st->expr->pos, "percorra a soa apontada com p^");
    }
    uint32_t altura = EscopoAltura(&s->escopo);
    NovoLocal indice = {st->nome_indice, pos, TIPO_INT, LOC_FOR, 0};
    st->tipo_local = ElementoDaSoa(s, st->expr->tipo);
    NovoLocal item = {st->nome, pos, st->tipo_local, LOC_FOR, 0};
    bool ok = (st->nome_indice < 0 || SemDeclararLocal(s, &indice, &st->local_indice)) &&
              SemDeclararLocal(s, &item, &st->local_id) && CorpoDeLaco(s, st->corpo);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}
