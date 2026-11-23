/* selecao.h - passe `selecao` (R5 do SIMD): losango ou triangulo PURO da BIR vira BIR_SELECIONA (cmp + cmov no x64; minss/maxss em real), sem desvio.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - R5: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe selecao
 *
 * Entra depois do `sroa` e antes do `cfg`: o passe deixa os braços e o desvio como blocos mortos (ele mesmo os remove) e o `cfg` funde o bloco da junção no da cabeça, e a `numeracao`
 * ja ve o bloco grande. Forma: cabeca termina em "CMP; DESVIA" (o bool so e usado ali); cada braço e um bloco de entrada unica com ate 4 instrucoes puras e SALTA para a mesma junção
 * (ou e a propria junção: triangulo). Pura = CONST COPIA ADD SUB MUL AND OR XOR NEG NOT EXT_S EXT_Z TRUNC (real: CONST COPIA ADD SUB MUL NEG); nada que trapa, grava, le memoria
 * ou chama. As instrucoes dos braços sobem para a cabeca com o dst renomeado para um vreg novo e cada vreg que um braço escreve ganha um SELECIONA com o valor de cada lado.
 * Real so no padrao exato de minss/maxss (BirSelecionaMinMax); fora dele o losango fica com o desvio. Nunca roda com --simbolos (a tabela de linhas da depuracao fica como esta).
 */
#ifndef FORJA_NATIVO_OPTIMIZER_SELECAO_H
#define FORJA_NATIVO_OPTIMIZER_SELECAO_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): roda no programa inteiro, so release. */
void OtimSelecaoAplicar(BirPrograma *programa, Buf *relato);

#endif
