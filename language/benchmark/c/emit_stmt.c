/* emit_stmt.c - emissao de C11 para comandos, declaracoes e o programa inteiro.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#include "forja.h"
#include "emit_internal.h"

#include <stdio.h>

static bool EmitStmt(Ctx *c, const Stmt *s);

static bool EmitLetVar(Ctx *c, const Stmt *s)
{
    return EmitType(c, s->ty) && BufCat(&c->out, " ") && EmitName(c, "v_", s->sym) && BufCat(&c->out, " = ")
           && EmitExpr(c, s->e) && BufCat(&c->out, ";\n");
}

static bool EmitAssign(Ctx *c, const Stmt *s)
{
    const char *op = s->op == TK_PLUSEQ ? " += " : (s->op == TK_MINUSEQ ? " -= " : " = ");
    return EmitExpr(c, s->lhs) && BufCat(&c->out, op) && EmitExpr(c, s->e) && BufCat(&c->out, ";\n");
}

static bool EmitCondition(Ctx *c, const char *keyword, const Expr *cond)
{
    return BufCat(&c->out, keyword) && BufCat(&c->out, " (") && EmitExpr(c, cond) && BufCat(&c->out, ") {\n");
}

static bool EmitIf(Ctx *c, const Stmt *s)
{
    if (!EmitCondition(c, "if", s->e) || !EmitStmts(c, s->body) || !BufCat(&c->out, "}")) return false;
    if (s->els != NULL) {
        if (!BufCat(&c->out, " else {\n") || !EmitStmts(c, s->els) || !BufCat(&c->out, "}")) return false;
    }
    return BufCat(&c->out, "\n");
}

static bool EmitFor(Ctx *c, const Stmt *s)
{
    int32_t n = c->for_counter++;
    if (!BufCat(&c->out, "{\n") || !EmitType(c, s->ty) || !BufPrintf(&c->out, " _fs%d = ", n)) return false;
    if (!EmitExpr(c, s->e) || !BufCat(&c->out, ";\n") || !EmitType(c, s->ty)) return false;
    if (!BufPrintf(&c->out, " _fe%d = ", n) || !EmitExpr(c, s->e2) || !BufCat(&c->out, ";\nfor (")) return false;
    if (!EmitType(c, s->ty) || !BufCat(&c->out, " ") || !EmitName(c, "v_", s->sym)) return false;
    if (!BufPrintf(&c->out, " = _fs%d; ", n) || !EmitName(c, "v_", s->sym)) return false;
    if (!BufPrintf(&c->out, " < _fe%d; ", n) || !EmitName(c, "v_", s->sym) || !BufCat(&c->out, "++) {\n")) return false;
    return EmitStmts(c, s->body) && BufCat(&c->out, "}\n}\n");
}

static bool EmitReturn(Ctx *c, const Stmt *s)
{
    if (s->e == NULL) return BufCat(&c->out, "return;\n");
    return BufCat(&c->out, "return ") && EmitExpr(c, s->e) && BufCat(&c->out, ";\n");
}

static bool EmitStmt(Ctx *c, const Stmt *s)
{
    switch (s->kind) {
    case S_LET:
    case S_VAR: return EmitLetVar(c, s);
    case S_ASSIGN: return EmitAssign(c, s);
    case S_IF: return EmitIf(c, s);
    case S_WHILE: return EmitCondition(c, "while", s->e) && EmitStmts(c, s->body) && BufCat(&c->out, "}\n");
    case S_FOR: return EmitFor(c, s);
    case S_RETURN: return EmitReturn(c, s);
    default: return EmitExpr(c, s->e) && BufCat(&c->out, ";\n");
    }
}

bool EmitStmts(Ctx *c, const Stmt *s)
{
    for (; s != NULL; s = s->next) {
        if (!EmitStmt(c, s)) return false;
    }
    return true;
}

static bool EmitStructDecl(Ctx *c, const Decl *d)
{
    if (!BufCat(&c->out, "typedef struct ") || !EmitName(c, "S_", d->sym) || !BufCat(&c->out, " {\n")) return false;
    if (d->nfields == 0 && !BufCat(&c->out, "char vazio_;\n")) return false;
    for (int32_t i = 0; i < d->nfields; i++) {
        if (!EmitType(c, d->fields[i].type.ty) || !BufCat(&c->out, " ") || !EmitName(c, "f_", d->fields[i].sym)) return false;
        if (!BufCat(&c->out, ";\n")) return false;
    }
    return BufCat(&c->out, "} ") && EmitName(c, "S_", d->sym) && BufCat(&c->out, ";\n");
}

static bool EmitConstDecl(Ctx *c, const Decl *d)
{
    return EmitName(c, "#define K_", d->sym) && BufCat(&c->out, " ((") && EmitType(c, d->type.ty) && BufCat(&c->out, ")(")
           && EmitExpr(c, d->init) && BufCat(&c->out, "))\n");
}

static bool EmitProcSignature(Ctx *c, const Decl *d)
{
    if (!BufCat(&c->out, "static ") || !EmitType(c, d->has_ret ? d->ret.ty : MakeType(T_VOID, 0))) return false;
    if (!BufCat(&c->out, " ") || !EmitName(c, "p_", d->sym) || !BufCat(&c->out, "(")) return false;
    if (d->nparams == 0 && !BufCat(&c->out, "void")) return false;
    for (int32_t i = 0; i < d->nparams; i++) {
        if (i > 0 && !BufCat(&c->out, ", ")) return false;
        if (!EmitType(c, d->params[i].type.ty) || !BufCat(&c->out, " ") || !EmitName(c, "v_", d->params[i].sym)) return false;
    }
    return BufCat(&c->out, ")");
}

static bool EmitProcBody(Ctx *c, const Decl *d)
{
    return EmitProcSignature(c, d) && BufCat(&c->out, " {\n") && EmitStmts(c, d->body) && BufCat(&c->out, "}\n");
}

static bool EmitHeaderDecls(Ctx *c)
{
    for (const Decl *d = c->first_decl; d != NULL; d = d->next) {
        if (d->kind == D_CONST && !EmitConstDecl(c, d)) return false;
        if (d->kind == D_STRUCT && !EmitStructDecl(c, d)) return false;
    }
    for (const Decl *d = c->first_decl; d != NULL; d = d->next) {
        if (d->kind == D_PROC && !(EmitProcSignature(c, d) && BufCat(&c->out, ";\n"))) return false;
    }
    return true;
}

bool EmitFile(Ctx *c)
{
    if (!BufPrepare(&c->out, c->src_len * 2 + 4096)) return false;
    if (!BufCat(&c->out, "#include <stdint.h>\n#include <stdbool.h>\n#include <stdio.h>\n")) return false;
    if (!EmitHeaderDecls(c)) return false;
    for (const Decl *d = c->first_decl; d != NULL; d = d->next) {
        if (d->kind == D_PROC && !EmitProcBody(c, d)) return false;
    }
    return BufCat(&c->out, "int main(void) {\np_main();\nreturn 0;\n}\n");
}
