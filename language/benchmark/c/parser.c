/* parser.c - parser descendente recursivo da Forja: tokens -> AST (E0101 no primeiro erro).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "parser_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LEVEL_OR, LEVEL_AND, LEVEL_CMP, LEVEL_SUM, LEVEL_MUL, MAX_CALL_ARGS = 64 };

Token *Cur(Ctx *c) { return &c->toks[c->cur]; }
TokKind CurKind(Ctx *c) { return (TokKind)c->toks[c->cur].kind; }

bool SyntaxError(Ctx *c, const char *esperado)
{
    const Token *t = Cur(c);
    if (t->kind == TK_NL) return Report(c, E0101, t->off, "fim de linha inesperado; esperado %s", esperado);
    if (t->kind == TK_EOF) return Report(c, E0101, t->off, "fim de arquivo inesperado; esperado %s", esperado);
    return Report(c, E0101, t->off, "token inesperado '%.*s'; esperado %s", (int)t->len, c->src + t->off, esperado);
}

bool Expect(Ctx *c, TokKind kind, const char *esperado)
{
    if (CurKind(c) != kind) return SyntaxError(c, esperado);
    c->cur++;
    return true;
}

bool ExpectEnd(Ctx *c)
{
    if (CurKind(c) == TK_EOF) return true;
    return Expect(c, TK_NL, "fim de linha");
}

bool ParseIdent(Ctx *c, int32_t *sym, uint32_t *pos, const char *esperado)
{
    if (CurKind(c) != TK_IDENT) return SyntaxError(c, esperado);
    *sym = Cur(c)->sym;
    *pos = Cur(c)->off;
    c->cur++;
    return true;
}

Expr *NewExpr(Ctx *c, ExprKind kind, uint32_t pos)
{
    Expr *e = ArenaAlloc(c, sizeof(Expr));
    e->kind = (uint8_t)kind;
    e->pos = pos;
    e->oppos = pos;
    e->sym = -1;
    return e;
}

/* Indexada pelo token: nivel+1 (0 = nao e operador binario) e o operador. */
static const struct { uint8_t level_plus1, op; } BINARY_OPS[TK_AMP + 1] = {
    [KW_OR] = {LEVEL_OR + 1, OP_OR}, [KW_AND] = {LEVEL_AND + 1, OP_AND},
    [TK_EQ] = {LEVEL_CMP + 1, OP_EQ}, [TK_NE] = {LEVEL_CMP + 1, OP_NE}, [TK_LT] = {LEVEL_CMP + 1, OP_LT},
    [TK_LE] = {LEVEL_CMP + 1, OP_LE}, [TK_GT] = {LEVEL_CMP + 1, OP_GT}, [TK_GE] = {LEVEL_CMP + 1, OP_GE},
    [TK_PLUS] = {LEVEL_SUM + 1, OP_ADD}, [TK_MINUS] = {LEVEL_SUM + 1, OP_SUB},
    [TK_STAR] = {LEVEL_MUL + 1, OP_MUL}, [TK_SLASH] = {LEVEL_MUL + 1, OP_DIV}, [TK_PERCENT] = {LEVEL_MUL + 1, OP_MOD},
};

/* Nivel do operador binario no token atual, ou -1. */
static int BinaryLevelAt(Ctx *c, uint8_t *op)
{
    TokKind k = CurKind(c);
    *op = BINARY_OPS[k].op;
    return (int)BINARY_OPS[k].level_plus1 - 1;
}

static Expr *NewBinary(Ctx *c, uint8_t op, Expr *a, Expr *b, uint32_t oppos)
{
    Expr *e = NewExpr(c, E_BINARY, a->pos);
    e->op = op;
    e->a = a;
    e->u.b = b;
    e->oppos = oppos;
    e->flex = op >= OP_ADD && op <= OP_MOD && a->flex && b->flex;
    return e;
}

/* Lista de argumentos "[nome =] expr" ate o fechador; named_only vale para literal de struct. */
static bool ParseArgList(Ctx *c, Expr *e, TokKind closer, bool named_only)
{
    Arg tmp[MAX_CALL_ARGS];
    int32_t n = 0;
    int salvo = c->no_struct;
    c->no_struct = 0;
    while (CurKind(c) != closer) {
        if (n == MAX_CALL_ARGS) return Report(c, E0101, Cur(c)->off, "mais de %d argumentos", MAX_CALL_ARGS);
        Arg *a = &tmp[n++];
        memset(a, 0, sizeof *a);
        a->name_sym = -1;
        a->idx = -1;
        bool named = CurKind(c) == TK_IDENT && c->toks[c->cur + 1].kind == TK_ASSIGN;
        if (named_only && !named) return SyntaxError(c, "nome do campo seguido de '='");
        if (named) { a->name_sym = Cur(c)->sym; a->name_pos = Cur(c)->off; c->cur += 2; }
        a->val = ParseExpr(c);
        if (a->val == NULL) return false;
        if (CurKind(c) != TK_COMMA) break;
        c->cur++;
    }
    c->no_struct = salvo;
    if (!Expect(c, closer, closer == TK_RPAREN ? "')'" : "'}'")) return false;
    e->nargs = n;
    e->u.args = ArenaAlloc(c, (size_t)(n ? n : 1) * sizeof(Arg));
    memcpy(e->u.args, tmp, (size_t)n * sizeof(Arg));
    return true;
}

static Expr *ParseNameOrCall(Ctx *c)
{
    const Token *t = Cur(c);
    Expr *e = NewExpr(c, E_NAME, t->off);
    e->sym = t->sym;
    bool upper = c->src[t->off] >= 'A' && c->src[t->off] <= 'Z';
    c->cur++;
    if (CurKind(c) == TK_LPAREN) {
        e->kind = E_CALL;
        c->cur++;
        return ParseArgList(c, e, TK_RPAREN, false) ? e : NULL;
    }
    if (CurKind(c) == TK_LBRACE && upper && !c->no_struct) {
        e->kind = E_STRUCTLIT;
        c->cur++;
        return ParseArgList(c, e, TK_RBRACE, true) ? e : NULL;
    }
    return e;
}

static Expr *ParseParenthesized(Ctx *c)
{
    uint32_t pos = Cur(c)->off;
    int salvo = c->no_struct;
    c->cur++;
    c->no_struct = 0;
    Expr *e = ParseExpr(c);
    c->no_struct = salvo;
    if (e == NULL || !Expect(c, TK_RPAREN, "')'")) return NULL;
    e->pos = pos;
    return e;
}

static Expr *ParseLiteral(Ctx *c, ExprKind kind, uint8_t op)
{
    Expr *e = NewExpr(c, kind, Cur(c)->off);
    e->op = op;
    e->flex = kind == E_INT || kind == E_REAL;
    c->cur++;
    return e;
}

static Expr *ParsePrimary(Ctx *c)
{
    switch (CurKind(c)) {
    case TK_INT: return ParseLiteral(c, E_INT, 0);
    case TK_REAL: return ParseLiteral(c, E_REAL, 0);
    case TK_STR: return ParseLiteral(c, E_STR, 0);
    case KW_TRUE: return ParseLiteral(c, E_BOOL, OP_TRUE);
    case KW_FALSE: return ParseLiteral(c, E_BOOL, OP_FALSE);
    case TK_IDENT: return ParseNameOrCall(c);
    case TK_LPAREN: return ParseParenthesized(c);
    default: SyntaxError(c, "expressao"); return NULL;
    }
}

Expr *ParsePostfix(Ctx *c)
{
    Expr *e = ParsePrimary(c);
    while (e != NULL && CurKind(c) == TK_DOT) {
        c->cur++;
        Expr *campo = NewExpr(c, E_FIELD, e->pos);
        campo->a = e;
        if (!ParseIdent(c, &campo->sym, &campo->oppos, "nome do campo")) return NULL;
        e = campo;
    }
    return e;
}

static Expr *ParseUnary(Ctx *c)
{
    TokKind k = CurKind(c);
    if (k != TK_MINUS && k != KW_NOT && k != TK_AMP) return ParsePostfix(c);
    Expr *e = NewExpr(c, E_UNARY, Cur(c)->off);
    e->op = k == TK_MINUS ? OP_NEG : (k == KW_NOT ? OP_NOT : OP_ADDR);
    c->cur++;
    e->a = ParseUnary(c);
    if (e->a == NULL) return NULL;
    e->flex = e->op == OP_NEG && e->a->flex;
    return e;
}

static Expr *ParseCast(Ctx *c)
{
    Expr *e = ParseUnary(c);
    while (e != NULL && CurKind(c) == KW_AS) {
        c->cur++;
        Expr *cast = NewExpr(c, E_CAST, e->pos);
        cast->a = e;
        if (!ParseIdent(c, &cast->sym, &cast->oppos, "nome de tipo")) return NULL;
        e = cast;
    }
    return e;
}

/* Escalada de precedencia: so consome operadores de nivel >= min_level; comparacao nao encadeia. */
static Expr *ParseBinary(Ctx *c, int min_level)
{
    Expr *left = ParseCast(c);
    uint8_t op;
    int level;
    while (left != NULL && (level = BinaryLevelAt(c, &op)) >= min_level) {
        uint32_t oppos = Cur(c)->off;
        c->cur++;
        Expr *right = ParseBinary(c, level + 1);
        if (right == NULL) return NULL;
        left = NewBinary(c, op, left, right, oppos);
        if (level == LEVEL_CMP && BinaryLevelAt(c, &op) == LEVEL_CMP) break;
    }
    return left;
}

Expr *ParseExpr(Ctx *c)
{
    return ParseBinary(c, LEVEL_OR);
}

bool ParseFile(Ctx *c)
{
    c->cur = 0;
    while (CurKind(c) != TK_EOF) {
        if (!ParseDeclaration(c)) return false;
    }
    return true;
}
