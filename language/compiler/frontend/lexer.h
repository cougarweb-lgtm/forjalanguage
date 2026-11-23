/* lexer.h - tokenizacao do arquivo .fj inteiro (E0001 no primeiro erro).
 * Versao: 0.5.0
 * Changelog: 0.5.0 - IA1-L3d2: Lexer.comentarios e LexerComentario (o forja migrar --comentarios).
 *            0.4.0 - IA1-b15: Lexer.erros (varios E0001 por execucao: o lexer guarda o erro, pula o resto da linha e segue).
 *            0.3.0 - Linguagem v1.8 P1: campo inicio (offset onde o lexer comeca; 0 = do comeco).
 *            0.2.0 - LexerLerFTexto e LexerLerTokenSimples (lexer_ftexto.c).
 *            0.1.0 - Fase 5b: Lexer proprio (Fonte/Interner/Vec/Diag separados, sem o Ctx unico do prototipo).
 * Verificacao: construir.bat && forja.exe check <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#ifndef FORJA_LEXER_H
#define FORJA_LEXER_H

#include "common/diag.h"
#include "common/intern.h"
#include "frontend/token.h"

/* Um comentario '//': onde comeca (a barra) e onde termina (exclusivo: no LF ou no fim do texto; o CR de um CRLF fica dentro). */
typedef struct { uint32_t inicio, fim; } LexerComentario;

/* Entrada: fonte, nomes e diag (o chamador preenche). Saida: tokens (Vec de Token, termina em TK_EOF). */
typedef struct {
    const Fonte *fonte;
    Interner *nomes;
    Diag *diag;
    DiagLista *erros; /* IA1-b15: onde guardar cada E0001; NULL = o lexer para no 1o (o erro fica pendente em diag) */
    Vec *comentarios; /* IA1-L3d2: NULL = nao registra; senao um Vec de LexerComentario (o chamador faz VecIniciar), um por '//' */
    Vec tokens;
    uint32_t inicio; /* offset onde comeca a tokenizacao: 0 = do comeco; o trecho lexado e sempre o ultimo anexado a Fonte */
} Lexer;

/* Interna as palavras-chave e os nomes embutidos na ordem de token.h. Chamar uma vez, com o Interner vazio. */
void LexerNomesIniciais(Interner *nomes);

/* Tokeniza tudo. false = diagnostico em lx->diag (E0001). Os tokens ficam em lx->tokens; libere com LexerLiberar. */
bool LexerAnalisar(Lexer *lx);

void LexerLiberar(Lexer *lx);

/* v1.10: valor de um literal inteiro (decimal, 0x... ou 0b...) lido do texto. false = passa de 64 bits sem sinal. */
bool LexerLerInteiroU64(const char *texto, uint64_t *valor);

/* Internas entre lexer.c e lexer_ftexto.c. LexerLerFTexto le f"..." (texto[*i] = 'f'); LexerLerTokenSimples le um token dentro de {...}. */
__attribute__((cold)) bool LexerLerFTexto(Lexer *lx, uint32_t *i);
bool LexerLerTokenSimples(Lexer *lx, uint32_t *i);

#endif
