/* cfg.h - passes `cfg` e `layout` (M7b R4b): simplificacao do grafo de fluxo da BIR e ordem dos blocos. Rodam so no release x64, sem --simbolos (a tabela de linhas da depuracao fica como esta).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b R4b: primeira versao (cfg.c simplifica, layout.c ordena, cfg_ordem.c aplica uma ordem de blocos).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe cfg,layout
 *
 * `cfg` (antes da `numeracao`): (1) a aresta cujo alvo e um bloco que so tem SALTA vai direto ao destino final (cadeia jmp->jmp; ciclo de blocos vazios fica como esta); (2) bloco que
 * termina em SALTA absorve o sucessor que so ele alcanca; (3) bloco inalcancavel some. Nunca mexe no bloco 0, no bloco frio, no bloco que termina em ARMADILHA, nem na aresta frio de *_OV:
 * a ordem de execucao e o rastreio R0001..R0009 ficam iguais.
 * `layout` (por ultimo antes do alocador): reordena os blocos (so a numeracao; o alocador anda pelo RPO, que nao depende do id): o sucessor quente cai direto (fall-through), o bloco que
 * sai do laco e o de armadilha vao para depois, e o cabecalho de laco com teste (DESVIA, um alvo dentro e outro fora) vai para o fim do corpo (laco rotacionado: 1 desvio por volta).
 */
#ifndef FORJA_NATIVO_OPTIMIZER_CFG_H
#define FORJA_NATIVO_OPTIMIZER_CFG_H

#include <stdint.h>

#include "nativo/bir/bir.h"

/* Passes da tabela (passes.c): transformam a funcao no lugar. */
void OtimCfgAplicar(const BirPrograma *programa, BirFuncao *funcao);
void OtimLayoutAplicar(const BirPrograma *programa, BirFuncao *funcao);

/* Interno (cfg*.c, layout.c): a funcao passa a ter so os blocos ordem[0..n) (ids antigos, ordem[0] == 0, sem repeticao), nessa ordem; os demais sao liberados; os alvos sao reescritos. */
void CfgAplicarOrdem(BirFuncao *funcao, const uint32_t *ordem, uint32_t n);

/* A funcao tem trabalho para estes passes (2+ blocos, todos com instrucao, e o programa nao e --simbolos)? */
bool CfgElegivel(const BirPrograma *programa, const BirFuncao *funcao);

#endif
