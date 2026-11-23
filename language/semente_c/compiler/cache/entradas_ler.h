/* entradas_ler.h - leitura do .entradas que o build x64 grava (--entradas) e o hash ATUAL de cada arquivo, com o atalho tamanho+mtime (P22-O1b do M3).
 * Versao: 0.1.1
 * Changelog: 0.1.1 - Linguagem v1.13 L1b: EntradaHashAtual de uma linha "pasta" e o hash da lista dos .fj da pasta de hoje.
 *            0.1.0 - M3 P22-O1b: ArquivoLerInteiro, ArquivoMedir, EntradasLer e EntradaHashAtual.
 * Verificacao: construir.bat && teste_common.exe
 */
#ifndef FORJA_CACHE_ENTRADAS_LER_H
#define FORJA_CACHE_ENTRADAS_LER_H

#include "cache/projeto_ler.h"
#include "common/vec.h"

/* Uma linha do .entradas (formato em nativo\nativo_entradas.h): tipo "fj" ou "lib", caminho absoluto, tamanho, mtime (FILETIME, 100 ns) e Hash64 gravados no fim do build. */
typedef struct {
    char tipo[4];
    char caminho[PROJETO_CAMINHO_MAX];
    uint64_t tamanho, mtime, hash;
} EntradaGravada;

/* Le o arquivo inteiro para um bloco de malloc (dados[tam] = '\0'; o chamador libera). false = nao abriu ou nao leu. */
bool ArquivoLerInteiro(const char *caminho, char **dados, size_t *tam);

/* Tamanho em bytes e mtime (FILETIME, 100 ns) do arquivo. false = nao existe. */
bool ArquivoMedir(const char *caminho, uint64_t *tamanho, uint64_t *mtime);

/* Le o .entradas para 'linhas' (Vec de EntradaGravada, iniciado aqui; libere com VecLiberar). false = nao existe, cabecalho "# forja-entradas 1" ausente ou linha torta;
 * 'linhas' fica vazio e liberavel. */
bool EntradasLer(const char *caminho, Vec *linhas);

/* Hash64 de hoje do arquivo da linha: com tamanho e mtime iguais aos gravados E mtime < inicio_build, devolve o gravado sem reler (o arquivo gravado depois do inicio do
 * build pode ter mudado dentro do mesmo tick do relogio: nesse caso rehasheia); senao rele. "fj" tira o BOM UTF-8 antes (o filho hasheou o texto sem BOM); "lib" hasheia os
 * bytes. false = o arquivo sumiu ou nao leu. */
bool EntradaHashAtual(const EntradaGravada *e, uint64_t inicio_build, uint64_t *hash);

#endif
