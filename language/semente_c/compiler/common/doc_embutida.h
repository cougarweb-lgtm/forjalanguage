/* doc_embutida.h - os textos de language\doc embutidos no binario (fonte unica da especificacao, do cartao e dos exemplos de erro).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P79: DOC_RECEITAS (receitas.txt, a colinha de bolso do 'forja spec --receitas').
 *            0.1.0 - IA1-b09: primeira versao (DocEmbutida; cartao.txt, spec.txt, mapa_spec.tsv, migracao.txt e exemplos.txt por .incbin).
 * Verificacao: construir.bat && teste_common.exe && python tests\ia_first\verificar_spec_b09.py --forja <forja.exe>
 */
#ifndef FORJA_DOC_EMBUTIDA_H
#define FORJA_DOC_EMBUTIDA_H

#include <stddef.h>

/* Trecho de texto em memoria (nao termina em NUL: use tam). */
typedef struct {
    const char *texto;
    size_t tam;
} DocTexto;

typedef enum { DOC_CARTAO, DOC_SPEC, DOC_MAPA, DOC_MIGRACAO, DOC_EXEMPLOS, DOC_RECEITAS, DOC_QTD } DocArquivo;

/* O arquivo inteiro como esta em language\doc (os de texto levam CRLF); sempre terminado em NUL depois de tam bytes. */
DocTexto DocEmbutida(DocArquivo qual);

#endif
