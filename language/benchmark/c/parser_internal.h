/* parser_internal.h - funcoes compartilhadas entre parser.c (expressoes) e parser_decl.c (declaracoes e comandos).
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "forja.h"

Token *Cur(Ctx *c);
TokKind CurKind(Ctx *c);
bool SyntaxError(Ctx *c, const char *esperado);
bool Expect(Ctx *c, TokKind kind, const char *esperado);
bool ExpectEnd(Ctx *c);
bool ParseIdent(Ctx *c, int32_t *sym, uint32_t *pos, const char *esperado);
Expr *NewExpr(Ctx *c, ExprKind kind, uint32_t pos);
Expr *ParseExpr(Ctx *c);
Expr *ParsePostfix(Ctx *c);
bool ParseDeclaration(Ctx *c);
bool ParseBlock(Ctx *c, Stmt **first);
bool ParseTypeRef(Ctx *c, TypeRef *t, bool allow_ptr);

#endif
