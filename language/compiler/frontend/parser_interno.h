/* parser_interno.h - funcoes compartilhadas entre parser.c, parser_expr.c, parser_stmt.c e parser_decl.c.
 * Versao: 0.7.1
 * Changelog: 0.7.1 - O0 P13: ParserIdiomaXor.
 *            0.7.0 - IA1-L2: ParserIdiomaArray e ParserIdiomaImport.
 *            0.6.0 - IA1-L1: ParserIdioma, ParserIdiomaOperador, ParserIdiomaNegacao, ParserIdiomaTryBloco e ParserIdiomaMain (idioma_parser.c, E0110).
 *            0.5.0 - IA1-b15: ParserGuardarErro (parser.c): guarda o erro pendente na lista e re-sincroniza.
 *            0.4.0 - IA1-b07: ParserFormaEsperado e ParserFormaDaRegra (parser_forma.c).
 *            0.3.0 - Linguagem v1.8 P2a: LerApelido.
 *            0.2.0 - Linguagem v1.3: AnalisarTipoRef sem o bool permite_ponteiro (TipoRef recursivo).
 *            0.1.0 - Fase 5b: separado em 4 arquivos por assunto (nucleo, expressoes, comandos, declaracoes).
 * Verificacao: construir.bat && python ..\tests\sintaxe\verificar_sintaxe.py
 */
#ifndef FORJA_PARSER_INTERNO_H
#define FORJA_PARSER_INTERNO_H

#include "frontend/parser.h"

static inline const Token *TokenAtual(const Parser *p) { return &p->toks[p->cur]; }
static inline TokenTipo TipoAtual(const Parser *p) { return (TokenTipo)p->toks[p->cur].tipo; }
static inline TokenTipo TipoSeguinte(const Parser *p) { return (TokenTipo)p->toks[p->cur + 1].tipo; }

/* ---- nucleo (parser.c) ---- */
bool ErroSintaxe(Parser *p, const char *esperado);   /* sempre false */
bool Esperar(Parser *p, TokenTipo tipo, const char *esperado);
bool EsperarFimDeLinha(Parser *p);
bool LerNome(Parser *p, int32_t *nome, uint32_t *pos, const char *esperado);
bool EsperarAbreBloco(Parser *p);                    /* '{' seguido de fim de linha */
void *CopiarLista(Parser *p, Vec *pilha, size_t base); /* pilha[base..n) -> arena; esvazia ate base; NULL se vazia */

uint8_t LerApelido(Parser *p, uint32_t *pos);        /* 'apelido .' de um import deste arquivo (so com napelidos != 0) */

/* IA1-b15: o comando que comecou no token 'ini' falhou (erro pendente em p->diag): se o erro esta na 1a linha do comando, guarda o erro na lista, pula o resto da
 * linha (e o bloco que ela abre) e devolve true; false = nao da para re-sincronizar aqui (sem lista, erro em outra linha, lista cheia): o erro sobe. */
bool ParserRecuperarComando(Parser *p, uint32_t ini);

/* ---- forma por ocorrencia do E0101 (parser_forma.c): escrevem em p->diag depois do DiagRelatar ---- */
void ParserFormaEsperado(Parser *p, const char *esperado); /* o que o parser esperava no token atual */
void ParserFormaDaRegra(Parser *p);                        /* E0101 de regra fixa que ainda nao tem forma: a causa vira a regra */

/* ---- idiomas de outras linguagens (idioma_parser.c): E0110 em p->diag; true = relatado ---- */
bool ParserIdioma(Parser *p, const char *esperado);   /* o token atual e idioma conhecido? ('esperado' e o que o ErroSintaxe esperava) */
bool ParserIdiomaOperador(Parser *p);                 /* '&&' ou '||' no lugar do operador binario atual */
void ParserIdiomaNegacao(Parser *p);                  /* '!' no inicio de expressao */
bool ParserIdiomaXor(Parser *p);                      /* O0 P13: '^' pos-fixo (o token atual) seguido de nome ou numero na linha: xor de C, true = relatado */
void ParserIdiomaTryBloco(Parser *p);                 /* 'try {' (o 'try' ja foi consumido) */
bool ParserIdiomaMain(Parser *p, const Decl *d);      /* 'proc main() -> int' (o token atual e o '->') */
void ParserIdiomaArray(Parser *p);                    /* IA1-L2: '[' no lugar de uma expressao (literal de array de Python, JS e Rust) */
bool ParserIdiomaImport(Parser *p);                   /* IA1-L2: 'import os' sem aspas (o token atual e o nome) */

/* ---- expressoes (parser_expr.c) ---- */
Expr *NovaExpr(Parser *p, ExprTipo tipo, uint32_t pos);
Expr *AnalisarExpr(Parser *p);
Expr *AnalisarPosfixo(Parser *p);
Expr *AnalisarCabecalho(Parser *p);                  /* expressao de cabecalho: sem literal de struct */

/* ---- comandos (parser_stmt.c) ---- */
bool AnalisarBloco(Parser *p, Stmt **primeiro, bool ate_caso); /* para em '}' (nao consome), e em case/default se ate_caso */
bool AnalisarCorpoComChaves(Parser *p, Stmt **corpo, uint32_t *pos_fecha); /* '{' NL comandos '}' (consome o '}') */
bool AnalisarTipoRef(Parser *p, TipoRef *t);         /* "T", "^T", "?^T" (recursivo) */

/* ---- declaracoes (parser_decl.c) ---- */
bool AnalisarDeclaracao(Parser *p);

#endif
