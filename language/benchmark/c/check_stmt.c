/* check_stmt.c - checagem dos comandos e dos corpos de proc (passe B).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"

#define VOID_TYPE MakeType(T_VOID, 0)

static bool CheckStmt(Ctx *c, Stmt *s);

static bool CheckCondition(Ctx *c, Expr *cond)
{
    Type bool_type = MakeType(T_BOOL, 0);
    if (!CheckExpr(c, cond, bool_type)) return false;
    return cond->ty.k == T_BOOL || Report(c, E0301, cond->pos, "a condicao precisa ser bool");
}

bool CheckBlock(Ctx *c, Stmt *s)
{
    int32_t altura = c->nscope;
    for (; s != NULL; s = s->next) {
        if (!CheckStmt(c, s)) return false;
    }
    ScopePop(c, altura);
    return true;
}

static bool CheckLetVar(Ctx *c, Stmt *s)
{
    Type want = VOID_TYPE;
    if (s->has_type) {
        if (!ResolveTypeRef(c, &s->type)) return false;
        want = s->type.ty;
    }
    if (!CheckExpr(c, s->e, want)) return false;
    if (s->has_type && !CompatTypes(c, want, s->e->ty, s->e->pos)) return false;
    if (s->e->ty.k == T_VOID) return Report(c, E0301, s->e->pos, "a expressao nao tem valor");
    s->ty = s->e->ty;
    ScopePush(c, s->sym, s->ty);
    return true;
}

static bool CheckAssign(Ctx *c, Stmt *s)
{
    if (!CheckExpr(c, s->lhs, VOID_TYPE)) return false;
    if (s->lhs->kind == E_NAME && s->lhs->op == NAME_CONST) {
        return Report(c, E0301, s->lhs->pos, "constante nao recebe atribuicao");
    }
    Type want = s->lhs->ty;
    if (!CheckExpr(c, s->e, want) || !CompatTypes(c, want, s->e->ty, s->e->pos)) return false;
    if (s->op != TK_ASSIGN && !IsNumeric(want)) return Report(c, E0301, s->e->pos, "'+=' e '-=' pedem numero");
    return true;
}

static bool CheckIf(Ctx *c, Stmt *s)
{
    return CheckCondition(c, s->e) && CheckBlock(c, s->body) && CheckBlock(c, s->els);
}

static bool CheckRangeBounds(Ctx *c, Stmt *s)
{
    Expr *a = s->e, *b = s->e2;
    if (a->flex && !b->flex) {
        if (!CheckExpr(c, b, VOID_TYPE) || !CheckExpr(c, a, b->ty)) return false;
    } else if (!CheckExpr(c, a, VOID_TYPE) || !CheckExpr(c, b, a->ty)) {
        return false;
    }
    if (!IsIntType(a->ty)) return Report(c, E0301, a->pos, "o limite do laco precisa ser inteiro");
    if (!TypeEq(a->ty, b->ty)) return CompatTypes(c, a->ty, b->ty, b->pos);
    s->ty = a->ty;
    return true;
}

static bool CheckFor(Ctx *c, Stmt *s)
{
    if (!CheckRangeBounds(c, s)) return false;
    int32_t altura = c->nscope;
    ScopePush(c, s->sym, s->ty);
    if (!CheckBlock(c, s->body)) return false;
    ScopePop(c, altura);
    return true;
}

static bool CheckReturn(Ctx *c, Stmt *s)
{
    const Decl *p = c->cur_proc;
    if (!p->has_ret) {
        return s->e == NULL || Report(c, E0301, s->e->pos, "o proc nao declara retorno");
    }
    if (s->e == NULL) return Report(c, E0301, s->pos, "return sem valor em proc com retorno");
    return CheckExpr(c, s->e, p->ret.ty) && CompatTypes(c, p->ret.ty, s->e->ty, s->e->pos);
}

static bool CheckStmt(Ctx *c, Stmt *s)
{
    switch (s->kind) {
    case S_LET:
    case S_VAR: return CheckLetVar(c, s);
    case S_ASSIGN: return CheckAssign(c, s);
    case S_IF: return CheckIf(c, s);
    case S_WHILE: return CheckCondition(c, s->e) && CheckBlock(c, s->body);
    case S_FOR: return CheckFor(c, s);
    case S_RETURN: return CheckReturn(c, s);
    default: return CheckExpr(c, s->e, VOID_TYPE);
    }
}

bool CheckProcBody(Ctx *c, Decl *d)
{
    int32_t altura = c->nscope;
    c->cur_proc = d;
    for (int32_t i = 0; i < d->nparams; i++) ScopePush(c, d->params[i].sym, d->params[i].type.ty);
    if (!CheckBlock(c, d->body)) return false;
    if (d->has_ret) {
        const Stmt *ultimo = d->body;
        while (ultimo != NULL && ultimo->next != NULL) ultimo = ultimo->next;
        if (ultimo == NULL || ultimo->kind != S_RETURN) {
            return Report(c, E0302, d->close_pos, "proc com retorno precisa terminar em return");
        }
    }
    ScopePop(c, altura);
    return true;
}
