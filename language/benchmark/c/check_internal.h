/* check_internal.h - funcoes compartilhadas entre os arquivos da checagem semantica.
 * Versao: 1.0.0
 * Changelog: 1.0.0 - primeira versao para o benchmark do ADR-01.
 * Verificacao: construir.bat && python ../medir/rodar_suite.py --compilador "<pasta>/forja.exe" --corpus ../corpus/dourado --registro _tentativas.jsonl
 */
#ifndef CHECK_INTERNAL_H
#define CHECK_INTERNAL_H

#include "forja.h"

void TypeDescr(const Ctx *c, Type t, char *out, size_t n);
bool AddDimBase(Ctx *c, int32_t *dim);
bool CheckCall(Ctx *c, Expr *e);
bool CheckStructLit(Ctx *c, Expr *e);
bool CheckLiteralRange(Ctx *c, const Field *f, const Expr *val);
bool CheckUnitDecl(Ctx *c, Decl *d);
bool CheckConstDecl(Ctx *c, Decl *d);
bool CheckStructDecl(Ctx *c, Decl *d);
bool CheckProcSignature(Ctx *c, Decl *d);
bool CheckProcBody(Ctx *c, Decl *d);
int32_t FindField(const Decl *sd, int32_t sym);

#endif
