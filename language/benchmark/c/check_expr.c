/* check_expr.c - checagem de tipos e unidades das expressoes (nomes, campos, unarios, binarios, cast).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"

#include <stdio.h>

#define VOID_TYPE MakeType(T_VOID, 0)

uint64_t LiteralValue(const Ctx *c, const Expr *e)
{
    uint64_t v = 0;
    for (const char *p = c->src + e->oppos; *p >= '0' && *p <= '9'; p++) v = v * 10 + (uint64_t)(*p - '0');
    return v;
}

static bool CheckName(Ctx *c, Expr *e)
{
    const Sym *y = &c->syms[e->sym];
    if (y->local != NULL) {
        e->op = NAME_LOCAL;
        e->ty = y->local->ty;
        return true;
    }
    if (y->cnst != NULL) {
        e->op = NAME_CONST;
        e->ty = y->cnst->type.ty;
        return true;
    }
    return Report(c, E0201, e->oppos, "nome '%.*s' nao declarado", (int)y->len, y->s);
}

int32_t FindField(const Decl *sd, int32_t sym)
{
    for (int32_t i = 0; i < sd->nfields; i++) {
        if (sd->fields[i].sym == sym) return i;
    }
    return -1;
}

static bool CheckField(Ctx *c, Expr *e)
{
    if (!CheckExpr(c, e->a, VOID_TYPE)) return false;
    Type base = e->a->ty;
    const Sym *y = &c->syms[e->sym];
    if (base.k != T_STRUCT && base.k != T_PTR) {
        return Report(c, E0201, e->oppos, "campo '%.*s' pedido em valor que nao e struct", (int)y->len, y->s);
    }
    const Decl *sd = c->structs[base.id];
    int32_t idx = FindField(sd, e->sym);
    if (idx < 0) return Report(c, E0201, e->oppos, "campo '%.*s' nao existe na struct", (int)y->len, y->s);
    e->ty = sd->fields[idx].type.ty;
    return true;
}

static bool CheckUnary(Ctx *c, Expr *e, Type ctx)
{
    Expr *a = e->a;
    if (e->op == OP_NOT) {
        if (!CheckExpr(c, a, MakeType(T_BOOL, 0))) return false;
        if (a->ty.k != T_BOOL) return Report(c, E0301, e->pos, "'not' pede bool");
        e->ty = a->ty;
        return true;
    }
    if (e->op == OP_ADDR) {
        if (a->kind != E_NAME) return Report(c, E0301, e->pos, "'&' so vale sobre o nome de uma variavel");
        if (!CheckExpr(c, a, VOID_TYPE)) return false;
        if (a->ty.k != T_STRUCT) return Report(c, E0301, e->pos, "'&' so vale sobre variavel de struct");
        e->ty = MakeType(T_PTR, a->ty.id);
        return true;
    }
    if (!CheckExpr(c, a, ctx)) return false;
    if (!IsNumeric(a->ty)) return Report(c, E0301, e->pos, "'-' pede numero");
    e->ty = a->ty;
    return true;
}

static bool CheckCast(Ctx *c, Expr *e)
{
    TypeRef alvo = {e->sym, e->oppos, false, VOID_TYPE};
    if (!CheckExpr(c, e->a, VOID_TYPE) || !ResolveTypeRef(c, &alvo)) return false;
    if (!IsNumeric(e->a->ty) || !IsNumeric(alvo.ty)) {
        return Report(c, E0301, e->pos, "'as' so converte entre i32, int, f32 e unidades");
    }
    e->ty = alvo.ty;
    return true;
}

/* Contexto de um literal dividido/multiplicado por 'outro': em REAL x REAL o literal e adimensional. */
static Type LiteralContext(Type outro, uint8_t op)
{
    if (outro.k == T_REAL && (op == OP_MUL || op == OP_DIV)) return MakeType(T_REAL, 0);
    return outro;
}

static bool CheckOperands(Ctx *c, Expr *e, Type ctx)
{
    Expr *a = e->a, *b = e->u.b;
    if (e->op == OP_AND || e->op == OP_OR) {
        Type bool_ctx = MakeType(T_BOOL, 0);
        return CheckExpr(c, a, bool_ctx) && CheckExpr(c, b, bool_ctx);
    }
    Type operand_ctx = (e->op >= OP_ADD && e->op <= OP_MOD) ? ctx : VOID_TYPE;
    if (a->flex && !b->flex) {
        return CheckExpr(c, b, operand_ctx) && CheckExpr(c, a, LiteralContext(b->ty, e->op));
    }
    if (!CheckExpr(c, a, operand_ctx)) return false;
    return CheckExpr(c, b, (b->flex) ? LiteralContext(a->ty, e->op) : operand_ctx);
}

static bool ApplyLogic(Ctx *c, Expr *e)
{
    if (e->a->ty.k != T_BOOL || e->u.b->ty.k != T_BOOL) {
        return Report(c, E0301, e->oppos, "'and'/'or' pedem operandos bool");
    }
    e->ty = MakeType(T_BOOL, 0);
    return true;
}

static bool ApplyCompare(Ctx *c, Expr *e)
{
    Type ta = e->a->ty, tb = e->u.b->ty;
    if (ta.k != tb.k || ta.k == T_VOID || ta.k == T_STRUCT || ta.k == T_PTR) {
        return Report(c, E0301, e->oppos, "comparacao entre tipos incompativeis");
    }
    if (ta.k == T_REAL && ta.id != tb.id) return Report(c, E0412, e->oppos, "comparacao entre dimensoes diferentes");
    e->ty = MakeType(T_BOOL, 0);
    return true;
}

static bool ApplyArith(Ctx *c, Expr *e)
{
    Type ta = e->a->ty, tb = e->u.b->ty;
    if (ta.k != tb.k || !IsNumeric(ta)) return Report(c, E0301, e->oppos, "operandos numericos de tipos diferentes");
    if (e->op == OP_MOD && ta.k == T_REAL) return Report(c, E0301, e->oppos, "'%%' so vale para inteiros");
    e->ty = ta;
    if (ta.k != T_REAL || e->op == OP_MOD) return true;
    if (e->op == OP_ADD || e->op == OP_SUB) {
        return ta.id == tb.id ? true : Report(c, E0412, e->oppos, "soma ou subtracao entre dimensoes diferentes");
    }
    int32_t dim = DimCombine(c, ta.id, tb.id, e->op == OP_MUL ? 1 : -1);
    if (dim < 0 || !c->dims[dim].declared) return Report(c, E0412, e->oppos, "a dimensao do resultado nao tem unidade declarada");
    e->ty = MakeType(T_REAL, dim);
    return true;
}

static bool CheckBinary(Ctx *c, Expr *e, Type ctx)
{
    if (!CheckOperands(c, e, ctx)) return false;
    if (e->op == OP_AND || e->op == OP_OR) return ApplyLogic(c, e);
    if (e->op >= OP_EQ && e->op <= OP_GE) return ApplyCompare(c, e);
    return ApplyArith(c, e);
}

static Type LiteralType(const Expr *e, Type ctx)
{
    if (e->kind == E_INT) return IsIntType(ctx) ? ctx : MakeType(T_I64, 0);
    return ctx.k == T_REAL ? ctx : MakeType(T_REAL, 0);
}

bool CheckExpr(Ctx *c, Expr *e, Type ctx)
{
    switch (e->kind) {
    case E_INT:
    case E_REAL: e->ty = LiteralType(e, ctx); return true;
    case E_BOOL: e->ty = MakeType(T_BOOL, 0); return true;
    case E_STR: e->ty = MakeType(T_STR, 0); return true;
    case E_NAME: return CheckName(c, e);
    case E_FIELD: return CheckField(c, e);
    case E_CALL: return CheckCall(c, e);
    case E_STRUCTLIT: return CheckStructLit(c, e);
    case E_UNARY: return CheckUnary(c, e, ctx);
    case E_BINARY: return CheckBinary(c, e, ctx);
    default: return CheckCast(c, e);
    }
}
