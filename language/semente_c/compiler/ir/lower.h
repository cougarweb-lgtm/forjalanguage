/* lower.h - entrada publica do lowering da Forja: AST anotada pela semantica -> IR (ir/ir.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Fase 5d: LowerPrograma. Plano do arquiteto Opus (2026-11-07) registrado em MEM-LJ-ADR-38.
 * Verificacao: construir.bat && python ..\tests\rodar_suite.py --compilador forja.exe --corpus ..\tests\dourado
 */
#ifndef FORJA_LOWER_H
#define FORJA_LOWER_H

#include "ir/ir.h"
#include "semantic/semantic.h"

/* Converte o programa ja analisado (SemanticaAnalisar devolveu true) em IR; tudo vive na arena de 'sem'.
 * false = diagnostico em sem->diag (hoje so E0420 ao dobrar uma constante que estoura o tipo).
 * Estado impossivel da AST (anotacao faltando) e FalhaInterna: nunca devolve IR incompleta. */
bool LowerPrograma(Semantica *sem, IrPrograma *saida);

#endif
