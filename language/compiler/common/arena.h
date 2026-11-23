/* arena.h - alocador por blocos: aloca rapido, libera tudo de uma vez ao fim da compilacao.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - S4c: campo usado_primeiro (ver arena.c 0.3.0).
 *            0.1.0 - Fase 5a: separada do Ctx unico do prototipo; a arena agora e um objeto proprio.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_ARENA_H
#define FORJA_ARENA_H

#include "common/base.h"

typedef struct ArenaBloco ArenaBloco;
typedef struct { ArenaBloco *topo; size_t usado; size_t usado_primeiro; /* so do arena.c: bytes usados do 1o bloco depois que a arena passou para o 2o */ } Arena;

/* Devolve n bytes zerados e alinhados em 8. Sem memoria: FalhaInterna (nunca devolve NULL). */
void *ArenaAlocar(Arena *a, size_t n);

/* Copia n bytes de s para a arena e termina com NUL. */
char *ArenaCopiarTexto(Arena *a, const char *s, size_t n);

/* Libera todos os blocos; a arena volta ao estado vazio. */
void ArenaLiberar(Arena *a);

#endif
