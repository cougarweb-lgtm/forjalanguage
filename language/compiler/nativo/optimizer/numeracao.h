/* numeracao.h - passe `numeracao` (M7b R3): numeracao de valores por EBB e por dominador, sem SSA. Copia propagada, conta pura repetida e carga repetida viram COPIA do vreg que ja
 * tem o valor; depois some a instrucao pura sem uso. Roda na BIR do release x64, depois do `sroa` e antes do alocador.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b R3: primeira versao (numeracao.c percorre e decide; numeracao_fatos.c tabela com desfazer; numeracao_alias.c sobreposicao; numeracao_morto.c limpeza).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe numeracao
 *
 * Remark do --debug-code otim (uma linha por funcao com acerto): "numeracao <funcao>: copia ebb=N dom=N, expr ebb=N dom=N, carga ebb=N dom=N, mortos=N"
 * (ebb = o fato nasceu na mesma EBB do uso; dom = veio de um bloco dominador fora da EBB; carga nunca e dom).
 * Nunca move, funde nem apaga trap/ARMADILHA, nem mexe no `cmp vN, #imed` + `desvia` (a guarda de DIV/REM/SHL/SHR e o desvio de checagem de indice): o divisor/contador de DIV, REM,
 * SHL e SHR e os operandos desses CMP ficam como estao; *_OV, CMP, END_SLOT, END_SIMBOLO e CONST nunca sao reaproveitados.
 */
#ifndef FORJA_NATIVO_OPTIMIZER_NUMERACAO_H
#define FORJA_NATIVO_OPTIMIZER_NUMERACAO_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Roda o passe no programa inteiro (so release). relato != NULL recebe as linhas "numeracao ..." do --debug-code otim. */
void OtimNumeracaoAplicar(BirPrograma *programa, Buf *relato);

#endif
