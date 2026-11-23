/* nativo_semente.h - semente do musl (language\third\musl\x86_64) para o alvo x86_64-linux com foreign (M5b P39-B2, D-R4): libc.a e crt1.o ficam FORA do forja.exe.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P39-B2: as funcoes moram em nativo_biblioteca.c (o construir.bat junta os .c por glob e a linha do cmd estoura com um arquivo a mais); este .h so declara.
 *            0.1.0 - M5b P39-B2: SementeMuslPasta (pasta da semente, ao lado do forja.exe), SementeMuslCompleta (crt1.o e libc.a existem) e SementeMuslCrt1 (crt1.o lido como Objeto ELF).
 * Verificacao: construir.bat && teste_nativo.exe (secao da semente) e tests\nativo\linux\libc\verificar_libc.py
 */
#ifndef FORJA_NATIVO_SEMENTE_H
#define FORJA_NATIVO_SEMENTE_H

#include <stdbool.h>
#include <stddef.h>

#include "nativo/obj/objeto.h"

/* Tamanho do buffer de SementeMuslPasta (MAX_PATH com folga; o caminho do forja.exe mais a subpasta cabe). */
#define SEMENTE_TAM_PASTA 512

/* Pasta da semente: <pasta do forja.exe>\..\third\musl\x86_64. false se o exe nao disser o proprio caminho ou o buffer for pequeno. */
bool SementeMuslPasta(char *saida, size_t tam);

/* true se a pasta tem crt1.o e libc.a (os dois que o ligador do alvo linux usa com foreign). */
bool SementeMuslCompleta(const char *pasta);

/* Le <pasta>\crt1.o como objeto ELF (o _start do musl). false se o arquivo nao abre ou nao e um ELF que o leitor aceita. */
bool SementeMuslCrt1(const char *pasta, bool pie, Objeto *o);

#endif
