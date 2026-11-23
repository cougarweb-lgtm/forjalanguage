/* spec_exemplos.h - os pares errado/certo de language\doc\exemplos.txt (gerado da bateria: tests\ia_first\gerar_exemplos_erro.py), um por codigo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao (SpecProximoExemplo, SpecExemploDoCodigo, SpecImprimirLinhasJson, SpecImprimirExemploJson).
 * Verificacao: construir.bat && teste_common.exe && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 *
 * Formato do exemplos.txt (linhas ate o proximo "@@"; o que vem antes do primeiro "@@" e cabecalho e nao conta):
 *   @@ <codigo> <fixture> <linha do erro> <numero da 1a linha do trecho>
 *   = <forma_certa da ocorrencia>
 *   < <linha errada do trecho>     (uma por linha, na ordem do arquivo da fixture)
 *   > <linha certa do trecho>      (so se o conserto automatico foi aplicado e o check do fonte consertado deu exit 0)
 */
#ifndef FORJA_SPEC_EXEMPLOS_H
#define FORJA_SPEC_EXEMPLOS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "common/doc_embutida.h"

enum { EXEMPLO_CODIGO_MAX = 8 };

typedef struct {
    char codigo[EXEMPLO_CODIGO_MAX];
    DocTexto fixture;
    uint32_t linha, primeira;
    DocTexto forma;
    DocTexto errado, certo; /* blocos de linhas com o prefixo "< " / "> "; certo.tam == 0 = sem conserto verificado */
} SpecExemplo;

/* Proximo exemplo do texto *resto (que avanca). false = acabou. */
bool SpecProximoExemplo(DocTexto *resto, SpecExemplo *e);

/* O exemplo do codigo. false = o codigo nao tem exemplo na bateria. */
bool SpecExemploDoCodigo(const char *codigo, SpecExemplo *e);

/* O bloco como lista JSON de linhas, sem o prefixo de 2 bytes: ["linha 1", "linha 2"]. */
void SpecImprimirLinhasJson(FILE *saida, DocTexto bloco);

/* O exemplo como objeto JSON: {"codigo", "fixture", "linha", "primeira_linha", "errado": [...], "certo": [...] ou null, "forma_certa"}. */
void SpecImprimirExemploJson(FILE *saida, const SpecExemplo *e);

#endif
