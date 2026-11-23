/* sem_fluxo.c - return, break/continue, defer e expect.
 * Versao: 0.7.0
 * Changelog: 0.7.0 - IA1-L4: o return de 'T ! E' com T dono confere o movimento (ReturnMoveOuCopia: local se move, campo/elemento e E0311).
 *            0.6.1 - IA1-b07: o return marca s->ctx_tipo antes da coercao (forma por ocorrencia do E0301).
 *            0.6.0 - Linguagem v1.11 L2: o E0706 de 'return &x' / 'return x[..]' sai de sem_fuga.c (fuga intraprocedural, por proc, depois do corpo tipado).
 *            0.5.2 - Linguagem v1.8 P7: return entrega a posse (PONTO_SAIDA, tambem em 'T ! E'); return/break/continue com heap vivo = E0317.
 *            0.5.1 - Linguagem v1.7 P10: 'return &x, 1' (retorno multiplo) tambem recebe E0706: o endereco local nao escapa por um elemento da tupla.
 *            0.5.0 - Linguagem v1.7 P5b: D20, proc exportado com retorno cstr/?cstr so devolve format_c(..., em = heap) direto ou none (E0901).
 *            0.4.2 - Linguagem v1.7 P4: SemReturn confere o retorno multiplo ('return a, b' ou 'return g()'; E0314) e recusa 'return a, b' fora dele.
 *            0.4.1 - Linguagem v1.7 P2: o return coage o valor ao retorno (SemCoagir: T -> ?T).
 *            0.4.0 - Linguagem v1.6 P4: 'return local' de dono move; outro lvalor dono = E0311.
 *            0.3.0 - Linguagem v1.5 P3b: E0706 tambem em 'return &x.c', '&x[i]' e 'return x[..]' de array local ou parametro.
 *            0.2.0 - Linguagem v1.3: E0706 em 'return &x'.
 *            0.1.0 - Fase 5c: return do prototipo + 'T ! E' (valor, membro do enum de erro ou o proprio resultado); break/continue
 *                    so em laco (E0701); defer de comando simples; expect so em test (E0801).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_interno.h"

/* 'return local' MOVE o dono para fora; qualquer outro lvalor dono seria copia (E0311). */
static bool ReturnMoveOuCopia(Semantica *s, const Expr *e)
{
    if (e->tipo_no == EX_NOME && e->ref == REF_LOCAL) return SemConferirPosse(s, e, PONTO_SAIDA);
    return SemConferirMovimento(s, e) && SemConferirPosse(s, e, PONTO_SAIDA);
}

/* return em proc 'T ! E' ou '! E': devolve T, um membro de E, o proprio resultado; sem valor so se nao ha T. L-01: T com dono se move como num 'return local'. */
static bool ReturnDeResultado(Semantica *s, Stmt *st, TipoId retorno)
{
    const TipoInfo resultado = *TipoDe(&s->tipos, retorno);
    char texto_retorno[96], texto_recebido[96];
    if (st->expr == NULL) {
        return resultado.a == TIPO_VAZIO || SemErro(s, E0301, st->pos, "return sem valor em proc com retorno");
    }
    if (!SemExpr(s, st->expr, resultado.a)) return false;
    TipoId t = st->expr->tipo;
    if (t != TIPO_VAZIO && (t == resultado.a || t == resultado.b || t == retorno)) {
        return ReturnMoveOuCopia(s, st->expr);
    }
    SemDescrever(s, retorno, texto_retorno, sizeof texto_retorno);
    SemDescrever(s, t, texto_recebido, sizeof texto_recebido);
    return SemErro(s, E0703, st->expr->pos, "return de %s em proc que devolve %s: devolva o valor ou um membro do enum de erro",
                   texto_recebido, texto_retorno);
}

/* 'return a, b' (D11): cada valor coage ao elemento da tupla e move como um 'return local'. */
static bool ReturnDeTuplaLiteral(Semantica *s, Expr *valor, TipoId retorno)
{
    uint32_t qtd = TipoTuplaQtd(&s->tipos, retorno);
    if (valor->nargs != qtd) return SemErro(s, E0314, valor->pos, "return com %u valores; o proc devolve %u", valor->nargs, qtd);
    for (uint32_t k = 0; k < qtd; k++) {
        Expr *v = valor->args[k].valor;
        if (!SemExprCom(s, v, TipoTuplaElemento(&s->tipos, retorno, k))) return false;
        if (!ReturnMoveOuCopia(s, v)) return false;
    }
    valor->tipo = retorno;
    return true;
}

/* proc com retorno multiplo: 'return a, b' (na ordem) ou 'return g()' de mesmo tipo. */
static bool ReturnDeTupla(Semantica *s, Stmt *st, TipoId retorno)
{
    Expr *valor = st->expr;
    if (valor == NULL) return SemErro(s, E0301, st->pos, "return sem valor em proc com retorno");
    if (valor->tipo_no == EX_TUPLA) return ReturnDeTuplaLiteral(s, valor, retorno);
    if (!SemExpr(s, valor, TIPO_VAZIO)) return false;
    if (valor->tipo == retorno) return true;
    return SemErro(s, E0314, valor->pos, "return com 1 valor; o proc devolve %u (return a, b)", TipoTuplaQtd(&s->tipos, retorno));
}

/* D20: proc exportado que devolve cstr ou ?cstr so devolve 'format_c(..., em = heap)' direto (ou none): o C libera com forja_liberar. */
static bool ReturnDeCstrExportado(Semantica *s, const Expr *valor, TipoId retorno)
{
    const Expr *v = valor->tipo_no == EX_EMBRULHA ? valor->a : valor;
    if (v->tipo_no == EX_NONE && retorno != TIPO_CSTR) return true;
    if (v->tipo_no == EX_CHAMADA && v->ref == REF_EMBUTIDO && v->ref_id == EMB_FORMAT_C && v->nargs > 1 &&
        v->args[1].valor->tipo_no == EX_NOME && v->args[1].valor->nome == s->nome_heap) {
        return true;
    }
    return SemErro(s, E0901, valor->pos,
                   "o proc exportado '%s' devolve cstr: o return precisa ser format_c(..., alloc = heap) direto (o C libera com forja_liberar)",
                   SemNomeTexto(s, s->proc->nome));
}

static bool RetornoEhCstrExportado(const Semantica *s, TipoId retorno)
{
    if (!s->proc->exportado) return false;
    return retorno == TIPO_CSTR || (TipoClasseDe(&s->tipos, retorno) == TC_OPCIONAL && TipoOpcionalNicho(&s->tipos, retorno));
}

/* O return em si (valor, tipo, movimento e entrega de posse); SemReturn acrescenta a conferencia de heap vivo na saida. */
static bool ReturnConferido(Semantica *s, Stmt *st)
{
    TipoId retorno = s->proc->retorno.tipo;
    bool tupla = TipoClasseDe(&s->tipos, retorno) == TC_TUPLA;
    if (tupla) return ReturnDeTupla(s, st, retorno);
    if (st->expr != NULL && st->expr->tipo_no == EX_TUPLA) {
        return SemErro(s, E0314, st->expr->pos, "'return a, b' pede retorno multiplo: -> (a: A, b: B)");
    }
    if (retorno == TIPO_VAZIO) return st->expr == NULL || SemErro(s, E0301, st->expr->pos, "o proc nao declara retorno");
    if (TipoClasseDe(&s->tipos, retorno) == TC_RESULTADO) return ReturnDeResultado(s, st, retorno);
    if (st->expr == NULL) return SemErro(s, E0301, st->pos, "return sem valor em proc com retorno");
    if (!SemExpr(s, st->expr, retorno)) return false;
    s->ctx_tipo = (ContextoDoTipo){CTX_RETORNO, st->expr, -1, s->proc->nome};
    if (!SemCoagir(s, st->expr, retorno, st->expr->pos)) return false;
    if (RetornoEhCstrExportado(s, retorno) && !ReturnDeCstrExportado(s, st->expr, retorno)) return false;
    return ReturnMoveOuCopia(s, st->expr);
}

bool SemReturn(Semantica *s, Stmt *st)
{
    return ReturnConferido(s, st) && SemConferirSaida(s, 0, st);
}

bool SemLacoControle(Semantica *s, Stmt *st)
{
    if (s->prof_laco > 0) return SemConferirSaida(s, s->marca_laco, st);
    return SemErro(s, E0701, st->pos, "%s fora de while ou for", st->tipo_no == ST_BREAK ? "break" : "continue");
}

bool SemDefer(Semantica *s, Stmt *st)
{
    bool salvo = s->em_defer;
    s->em_defer = true;
    bool ok = SemComando(s, st->corpo);
    s->em_defer = salvo;
    return ok;
}

bool SemExpect(Semantica *s, Stmt *st)
{
    if (!s->em_teste) return SemErro(s, E0801, st->pos, "expect so pode aparecer dentro de um bloco test");
    return SemCondicao(s, st->expr);
}
