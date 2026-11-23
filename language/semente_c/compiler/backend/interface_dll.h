/* interface_dll.h - texto canonico da interface de uma DLL Forja (interface_dll.c) e o formato da struct que forja_dll_info devolve.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M8 H1 (D-M8-2): primeira versao.
 * Verificacao: python -I ..\tests\rodar_bateria.py --so interop (caso dinamica\dll_info)
 */
#ifndef FORJA_INTERFACE_DLL_H
#define FORJA_INTERFACE_DLL_H

#include "common/buf.h"
#include "common/intern.h"
#include "ir/ir.h"
#include "semantic/tipos.h"

/* forja_dll_info() devolve um ponteiro para esta struct estatica (.rdata, 32 bytes, little endian): magic u32, formato u32, hash_interface u64, texto u64 (endereco
 * do texto, com NUL depois do ultimo byte), texto_len u64. */
#define DLL_INFO_MAGIC 0x494A4446u /* "FDJI" lido em little endian */
#define DLL_INFO_FORMATO 1u
#define DLL_INFO_TAM 32u
#define DLL_INFO_OFF_TEXTO 16u

/* Texto canonico: uma linha por 'pub export proc' em ordem de nome, "nome(T1, T2) -> R" (sem nome de parametro: renomear nao muda a ABI), e uma linha por tipo
 * alcancado, em ordem de nome: "struct N size S align A { campo: T @off; }", "enum N: i32 { m = v; }" ou "struct N opaque". Mesma IR = mesmo texto. */
void EmitirInterfaceDll(const IrPrograma *prog, const Tipos *tipos, const Interner *nomes, Buf *saida);

#endif
