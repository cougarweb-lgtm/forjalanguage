/* ligar.h - subcomando 'forja ligar': roda o script Python embutido (gerador de 'foreign' a partir de header).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.8b P7: RodarLigar.
 * Verificacao: python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_LIGAR_H
#define FORJA_LIGAR_H

/* Repassa argv (sem "forja" e sem "ligar") ao script; devolve o exit dele, 2 se o Python nao iniciou, 3 se nao gravou o script. */
int RodarLigar(int argc, char **argv);

#endif
