/* parser_stmt.c - parser dos comandos (let, var, atribuicao, if, while, for, return, chamada) e dos blocos.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "parser_internal.h"

static Stmt *ParseStmt(Ctx *c);

static Stmt *NewStmt(Ctx *c, StmtKind kind, uint32_t pos)
{
    Stmt *s = ArenaAlloc(c, sizeof(Stmt));
    s->kind = (uint8_t)kind;
    s->pos = pos;
    s->sym = -1;
    return s;
}

bool ParseTypeRef(Ctx *c, TypeRef *t, bool allow_ptr)
{
    t->ptr = false;
    if (allow_ptr && CurKind(c) == TK_CARET) {
        t->ptr = true;
        c->cur++;
    }
    return ParseIdent(c, &t->sym, &t->pos, "nome de tipo");
}

/* Cabecalho de if/while/for: '{' termina a expressao em vez de abrir literal de struct. */
static Expr *ParseHeaderExpr(Ctx *c)
{
    c->no_struct = 1;
    Expr *e = ParseExpr(c);
    c->no_struct = 0;
    return e;
}

static bool ParseBraceNewline(Ctx *c)
{
    return Expect(c, TK_LBRACE, "'{'") && Expect(c, TK_NL, "fim de linha depois de '{'");
}

/* Le comandos ate o '}' (que fica sem consumir) e encadeia em *first. */
bool ParseBlock(Ctx *c, Stmt **first)
{
    Stmt *last = NULL;
    *first = NULL;
    while (CurKind(c) != TK_RBRACE) {
        Stmt *s = ParseStmt(c);
        if (s == NULL) return false;
        if (last == NULL) *first = s; else last->next = s;
        last = s;
    }
    return true;
}

static Stmt *ParseLetVar(Ctx *c)
{
    Stmt *s = NewStmt(c, CurKind(c) == KW_LET ? S_LET : S_VAR, Cur(c)->off);
    uint32_t name_pos;
    c->cur++;
    if (!ParseIdent(c, &s->sym, &name_pos, "nome da variavel")) return NULL;
    if (CurKind(c) == TK_COLON) {
        c->cur++;
        s->has_type = true;
        if (!ParseTypeRef(c, &s->type, false)) return NULL;
    }
    if (!Expect(c, TK_ASSIGN, "'='")) return NULL;
    s->e = ParseExpr(c);
    return s->e != NULL && ExpectEnd(c) ? s : NULL;
}

/* Corpo '{' NL comandos '}' ; deixa o '}' consumido e devolve a posicao dele. */
static bool ParseBracedBlock(Ctx *c, Stmt **body, uint32_t *close_pos)
{
    if (!ParseBraceNewline(c) || !ParseBlock(c, body)) return false;
    if (close_pos != NULL) *close_pos = Cur(c)->off;
    c->cur++;
    return true;
}

static Stmt *ParseIf(Ctx *c)
{
    Stmt *s = NewStmt(c, S_IF, Cur(c)->off);
    c->cur++;
    s->e = ParseHeaderExpr(c);
    if (s->e == NULL || !ParseBracedBlock(c, &s->body, &s->close_pos)) return NULL;
    if (CurKind(c) != KW_ELSE) return ExpectEnd(c) ? s : NULL;
    c->cur++;
    if (CurKind(c) == KW_IF) {
        s->els = ParseIf(c);
        return s->els != NULL ? s : NULL;
    }
    if (!ParseBracedBlock(c, &s->els, NULL)) return NULL;
    return ExpectEnd(c) ? s : NULL;
}

static Stmt *ParseWhile(Ctx *c)
{
    Stmt *s = NewStmt(c, S_WHILE, Cur(c)->off);
    c->cur++;
    s->e = ParseHeaderExpr(c);
    if (s->e == NULL || !ParseBracedBlock(c, &s->body, NULL)) return NULL;
    return ExpectEnd(c) ? s : NULL;
}

static Stmt *ParseFor(Ctx *c)
{
    Stmt *s = NewStmt(c, S_FOR, Cur(c)->off);
    uint32_t name_pos;
    c->cur++;
    if (!ParseIdent(c, &s->sym, &name_pos, "nome da variavel do laco")) return NULL;
    if (!Expect(c, KW_IN, "'in'")) return NULL;
    s->e = ParseHeaderExpr(c);
    if (s->e == NULL || !Expect(c, TK_DOTDOTLT, "'..<'")) return NULL;
    s->e2 = ParseHeaderExpr(c);
    if (s->e2 == NULL || !ParseBracedBlock(c, &s->body, NULL)) return NULL;
    return ExpectEnd(c) ? s : NULL;
}

static Stmt *ParseReturn(Ctx *c)
{
    Stmt *s = NewStmt(c, S_RETURN, Cur(c)->off);
    c->cur++;
    if (CurKind(c) != TK_NL && CurKind(c) != TK_EOF) {
        s->e = ParseExpr(c);
        if (s->e == NULL) return NULL;
    }
    return ExpectEnd(c) ? s : NULL;
}

static Stmt *ParseCallStmt(Ctx *c)
{
    Stmt *s = NewStmt(c, S_CALL, Cur(c)->off);
    s->e = ParsePostfix(c);
    if (s->e == NULL) return NULL;
    if (s->e->kind != E_CALL) { SyntaxError(c, "chamada de proc"); return NULL; }
    return ExpectEnd(c) ? s : NULL;
}

/* lvalor := nome ("." nome)? seguido de '=', '+=' ou '-=' */
static Stmt *ParseAssign(Ctx *c)
{
    Stmt *s = NewStmt(c, S_ASSIGN, Cur(c)->off);
    s->lhs = ParsePostfix(c);
    if (s->lhs == NULL) return NULL;
    bool lvalor = s->lhs->kind == E_NAME || (s->lhs->kind == E_FIELD && s->lhs->a->kind == E_NAME);
    TokKind k = CurKind(c);
    if (!lvalor || (k != TK_ASSIGN && k != TK_PLUSEQ && k != TK_MINUSEQ)) {
        SyntaxError(c, "'=', '+=' ou '-='");
        return NULL;
    }
    s->op = (uint8_t)k;
    c->cur++;
    s->e = ParseExpr(c);
    return s->e != NULL && ExpectEnd(c) ? s : NULL;
}

static Stmt *ParseIdentStmt(Ctx *c)
{
    if (c->toks[c->cur + 1].kind == TK_LPAREN) return ParseCallStmt(c);
    return ParseAssign(c);
}

static Stmt *ParseStmt(Ctx *c)
{
    switch (CurKind(c)) {
    case KW_LET:
    case KW_VAR: return ParseLetVar(c);
    case KW_IF: return ParseIf(c);
    case KW_WHILE: return ParseWhile(c);
    case KW_FOR: return ParseFor(c);
    case KW_RETURN: return ParseReturn(c);
    case TK_IDENT: return ParseIdentStmt(c);
    default: SyntaxError(c, "comando"); return NULL;
    }
}
