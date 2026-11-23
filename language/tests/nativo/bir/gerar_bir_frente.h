/* gerar_bir_frente.h - o front-end completo (lexico, parser, modulos, semantica, lowering) sobre um arquivo ou texto, compartilhado por gerar_bir.c (modos bir) e gerar_bir_verifica.c (unidade).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M2 P15a: Frente e as quatro funcoes saem de gerar_bir.c (que passou de 800 linhas) sem mudar o comportamento.
 * Verificacao: construir.bat (compila com -Werror)
 */
#ifndef FORJA_TESTE_NATIVO_BIR_GERAR_BIR_FRENTE_H
#define FORJA_TESTE_NATIVO_BIR_GERAR_BIR_FRENTE_H

#include "common/arena.h"
#include "common/diag.h"
#include "common/fonte.h"
#include "common/intern.h"
#include "frontend/lexer.h"
#include "frontend/modulos.h"
#include "frontend/parser.h"
#include "ir/ir.h"
#include "nativo/bir/bir.h"
#include "semantic/semantic.h"

typedef struct {
    Fonte fonte;
    Arena arena;
    Interner nomes;
    Diag diag;
    Lexer lexer;
    Parser parser;
    Modulos modulos;
    Semantica sem;
    IrPrograma ir;
} Frente;

void IniciarFrente(Frente *f);
void LiberarFrente(Frente *f, bool texto_proprio);
bool AnalisarFrente(Frente *f);                              /* lexico ate o lowering; false = diagnostico em f->diag */
BirEntrada EntradaDaFrente(const Frente *f, uint8_t modo);

#endif
