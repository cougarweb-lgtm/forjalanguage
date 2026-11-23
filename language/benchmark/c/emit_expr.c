/* emit_expr.c - emissao de C11 para tipos e expressoes.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"
#include "emit_internal.h"

#include <stdio.h>

bool EmitName(Ctx *c, const char *prefix, int32_t sym)
{
    return BufCat(&c->out, prefix) && BufCatN(&c->out, c->syms[sym].s, c->syms[sym].len);
}

bool EmitType(Ctx *c, Type t)
{
    switch (t.k) {
    case T_I32: return BufCat(&c->out, "int32_t");
    case T_I64: return BufCat(&c->out, "int64_t");
    case T_REAL: return BufCat(&c->out, "float");
    case T_BOOL: return BufCat(&c->out, "bool");
    case T_STRUCT: return EmitName(c, "S_", c->structs[t.id]->sym);
    case T_PTR: return EmitName(c, "S_", c->structs[t.id]->sym) && BufCat(&c->out, " *");
    default: return BufCat(&c->out, "void");
    }
}

bool EmitZero(Ctx *c, Type t)
{
    switch (t.k) {
    case T_REAL: return BufCat(&c->out, "0.0f");
    case T_BOOL: return BufCat(&c->out, "false");
    default: return BufCat(&c->out, "0");
    }
}

static bool EmitRealLiteral(Ctx *c, const Expr *e)
{
    const char *ini = c->src + e->oppos, *p = ini;
    while ((*p >= '0' && *p <= '9') || *p == '.') p++;
    return BufCatN(&c->out, ini, (size_t)(p - ini)) && BufCat(&c->out, "f");
}

static bool EmitLiteral(Ctx *c, const Expr *e)
{
    if (e->kind == E_REAL) return EmitRealLiteral(c, e);
    if (e->kind == E_BOOL) return BufCat(&c->out, e->op == OP_TRUE ? "true" : "false");
    return BufCatU64(&c->out, LiteralValue(c, e)) && (e->ty.k == T_I32 || BufCat(&c->out, "LL"));
}

static bool EmitField(Ctx *c, const Expr *e)
{
    return BufCat(&c->out, "(") && EmitExpr(c, e->a) && BufCat(&c->out, e->a->ty.k == T_PTR ? ")->" : ").")
           && EmitName(c, "f_", e->sym);
}

static bool EmitStringLiteral(Ctx *c, const Expr *e)
{
    const char *ini = c->src + e->oppos, *p = ini + 1;
    while (*p != '"') p++;
    return BufCatN(&c->out, ini, (size_t)(p + 1 - ini));
}

static bool EmitPrint(Ctx *c, const Expr *e)
{
    const Expr *v = e->u.args[0].val;
    if (v->ty.k == T_STR) return BufCat(&c->out, "puts(") && EmitStringLiteral(c, v) && BufCat(&c->out, ")");
    if (v->ty.k == T_BOOL) {
        return BufCat(&c->out, "puts((") && EmitExpr(c, v) && BufCat(&c->out, ") ? \"true\" : \"false\")");
    }
    bool wide = v->ty.k == T_I64;
    return BufCat(&c->out, wide ? "printf(\"%lld\\n\", (long long)(" : "printf(\"%d\\n\", (int)(")
           && EmitExpr(c, v) && BufCat(&c->out, "))");
}

static const Arg *FindArgFor(const Expr *e, int32_t param_idx)
{
    for (int32_t i = 0; i < e->nargs; i++) {
        if (e->u.args[i].idx == param_idx) return &e->u.args[i];
    }
    return NULL;
}

static bool EmitCall(Ctx *c, const Expr *e)
{
    if (e->sym == SYM_PRINT) return EmitPrint(c, e);
    const Decl *p = c->syms[e->sym].proc;
    if (!EmitName(c, "p_", e->sym) || !BufCat(&c->out, "(")) return false;
    for (int32_t i = 0; i < p->nparams; i++) {
        const Arg *a = FindArgFor(e, i);
        if (i > 0 && !BufCat(&c->out, ", ")) return false;
        if (!EmitExpr(c, a != NULL ? a->val : p->params[i].def)) return false;
    }
    return BufCat(&c->out, ")");
}

static bool EmitStructField(Ctx *c, const Expr *e, const Decl *sd, int32_t fi)
{
    const Arg *a = FindArgFor(e, fi);
    const Field *f = &sd->fields[fi];
    if (!BufCat(&c->out, fi > 0 ? ", " : "") || !EmitName(c, ".f_", f->sym) || !BufCat(&c->out, " = ")) return false;
    if (a != NULL) return EmitExpr(c, a->val);
    return f->def != NULL ? EmitExpr(c, f->def) : EmitZero(c, f->type.ty);
}

static bool EmitStructLit(Ctx *c, const Expr *e)
{
    const Decl *sd = c->structs[e->ty.id];
    if (!BufCat(&c->out, "((") || !EmitType(c, e->ty) || !BufCat(&c->out, "){")) return false;
    if (sd->nfields == 0) BufCat(&c->out, "0");
    for (int32_t i = 0; i < sd->nfields; i++) {
        if (!EmitStructField(c, e, sd, i)) return false;
    }
    return BufCat(&c->out, "})");
}

static bool EmitUnary(Ctx *c, const Expr *e)
{
    static const char *const PREFIXOS[] = {"(-(", "(!(", "(&"};
    const char *pre = PREFIXOS[e->op - OP_NEG];
    return BufCat(&c->out, pre) && EmitExpr(c, e->a) && BufCat(&c->out, e->op == OP_ADDR ? ")" : "))");
}

static bool EmitBinary(Ctx *c, const Expr *e)
{
    static const char *const SIMBOLOS[] = {"||", "&&", "==", "!=", "<", "<=", ">", ">=", "+", "-", "*", "/", "%"};
    return BufCat(&c->out, "(") && EmitExpr(c, e->a) && BufCat(&c->out, " ") && BufCat(&c->out, SIMBOLOS[e->op])
           && BufCat(&c->out, " ") && EmitExpr(c, e->u.b) && BufCat(&c->out, ")");
}

static bool EmitCast(Ctx *c, const Expr *e)
{
    return BufCat(&c->out, "((") && EmitType(c, e->ty) && BufCat(&c->out, ")(") && EmitExpr(c, e->a)
           && BufCat(&c->out, "))");
}

bool EmitExpr(Ctx *c, const Expr *e)
{
    switch (e->kind) {
    case E_INT:
    case E_REAL:
    case E_BOOL: return EmitLiteral(c, e);
    case E_STR: return EmitStringLiteral(c, e);
    case E_NAME: return EmitName(c, e->op == NAME_CONST ? "K_" : "v_", e->sym);
    case E_FIELD: return EmitField(c, e);
    case E_CALL: return EmitCall(c, e);
    case E_STRUCTLIT: return EmitStructLit(c, e);
    case E_UNARY: return EmitUnary(c, e);
    case E_BINARY: return EmitBinary(c, e);
    default: return EmitCast(c, e);
    }
}
