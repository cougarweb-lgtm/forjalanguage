/* escopo.h - escopo em pilha dos locais de um proc/test (parametros, let, var, for, ligacao de switch).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - B01a: ORIGEM_LITERAL = 7 e ORIGEM_C = 8 (ALOC_PARAM, em ast.h, passa a ser o 6); os dois seguem depois de todo ALOC_*.
 *            0.4.1 - Linguagem v1.11 L10a-bis: LocalInfo.grupo (var multipla: o E0707 vale pela declaracao inteira).
 *            0.4.0 - Linguagem v1.11 L10a: LocalInfo.pos e LocalInfo.usos (LOCAL_LIDO, LOCAL_ALTERADO) e EscopoBuscarMut: a higiene le as flags ao fechar o bloco.
 *            0.3.0 -Linguagem v1.8b P6: ORIGEM_C = 7 (texto devolvido por proc foreign).
 *            0.2.0 - Linguagem v1.8 P6a: LocalInfo.origem (AlocacaoTipo do new/make/format que criou o valor; ORIGEM_LITERAL = 6).
 *            0.1.0 - Fase 5c: busca por nome em O(1) (cabeca da cadeia por nome); sombra e decidida por quem chama (a semantica a proibe).
 * Verificacao: construir.bat && teste_common.exe && python ..\tests\semantica\verificar_semantica.py
 */
#ifndef FORJA_ESCOPO_H
#define FORJA_ESCOPO_H

#include "common/arena.h"
#include "common/vec.h"
#include "frontend/ast.h"

typedef enum { LOC_LET, LOC_VAR, LOC_PARAM, LOC_FOR, LOC_LIGACAO } LocalNatureza;

enum { ORIGEM_LITERAL = ALOC_PARAM + 1, ORIGEM_C }; /* v1.8: depois dos valores de AlocacaoTipo (ALOC_*, ast.h); v1.8b: texto devolvido por proc foreign; B01a: 7 e 8 (ALOC_PARAM ocupa o 6) */

typedef struct {
    int32_t nome;
    TipoId tipo;
    uint8_t natureza;  /* LocalNatureza: so LOC_VAR aceita atribuicao direta */
    uint8_t origem;    /* v1.8: ALOC_* do valor guardado pelo let/var, ORIGEM_LITERAL (texto literal) ou 0 */
    int32_t id;       /* numero do local no proc (Stmt.local_id, Caso.local_id; parametros: posicao) */
    int32_t anterior;  /* cabeca anterior da cadeia do mesmo nome (indice+1 na pilha), 0 = nenhuma */
    uint32_t pos;      /* v1.11 L10a: offset do nome na declaracao (onde E0707/E0708 apontam) */
    uint8_t usos;      /* v1.11 L10a: LOCAL_LIDO | LOCAL_ALTERADO (higiene, sem_higiene.c) */
    int32_t grupo;     /* v1.11 L10a-bis: 'var a, b = f()' e uma declaracao so: os nomes dela dividem o grupo (id do 1o + 1); 0 = declaracao de um nome */
} LocalInfo;

enum { LOCAL_LIDO = 1, LOCAL_ALTERADO = 2 }; /* v1.11 L10a: lido = qualquer uso em expressao; alterado = atribuicao, '&', fatia, for &item, append */

typedef struct {
    Vec pilha;          /* LocalInfo */
    int32_t *cabeca;    /* por id de nome: indice+1 do local visivel mais recente, 0 = nenhum */
} Escopo;

void EscopoIniciar(Escopo *e, Arena *arena, uint32_t qtd_nomes);

static inline uint32_t EscopoAltura(const Escopo *e) { return (uint32_t)e->pilha.n; }

/* Local visivel com esse nome, ou NULL. */
const LocalInfo *EscopoBuscar(const Escopo *e, int32_t nome);

/* v1.11 L10a: o mesmo local, para marcar LocalInfo.usos (um acesso a cabeca, sem varredura). */
static inline LocalInfo *EscopoBuscarMut(Escopo *e, int32_t nome)
{
    return (LocalInfo *)EscopoBuscar(e, nome);
}

void EscopoEmpurrar(Escopo *e, const LocalInfo *local);

/* Descarta os locais acima de 'altura' (fim de bloco). */
void EscopoRecuar(Escopo *e, uint32_t altura);

void EscopoLiberar(Escopo *e);

#endif
