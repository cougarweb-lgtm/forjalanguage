/* parser_decl.c - parser das declaracoes de topo: unit, const, struct e proc.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "parser_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Decl *NewDecl(Ctx *c, DeclKind kind, uint32_t pos)
{
    Decl *d = ArenaAlloc(c, sizeof(Decl));
    d->kind = (uint8_t)kind;
    d->pos = pos;
    d->sym = -1;
    if (c->last_decl == NULL) c->first_decl = d; else c->last_decl->next = d;
    c->last_decl = d;
    return d;
}

/* Cresce *arr (malloc proprio) para caber 'need' elementos de 'size' bytes. */
static bool GrowList(void **arr, int32_t *cap, int32_t need, size_t size)
{
    if (need <= *cap) return true;
    int32_t novo_cap = *cap ? *cap * 2 : 8;
    void *novo = realloc(*arr, (size_t)novo_cap * size);
    if (novo == NULL) {
        fprintf(stderr, "forja: memoria esgotada com %d elementos de %zu bytes\n", need, size);
        return false;
    }
    *arr = novo;
    *cap = novo_cap;
    return true;
}

static bool ParseUnit(Ctx *c)
{
    Decl *d = NewDecl(c, D_UNIT_BASE, Cur(c)->off);
    c->cur++;
    if (!ParseIdent(c, &d->sym, &d->pos, "nome da unidade")) return false;
    if (CurKind(c) == TK_COLON) {
        c->cur++;
        if (CurKind(c) != TK_IDENT || Cur(c)->sym != SYM_F32) return SyntaxError(c, "'f32'");
        c->cur++;
        return ExpectEnd(c);
    }
    if (!Expect(c, TK_ASSIGN, "':' ou '='")) return false;
    d->kind = D_UNIT_DERIV;
    if (!ParseIdent(c, &d->sym_a, &d->pos_a, "nome de unidade")) return false;
    if (CurKind(c) != TK_STAR && CurKind(c) != TK_SLASH) return SyntaxError(c, "'*' ou '/'");
    d->op = (uint8_t)(CurKind(c) == TK_STAR ? OP_MUL : OP_DIV);
    c->cur++;
    return ParseIdent(c, &d->sym_b, &d->pos_b, "nome de unidade") && ExpectEnd(c);
}

static bool ParseConst(Ctx *c)
{
    Decl *d = NewDecl(c, D_CONST, Cur(c)->off);
    c->cur++;
    if (!ParseIdent(c, &d->sym, &d->pos, "nome da constante")) return false;
    if (!Expect(c, TK_COLON, "':'") || !ParseTypeRef(c, &d->type, false)) return false;
    if (!Expect(c, TK_ASSIGN, "'='")) return false;
    d->init = ParseExpr(c);
    return d->init != NULL && ExpectEnd(c);
}

static bool ParseLimit(Ctx *c, int64_t *value)
{
    bool neg = CurKind(c) == TK_MINUS;
    if (neg) c->cur++;
    if (CurKind(c) != TK_INT) return SyntaxError(c, "inteiro");
    uint64_t v = 0;
    for (uint32_t i = 0; i < Cur(c)->len; i++) v = v * 10 + (uint64_t)(c->src[Cur(c)->off + i] - '0');
    *value = neg ? -(int64_t)v : (int64_t)v;
    c->cur++;
    return true;
}

static bool ParseFieldRange(Ctx *c, Field *f)
{
    f->has_range = true;
    c->cur++;
    if (!ParseLimit(c, &f->lo)) return false;
    if (CurKind(c) != TK_DOTDOTEQ && CurKind(c) != TK_DOTDOTLT) return SyntaxError(c, "'..=' ou '..<'");
    f->excl = CurKind(c) == TK_DOTDOTLT;
    c->cur++;
    return ParseLimit(c, &f->hi);
}

static bool ParseField(Ctx *c, Field *f)
{
    memset(f, 0, sizeof *f);
    if (!ParseIdent(c, &f->sym, &f->pos, "nome do campo")) return false;
    if (!Expect(c, TK_COLON, "':'") || !ParseTypeRef(c, &f->type, false)) return false;
    if (CurKind(c) == KW_IN && !ParseFieldRange(c, f)) return false;
    if (CurKind(c) == TK_ASSIGN) {
        c->cur++;
        f->def = ParseExpr(c);
        if (f->def == NULL) return false;
    }
    return ExpectEnd(c);
}

static bool ParseStruct(Ctx *c)
{
    Decl *d = NewDecl(c, D_STRUCT, Cur(c)->off);
    Field *list = NULL;
    int32_t cap = 0;
    c->cur++;
    if (!ParseIdent(c, &d->sym, &d->pos, "nome da struct")) return false;
    if (!Expect(c, TK_LBRACE, "'{'") || !Expect(c, TK_NL, "fim de linha depois de '{'")) return false;
    while (CurKind(c) != TK_RBRACE) {
        if (CurKind(c) != TK_IDENT) return SyntaxError(c, "campo ou '}'");
        if (!GrowList((void **)&list, &cap, d->nfields + 1, sizeof(Field))) return false;
        if (!ParseField(c, &list[d->nfields])) return false;
        d->nfields++;
    }
    d->fields = ArenaAlloc(c, (size_t)(d->nfields ? d->nfields : 1) * sizeof(Field));
    if (d->nfields) memcpy(d->fields, list, (size_t)d->nfields * sizeof(Field));
    free(list);
    c->cur++;
    return ExpectEnd(c);
}

static bool ParseParam(Ctx *c, Param *p)
{
    memset(p, 0, sizeof *p);
    if (!ParseIdent(c, &p->sym, &p->pos, "nome do parametro")) return false;
    if (!Expect(c, TK_COLON, "':'") || !ParseTypeRef(c, &p->type, true)) return false;
    if (CurKind(c) == TK_ASSIGN) {
        c->cur++;
        p->def = ParseExpr(c);
        if (p->def == NULL) return false;
    }
    return true;
}

static bool ParseParamList(Ctx *c, Decl *d)
{
    Param *list = NULL;
    int32_t cap = 0;
    while (CurKind(c) != TK_RPAREN) {
        if (!GrowList((void **)&list, &cap, d->nparams + 1, sizeof(Param))) return false;
        if (!ParseParam(c, &list[d->nparams])) return false;
        d->nparams++;
        if (CurKind(c) != TK_COMMA) break;
        c->cur++;
    }
    d->params = ArenaAlloc(c, (size_t)(d->nparams ? d->nparams : 1) * sizeof(Param));
    if (d->nparams) memcpy(d->params, list, (size_t)d->nparams * sizeof(Param));
    free(list);
    return Expect(c, TK_RPAREN, "')'");
}

static bool ParseProc(Ctx *c)
{
    Decl *d = NewDecl(c, D_PROC, Cur(c)->off);
    c->cur++;
    if (!ParseIdent(c, &d->sym, &d->pos, "nome do proc")) return false;
    if (!Expect(c, TK_LPAREN, "'('") || !ParseParamList(c, d)) return false;
    if (CurKind(c) == TK_ARROW) {
        c->cur++;
        d->has_ret = true;
        if (!ParseTypeRef(c, &d->ret, false)) return false;
    }
    if (!Expect(c, TK_LBRACE, "'{'") || !Expect(c, TK_NL, "fim de linha depois de '{'")) return false;
    if (!ParseBlock(c, &d->body)) return false;
    d->close_pos = Cur(c)->off;
    c->cur++;
    return ExpectEnd(c);
}

bool ParseDeclaration(Ctx *c)
{
    switch (CurKind(c)) {
    case KW_UNIT: return ParseUnit(c);
    case KW_CONST: return ParseConst(c);
    case KW_STRUCT: return ParseStruct(c);
    case KW_PROC: return ParseProc(c);
    default: return SyntaxError(c, "declaracao (unit, const, struct ou proc)");
    }
}
