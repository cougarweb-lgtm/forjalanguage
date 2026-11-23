/* compilacao.h - o estado de UMA compilacao (fonte, arena, lexer, parser, modulos, semantica, IR) e as etapas do front-end que o main.c e o cmd_ast.c dividem.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - Linguagem v1.13 L1b: Compilacao.pastas_carregadas (NativoPacote de cada modulo de pasta; vai ao --entradas).
 *            0.4.0 - Linguagem v1.13 L1a: AnalisarModulo visivel (a carga de pasta, driver\carga_pacote.c, a chama por arquivo).
 *            0.3.0 - B12: Compilacao.cargas (Expr* de cada #load, do parser) e Compilacao.resolvidos (NativoCarga: caminho absoluto e Hash64 do arquivo lido).
 *            0.2.0 - F1 P2: ConferirCanonicoSeHa (check --canonico; a definicao fica no cmd_fmt.c, que nao inclui este header por causa do enum SAIDA_* dele).
 *            0.1.0 - IA1-I1: Compilacao saiu do main.c 0.43.0 (sem mudar campo); IniciarCompilacao, LiberarCompilacao, AnalisarSintaxe, CarregarModulos, AnalisarSemantica e RelatarFalha ficam visiveis.
 * Verificacao: construir.bat && python ..\tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_COMPILACAO_H
#define FORJA_DRIVER_COMPILACAO_H

#include "common/arena.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "driver/opcoes.h"
#include "frontend/lexer.h"
#include "frontend/modulos.h"
#include "frontend/parser.h"
#include "ir/lower.h"
#include "nativo/nativo.h"
#include "semantic/semantic.h"

typedef struct {
    Fonte fonte;
    Arena arena;
    Interner nomes;
    Diag diag;
    Lexer lexer;
    Parser parser;
    Modulos modulos;
    Vec cargas;         /* B12: Expr* de cada #load (o parser enche em ordem) */
    Vec resolvidos;     /* B12: NativoCarga (caminho absoluto e Hash64) de cada #load lido (ResolverLoads, driver/main.c); vai ao --entradas */
    Vec pastas_carregadas; /* L1b: NativoPacote (pasta e hash da lista de .fj) de cada modulo de pasta carregado (CargaPacote); vai ao --entradas */
    Semantica sem;
    IrPrograma ir;
    uint64_t emit_ns;           /* lowering + emissao (0 no x64) */
    NativoTempos tempos;        /* lex, parse, check, ir (aqui) e o resto (nativo); so vale por inteiro com nativo_medido (build x64) */
    bool nativo_medido;
    const Opcoes *opcoes;       /* IA1-b03: a conferencia de bibliotecas do backend C monta o mesmo NativoPedido do x64 */
} Compilacao;

/* Fonte ja lida em c->fonte: prepara interner, lexer, parser e modulos. */
void IniciarCompilacao(Compilacao *c);
void LiberarCompilacao(Compilacao *c);

/* Lexico e parser da raiz; com 'carregar' e algum import, carrega os modulos importados. false = diagnostico em c->diag. */
bool AnalisarSintaxe(Compilacao *c, bool carregar);

/* Carga dos modulos importados (a raiz ja foi lida e analisada), ciclos (E1002) e ordem das Decl. false = diagnostico em c->diag. */
bool CarregarModulos(Compilacao *c);

/* Lexico e parser do ultimo trecho anexado, como arquivo do modulo k (v1.13: o modulo de pasta tem varios arquivos). false = diagnostico em c->diag. */
bool AnalisarModulo(Compilacao *c, uint32_t k);

/* Semantica sobre a AST. false = erros em c->sem.lista (ou c->diag). */
bool AnalisarSemantica(Compilacao *c);

/* Imprime o diagnostico da falha (a lista da semantica ou o Diag unico) e devolve o exit code (1, ou 3 sem diagnostico). */
int RelatarFalha(const Compilacao *c);

/* check --canonico (F1 P2, definicao em driver\cmd_fmt.c): so com 'canonico' e com o fonte fora da forma, imprime UM F0001 (formato) e devolve false. true = nada a dizer. */
bool ConferirCanonicoSeHa(bool canonico, const Fonte *f);

#endif
