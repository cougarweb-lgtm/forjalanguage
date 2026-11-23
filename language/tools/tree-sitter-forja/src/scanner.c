/* scanner.c - scanner externo da gramatica tree-sitter da Forja: emite o token _nl (quebra de linha significativa).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - o token termina ANTES do '\n' (a quebra em si vira espaco/extras): o fim do no de um comando fica na linha
 *                    dele, entao end_line do CodeGraph sai certo. Antes o NL engolia as linhas em branco seguintes.
 *            0.1.0 - primeira versao.
 * Regras (espelham lexer.c do compilador): linhas em branco e comentarios '//' colapsam no mesmo NL (aqui por extras); o scanner
 * so age quando a gramatica admite _nl no ponto (valid_symbols); no fim do arquivo sem '\n' o NL tem largura zero.
 * O comentario na mesma linha do comando entra no token (espacos e '//' antes da quebra).
 * Verificacao: python verificar.py
 */
#include "tree_sitter/parser.h"

enum TipoToken { NL, ERRO_SENTINELA };

void *tree_sitter_forja_external_scanner_create(void) { return NULL; }
void tree_sitter_forja_external_scanner_destroy(void *payload) { (void)payload; }
unsigned tree_sitter_forja_external_scanner_serialize(void *payload, char *buffer) { (void)payload; (void)buffer; return 0; }
void tree_sitter_forja_external_scanner_deserialize(void *payload, const char *buffer, unsigned tamanho) { (void)payload; (void)buffer; (void)tamanho; }

/* Consome o resto de um comentario '//' (a primeira '/' ja foi lida). Devolve false se a '/' nao abre comentario. */
static bool consumir_comentario(TSLexer *lexer)
{
    if (lexer->lookahead != '/') return false;
    while (lexer->lookahead != '\n' && !lexer->eof(lexer)) lexer->advance(lexer, false);
    return true;
}

bool tree_sitter_forja_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols)
{
    (void)payload;
    /* recuperacao de erro: todos os simbolos ficam validos; nao emitir NL fantasma nem de largura zero em loop */
    if (!valid_symbols[NL] || valid_symbols[ERRO_SENTINELA]) return false;
    for (;;) {
        int32_t ch = lexer->lookahead;
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f') {
            lexer->advance(lexer, false);
        } else if (ch == '/') {
            lexer->advance(lexer, false);
            if (!consumir_comentario(lexer)) return false;
        } else if (ch == '\n' || lexer->eof(lexer)) {
            lexer->mark_end(lexer);
            lexer->result_symbol = NL;
            return true;
        } else {
            return false;
        }
    }
}
