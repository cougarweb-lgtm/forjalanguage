/* doc_embutida.c - os arquivos de language\doc entram no binario por .incbin; a pasta vem de -DFORJA_DOC_PASTA (barras normais, sem aspas), como o modulos_std.c.
 * Texto embutido separado da logica (CROSS-R05): so dados e DocEmbutida.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M6b P79: receitas.txt entra por .incbin (DOC_RECEITAS).
 *            0.1.0 - IA1-b09: primeira versao.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/doc_embutida.h"

#ifndef FORJA_DOC_PASTA
#error "defina -DFORJA_DOC_PASTA=<pasta language/doc, com barras normais>"
#endif

#define FORJA_TEXTO_AUX(x) #x
#define FORJA_TEXTO(x) FORJA_TEXTO_AUX(x)

/* Um arquivo: simbolo de inicio, o .incbin, simbolo de fim e um NUL depois do fim. */
#define DOC_INCBIN(simbolo, arquivo)                                                                           \
    ".globl " #simbolo "\n" #simbolo ":\n"                                                                      \
    ".incbin \"" FORJA_TEXTO(FORJA_DOC_PASTA) "/" arquivo "\"\n"                                               \
    ".globl " #simbolo "_fim\n" #simbolo "_fim:\n"                                                              \
    ".byte 0\n"

__asm__(".section .rdata,\"dr\"\n"
        DOC_INCBIN(forja_doc_cartao, "cartao.txt")
        DOC_INCBIN(forja_doc_spec, "spec.txt")
        DOC_INCBIN(forja_doc_mapa, "mapa_spec.tsv")
        DOC_INCBIN(forja_doc_migracao, "migracao.txt")
        DOC_INCBIN(forja_doc_exemplos, "exemplos.txt")
        DOC_INCBIN(forja_doc_receitas, "receitas.txt")
        ".text\n");

extern const char forja_doc_cartao[], forja_doc_cartao_fim[];
extern const char forja_doc_spec[], forja_doc_spec_fim[];
extern const char forja_doc_mapa[], forja_doc_mapa_fim[];
extern const char forja_doc_migracao[], forja_doc_migracao_fim[];
extern const char forja_doc_exemplos[], forja_doc_exemplos_fim[];
extern const char forja_doc_receitas[], forja_doc_receitas_fim[];

DocTexto DocEmbutida(DocArquivo qual)
{
    switch (qual) {
    case DOC_CARTAO: return (DocTexto){forja_doc_cartao, (size_t)(forja_doc_cartao_fim - forja_doc_cartao)};
    case DOC_SPEC: return (DocTexto){forja_doc_spec, (size_t)(forja_doc_spec_fim - forja_doc_spec)};
    case DOC_MAPA: return (DocTexto){forja_doc_mapa, (size_t)(forja_doc_mapa_fim - forja_doc_mapa)};
    case DOC_MIGRACAO: return (DocTexto){forja_doc_migracao, (size_t)(forja_doc_migracao_fim - forja_doc_migracao)};
    case DOC_EXEMPLOS: return (DocTexto){forja_doc_exemplos, (size_t)(forja_doc_exemplos_fim - forja_doc_exemplos)};
    case DOC_RECEITAS: return (DocTexto){forja_doc_receitas, (size_t)(forja_doc_receitas_fim - forja_doc_receitas)};
    default: return (DocTexto){"", 0};
    }
}
