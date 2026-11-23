/* ast_fim.h - posicao final ("fim") e inicio de cada no da AST, lidos dos tokens do fonte (AST 1.0, D-IA1-21).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I1: AstFimIniciar (relexa a Fonte inteira, todos os modulos, e casa os pares ( ) { } [ ] f"..."), AstFimExpr/Tipo/Stmt/Decl/Caso/Param/Campo/Membro e AstInicioDecl.
 * Verificacao: construir.bat && forja.exe ast <arq.fj>; python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 *
 * Convencao: "fim" e o offset logo DEPOIS do ultimo byte do no (exclusivo), entao o texto do no e fonte[pos, fim). A AST nao guarda o fim das expressoes
 * (Expr nao pode crescer); ele sai do ultimo token do no: nome, literal, fechador do par ( ) { } [ ], ou o fim do filho mais a direita.
 * Expressao entre parenteses: o parser poe Expr.pos no '(' (AnalisarParenteses), entao o fim inclui o ')' que fecha esse '('.
 */
#ifndef FORJA_AST_FIM_H
#define FORJA_AST_FIM_H

#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "common/vec.h"
#include "frontend/ast.h"

typedef struct {
    Vec toks; /* Token da Fonte inteira (todos os modulos), terminado em TK_EOF */
    Vec par;  /* uint32_t por token: indice do fechador/abridor correspondente; AST_FIM_NENHUM sem par */
} AstFim;

enum { AST_FIM_NENHUM = UINT32_MAX };

/* Relexa a Fonte inteira (os tokens do driver sao reaproveitados a cada modulo, entao nao sobra o de todos). false = erro lexico (diagnostico em diag). */
bool AstFimIniciar(AstFim *f, const Fonte *fonte, Interner *nomes, Diag *diag);
void AstFimLiberar(AstFim *f);

uint32_t AstFimExpr(const AstFim *f, const Expr *e);
uint32_t AstFimTipo(const AstFim *f, const TipoRef *t);
uint32_t AstFimStmt(const AstFim *f, const Stmt *s);
uint32_t AstFimDecl(const AstFim *f, const Decl *d);
uint32_t AstFimCaso(const AstFim *f, const Caso *c);
uint32_t AstFimParam(const AstFim *f, const Param *p);
uint32_t AstFimCampo(const AstFim *f, const Campo *c);
uint32_t AstFimMembro(const AstFim *f, const Membro *m);

/* Offset do 1o token da declaracao: 'pub', 'export' e a palavra-chave que Decl.pos (o nome) deixa para tras. */
uint32_t AstInicioDecl(const AstFim *f, const Decl *d);

#endif
