/* sem_fuga_chamada.c - efeitos das chamadas na fuga de endereco (K1): recipiente contaminado (append, insert) e proc que guarda parametro.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - IA1-L5b: append tem 3 parametros (item, items); so item e guardado.
 *            0.1.0 - Linguagem v1.11 L3: primeira versao (FugaArmazenar tirado de sem_fuga.c; FugaEfeitos).
 * Verificacao: construir.bat && python ..\tests\semantica\verificar_semantica.py
 */
#include "semantic/sem_fuga.h"

/* ---------- gravacao ---------- */

/* O valor gravado vai tambem para cada local que o ponteiro de destino pode apontar. */
static void PropagarParaApontados(Fuga *f, const Expr *origem)
{
    for (uint32_t k = 0; k < f->nl; k++) {
        if (FugaTem(f->p, k)) FugaUnirEm(f, k, f->v, origem);
    }
}

/* Grava f->v em memoria apontada por f->p: contamina os locais apontados, alimenta o resumo do proc e, se a memoria vive alem do proc e o
 * valor tem endereco local, E0706. Gravacao feita por callee (g->callee) relata "o proc guarda o parametro". */
bool FugaArmazenar(Fuga *f, const FugaGravacao *g)
{
    FugaSumidouro sum;
    PropagarParaApontados(f, g->valor);
    FugaRegistrarGuarda(f, f->p, f->v);
    int32_t bit = FugaPrimeiroLocal(f, f->v);
    if (bit < 0 || !FugaSumidouroDe(f, f->p, g->pos, &sum)) return true;
    f->candidato = true;
    if (f->silencioso) return true;
    if (g->callee != NULL) sum = (FugaSumidouro){SUM_CALLEE, g->callee->params[g->param].nome, g->pos, g->callee->nome};
    return FugaReportar(f, g->valor, (uint32_t)bit, &sum);
}

/* ---------- o que a chamada guarda ---------- */

/* Resumo fixo dos embutidos: guarda[d] = parametros guardados na memoria do parametro d. append(lista, item): item na lista; insert(colecao,
 * key, value): key e value na colecao. O destino e sempre um ponteiro ('&l', 'p'), por isso a origem LOCAL do destino e o recipiente. */
static const uint64_t GUARDA_APPEND[3] = {1u << 1, 0, 0}; /* L5b: items (indice 2) copia os elementos e nao guarda a fatia: fora da mascara */
static const uint64_t GUARDA_INSERT[4] = {(1u << 1) | (1u << 2), 0, 0, 0};

typedef struct { const uint64_t *guarda; uint32_t nparams; const Decl *callee; } AlvoDaChamada;

static bool EmbutidoQueGuarda(const Expr *e, AlvoDaChamada *a)
{
    if (e->ref_id == EMB_APPEND) *a = (AlvoDaChamada){GUARDA_APPEND, 3, NULL};
    else if (e->ref_id == EMB_INSERT) *a = (AlvoDaChamada){GUARDA_INSERT, 3, NULL};
    else return false;
    return true;
}

/* O que a chamada guarda: o resumo do proc da Forja ou a tabela fixa de append/insert. false = nada a fazer (foreign, outro embutido). */
static bool ResolverAlvo(const Fuga *f, const Expr *e, AlvoDaChamada *a)
{
    if (e->ref == REF_EMBUTIDO) return EmbutidoQueGuarda(e, a);
    if (e->ref != REF_PROC) return false;
    const Decl *d = SemAcharSimbolo(f->s, e->nome, e->modulo)->decl;
    const FugaResumo *r = d == NULL ? NULL : FugaResumoDe(f, d);
    if (r == NULL) return false;
    *a = (AlvoDaChamada){r->guarda, r->nparams, d};
    return true;
}

static const Arg *ArgumentoDe(const Expr *e, uint32_t parametro)
{
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (e->args[i].indice == (int32_t)parametro) return &e->args[i];
    }
    return NULL;
}

/* Parametro 'i' do alvo guardado na memoria apontada pelo f->p ja calculado. */
static bool GuardarParametro(Fuga *f, const Expr *e, const AlvoDaChamada *a, uint32_t i)
{
    const Arg *arg = ArgumentoDe(e, i);
    if (arg == NULL || !FugaRastreia(f, arg->valor->tipo)) return true;
    FugaZerar(f, f->v);
    FugaAvaliar(f, arg->valor, f->v);
    return FugaArmazenar(f, &(FugaGravacao){arg->valor, e->pos, a->callee, (int32_t)i});
}

/* guarda[d]: os parametros marcados vao para a memoria do parametro d (d == nparams: memoria alheia ao proc, tratada como heap). */
static bool GuardarNoDestino(Fuga *f, const Expr *e, const AlvoDaChamada *a, uint32_t d)
{
    FugaZerar(f, f->p);
    if (d == a->nparams) {
        FugaLigarFlag(f, f->p, FUGA_HEAP);
    } else {
        const Arg *destino = ArgumentoDe(e, d);
        if (destino == NULL) return true;
        FugaAvaliar(f, destino->valor, f->p);
    }
    for (uint32_t i = 0; i < a->nparams && i < FUGA_PARAMS_MAX; i++) {
        if (((a->guarda[d] >> i) & 1) && !GuardarParametro(f, e, a, i)) return false;
    }
    return true;
}

static bool EfeitosDaChamada(Fuga *f, const Expr *e)
{
    AlvoDaChamada a;
    if (!ResolverAlvo(f, e, &a)) return true;
    for (uint32_t d = 0; d <= a.nparams; d++) {
        if (a.guarda[d] != 0 && !GuardarNoDestino(f, e, &a, d)) return false;
    }
    return true;
}

/* ---------- percurso ---------- */

/* Cada chamada da expressao, os argumentos antes da chamada que os recebe. */
static bool VisitarChamadas(Fuga *f, const Expr *e)
{
    if (e == NULL) return true;
    if (!VisitarChamadas(f, e->a) || !VisitarChamadas(f, e->b)) return false;
    for (uint32_t i = 0; i < e->nargs; i++) {
        if (!VisitarChamadas(f, e->args[i].valor)) return false;
    }
    return e->tipo_no != EX_CHAMADA || EfeitosDaChamada(f, e);
}

/* Roda antes do comando: o valor que o comando le ja enxerga o que as chamadas dele guardaram. */
bool FugaEfeitos(Fuga *f, const Stmt *st)
{
    return VisitarChamadas(f, st->alvo) && VisitarChamadas(f, st->expr) && VisitarChamadas(f, st->expr2);
}
