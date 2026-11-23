/* x64_regalloc.h - passe `alocador` do release x64 (M7a F3): linear scan dos vregs inteiros em registradores.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7a F3: primeira versao (intervalos sobre o RPO, peso = usos x 10^profundidade, spill do intervalo inteiro).
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F2>; --sem-otim alocador volta ao exe da F2
 *
 * Registradores (Win64): rbp = quadro; rax, rcx, rdx, r11 = rascunho do isel (div, shift por cl, chamada, mov de operando); alocaveis rbx, rsi, rdi, r12..r15 (callee-saved:
 * o prologo os empurra, UWOP_PUSH_NONVOL) e r8..r10, so para intervalo que nao cruza chamada. Vreg real (F32/F64), parametro agregado por bytes e vreg sem uso ficam no slot.
 * Invariante: o vreg I8, I16 e I32 em registrador fica estendido com ZERO ate 64 bits (cada definicao refaz a extensao; ver AlocGravar em x64_alocar.c).
 * Funcao com chamada SysV de foreign nao usa rsi/rdi: o chamado SysV os destroi (o prologo ja os salva para o chamador Win64, x64_quadro.c).
 * Determinismo: a ordem do scan e (inicio, id do vreg); nenhum ponteiro e comparado.
 */
#ifndef FORJA_NATIVO_X64_X64_REGALLOC_H
#define FORJA_NATIVO_X64_X64_REGALLOC_H

#include "nativo/bir/bir.h"

/* Passe da tabela de optimizer/passes.c: preenche funcao->reg_vreg e funcao->regs_salvos (NULL/0 se nenhum vreg ganhou registrador). So o release a chama. */
void X64AlocadorAplicar(const BirPrograma *programa, BirFuncao *funcao);

#endif
