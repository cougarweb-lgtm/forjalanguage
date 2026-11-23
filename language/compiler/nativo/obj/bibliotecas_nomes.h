/* bibliotecas_nomes.h - diz se o nome de um .drectve ou de um 'link' e coberto pelo ligador (o CRT e o kernel32 que ele liga sozinho): nome normalizado
 * (minusculas, sem o "lib" e sem .lib/.a/.dll) na lista BIBS_COBERTAS de bibliotecas_nomes.c.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P39-B3: BibsCoberta sai de bibliotecas.c (que passou de 800 linhas com o P39-B2). Mesmo comportamento.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_BIBLIOTECAS_NOMES_H
#define FORJA_NATIVO_OBJ_BIBLIOTECAS_NOMES_H

#include <stdbool.h>

/* true se o nome (com ou sem caminho de extensao e "lib") e um dos cobertos. */
bool BibsCoberta(const char *nome);

#endif
