/* ir_numerar.h - numeracao fixa das instrucoes de uma funcao da IR (pre-ordem), usada pelo traco do diferencial (nivel 2) nos dois backends.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - P18a: primeira versao. IrNumerarFuncao + IrIndiceDe (a instrucao, depois cond, corpo, senao e os casos na ordem; indice recomeca em 0 em cada funcao),
 *                    IrTracoDestino e IrTracoForma (que instrucao grava um local ou temporario ESCALAR e a largura do valor: a regra e UMA so para o emit_c e para o BIR).
 * Verificacao: construir.bat && teste_nativo.exe (IrNumerarFuncao) && python -I ..\tests\nativo\verificar_nativo.py --pasta traco
 *
 * ir.h NAO muda: a numeracao e uma tabela ponteiro -> indice montada fora da IR. Os dois backends chamam a MESMA funcao; se um deles
 * numerasse por conta propria, o X0802 apontaria a instrucao errada.
 */
#ifndef FORJA_IR_NUMERAR_H
#define FORJA_IR_NUMERAR_H

#include "common/arena.h"
#include "ir/ir.h"
#include "semantic/tipos.h"

#define IR_SEM_INDICE UINT32_MAX

typedef struct { const IrInstr *instr; uint32_t indice; } IrNumeroPar;

/* Pares ordenados pelo endereco da instrucao (busca binaria em IrIndiceDe); n = quantidade de instrucoes da funcao. */
typedef struct { IrNumeroPar *pares; uint32_t n; } IrNumeracao;

/* Numera em pre-ordem todas as instrucoes da funcao (inclusive as dos blocos filhos). A memoria sai de 'arena'. */
void IrNumerarFuncao(const IrFuncao *f, Arena *arena, IrNumeracao *saida);

/* Indice da instrucao na funcao numerada, ou IR_SEM_INDICE se ela nao pertence a ela. */
uint32_t IrIndiceDe(const IrNumeracao *num, const IrInstr *instr);

/* Forma do valor tracado: bits = largura em bits (8, 16, 32 ou 64; 0 = nao entra no traco); real = os bits sao os de um f32/f64. */
typedef struct { uint8_t bits; bool real; } IrTracoForma;

/* Inteiros, bool e reais entram; ponteiro, enum, str, struct e demais agregados nao (endereco difere entre backends; agregado nao e escalar). */
IrTracoForma IrTracoFormaDe(const Tipos *tipos, TipoId tipo);

/* A instrucao grava um local (*e_local = true) ou um temporario (false) escalar? false = nao entra no traco. O tipo escalar e conferido por quem chama. */
bool IrTracoDestino(const IrInstr *i, bool *e_local);

#endif
