/* hash.h - Hash64: FNV-1a de 64 bits sobre bytes (chave de cache e linha do .entradas do orquestrador de projeto).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1a: Hash64 e HASH64_SEMENTE.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_HASH_H
#define FORJA_HASH_H

#include "common/base.h"

/* Base do FNV-1a de 64 bits: o hash de zero bytes. */
#define HASH64_SEMENTE UINT64_C(0xcbf29ce484222325)

/* FNV-1a 64: continua a partir de 'semente' (HASH64_SEMENTE no primeiro bloco; o resultado de um bloco serve de semente do seguinte).
 * Vetores: Hash64("", 0, HASH64_SEMENTE) = cbf29ce484222325 e Hash64("a", 1, HASH64_SEMENTE) = af63dc4c8601ec8c. Nao e criptografico. */
uint64_t Hash64(const void *dados, size_t n, uint64_t semente);

#endif
