/* lower_ops.h - pecas de IR para os embutidos de aritmetica com volta e de bits (lower_aritmetica.c, lower_bits.c; IA1-L9a, IA1-L9b).
 * Cada Lo* anexa uma instrucao no bloco atual e devolve o Valor do temporario; nao existe IrOp nova (so os Operadores OP_*_VOLTA em IR_BINARIO).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L9a/L9b: primeira versao (lower_interno.h, congelado, ficou como estava).
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#ifndef FORJA_LOWER_OPS_H
#define FORJA_LOWER_OPS_H

#include "ir/lower_interno.h"

/* O contexto das pecas: o lowering em curso e a posicao (Expr.pos_op da chamada) que toda instrucao leva. */
typedef struct { LowerCtx *c; uint32_t pos; } LoCtx;

TipoId LoSemSinal(TipoId tipo);                                            /* i8 -> u8, i16 -> u16, i32 -> u32, int -> uint; os sem sinal ficam */
uint32_t LoLargura(const LoCtx *k, TipoId tipo);                           /* largura em bits do inteiro */
Valor LoMascara(const LoCtx *k, TipoId tipo_sem_sinal, uint64_t bits);     /* V_INT com os 'bits' truncados a largura do tipo */
Valor LoBinario(const LoCtx *k, uint8_t operador, TipoId tipo, Valor a, Valor b);
Valor LoNao(const LoCtx *k, Valor a);                                      /* not de bool */
Valor LoBitNao(const LoCtx *k, Valor a);                                   /* ~a (sem sinal; trunca ao tipo) */
Valor LoConverte(const LoCtx *k, TipoId alvo, Valor a);                    /* 'as' (checado em dev quando pode nao caber); mesmo tipo = a */
Valor LoTransmuta(const LoCtx *k, TipoId alvo, Valor a);                   /* troca de bits (i32 <-> u32 ...); mesmo tipo = a */
int32_t LoLocal(const LoCtx *k, TipoId tipo, Valor inicial);               /* local mutavel; inicial V_NENHUM = zerado */
Valor LoEmLocal(const LoCtx *k, Valor v);                                  /* constante inteira (V_INT) vira local; o resto passa como esta */
void LoAtribui(const LoCtx *k, int32_t local, TipoId tipo, Valor valor);
IrBloco *LoEntraSe(const LoCtx *k, Valor condicao);                        /* abre "se cond { ... }": devolve o bloco de fora */
void LoSaiDoSe(const LoCtx *k, IrBloco *externo);

Valor LwAritmetica(LowerCtx *c, const Expr *e);  /* EMB_WRAPPING_ADD..EMB_OVERFLOWING_MUL, EMB_MUL_HI_U64; desvia o resto (bits) para LwBits */
Valor LwBits(LowerCtx *c, const Expr *e);        /* EMB_ROTATE_LEFT..EMB_BYTE_SWAP */
Valor LoMulHi(const LoCtx *k, Valor a, Valor b); /* metade alta do produto de dois u64 (4 produtos de 32 bits; sem extensao do compilador de C) */

#endif
