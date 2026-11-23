/* slp.h - passe `slp` (S3 do SIMD): 4 gravacoes F32 (ou 2 F64) em enderecos contiguos cujo valor sai da mesma conta lane a lane viram carga vetorial + conta vetorial + gravacao vetorial.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - S3 (D0703): primeira versao (superword level parallelism local ao bloco, so ADD SUB MUL DIV real; sem NEG, sem conversao, sem V128 inteiro).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe> --contra <base.exe> --passe-neutro slp && python tests\nativo\otim\verificar_otim.py --contra <base.exe> --sem-passe slp
 *
 * Entra depois do `inline` e ANTES do `sroa` (o agregado ainda esta no slot e as gravacoes ainda sao memoria). Semente: 4 GRAVA F32 (2 F64) da mesma base em deslocamentos d, d+4, d+8, d+12
 * (d, d+8), grupo cheio (nunca 3 lanes de 4: o movups gravaria 4 bytes alem). Arvore isomorfa: o valor de cada lane sai, no mesmo bloco, da MESMA sequencia de ADD SUB MUL DIV real (mesma
 * operacao, operandos na mesma ordem), recursivamente; folhas: CARREGA contigua na ordem das lanes (uma carga vetorial), o mesmo valor em todas as lanes (mesmo vreg ou copia estavel dele,
 * mesma constante, mesma carga repetida: difundido por BIR_SPLAT) ou recusa. Cada lane faz a mesma conta em IEEE sem reassociar nem FMA: o resultado e bit a bit o escalar.
 * Intermediarios e cargas so podem ser lidos dentro da arvore. A carga e a conta vao para a posicao da ULTIMA instrucao da arvore e a gravacao para a da ULTIMA gravacao; recusa se entre
 * elas houver CHAMA, ARMADILHA, *_OV (ponto de trap), COPIA_MEM/ZERA_MEM ou carga/gravacao de memoria que possa ser a mesma (slp_seguro.c). Relato: `slp <funcao>: N grupos vetorizados` e
 * `slp recusou: <motivo> (<funcao> b<bloco>)`. Nunca roda com --simbolos (a tabela de linhas da depuracao fica como esta).
 */
#ifndef FORJA_NATIVO_OPTIMIZER_SLP_H
#define FORJA_NATIVO_OPTIMIZER_SLP_H

#include "common/buf.h"
#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): roda no programa inteiro, so release. */
void OtimSlpAplicar(BirPrograma *programa, Buf *relato);

#endif
