/* diag_pacote.h - interface interna entre diag.c e diag_pacote.c: o pacote do erro (IA1-b04, contrato 1.1).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - Portao Z z5: DiagPacoteSemSugestao (o diagnostico sem sugestao tambem sai com correcao [], confianca "so_dica", relacionados, spec e id).
 *            0.3.0 - IA1-b14: DiagPacoteRelacionados (o DiagPacoteFim o chama; o diag.c o chama sozinho no diagnostico sem sugestao que tem relacionados).
 *            0.2.0 - IA1-b09: sem DiagPacoteIniciarLinha (vai para DiagSpecIniciarLinha, diag_spec.h); DiagPacoteFim sem o parametro 'forma'.
 *            0.1.0 - IA1-b04: primeira versao (DiagPacoteTrecho, DiagPacoteIniciarLinha, DiagPacoteFim e o ImprimirTextoJson exportado pelo diag.c).
 * Verificacao: construir.bat && teste_common.exe
 *
 * So o diag.c, o diag_spec.c e o spec_doc.c incluem este arquivo (ImprimirTextoJson); quem imprime diagnostico usa o diag.h. Todo diagnostico sai com o pacote (trecho, correcao, confianca, relacionados, spec, id):
 * com sugestao (Diag.sug.n > 0) pelo DiagPacoteFim; sem sugestao pelo DiagPacoteSemSugestao (correcao [] e confianca "so_dica", Portao Z z5).
 */
#ifndef FORJA_DIAG_PACOTE_H
#define FORJA_DIAG_PACOTE_H

#include <stdio.h>

#include "common/diag.h"
#include "common/fonte.h"

/* Texto entre aspas, escapando aspas, barra e controles (definido em diag.c). */
void ImprimirTextoJson(FILE *saida, const char *s);

/* ", \"trecho\": [...]": a linha do erro com ate 2 linhas de contexto de cada lado e a linha do marcador ^~~~ logo depois da linha do erro.
 * tam_token = bytes do token no offset do diagnostico (a faixa do marcador). */
void DiagPacoteTrecho(const Diag *d, const Fonte *f, uint32_t tam_token, FILE *saida);

/* ", \"relacionados\": [{papel, arquivo, linha, coluna, texto, nota}, ...]" (vazio = []). texto = a linha do offset do relacionado, sem a indentacao, ate DIAG_REL_TEXTO_MAX bytes. */
void DiagPacoteRelacionados(const Diag *d, const Fonte *f, FILE *saida);

/* ", \"correcao\": ..., \"confianca\": ..., \"sugestao\": ..., [\"alternativas\": ...,] \"relacionados\": [...], \"spec\"|\"spec_ref\": ..., \"id\": ...".
 * codigo = texto do codigo do diagnostico; o spec vem de diag_spec.c (DiagSpecImprimir). */
void DiagPacoteFim(const Diag *d, const Fonte *f, const char *codigo, FILE *saida);

/* Diagnostico SEM sugestao (z5): ", \"correcao\": [], \"confianca\": \"so_dica\", \"relacionados\": [...], \"spec\"|\"spec_ref\": ..., \"id\": ..." (sem "sugestao":
 * a forma_certa ja e a dica). */
void DiagPacoteSemSugestao(const Diag *d, const Fonte *f, const char *codigo, FILE *saida);

#endif
