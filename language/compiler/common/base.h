/* base.h - includes padrao, falha interna e relogio do compilador Forja.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Fase 5a: base comum, reescrita a partir das pecas do prototipo do benchmark (ADR-01).
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_BASE_H
#define FORJA_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Erro interno do compilador (bug ou memoria esgotada): imprime no stderr e sai com codigo 3. */
_Noreturn void FalhaInterna(const char *fmt, ...) __attribute__((format(gnu_printf, 1, 2)));

/* Relogio monotonico em nanossegundos, para --tempos. */
uint64_t RelogioNs(void);

#endif
