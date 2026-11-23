/* spec_doc.h - leitura da doc embutida (doc_embutida.h): linhas, secoes "== nome ==", mapa codigo -> secao e o trecho minimo da spec de um codigo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao (SpecProximaLinha, SpecSecoes, SpecAcharSecao, SpecSecaoDoCodigo, SpecTrechoDoCodigo, SpecImprimirTextoJson).
 * Verificacao: construir.bat && teste_common.exe && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#ifndef FORJA_SPEC_DOC_H
#define FORJA_SPEC_DOC_H

#include <stdbool.h>
#include <stdio.h>

#include "common/doc_embutida.h"

enum { SPEC_NOME_MAX = 32, SPEC_SECOES_MAX = 16, SPEC_TRECHO_MAX = 320 };

/* Uma secao "== nome ==": bloco = do titulo ate antes do proximo titulo; corpo = o bloco sem a linha do titulo. */
typedef struct {
    char nome[SPEC_NOME_MAX];
    DocTexto bloco, corpo;
} SpecSecao;

/* Proxima linha de *resto sem o CR/LF final e avanca *resto. false = acabou o texto. */
bool SpecProximaLinha(DocTexto *resto, DocTexto *linha);

/* Secoes do cartao ou da spec na ordem do arquivo; devolve quantas (no maximo cap). */
size_t SpecSecoes(DocArquivo fonte, SpecSecao *saida, size_t cap);

/* A secao de nome dado na lista, ou NULL. */
const SpecSecao *SpecAcharSecao(const SpecSecao *lista, size_t n, const char *nome);

/* Secao do mapa_spec.tsv para o codigo ("E0502" -> "proc"). false = o mapa nao conhece o codigo. */
bool SpecSecaoDoCodigo(const char *codigo, char secao[SPEC_NOME_MAX]);

/* Trecho minimo da spec.txt para o codigo: a clausula da secao que cita o codigo ou, se nenhuma cita, as primeiras clausulas da secao (no maximo SPEC_TRECHO_MAX bytes,
 * cortado em palavra e terminado em "..."). Devolve o tamanho; 0 = secao inexistente na spec. */
size_t SpecTrechoDoCodigo(const char *codigo, const char *secao, char *saida, size_t cap);

/* Texto entre aspas em JSON (sem os CR do CRLF; o resto escapado como o do diagnostico). */
void SpecImprimirTextoJson(FILE *saida, DocTexto texto);

#endif
