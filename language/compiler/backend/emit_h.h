/* emit_h.h - cabecalho C da DLL (emit_h.c): um prototipo por 'pub export proc', ContarExportados e ExportaCstr.
 * Sobrou do emissor C quando ele saiu (0.21.0 F4 passo 2a): o .h da DLL e o unico uso do IR que ficou nesta pasta.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - 0.21.0 F4 passo 2a: a interface do emit_c.h que o emit_h.c usa vem para ca (EMIT_H_VERSAO 0.12.0, o mesmo valor: o .h gerado nao muda de bytes).
 * Verificacao: python ..\tests\cli\verificar_cli.py --forja <exe> (o .h passa em gcc -fsyntax-only e no FFI::cdef do PHP)
 */
#ifndef FORJA_EMIT_H_H
#define FORJA_EMIT_H_H

#include "common/buf.h"
#include "common/intern.h"
#include "ir/ir.h"
#include "semantic/tipos.h"

/* Versao do emissor que o cabecalho registra no comentario da 1a linha ("Gerado pela Forja (emit_c 0.12.0)"). */
#define EMIT_H_VERSAO "0.12.0"

/* emit_h.c: cabecalho C da DLL (um prototipo por 'pub export proc'), sem nenhuma diretiva '#': o FFI::cdef do PHP recusa
 * pre-processador. Sem export = 0 prototipos. */
void EmitirCabecalho(const IrPrograma *prog, const Tipos *tipos, const Interner *nomes, Buf *saida);
uint32_t ContarExportados(const IrPrograma *prog);
/* Algum 'pub export proc' devolve cstr ou ?cstr: a DLL ganha forja_liberar (v1.7, D20). */
bool ExportaCstr(const IrPrograma *prog, const Tipos *tipos);

#endif
