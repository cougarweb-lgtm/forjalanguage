/* limites.h - forja run/test: --limite-tempo, --limite-mem, --limite-saida e --args-file (contrato IA1, b10, C.9) e a execucao do programa do usuario (filho_win.c).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b10: primeira versao (LimitesLer, LimitesPrepararArgumentos, LimitesExecutar).
 * Verificacao: construir.bat && python -I ..\tests\ia_first\verificar_limites_b10.py --forja .\forja.exe
 *
 * Exit codes do forja quando um limite estoura (alem de 0 ok, 1 diagnostico, 2 uso, 3 interno e 101 falha de execucao do programa): 102 tempo (R0013), 103 memoria (R0014), 104 saida (R0015).
 * --args-file <arq> (so run): UTF-8, um argumento por linha (LF ou CRLF; BOM do inicio ignorado); linha vazia = argumento vazio; arquivo vazio = nenhum argumento; a ultima linha dispensa o
 * fim de linha. Escapes: \n vira quebra de linha e \\ vira uma barra (outra barra fica literal: C:\pasta vale como esta, C:\novo pede C:\\novo). Os argumentos do arquivo vem ANTES dos que
 * ficam depois do '--' na linha de comando. As opcoes --limite-* e --args-file valem antes do '--' (depois dele tudo e do programa).
 */
#ifndef FORJA_DRIVER_LIMITES_H
#define FORJA_DRIVER_LIMITES_H

#include <stdint.h>

#include "driver/opcoes.h"

enum { LIMITES_SAIDA_TEMPO = 102, LIMITES_SAIDA_MEMORIA = 103, LIMITES_SAIDA_SAIDA = 104 };

typedef struct {
    double tempo_s;                 /* 0 = sem limite */
    uint64_t mem_bytes, saida_bytes;    /* 0 = sem limite */
} Limites;

/* Converte --limite-tempo (segundos, 0.001 a 86400), --limite-mem (MiB inteiros, 1 a 1048576) e --limite-saida (bytes, ou numero com KiB/MiB, 1 byte a 1 TiB). NULL = ok (opcao ausente = 0);
 * senao o motivo do uso errado. */
const char *LimitesLer(const Opcoes *o, Limites *limites);

/* --args-file: le o arquivo e poe os argumentos dele na frente de args_programa. NULL = ok (sem a opcao nao faz nada); senao o motivo do uso errado. */
const char *LimitesPrepararArgumentos(Opcoes *o);

/* Roda o programa (arq_exe) com args_programa e os limites; devolve o exit code do forja: o do programa, 102..104 se um limite estourou (com o R0013..R0015 no stderr) ou 3 se nao iniciou. */
int LimitesExecutar(const Opcoes *o, const char *arq_exe);

#endif
