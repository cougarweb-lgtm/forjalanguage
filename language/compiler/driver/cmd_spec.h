/* cmd_spec.h - 'forja spec' e as variantes 'forja explicar --lista' e 'forja explicar [CODIGO] --exemplos' (IA1-b09).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao (RodarSpec; ExplicarEhExtenso e RodarExplicarExtenso em cmd_explicar.c).
 * Verificacao: construir.bat && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#ifndef FORJA_CMD_SPEC_H
#define FORJA_CMD_SPEC_H

#include <stdbool.h>

/* forja spec [--cartao] [--secao S] [--json] [--exemplo CODIGO] [CODIGO]; argv sem "forja spec". 'versao' = o texto de "forja --versao" (campo "versao" do --json).
 * Texto da doc embutida (language\doc), sem ler o disco. Exit 0 ok, 2 uso errado. */
int RodarSpec(int argc, char **argv, const char *versao);

/* true se os argumentos de "forja explicar" pedem --lista ou --exemplos (argv sem "forja explicar"). */
bool ExplicarEhExtenso(int argc, char **argv);

/* forja explicar --lista [--json] | forja explicar [CODIGO] --exemplos [--json]; argv sem "forja explicar". Exit 0 ok, 2 uso errado. */
int RodarExplicarExtenso(int argc, char **argv);

#endif
