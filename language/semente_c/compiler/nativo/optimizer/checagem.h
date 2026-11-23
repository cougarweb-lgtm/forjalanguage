/* checagem.h - passe `checagem` (M7a F6): tira da BIR a checagem de indice que o laco contado ja prova.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M7a F6b: vreg de definicao unica `add`/`mul` (I64) de operandos provados/constantes >= 0 (<= 2^31) herda o intervalo somado/multiplicado (i*C+k).
 *            0.1.0 - M7a F6: primeira versao (item (i) do contrato: limite constante do laco <= capacidade constante).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py
 *
 * So atua com BirPrograma.checagem (--checagem sim no release). Reconhece o laco contado que o construtor emite para `for i in S..<K` (S e K constantes,
 * i64): V = const S; guarda cmp.lt_s V, K; corpo; cmp.eq V, K-1 -> saida; V = V + 1. Ali V vale sempre em [S, K-1]; a copia do usuario (vreg de
 * definicao unica `copia V`) herda o intervalo. A checagem `cmp.ge_u X, #N` com X nesse intervalo e K <= N vira SALTA e o bloco frio sem predecessor sai.
 * Qualquer forma diferente da esperada nao e tocada (a checagem fica e traps seguem iguais).
 */
#ifndef FORJA_NATIVO_OPTIMIZER_CHECAGEM_H
#define FORJA_NATIVO_OPTIMIZER_CHECAGEM_H

#include "nativo/bir/bir.h"

/* Passe da tabela (passes.c): transforma a funcao no lugar. */
void OtimChecagemAplicar(const BirPrograma *programa, BirFuncao *funcao);

#endif
