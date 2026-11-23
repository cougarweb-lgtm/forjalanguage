/* sem_mapa.c - map[K]V (Linguagem v1.9): tipo, insert/get/get_ptr/has/remove, for k, v in m e a guarda de mudar o map dentro do for (E0318).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M5 P48 (S18): ResolverMapa recusa valor de map que nao se copia (E0339, SemExigeCopiavel).
 *            0.2.0 - IA1-L9e: SemConferirClearMapa (clear(&m); sem_mapa_vistas.c).
 *            0.1.0 - Linguagem v1.9 P10: ResolverMapa, ChaveDeMapaValida, ValorDeMapaValido, SemChamarMapa, SemForMapa e SemConferirMapaNoFor (3.8).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

#include <stdio.h>

/* Chave de map: inteiro, enum, bool ou str (f32/f64, struct, array, ponteiro e cstr ficam fora: D8). */
static bool ChaveDeMapaValida(const Semantica *s, TipoId k)
{
    return TipoEhInteiro(k) || TipoClasseDe(&s->tipos, k) == TC_ENUM || k == TIPO_BOOL || k == TIPO_STR;
}

/* Valor de map: qualquer tipo com valor, menos ?T, ?^T, rawptr, resultado com erro, tupla e dono (D9). */
static bool ValorDeMapaValido(const Semantica *s, TipoId v)
{
    TipoClasse c = TipoClasseDe(&s->tipos, v);
    if (c == TC_OPCIONAL || c == TC_ANULAVEL || c == TC_RAWPTR || c == TC_RESULTADO || c == TC_TUPLA || c == TC_VAZIO) return false;
    return !SemTipoEhDono(s, v);
}

/* map[K]V: alvo[0] = K e alvo[1] = V. */
bool ResolverMapa(Semantica *s, TipoRef *t, TipoId *base)
{
    char tipo[96];
    if (!SemResolverTipoRef(s, &t->alvo[0]) || !SemResolverTipoRef(s, &t->alvo[1])) return false;
    TipoId k = t->alvo[0].tipo, v = t->alvo[1].tipo;
    if (!ChaveDeMapaValida(s, k)) {
        SemDescrever(s, k, tipo, sizeof tipo);
        return SemErro(s, E0318, t->alvo[0].pos, "chave de map e inteiro, enum, bool ou str (recebeu %s)", tipo);
    }
    if (!ValorDeMapaValido(s, v)) {
        SemDescrever(s, v, tipo, sizeof tipo);
        return SemErro(s, E0318, t->alvo[1].pos,
                       "o valor de map nao pode ser %s (?T, ?^T, rawptr, resultado com erro, tupla e dono ficam fora): get ja devolve ?V", tipo);
    }
    if (!SemExigeCopiavel(s, v, t->alvo[1].pos, "valor de map por valor: o get devolve copia")) return false;
    *base = TipoMapa(&s->tipos, k, v);
    return true;
}

/* Nome do map que o 1o argumento aponta: '&x' -> x; o proprio nome quando e ^map; senao -1. */
static int32_t NomeDoMapaAlterado(const Expr *primeiro)
{
    if (primeiro->tipo_no == EX_UNARIO && primeiro->op == OP_ENDERECO) {
        return primeiro->a->tipo_no == EX_NOME ? primeiro->a->nome : -1;
    }
    return primeiro->tipo_no == EX_NOME ? primeiro->nome : -1;
}

/* Mudar o map dentro do for que o percorre (D12): so se detecta pelo mesmo nome. */
bool SemConferirMapaNoFor(Semantica *s, const Expr *onde, int32_t nome)
{
    if (nome < 0) return true;
    for (size_t i = 0; i < s->mapas_em_for.n; i++) {
        if (*(const int32_t *)VecEm(&s->mapas_em_for, i) != nome) continue;
        return SemErro(s, E0318, onde->pos,
                       "o map '%s' esta sendo percorrido por este for: insert, remove e delete mudam a ordem; guarde as chaves numa lista e altere depois do for",
                       SemNomeTexto(s, nome));
    }
    return true;
}

/* insert e remove pedem ^map (e var quando e '&x') e nao valem dentro do for do mesmo map. */
static bool ConferirAlteracao(Semantica *s, Expr *colecao, const char *nome, bool insere)
{
    char motivo[64];
    if (TipoClasseDe(&s->tipos, colecao->tipo) != TC_PONTEIRO) {
        return SemErro(s, E0318, colecao->pos, "%s recebe o endereco do map: %s", nome,
                       insere ? "insert(&m, key = k, value = v)" : "remove(&m, key = k)");
    }
    if (colecao->tipo_no == EX_UNARIO && colecao->op == OP_ENDERECO) {
        snprintf(motivo, sizeof motivo, "%s altera o map: declare com var", nome);
        if (!SemLvalorMutavelDe(s, colecao->a, motivo)) return false;
    }
    return SemConferirMapaNoFor(s, colecao, NomeDoMapaAlterado(colecao));
}

/* clear(&m) (IA1-L9e): ^map, var quando e '&x', e fora do for que percorre o mesmo map (a mesma regra do insert e do remove). */
bool SemConferirClearMapa(Semantica *s, Expr *colecao)
{
    if (colecao->tipo_no == EX_UNARIO && colecao->op == OP_ENDERECO &&
        !SemLvalorMutavelDe(s, colecao->a, "clear altera o map: declare com var")) {
        return false;
    }
    return SemConferirMapaNoFor(s, colecao, NomeDoMapaAlterado(colecao));
}

/* get_ptr pede ^map (e var quando e '&x'): o ponteiro devolvido permite alterar o valor. */
static bool ConferirGetPtr(Semantica *s, Expr *colecao)
{
    if (TipoClasseDe(&s->tipos, colecao->tipo) != TC_PONTEIRO) {
        return SemErro(s, E0318, colecao->pos, "get_ptr recebe o endereco do map: get_ptr(&m, key = k)");
    }
    if (colecao->tipo_no == EX_UNARIO && colecao->op == OP_ENDERECO) {
        return SemLvalorMutavelDe(s, colecao->a, "get_ptr permite alterar o valor: declare o map com var");
    }
    return true;
}

/* value do insert: do tipo V, entra por movimento e e ponto de guarda (como o item do append). */
static bool ConferirValorDoInsert(Semantica *s, Expr *e, TipoId v)
{
    Arg *valor = ArgumentoDoParametro(e, 2);
    if (valor == NULL) return SemErro(s, E0318, e->pos, "insert em map pede value: insert(&m, key = k, value = v)");
    Expr *x = valor->valor;
    if (!SemChecar(s, x, v) || !SemCoagir(s, x, v, x->pos)) return false;
    return SemConferirMovimento(s, x) && SemConferirPosse(s, x, PONTO_GUARDA);
}

/* Tipo do resultado: get devolve ?V (V ponteiro vira ?^X), get_ptr devolve ?^V. */
static TipoId TipoDoResultado(Semantica *s, int32_t embutido, TipoId v)
{
    if (embutido == EMB_INSERT || embutido == EMB_REMOVE) return TIPO_VAZIO;
    if (embutido == EMB_HAS) return TIPO_BOOL;
    if (embutido == EMB_GET_PTR) return TipoAnulavel(&s->tipos, v);
    if (TipoClasseDe(&s->tipos, v) == TC_PONTEIRO) return TipoAnulavel(&s->tipos, (TipoId)TipoDe(&s->tipos, v)->a);
    return TipoOpcional(&s->tipos, v);
}

/* insert, get, get_ptr, has e remove sobre map; colecao e o 1o argumento, ja checado. */
bool SemChamarMapa(Semantica *s, Expr *e, int32_t embutido, Expr *colecao)
{
    const char *nome = SemNomeTexto(s, e->nome);
    bool por_ponteiro = TipoClasseDe(&s->tipos, colecao->tipo) == TC_PONTEIRO;
    if (embutido == EMB_GET_PTR) {
        if (!ConferirGetPtr(s, colecao)) return false;
    } else if (embutido == EMB_INSERT || embutido == EMB_REMOVE) {
        if (!ConferirAlteracao(s, colecao, nome, embutido == EMB_INSERT)) return false;
    }
    TipoId mapa = por_ponteiro ? (TipoId)TipoDe(&s->tipos, colecao->tipo)->a : colecao->tipo;
    TipoId k = (TipoId)TipoDe(&s->tipos, mapa)->a, v = (TipoId)TipoDe(&s->tipos, mapa)->b;
    if (embutido == EMB_INSERT && !ConferirValorDoInsert(s, e, v)) return false;
    Expr *chave = ArgumentoDoParametro(e, 1)->valor;
    if (!SemChecar(s, chave, k) || !SemCoagir(s, chave, k, chave->pos)) return false;
    e->tipo = TipoDoResultado(s, embutido, v);
    return true;
}

/* O for sobre o map: a raiz do que se percorre, para a guarda do D12 (-1 nao casa com nada). */
static int32_t NomeDaRaizDoFor(const Expr *alvo)
{
    if (alvo->tipo_no == EX_NOME) return alvo->nome;
    if (alvo->tipo_no == EX_UNARIO && alvo->op == OP_DEREF && alvo->a->tipo_no == EX_NOME) return alvo->a->nome;
    return -1;
}

/* Corpo do for com o nome do map na pilha; tira o nome antes de devolver, com ou sem erro. */
static bool CorpoDoForDeMapa(Semantica *s, Stmt *st)
{
    *(int32_t *)VecEmpurrar(&s->mapas_em_for) = NomeDaRaizDoFor(st->expr);
    bool ok = CorpoDeLaco(s, st->corpo);
    s->mapas_em_for.n--;
    return ok;
}

/* for k, v in m / for k, &v in m: percorre na ordem de insercao; o map pode vir como m ou m^ (^map). */
bool SemForMapa(Semantica *s, Stmt *st)
{
    uint32_t pos = SemPosDepoisDaPalavra(s, st->pos);
    if (st->nome_indice < 0) {
        return SemErro(s, E0318, pos, "for de map pede chave e valor: for k, v in m (ou for k, &v in m)");
    }
    bool por_ponteiro = TipoClasseDe(&s->tipos, st->expr->tipo) == TC_PONTEIRO;
    bool deref = st->expr->tipo_no == EX_UNARIO && st->expr->op == OP_DEREF;
    if (st->item_ref && !por_ponteiro && !deref &&
        !SemLvalorMutavelDe(s, st->expr, "'for k, &v' altera os valores: declare o map com var")) {
        return false;
    }
    TipoId mapa = por_ponteiro ? (TipoId)TipoDe(&s->tipos, st->expr->tipo)->a : st->expr->tipo;
    TipoId k = (TipoId)TipoDe(&s->tipos, mapa)->a, v = (TipoId)TipoDe(&s->tipos, mapa)->b;
    uint32_t altura = EscopoAltura(&s->escopo);
    NovoLocal chave = {st->nome_indice, pos, k, LOC_FOR, 0};
    st->tipo_local = st->item_ref ? TipoPonteiro(&s->tipos, v) : v;
    NovoLocal item = {st->nome, pos, st->tipo_local, LOC_FOR, 0};
    bool ok = SemDeclararLocal(s, &chave, &st->local_indice) && SemDeclararLocal(s, &item, &st->local_id) &&
              CorpoDoForDeMapa(s, st);
    EscopoRecuar(&s->escopo, altura);
    return ok;
}
