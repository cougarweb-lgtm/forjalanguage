/* intern.h - internacao de nomes: cada texto distinto vira um int32 estavel (comparacao por inteiro).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M2 P17c: InternerAchar (busca sem criar; o Objeto do backend nativo acha simbolo por nome em tempo constante).
 *            0.1.0 - Fase 5a: tabela hash de enderecamento aberto (FNV-1a) do prototipo; os campos de
 *                    semantica (proc, local, struct...) saem daqui e vivem em semantic/.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_INTERN_H
#define FORJA_INTERN_H

#include "common/arena.h"
#include "common/vec.h"

typedef struct { const char *texto; uint32_t tam, hash; } Nome;
typedef struct { int32_t idx; uint32_t hash; } InternSlot; /* idx -1 = vazio; o hash evita tocar no Nome */

typedef struct {
    Arena *arena;   /* guarda os textos copiados */
    Vec nomes;      /* Nome, indexado pelo id devolvido por Internar */
    InternSlot *tabela;
    uint32_t tabela_cap;
} Interner;

void InternerIniciar(Interner *in, Arena *arena);

/* Id do texto (cria se for novo). Ids sao sequenciais a partir de 0, na ordem da 1a aparicao. */
int32_t Internar(Interner *in, const char *s, uint32_t tam);

/* Id do texto se ele ja foi internado; -1 se nao (nunca cria). */
int32_t InternerAchar(const Interner *in, const char *s, uint32_t tam);

/* Texto terminado em NUL e tamanho do id (id valido: 0 <= id < in->nomes.n). */
const char *InternerTexto(const Interner *in, int32_t id);
uint32_t InternerTam(const Interner *in, int32_t id);

void InternerLiberar(Interner *in);

#endif
