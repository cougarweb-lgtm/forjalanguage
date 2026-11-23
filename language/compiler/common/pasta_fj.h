/* pasta_fj.h - a lista dos .fj de uma pasta (o modulo de pasta da Linguagem v1.13) e o hash dela, que a carga e o cache do build (.entradas) usam do mesmo jeito.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - Linguagem v1.13 L1b: PastaListarFj sai do driver\carga_pacote.c (ordem de byte do caminho, sem subpasta); PastaHashNomes e PastaHashDeHoje (a "pasta" do .entradas).
 * Verificacao: construir.bat && python ..\tests\cli\verificar_cli.py --forja .\forja.exe (caso cache_pasta)
 */
#ifndef FORJA_COMMON_PASTA_FJ_H
#define FORJA_COMMON_PASTA_FJ_H

#include "common/arena.h"
#include "common/base.h"
#include "common/vec.h"

/* Os .fj diretos da pasta (sem subpasta), como "pasta\nome" na arena (Vec de const char *, iniciado por quem chama), ordenados por byte do caminho: a carga nao depende da ordem do disco.
 * Pasta ilegivel = lista vazia. */
void PastaListarFj(Arena *arena, const char *pasta, Vec *caminhos);

/* Hash64 da lista dos NOMES (sem a pasta) dos .fj de 'caminhos', na ordem (cada nome seguido de LF). O conteudo de cada .fj ja tem a linha "fj" propria do .entradas: a "pasta" cobre so
 * o que as linhas "fj" nao veem, o .fj que entra ou sai da pasta. */
uint64_t PastaHashNomes(const Vec *caminhos);

/* PastaHashNomes da pasta no disco hoje. */
uint64_t PastaHashDeHoje(const char *pasta);

#endif
