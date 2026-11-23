/* x64_isel_otim.h - selecao pos-alocador do x64 (M7a F5, passe `isel`): auxiliares e entradas dos x64_isel_otim*.c. Nao incluir fora deles.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - R5 (SIMD): IselOtimFlagsCmpInteiro e IselOtimSelecionaReal.
 *            0.2.0 - M7b F1: IMED_COLIDE_MIN/MAX (movidos de x64_isel_otim.c), X64DivConstMarcar e IselOtimDivConst (x64_isel_otim_div.c).
 *            0.1.0 - M7a F5: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_otim.py --forja <forja.exe> --contra <exe da F4> --release-muda
 *
 * Contrato de cada IselOtim*: devolve 0 SEM ter emitido nada quando o caso nao e dele (a selecao comum assume), -1 se a emissao falhou e n >= 1 com as instrucoes
 * da BIR consumidas a partir de k. Invariantes herdadas do alocador: vreg inteiro em registrador vale zero-extendido ate 64 bits; rax, rcx, rdx, r11, xmm0 e xmm1 sao
 * rascunho; dst e operando de vregs diferentes nunca dividem registrador.
 */
#ifndef FORJA_NATIVO_X64_X64_ISEL_OTIM_H
#define FORJA_NATIVO_X64_X64_ISEL_OTIM_H

#include "nativo/x64/x64_interno.h"

#define IMED_COLIDE_MIN 0x204 /* o fadec confunde 0x204..0x207 com AH..BH (x64_codificar.c); o imul r,r,imm nao tem o contorno */
#define IMED_COLIDE_MAX 0x207

/* Registrador inteiro em que o operando (vreg I8..I64) mora; -1 se e imediato, esta no slot ou e real. */
int IselOtimRegInt(const X64Ctx *c, const BirOperando *o);
/* O mesmo para o dst (numero de vreg). */
int IselOtimRegDestino(const X64Ctx *c, uint32_t vreg);
/* xmm em que o operando real mora; -1 se imediato ou no slot. */
int IselOtimRegXmm(const X64Ctx *c, const BirOperando *o);
int IselOtimRegXmmDestino(const X64Ctx *c, uint32_t vreg);
/* vreg com uma unica definicao e um unico uso na funcao inteira (pode ser fundido na instrucao que o usa). */
bool IselOtimTempUnico(const X64Ctx *c, uint32_t vreg);
/* O valor do operando num registrador inteiro: o proprio, se ja mora em um; senao carregado em rascunho. */
X64Reg IselOtimFonte(X64Ctx *c, const BirOperando *o, X64Reg rascunho, bool *ok);
/* reg = o (extensao ext); nada se o valor ja mora em reg (invariante da zero-extensao). */
bool IselOtimCarregarEm(X64Ctx *c, X64Reg reg, const BirOperando *o, uint8_t ext);
/* O fadec recusa (MemoriaAceita) [rbp|r13 + indice*escala] sem deslocamento: troca base e indice quando a escala e 1; false = o isel desiste do modo com indice. */
bool IselOtimEncodavel(MOperando *mem);
/* Imediato do operando que cabe em imm32 (como a instrucao o enxerga na sua classe); false se nao e imediato ou nao cabe. */
bool IselOtimImediato(const BirOperando *o, int64_t *valor);
/* Operando e imediato potencia de 2 (> 0); *k = log2. */
bool IselOtimPotenciaDe2(const BirOperando *o, uint8_t *k);

/* x64_isel_otim.c */
void X64IselMarcar(const BirPrograma *programa, BirFuncao *funcao); /* passe `isel` */
int32_t IselOtimTentar(X64Ctx *c, const BirBloco *bloco, uint32_t k);
void IselOtimContarUsos(const BirFuncao *f, uint8_t *usos, uint8_t *defs);
int32_t IselOtimAritmetica(X64Ctx *c, const BirInstr *i);
int32_t IselOtimOverflow(X64Ctx *c, const BirInstr *i);
int32_t IselOtimMover(X64Ctx *c, const BirInstr *i);
int32_t IselOtimUnaria(X64Ctx *c, const BirInstr *i);
int32_t IselOtimDeslocaDivide(X64Ctx *c, const BirInstr *i);
/* x64_isel_otim_div.c */
void X64DivConstMarcar(const BirPrograma *programa, BirFuncao *funcao); /* passe `divconst` */
/* DIV/REM de I8..I64 por imediato: mul magico, shifts, sem div. 0 sem emitir se o caso nao e dele (div_const desligado, plano sem ganho, dividendo imediato). */
int32_t IselOtimDivConst(X64Ctx *c, const BirInstr *i);
/* x64_isel_otim_fluxo.c */
int32_t IselOtimComparaDesvia(X64Ctx *c, const BirBloco *bloco, uint32_t k);
int32_t IselOtimCompara(X64Ctx *c, const BirInstr *i);
int32_t IselOtimDesvia(X64Ctx *c, const BirInstr *i);
/* As flags do CMP de inteiro (cc em *cc); 0 sem emitir se a classe (so I32/I64) ou a condicao nao e deste caminho, -1 falha. */
int32_t IselOtimFlagsCmpInteiro(X64Ctx *c, const BirInstr *i, X64Cond *cc);
/* x64_isel_otim_mem.c */
int32_t IselOtimMemoria(X64Ctx *c, const BirBloco *bloco, uint32_t k);
int32_t IselOtimCarregaSimples(X64Ctx *c, const BirInstr *i); /* CARREGA sem cadeia: o dst pode ser o de uma copia (destino fundido) */
int32_t IselOtimEndSimbolo(X64Ctx *c, const BirInstr *i);
int32_t IselOtimEndSlot(X64Ctx *c, const BirInstr *i);
/* x64_isel_otim_real.c */
int32_t IselOtimReal(X64Ctx *c, const BirInstr *i);
/* SELECIONA real (minss/maxss); chamada por IselSeleciona (x64_isel_otim_fluxo.c). */
bool IselOtimSelecionaReal(X64Ctx *c, const BirInstr *i);
/* So o ucomis do CMP de real (operandos nos xmm que ja os guardam); *cc = condicao do salto. 0 sem emitir se a condicao pede a paridade (FEQ, FNE), 1 ok, -1 falha. */
int32_t IselOtimCmpReal(X64Ctx *c, const BirInstr *i, X64Cond *cc);

#endif
