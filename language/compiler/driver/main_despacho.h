/* main_despacho.h - tabela de despacho dos subcomandos do 'forja' que nao passam pela leitura de opcoes do main.c.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I4: DespacharSubcomando (os `if (argc >= 2 && strcmp(argv[1], "...") == 0)` do main.c 0.49.0 viraram a tabela SUBCOMANDOS; ganha o 'editar').
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_MAIN_DESPACHO_H
#define FORJA_DRIVER_MAIN_DESPACHO_H

#include <stdbool.h>

/* argv[1] e um subcomando da tabela (ligar, corrigir, editar, consultar, mapa, spec, explicar --lista|--exemplos)? Roda-o com argv sem "forja" e sem o nome, devolve true
 * e poe o exit dele em *codigo; false = nao e (o main segue para as opcoes). Pre-condicao: argc >= 2. */
bool DespacharSubcomando(int argc, char **argv, int *codigo);

#endif
