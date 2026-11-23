/* diag_corrige.h - a correcao por maquina de um erro sem edicao (P72, contrato M6 D-M6-1 e D0653 item 1): a tabela por codigo e trecho da causa (diag_corrige.c) diz
 * COMO consertar a ocorrencia (inserir, trocar ou tirar um pedaco da linha) e o diag.c preenche "correcao" e "confianca" do pacote do erro.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - P72: primeira versao (DiagCorrigirPadrao, DiagTamanhoDoToken).
 * Verificacao: construir.bat && python ..\benchmark\ia_first\verificar_corrigir.py --forja compiler\forja.exe
 *
 * So o diag.c inclui este arquivo. O passo da analise que ja escreveu uma sugestao com edicao sempre vence: esta tabela so age em diagnostico SEM sugestao
 * ou com sugestao so_dica sem edicao (nesse caso acrescenta a edicao e sobe a confianca para provavel, no maximo).
 */
#ifndef FORJA_DIAG_CORRIGE_H
#define FORJA_DIAG_CORRIGE_H

#include "common/diag.h"
#include "common/fonte.h"

/* Bytes do token que comeca em off (definido em diag.c; fora do texto vale 1). */
uint32_t DiagTamanhoDoToken(const Fonte *f, uint32_t off);

/* O k-esimo (0, 1) texto entre aspas simples da causa em 'saida' (NOME_MAX = 64 bytes); vazio se nao ha (definido em diag_forma.c). */
void DiagNomeEntreAspas(const char *causa, int k, char *saida);

/* Procura a regra da ocorrencia. Achou e a edicao coube: 'saida' recebe a lista de sugestoes a imprimir (a principal com a edicao) e devolve true.
 * Nao achou, ja tem edicao ou a regra nao se aplica a esta linha: false e 'saida' nao vale. */
bool DiagCorrigirPadrao(const Diag *d, const Fonte *f, DiagSugestoes *saida);

#endif
