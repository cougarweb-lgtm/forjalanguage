/* check_call.c - checagem de chamadas (regras de argumento), print, literal de struct e faixa constante.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"

static int32_t FindParam(const Decl *p, int32_t sym)
{
    for (int32_t i = 0; i < p->nparams; i++) {
        if (p->params[i].sym == sym) return i;
    }
    return -1;
}

/* Passo 1: estrutura dos argumentos (1o posicional, demais nomeados, ordem declarada). */
static bool BindArguments(Ctx *c, Expr *e, const Decl *p)
{
    int32_t last = -1;
    for (int32_t i = 0; i < e->nargs; i++) {
        Arg *a = &e->u.args[i];
        bool named = a->name_sym >= 0;
        if (i == 0) {
            if (named) return Report(c, E0502, a->name_pos, "o 1o argumento deve ser posicional");
            if (p->nparams == 0) return Report(c, E0502, a->val->pos, "o proc nao tem parametros");
            a->idx = last = 0;
            continue;
        }
        if (!named) return Report(c, E0502, a->val->pos, "argumento posicional depois do 1o; use nome = valor");
        int32_t idx = FindParam(p, a->name_sym);
        if (idx < 0) return Report(c, E0504, a->name_pos, "o proc nao tem o parametro '%s'", SymText(c, a->name_sym));
        if (idx <= last) return Report(c, E0501, a->name_pos, "argumento fora da ordem declarada ou repetido");
        a->idx = last = idx;
    }
    return true;
}

/* Passo 2: tipo de cada argumento ja ligado, da esquerda para a direita. */
static bool CheckArgTypes(Ctx *c, Expr *e, const Decl *p)
{
    for (int32_t i = 0; i < e->nargs; i++) {
        Arg *a = &e->u.args[i];
        Type want = p->params[a->idx].type.ty;
        if (!CheckExpr(c, a->val, want) || !CompatTypes(c, want, a->val->ty, a->val->pos)) return false;
    }
    return true;
}

/* Passo 3: parametro sem padrao e sem argumento. */
static bool CheckMissingArgs(Ctx *c, const Expr *e, const Decl *p)
{
    for (int32_t i = 0; i < p->nparams; i++) {
        bool bound = false;
        for (int32_t k = 0; k < e->nargs; k++) bound = bound || e->u.args[k].idx == i;
        if (!bound && p->params[i].def == NULL) {
            return Report(c, E0503, e->oppos, "falta o argumento '%s'", SymText(c, p->params[i].sym));
        }
    }
    return true;
}

static bool CheckPrint(Ctx *c, Expr *e)
{
    if (e->nargs != 1 || e->u.args[0].name_sym >= 0) {
        return Report(c, E0301, e->oppos, "print recebe exatamente 1 argumento posicional");
    }
    Expr *v = e->u.args[0].val;
    if (!CheckExpr(c, v, MakeType(T_VOID, 0))) return false;
    if (!IsIntType(v->ty) && v->ty.k != T_BOOL && v->ty.k != T_STR) {
        return Report(c, E0301, v->pos, "print so aceita i32, int, bool ou texto literal");
    }
    e->ty = MakeType(T_VOID, 0);
    return true;
}

bool CheckCall(Ctx *c, Expr *e)
{
    if (e->sym == SYM_PRINT) return CheckPrint(c, e);
    const Decl *p = c->syms[e->sym].proc;
    if (p == NULL) return Report(c, E0201, e->oppos, "proc '%s' nao declarado", SymText(c, e->sym));
    if (!BindArguments(c, e, p) || !CheckArgTypes(c, e, p) || !CheckMissingArgs(c, e, p)) return false;
    e->ty = p->has_ret ? p->ret.ty : MakeType(T_VOID, 0);
    return true;
}

static bool ConstIntValue(const Ctx *c, const Expr *e, int64_t *value)
{
    if (e->kind == E_INT) { *value = (int64_t)LiteralValue(c, e); return true; }
    if (e->kind == E_UNARY && e->op == OP_NEG && e->a->kind == E_INT) {
        *value = -(int64_t)LiteralValue(c, e->a);
        return true;
    }
    return false;
}

bool CheckLiteralRange(Ctx *c, const Field *f, const Expr *val)
{
    int64_t v;
    if (!f->has_range || !IsIntType(f->type.ty) || !ConstIntValue(c, val, &v)) return true;
    bool dentro = v >= f->lo && (f->excl ? v < f->hi : v <= f->hi);
    if (dentro) return true;
    return Report(c, E0420, val->pos, "valor %lld fora da faixa %lld%s%lld do campo '%s'", (long long)v,
                  (long long)f->lo, f->excl ? "..<" : "..=", (long long)f->hi, SymText(c, f->sym));
}

static bool CheckFieldInit(Ctx *c, Expr *e, const Decl *sd, int32_t i)
{
    Arg *a = &e->u.args[i];
    int32_t idx = FindField(sd, a->name_sym);
    if (idx < 0) return Report(c, E0504, a->name_pos, "a struct nao tem o campo '%s'", SymText(c, a->name_sym));
    for (int32_t k = 0; k < i; k++) {
        if (e->u.args[k].idx == idx) return Report(c, E0501, a->name_pos, "campo '%s' repetido", SymText(c, a->name_sym));
    }
    a->idx = idx;
    const Field *f = &sd->fields[idx];
    if (!CheckExpr(c, a->val, f->type.ty) || !CompatTypes(c, f->type.ty, a->val->ty, a->val->pos)) return false;
    return CheckLiteralRange(c, f, a->val);
}

bool CheckStructLit(Ctx *c, Expr *e)
{
    int32_t si = c->syms[e->sym].struct_idx;
    if (si < 0) return Report(c, E0201, e->oppos, "struct '%s' nao declarada", SymText(c, e->sym));
    const Decl *sd = c->structs[si];
    for (int32_t i = 0; i < e->nargs; i++) {
        if (!CheckFieldInit(c, e, sd, i)) return false;
    }
    e->ty = MakeType(T_STRUCT, si);
    return true;
}
