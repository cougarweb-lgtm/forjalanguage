/* sem_v19.c - despachos da Linguagem v1.9 (map, bit_set, soa e matrix) para os arquivos de cada recurso.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - Linguagem v1.9 P10: map sai do provisorio (ResolverMapa, SemChamarMapa, SemForMapa em sem_mapa.c; indice, literal, conta e comparacao de map = E0318 aqui); ArgumentoDoParametro deixa de ser static.
 *            0.4.0- Linguagem v1.9 P7: soa sai do provisorio (ResolverSoa, SemIndiceSoa, SemLiteralSoa, SemColunaDaSoa, SemForSoa em sem_soa.c; conta e comparacao de soa = E0320 aqui).
 *            0.3.0 - Linguagem v1.9 P5: matrix sai do provisorio (ResolverMatriz, SemIndiceMatriz, SemLiteralMatriz, SemContaMatriz, SemChamarTranspose em sem_matriz.c; comparacao, campo e for de matrix com E0321 aqui).
 *            0.2.0 -Linguagem v1.9 P3: SemEhTipoV19, SemTipoTemV19 e os despachos SemIndiceV19, SemLiteralV19, SemContaV19, SemComparacaoV19, SemChamarV19, SemForV19 e SemCampoV19; bit_set sai do provisorio (ResolverBitSet e demais em sem_bitset.c); matrix, soa e map seguem com "chega no P<n>".
 *            0.1.0 - Linguagem v1.9 P2: SemResolverTipoV19 e SemIndiceDuplo so com os provisorios (E0101/E0308 "chega no P<n>"); cada passo de recurso troca o seu ramo.
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* Tipo escrito de uma das 4 formas novas (forma >= TR_MAPA). */
bool SemResolverTipoV19(Semantica *s, TipoRef *t, TipoId *base)
{
    if (t->forma == TR_BIT_SET) return ResolverBitSet(s, t, base);
    if (t->forma == TR_MATRIZ) return ResolverMatriz(s, t, base);
    if (t->forma == TR_SOA) return ResolverSoa(s, t, base);
    return ResolverMapa(s, t, base);
}

/* m[i, j] (Expr.nargs != 0 em EX_INDICE): so a matriz tem indice duplo. */
bool SemIndiceDuplo(Semantica *s, Expr *e)
{
    return SemIndiceMatriz(s, e);
}

/* Classe do tipo, descendo UM ^ ; *por_ponteiro diz se desceu. */
static TipoClasse ClasseDoAlvo(const Semantica *s, TipoId t, bool *por_ponteiro)
{
    TipoClasse c = TipoClasseDe(&s->tipos, t);
    *por_ponteiro = c == TC_PONTEIRO;
    return *por_ponteiro ? TipoClasseDe(&s->tipos, TipoDe(&s->tipos, t)->a) : c;
}

/* Classe >= TC_MAPA, ou ^ para uma delas. */
bool SemEhTipoV19(const Semantica *s, TipoId t)
{
    bool por_ponteiro;
    return ClasseDoAlvo(s, t, &por_ponteiro) >= TC_MAPA;
}

/* Tipo v1.9 direto, como elemento de array ou em algum campo de struct (const nao guarda nenhum). */
bool SemTipoTemV19(const Semantica *s, TipoId t)
{
    const TipoInfo *k = TipoDe(&s->tipos, t);
    if (k->classe >= TC_MAPA) return true;
    if (k->classe == TC_ARRAY) return SemTipoTemV19(s, k->a);
    if (k->classe != TC_STRUCT) return false;
    const Decl *d = s->globais[k->a].decl;
    for (uint32_t i = 0; i < d->ncampos; i++) {
        if (SemTipoTemV19(s, d->campos[i].tipo.tipo)) return true;
    }
    return false;
}

/* a[i] onde a e map, bit_set, soa ou matrix (SemIndice so chega aqui com TiposTemV19). */
bool SemIndiceV19(Semantica *s, Expr *e)
{
    bool por_ponteiro;
    TipoClasse c = ClasseDoAlvo(s, e->a->tipo, &por_ponteiro);
    if (c == TC_BIT_SET) return SemErro(s, E0319, e->pos_op, "bit_set nao se indexa: has(s, key = m)");
    if (c == TC_MATRIZ) return SemErro(s, E0321, e->pos_op, "matrix se indexa com m[linha, coluna]");
    if (c == TC_SOA) return SemIndiceSoa(s, e);
    return SemErro(s, E0318, e->pos_op, "map nao se indexa com []: get(m, key = k) le e insert(&m, key = k, value = v) grava");
}

/* '{...}' com tipo pelo contexto de classe >= TC_MAPA. */
bool SemLiteralV19(Semantica *s, Expr *e, TipoId ctx)
{
    TipoClasse c = TipoClasseDe(&s->tipos, ctx);
    if (c == TC_BIT_SET) return SemLiteralBitSet(s, e, ctx);
    if (c == TC_MATRIZ) return SemLiteralMatriz(s, e, ctx);
    if (c == TC_SOA) return SemLiteralSoa(s, e, ctx);
    return SemErro(s, E0318, e->pos, "map nasce de make: make(map[K]V, alloc = heap); {} nao cria map");
}

/* + - * / % e bits com operando de classe >= TC_MAPA. */
bool SemContaV19(Semantica *s, Expr *e)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, e->a->tipo), cb = TipoClasseDe(&s->tipos, e->b->tipo);
    if (ca == TC_MATRIZ || cb == TC_MATRIZ) return SemContaMatriz(s, e);
    if (ca == TC_BIT_SET || cb == TC_BIT_SET) return SemContaBitSet(s, e);
    if (ca == TC_SOA || cb == TC_SOA) return SemErro(s, E0320, e->pos_op, "soa nao faz conta nem comparacao");
    return SemErro(s, E0318, e->pos_op, "map nao faz conta nem comparacao");
}

/* == != < <= > >= com operando de classe >= TC_MAPA. */
bool SemComparacaoV19(Semantica *s, Expr *e)
{
    TipoClasse ca = TipoClasseDe(&s->tipos, e->a->tipo), cb = TipoClasseDe(&s->tipos, e->b->tipo);
    if (ca == TC_MATRIZ || cb == TC_MATRIZ) {
        return SemErro(s, E0321, e->pos_op, "matrix nao tem == nem ordem: compare elemento a elemento");
    }
    if (ca == TC_BIT_SET || cb == TC_BIT_SET) return SemComparacaoBitSet(s, e);
    if (ca == TC_SOA || cb == TC_SOA) return SemErro(s, E0320, e->pos_op, "soa nao faz conta nem comparacao");
    return SemErro(s, E0318, e->pos_op, "map nao faz conta nem comparacao");
}

/* Argumento ligado ao parametro k (Arg.indice == k), ou NULL. */
Arg *ArgumentoDoParametro(Expr *e, int32_t k)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].indice == k) return &e->args[i];
    }
    return NULL;
}

/* insert, get, get_ptr, has, remove e transpose: liga os argumentos, confere o 1o e despacha pela classe dele. */
bool SemChamarV19(Semantica *s, Expr *e, int32_t embutido)
{
    char tipo[96];
    bool por_ponteiro;
    if (embutido == EMB_TRANSPOSE) return SemChamarTranspose(s, e);
    bool insere = embutido == EMB_INSERT;
    if (!SemLigarEmbutido(s, e, insere ? s->params_insert : s->params_chave, insere ? 3 : 2)) return false;
    Expr *colecao = ArgumentoDoParametro(e, 0)->valor;
    if (!SemChecar(s, colecao, TIPO_VAZIO)) return false;
    TipoClasse c = ClasseDoAlvo(s, colecao->tipo, &por_ponteiro);
    if (c == TC_BIT_SET) return SemChamarBitSet(s, e, embutido, colecao);
    if (c == TC_MAPA) return SemChamarMapa(s, e, embutido, colecao);
    SemDescrever(s, colecao->tipo, tipo, sizeof tipo);
    if (embutido == EMB_GET || embutido == EMB_GET_PTR) {
        return SemErro(s, E0301, colecao->pos, "%s pede map[K]V (tipo %s)", SemNomeTexto(s, e->nome), tipo);
    }
    return SemErro(s, E0301, colecao->pos, "%s pede map[K]V ou bit_set[E] (tipo %s)", SemNomeTexto(s, e->nome), tipo);
}

/* for sobre map, bit_set, soa ou matrix. */
bool SemForV19(Semantica *s, Stmt *st)
{
    bool por_ponteiro;
    TipoClasse c = ClasseDoAlvo(s, st->expr->tipo, &por_ponteiro);
    if (c == TC_BIT_SET) return SemForBitSet(s, st);
    if (c == TC_MATRIZ) return SemErro(s, E0321, st->expr->pos, "for nao percorre matrix: use for i in 0..<R e m[i, j]");
    if (c == TC_SOA) return SemForSoa(s, st);
    return SemForMapa(s, st);
}

/* a.campo onde a e map, bit_set, soa ou matrix: bit_set e map ficam no E0201 de sempre; matrix e soa chegam com o recurso. */
bool SemCampoV19(Semantica *s, Expr *e)
{
    bool por_ponteiro;
    TipoClasse c = ClasseDoAlvo(s, e->a->tipo, &por_ponteiro);
    if (c == TC_MATRIZ && !por_ponteiro) return SemErro(s, E0321, e->pos_op, "matrix nao tem campo: use m[linha, coluna]");
    if (c == TC_SOA && !por_ponteiro) return SemColunaDaSoa(s, e);
    return SemErro(s, E0201, e->pos_op, "campo '%s' pedido em valor que nao e struct", SemNomeTexto(s, e->nome));
}
