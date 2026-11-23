/* pe_ler.h - leitor de PE de terceiro (DLL ou exe x64): a tabela de exportacoes, para o ligador achar o simbolo de um foreign.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19a: PeLerExportacoes (nomes na ordem da tabela, ordinal, RVA, forwarder marcado; PE32 = X0105; sem export = lista vazia).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_PE_LER_H
#define FORJA_NATIVO_OBJ_PE_LER_H

#include <stddef.h>
#include <stdint.h>

#include "common/arena.h"
#include "common/diag.h"
#include "common/vec.h"

/* Um simbolo exportado pelo nome. forwarder e "dll.simbolo" quando o RVA cai dentro do diretorio de exportacao (a DLL repassa o simbolo a outra);
 * senao NULL. Os textos moram na arena de PeExportacoes. */
typedef struct {
    const char *nome;
    uint32_t ordinal;
    uint32_t rva;
    const char *forwarder;
} PeExportacao;

typedef struct {
    Arena textos;
    Vec itens;  /* PeExportacao, na ordem da tabela de nomes do PE */
} PeExportacoes;

/* Le os simbolos exportados POR NOME de um PE32+ AMD64 em memoria (so os bytes de bytes[0..n); nada fora deles e lido). Sem diretorio de exportacao
 * a lista fica vazia e devolve true. PE32 (Magic 0x10B), maquina que nao e AMD64, ou PE malformado: false com X0105 em erro. Em qualquer caso
 * saida fica pronta para PeExportacoesLiberar (chame-a sempre, ate quando devolver false). */
bool PeLerExportacoes(const uint8_t *bytes, size_t n, PeExportacoes *saida, Diag *erro);

void PeExportacoesLiberar(PeExportacoes *saida);

#endif
