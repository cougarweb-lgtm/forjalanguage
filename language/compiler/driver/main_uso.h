/* main_uso.h - mensagem de uso do 'forja' (saiu do main.c para ele ficar abaixo de 800 linhas).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-I4: MainUsoErrado (o texto de UsoErrado do main.c 0.49.0, mais a linha do 'forja editar').
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_MAIN_USO_H
#define FORJA_DRIVER_MAIN_USO_H

/* Escreve "forja: <motivo>" e a lista de comandos no stderr; devolve SAIDA_USO (2). */
int MainUsoErrado(const char *motivo);

#endif
