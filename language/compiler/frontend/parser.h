/* parser.h - parser descendente recursivo da Forja: tokens -> AST (E0101 no primeiro erro).
 * Versao: 0.6.0
 * Changelog: 0.6.0 - Linguagem v1.13 L1a: Parser.arquivo_atual (trecho da Fonte; os apelidos de import sao por arquivo, modulo_atual segue sendo o pacote).
 *            0.5.0 - B12: Parser.cargas (Vec de Expr*, um por #load; NULL = #load e erro de sintaxe).
 *            0.4.0 - F1 P1a: Parser.chaves_literal (opcional; o forja fmt separa a '{' de literal da '{' de bloco). NULL = nada muda.
 *            0.3.0 - IA1-b15: Parser.erros (lista de diagnosticos): o parser guarda o erro, re-sincroniza no fim da linha/bloco ou na proxima declaracao e segue; NULL = para no 1o erro como antes.
 *            0.2.0 - Linguagem v1.8 P2a: modulos, modulo_atual, viu_declaracao, napelidos (ParserAnalisar pode ser chamado varias vezes).
 *            0.1.1 - Linguagem v1.4 P1: comentario sem 'foreign' na lista de recusados.
 *            0.1.0 - Fase 5b: Parser proprio sobre Fonte/Interner/Arena/Diag; gramatica do subconjunto v0 + enum, T ! E,
 *                    try, or_else, defer, switch, break/continue, test, expect. union/pub/export/import/none
 *                    sao recusados com E0101 "construcao ainda nao suportada na Fase 5".
 * Verificacao: construir.bat && forja.exe check <arq.fj>; python ..\tests\sintaxe\verificar_sintaxe.py
 */
#ifndef FORJA_PARSER_H
#define FORJA_PARSER_H

#include "common/arena.h"
#include "common/diag.h"
#include "common/intern.h"
#include "frontend/ast.h"
#include "frontend/modulos.h"
#include "frontend/token.h"

typedef struct {
    /* entrada (o chamador preenche) */
    const Fonte *fonte;
    const Vec *tokens; /* Vec de Token vindo do Lexer, terminado em TK_EOF */
    Interner *nomes;
    Arena *arena;      /* guarda a AST inteira */
    Diag *diag;
    DiagLista *erros;  /* IA1-b15: onde guardar cada erro de sintaxe; NULL = o parser para no 1o (o erro fica pendente em diag) */
    Modulos *modulos;  /* v1.8: NULL = 'import' continua E0101 (quem nao e o driver) */
    uint8_t modulo_atual; /* modulo (pacote) do arquivo que esta sendo lido (0 = raiz) */
    uint32_t arquivo_atual; /* v1.13: trecho da Fonte do arquivo que esta sendo lido (0 = raiz); modulo de pasta: varios arquivos por modulo_atual */
    Vec *cargas;          /* B12: NULL = #load e erro de sintaxe; senao um Vec de Expr* (cada #load, em ordem); o driver le antes da semantica */
    Vec *chaves_literal;  /* F1: NULL = nao registra; senao um Vec de uint32_t (o chamador faz VecIniciar), o offset de cada '{' de literal (struct ou array), em ordem crescente */
    /* saida */
    Programa programa;
    /* estado interno do parser */
    const Token *toks;
    uint32_t cur;
    bool sem_literal_struct; /* cabecalho de if/while/for/switch: '{' abre o corpo, nao um literal de struct */
    bool viu_declaracao;     /* v1.8: ja leu uma declaracao que nao e import neste arquivo (import so no topo) */
    uint8_t napelidos;       /* v1.8: imports deste arquivo (0 = caminho rapido do nome qualificado) */
    Vec pilha_args, pilha_campos, pilha_params, pilha_membros, pilha_casos; /* listas em construcao */
} Parser;

/* Monta a AST. false = diagnostico em p->diag (E0101). Libere os auxiliares com ParserLiberar. */
bool ParserAnalisar(Parser *p);

void ParserLiberar(Parser *p);

#endif
