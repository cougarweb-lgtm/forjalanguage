/* coff.h - leitor de objeto COFF (AMD64) para o ligador do backend nativo: le um .obj em memoria para um Objeto.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M3b P23d: CoffListarIndefinidos (so a tabela de simbolos: os externos indefinidos de um membro de .a, para escolher quais membros puxar).
 *            0.2.0 - M2 P17c: secao "<base>$<sufixo>" aceita (o '$' nao e mais recusado).
 *            0.1.0 - backend x64 P5: CoffLer (cabecalho 0x8664, nomes longos "/n", simbolos auxiliares pulados, BSS, adendo lido e zerado).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_COFF_H
#define FORJA_NATIVO_OBJ_COFF_H

#include <stddef.h>
#include <stdint.h>

#include "common/diag.h"
#include "nativo/obj/objeto.h"

/* Le o COFF de b (n bytes) para o Objeto o, que tem de vir de ObjIniciar. Devolve false com X0903 em diag quando o formato
 * nao e o que o backend aceita (maquina diferente de 0x8664, relocacao fora de ADDR64/ADDR32NB/REL32, etc.).
 * Em caso de falha o Objeto fica parcial: quem chamou libera com ObjLiberar. */
bool CoffLer(const uint8_t *b, size_t n, Objeto *o, Diag *erro);

/* Para onde vao os nomes: cada um e copiado para a arena e o const char * entra em lista (Vec iniciado por quem chama com VecIniciar(.., sizeof(const char *))). */
typedef struct {
    Arena *arena;
    Vec *lista;
} CoffDestinoNomes;

/* Os simbolos externos indefinidos do COFF (classe EXTERNAL, secao 0, valor 0; simbolo comum, weak external e local nao entram), na ordem da tabela. Nao le secoes nem
 * relocacoes: um membro com relocacao que o CoffLer ainda recusa tambem pode ser varrido. Maquina diferente de 0x8664 ou tabela fora do arquivo: false com X0903 em erro. */
bool CoffListarIndefinidos(const uint8_t *b, size_t n, const CoffDestinoNomes *destino, Diag *erro);

#endif
