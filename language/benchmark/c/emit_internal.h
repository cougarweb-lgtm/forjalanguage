/* emit_internal.h - funcoes compartilhadas entre os arquivos de emissao de C11.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#ifndef EMIT_INTERNAL_H
#define EMIT_INTERNAL_H

#include "forja.h"

bool EmitName(Ctx *c, const char *prefix, int32_t sym);
bool EmitType(Ctx *c, Type t);
bool EmitZero(Ctx *c, Type t);
bool EmitExpr(Ctx *c, const Expr *e);
bool EmitStmts(Ctx *c, const Stmt *s);

#endif
