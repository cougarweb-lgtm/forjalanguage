/* diag_spec.h - o campo "spec" de todo diagnostico (contrato IA1, 1.1 item 5): trecho minimo da spec do codigo (mapa_spec) e, se a bateria tem, o exemplo certo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-b09: primeira versao (DiagSpecIniciarLinha, DiagSpecImprimir, DiagSpecImprimirCompleto, DiagSpecTexto).
 * Verificacao: construir.bat && teste_common.exe
 *
 * So o diag.c e o driver (explicar) incluem este arquivo. O texto: "<secao>: <trecho da spec>" e, quando ha, "\nexemplo certo:\n<linhas>".
 */
#ifndef FORJA_DIAG_SPEC_H
#define FORJA_DIAG_SPEC_H

#include <stdio.h>

#include "common/diag.h"

enum { DIAG_SPEC_MAX = 2048 };

/* O texto do spec do codigo em 'saida' (no maximo cap - 1 bytes). Codigo fora do mapa devolve "forja spec". */
void DiagSpecTexto(const char *codigo, char *saida, size_t cap);

/* Chamar UMA vez por diagnostico impresso (antes de escrever a saida padrao e o log): decide se o spec do codigo sai inteiro ou como spec_ref. */
void DiagSpecIniciarLinha(DiagCodigo codigo);

/* ", \"spec\": <texto>" na 1a vez do codigo na execucao (DiagSpecIniciarLinha); nas seguintes ", \"spec_ref\": \"forja explicar CODIGO\"". */
void DiagSpecImprimir(DiagCodigo codigo, FILE *saida);

/* ", \"spec\": <texto>" sempre (forja explicar). */
void DiagSpecImprimirCompleto(DiagCodigo codigo, FILE *saida);

#endif
