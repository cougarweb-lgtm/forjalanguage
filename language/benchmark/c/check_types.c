/* check_types.c - tipos, dimensoes de unidades, escopo de locais e compatibilidade.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "check_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Type MakeType(TypeKind k, int32_t id)
{
    Type t = {(uint8_t)k, id};
    return t;
}

bool TypeEq(Type a, Type b) { return a.k == b.k && a.id == b.id; }
bool IsNumeric(Type t) { return t.k == T_I32 || t.k == T_I64 || t.k == T_REAL; }
bool IsIntType(Type t) { return t.k == T_I32 || t.k == T_I64; }

const char *SymText(const Ctx *c, int32_t sym) { return c->syms[sym].s; }

void TypeDescr(const Ctx *c, Type t, char *out, size_t n)
{
    switch (t.k) {
    case T_I32: snprintf(out, n, "i32"); break;
    case T_I64: snprintf(out, n, "int"); break;
    case T_BOOL: snprintf(out, n, "bool"); break;
    case T_REAL:
        if (t.id == 0) snprintf(out, n, "f32");
        else snprintf(out, n, "unidade de dimensao #%d", t.id);
        break;
    case T_STRUCT:
    case T_PTR: {
        const Sym *y = &c->syms[c->structs[t.id]->sym];
        snprintf(out, n, "%s%.*s", t.k == T_PTR ? "^" : "", (int)y->len, y->s);
        break;
    }
    default: snprintf(out, n, "(sem valor)"); break;
    }
}

static int32_t AppendDim(Ctx *c, const Dim *d)
{
    if (c->ndims == c->capdims) {
        int32_t cap = c->capdims ? c->capdims * 2 : 16;
        Dim *novo = realloc(c->dims, (size_t)cap * sizeof(Dim));
        if (novo == NULL) return -1;
        c->dims = novo;
        c->capdims = cap;
    }
    c->dims[c->ndims] = *d;
    return c->ndims++;
}

static int32_t InternDim(Ctx *c, const Dim *d)
{
    for (int32_t i = 0; i < c->ndims; i++) {
        if (memcmp(c->dims[i].e, d->e, MAX_DIMBASE) == 0) return i;
    }
    return AppendDim(c, d);
}

/* sign = +1 multiplica as dimensoes, -1 divide. Dimensao nova nasce sem unidade declarada. */
int32_t DimCombine(Ctx *c, int32_t a, int32_t b, int sign)
{
    Dim r;
    memset(&r, 0, sizeof r);
    for (int i = 0; i < MAX_DIMBASE; i++) {
        r.e[i] = (int8_t)(c->dims[a].e[i] + sign * c->dims[b].e[i]);
    }
    return InternDim(c, &r);
}

bool AddDimBase(Ctx *c, int32_t *dim)
{
    Dim d;
    memset(&d, 0, sizeof d);
    if (c->nbase == MAX_DIMBASE) {
        fprintf(stderr, "forja: limite de %d unidades base excedido\n", MAX_DIMBASE);
        return false;
    }
    d.e[c->nbase++] = 1;
    d.declared = true;
    *dim = AppendDim(c, &d);
    return *dim >= 0;
}

void ScopePush(Ctx *c, int32_t sym, Type ty)
{
    if (c->nscope == c->capscope) {
        c->capscope = c->capscope ? c->capscope * 2 : 64;
        c->scope = realloc(c->scope, (size_t)c->capscope * sizeof(ScopeEntry));
        if (c->scope == NULL) {
            fprintf(stderr, "forja: memoria esgotada no escopo (%d entradas)\n", c->capscope);
            exit(3);
        }
    }
    Local *l = ArenaAlloc(c, sizeof(Local));
    l->ty = ty;
    c->scope[c->nscope].sym = sym;
    c->scope[c->nscope].prev = c->syms[sym].local;
    c->nscope++;
    c->syms[sym].local = l;
}

void ScopePop(Ctx *c, int32_t height)
{
    while (c->nscope > height) {
        c->nscope--;
        c->syms[c->scope[c->nscope].sym].local = c->scope[c->nscope].prev;
    }
}

static bool ResolveBaseType(Ctx *c, TypeRef *t)
{
    switch (t->sym) {
    case SYM_I32: t->ty = MakeType(T_I32, 0); return true;
    case SYM_INT: t->ty = MakeType(T_I64, 0); return true;
    case SYM_F32: t->ty = MakeType(T_REAL, 0); return true;
    case SYM_BOOL: t->ty = MakeType(T_BOOL, 0); return true;
    default: break;
    }
    const Sym *y = &c->syms[t->sym];
    if (y->unit_dim >= 0) { t->ty = MakeType(T_REAL, y->unit_dim); return true; }
    if (y->struct_idx >= 0) { t->ty = MakeType(T_STRUCT, y->struct_idx); return true; }
    return Report(c, E0201, t->pos, "tipo '%.*s' nao declarado", (int)y->len, y->s);
}

bool ResolveTypeRef(Ctx *c, TypeRef *t)
{
    if (!ResolveBaseType(c, t)) return false;
    if (!t->ptr) return true;
    if (t->ty.k != T_STRUCT) return Report(c, E0301, t->pos, "'^' so vale sobre struct");
    t->ty.k = T_PTR;
    return true;
}

bool CompatTypes(Ctx *c, Type want, Type got, uint32_t pos)
{
    if (TypeEq(want, got)) return true;
    char esperado[96], recebido[96];
    TypeDescr(c, want, esperado, sizeof esperado);
    TypeDescr(c, got, recebido, sizeof recebido);
    if (want.k == T_REAL && got.k == T_REAL) {
        return Report(c, E0412, pos, "dimensoes diferentes: esperado %s, recebido %s", esperado, recebido);
    }
    return Report(c, E0301, pos, "tipo diferente: esperado %s, recebido %s", esperado, recebido);
}
