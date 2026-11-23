/* check_decl.c - checagem das declaracoes (passe A) e orquestracao da analise (passes A, B e main).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"

#include <stdio.h>
#include <stdlib.h>

static bool LookupUnitDim(Ctx *c, int32_t sym, uint32_t pos, int32_t *dim)
{
    *dim = c->syms[sym].unit_dim;
    if (*dim >= 0) return true;
    return Report(c, E0201, pos, "unidade '%s' nao declarada", SymText(c, sym));
}

bool CheckUnitDecl(Ctx *c, Decl *d)
{
    int32_t dim;
    if (d->kind == D_UNIT_BASE) {
        if (!AddDimBase(c, &dim)) return false;
    } else {
        int32_t a, b;
        if (!LookupUnitDim(c, d->sym_a, d->pos_a, &a) || !LookupUnitDim(c, d->sym_b, d->pos_b, &b)) return false;
        dim = DimCombine(c, a, b, d->op == OP_MUL ? 1 : -1);
        if (dim < 0) return false;
        c->dims[dim].declared = true;
    }
    c->syms[d->sym].unit_dim = dim;
    return true;
}

bool CheckConstExprAgainst(Ctx *c, Expr *e, Type want)
{
    return CheckExpr(c, e, want) && CompatTypes(c, want, e->ty, e->pos);
}

bool CheckConstDecl(Ctx *c, Decl *d)
{
    if (!ResolveTypeRef(c, &d->type)) return false;
    if (!CheckConstExprAgainst(c, d->init, d->type.ty)) return false;
    c->syms[d->sym].cnst = d;
    return true;
}

static bool RegisterStruct(Ctx *c, Decl *d)
{
    if (c->nstructs == c->capstructs) {
        int32_t cap = c->capstructs ? c->capstructs * 2 : 16;
        Decl **novo = realloc(c->structs, (size_t)cap * sizeof(Decl *));
        if (novo == NULL) {
            fprintf(stderr, "forja: memoria esgotada com %d structs\n", c->nstructs);
            return false;
        }
        c->structs = novo;
        c->capstructs = cap;
    }
    d->index = c->nstructs;
    c->structs[c->nstructs++] = d;
    c->syms[d->sym].struct_idx = d->index;
    return true;
}

static bool CheckFieldDecl(Ctx *c, Field *f)
{
    if (!ResolveTypeRef(c, &f->type)) return false;
    if (f->def == NULL) return true;
    return CheckConstExprAgainst(c, f->def, f->type.ty) && CheckLiteralRange(c, f, f->def);
}

bool CheckStructDecl(Ctx *c, Decl *d)
{
    for (int32_t i = 0; i < d->nfields; i++) {
        if (!CheckFieldDecl(c, &d->fields[i])) return false;
    }
    return RegisterStruct(c, d);
}

bool CheckProcSignature(Ctx *c, Decl *d)
{
    for (int32_t i = 0; i < d->nparams; i++) {
        Param *p = &d->params[i];
        if (!ResolveTypeRef(c, &p->type)) return false;
        if (p->def != NULL && !CheckConstExprAgainst(c, p->def, p->type.ty)) return false;
    }
    if (d->has_ret && !ResolveTypeRef(c, &d->ret)) return false;
    c->syms[d->sym].proc = d;
    return true;
}

static bool CheckDeclaration(Ctx *c, Decl *d)
{
    switch (d->kind) {
    case D_UNIT_BASE:
    case D_UNIT_DERIV: return CheckUnitDecl(c, d);
    case D_CONST: return CheckConstDecl(c, d);
    case D_STRUCT: return CheckStructDecl(c, d);
    default: return CheckProcSignature(c, d);
    }
}

/* Passe A por categoria (adendo 1.0.1): cada categoria na ordem do fonte. */
static bool CheckCategory(Ctx *c, DeclKind first, DeclKind last)
{
    for (Decl *d = c->first_decl; d != NULL; d = d->next) {
        if (d->kind >= first && d->kind <= last && !CheckDeclaration(c, d)) return false;
    }
    return true;
}

static bool CheckMainExists(Ctx *c)
{
    const Decl *m = c->syms[SYM_MAIN].proc;
    if (m != NULL && m->nparams == 0 && !m->has_ret) return true;
    return Report(c, E0201, 0, "falta 'proc main()' sem parametros e sem retorno");
}

bool CheckFile(Ctx *c)
{
    Dim adimensional = {{0}, true};
    c->dims = malloc(16 * sizeof(Dim));
    if (c->dims == NULL) return false;
    c->capdims = 16;
    c->dims[0] = adimensional;
    c->ndims = 1;
    if (!CheckCategory(c, D_UNIT_BASE, D_UNIT_DERIV) || !CheckCategory(c, D_CONST, D_CONST)) return false;
    if (!CheckCategory(c, D_STRUCT, D_STRUCT) || !CheckCategory(c, D_PROC, D_PROC)) return false;
    for (Decl *d = c->first_decl; d != NULL; d = d->next) {
        if (d->kind == D_PROC && !CheckProcBody(c, d)) return false;
    }
    return CheckMainExists(c);
}
