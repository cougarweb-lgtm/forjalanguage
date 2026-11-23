/* cmd_ast.h - 'forja ast' (AST 1.0 em JSON).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I1: RodarAst e AstResolverEntrada.
 * Verificacao: construir.bat && python ..\tests\ast\verificar_ast.py --forja <forja.exe>
 */
#ifndef FORJA_CMD_AST_H
#define FORJA_CMD_AST_H

#include "driver/compilacao.h"
#include "driver/opcoes.h"

/* forja ast [--tipos] [--sem-pos] [--no <id>] [--projeto] <arq.fj | pasta>, com o arquivo ja lido em c->fonte.
 * Sem --tipos a AST impressa e a do parser (sem a reescrita da semantica) e os "ref" vem de uma segunda compilacao analisada do mesmo arquivo (ast_json_ref.h);
 * com --tipos e a propria arvore analisada. Erro lexico ou de sintaxe = diagnostico e exit 1; erro de semantica no sem --tipos nao impede a AST (as ligacoes
 * feitas antes do erro saem); --projeto exige que os modulos carreguem. Id de --no inexistente = exit 2. */
int RodarAst(const Opcoes *o, Compilacao *c);

/* Argumento que e pasta: usa <pasta>\main.fj e liga o --projeto. --projeto sem arquivo e --no com --projeto sao validados aqui. false = mensagem no stderr (exit 2). */
bool AstResolverEntrada(Opcoes *o);

#endif
