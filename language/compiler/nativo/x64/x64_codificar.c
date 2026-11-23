/* x64_codificar.c - codifica uma MInstr em bytes x86-64 pela tabela FORMAS_X64 e pelo encoder do fadec (envelope em fadec_envelope.c).
 * Versao: 0.11.0
 * Changelog: 0.11.0 - S2 (D0703): FX_ADDPS..DIVPD_XX, FX_SHUFPS_XXI (imm8 de 0 a 255; o fadec o le com sinal) e FX_UNPCKLPD_XX (caso E48); FORMAS_X64 com 137.
 *            0.10.0 - R5 (SIMD): FX_CMOV32_RR, FX_CMOV64_RR, FX_MINSS/MAXSS/MINSD/MAXSD_XX (caso E47); FORMAS_X64 com 127.
 *            0.9.0 - M7b F1: FX_SAR32_RI, FX_SAR64_RI e FX_IMUL64_R (caso E46); FORMAS_X64 com 121.
 *            0.8.0 - M7a F5: FX_IMUL32_RRI e FX_IMUL64_RRI (imul r,r,imm32; caso E44); memoria com indice*escala (FE_MEM com idx); FORMAS_X64 com 118.
 *            0.7.0 - M7a F4: FX_MOVAPS_XX (movaps xmm,xmm, caso E43); FORMAS_X64 com 116.
 *            0.6.1 - M3 F9b-1: decisoes extraidas de X64CodificarInstr.
 *            0.6.0 - M3 P21v-c: FX_CALL_R64 (call r64, caso E42); FORMAS_X64 com 115.
 *            0.5.0 - M3 P19: FX_CALL_M64 (call qword [reg], caso E41); FORMAS_X64 com 114.
 *            0.4.0 - Linguagem v1.11 L13: SQRTSS e SQRTSD xmm,xmm (E38); FORMAS_X64 com 113.
 *            0.3.0 - M2 P10b: 30 formas SSE (E31..E37 e E40) e o operando MO_XMM (FE_XMM0 + n); FORMAS_X64 com 111.
 *            0.2.0 - M2 P9: 25 formas novas (E19..E27: MOVSX/MOVZX de 8 e 16 bits, MOV m16,r16, CDQ, CQO, IDIV, DIV, SHL/SHR por cl e imm8, NOT); FORMAS_X64 com 81.
 *            0.1.1 - backend x64 P7a: imediato 0x204..0x207 em op1 (o fadec le como AH..BH e recusa) codificado com substituto e regravado.
 *            0.1.0 - backend x64 P4b: tabela FORMAS_X64 (56 formas do M1), X64CodificarInstr (X0902 quando recusa) e zeragem do
 *                    disp32 de [rip+d32] (a relocacao OR_REL32 e do P7). Saltos e calls saem sempre em rel32 (FE_JMPL).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\enc\verificar_enc.py
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fadec-enc.h"
#include "nativo/x64/x64.h"

/* Tamanho do rel32 base: JMP e CALL tem 1 byte de opcode; Jcc tem 0F 8x (2 bytes); com o imediato de 4 bytes, 5 e 6. */
#define X64_REL32_BASE_JMP 5u
#define X64_REL32_BASE_JCC 6u
#define X64_DISP_RIP_BYTES 4u
/* Imediatos que o fadec confunde com FE_AH..FE_BH (0x204..0x207) e o substituto usado na codificacao. */
#define X64_IMED_COLIDE_MIN 0x204
#define X64_IMED_COLIDE_MAX 0x207
#define X64_IMED_SUBSTITUTO 0x10000

#define FL_NADA 0

const FormaX64 FORMAS_X64[FX_QTD] = {
    {FX_MOV32_RR, FE_MOV32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV64_RR, FE_MOV64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV32_RM, FE_MOV32rm, 2, {MO_REG, MO_MEM, MO_NADA}, {32, 32, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV64_RM, FE_MOV64rm, 2, {MO_REG, MO_MEM, MO_NADA}, {64, 64, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOVZX32_RM8, FE_MOVZXr32m8, 2, {MO_REG, MO_MEM, MO_NADA}, {32, 8, 0}, FL_NADA, "movzx %0, %1"},
    {FX_MOV8_MR, FE_MOV8mr, 2, {MO_MEM, MO_REG, MO_NADA}, {8, 8, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV32_MR, FE_MOV32mr, 2, {MO_MEM, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV64_MR, FE_MOV64mr, 2, {MO_MEM, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV32_RI, FE_MOV32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOV64_RI, FE_MOV64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOVZX32_RR8, FE_MOVZXr32r8, 2, {MO_REG, MO_REG, MO_NADA}, {32, 8, 0}, FL_NADA, "movzx %0, %1"},
    {FX_MOVSXD64_RR32, FE_MOVSXr64r32, 2, {MO_REG, MO_REG, MO_NADA}, {64, 32, 0}, FL_NADA, "movsxd %0, %1"},
    {FX_LEA64_RM, FE_LEA64rm, 2, {MO_REG, MO_MEM, MO_NADA}, {64, 0, 0}, X64_FL_RIP, "lea %0, %1"},
    {FX_ADD32_RR, FE_ADD32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "add %0, %1"},
    {FX_ADD64_RR, FE_ADD64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "add %0, %1"},
    {FX_SUB32_RR, FE_SUB32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "sub %0, %1"},
    {FX_SUB64_RR, FE_SUB64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "sub %0, %1"},
    {FX_AND32_RR, FE_AND32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "and %0, %1"},
    {FX_AND64_RR, FE_AND64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "and %0, %1"},
    {FX_OR32_RR, FE_OR32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "or %0, %1"},
    {FX_OR64_RR, FE_OR64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "or %0, %1"},
    {FX_XOR32_RR, FE_XOR32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "xor %0, %1"},
    {FX_XOR64_RR, FE_XOR64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "xor %0, %1"},
    {FX_CMP32_RR, FE_CMP32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "cmp %0, %1"},
    {FX_CMP64_RR, FE_CMP64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "cmp %0, %1"},
    {FX_ADD32_RI, FE_ADD32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "add %0, %1"},
    {FX_ADD64_RI, FE_ADD64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "add %0, %1"},
    {FX_SUB32_RI, FE_SUB32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "sub %0, %1"},
    {FX_SUB64_RI, FE_SUB64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "sub %0, %1"},
    {FX_AND32_RI, FE_AND32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "and %0, %1"},
    {FX_AND64_RI, FE_AND64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "and %0, %1"},
    {FX_OR32_RI, FE_OR32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "or %0, %1"},
    {FX_OR64_RI, FE_OR64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "or %0, %1"},
    {FX_XOR32_RI, FE_XOR32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "xor %0, %1"},
    {FX_XOR64_RI, FE_XOR64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "xor %0, %1"},
    {FX_CMP32_RI, FE_CMP32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "cmp %0, %1"},
    {FX_CMP64_RI, FE_CMP64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "cmp %0, %1"},
    {FX_IMUL32_RR, FE_IMUL32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "imul %0, %1"},
    {FX_IMUL64_RR, FE_IMUL64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "imul %0, %1"},
    {FX_MUL32_R, FE_MUL32r, 1, {MO_REG, MO_NADA, MO_NADA}, {32, 0, 0}, FL_NADA, "mul %0"},
    {FX_MUL64_R, FE_MUL64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "mul %0"},
    {FX_NEG32_R, FE_NEG32r, 1, {MO_REG, MO_NADA, MO_NADA}, {32, 0, 0}, FL_NADA, "neg %0"},
    {FX_NEG64_R, FE_NEG64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "neg %0"},
    {FX_TEST8_RR, FE_TEST8rr, 2, {MO_REG, MO_REG, MO_NADA}, {8, 8, 0}, FL_NADA, "test %0, %1"},
    {FX_TEST32_RR, FE_TEST32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, FL_NADA, "test %0, %1"},
    {FX_TEST64_RR, FE_TEST64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "test %0, %1"},
    {FX_TEST64_MR, FE_TEST64mr, 2, {MO_MEM, MO_REG, MO_NADA}, {64, 64, 0}, FL_NADA, "test %0, %1"},
    {FX_SETCC8_R, FE_SETcc8r, 1, {MO_REG, MO_NADA, MO_NADA}, {8, 0, 0}, X64_FL_CC, "set%k %0"},
    {FX_JCC_REL32, FE_Jcc, 1, {MO_BLOCO, MO_NADA, MO_NADA}, {0, 0, 0}, X64_FL_CC | X64_FL_REL32, "j%k %0"},
    {FX_JMP_REL32, FE_JMP, 1, {MO_BLOCO, MO_NADA, MO_NADA}, {0, 0, 0}, X64_FL_REL32, "jmp %0"},
    {FX_CALL_REL32, FE_CALL, 1, {MO_SIMBOLO, MO_NADA, MO_NADA}, {0, 0, 0}, X64_FL_REL32, "call %0"},
    {FX_RET, FE_RET, 0, {MO_NADA, MO_NADA, MO_NADA}, {0, 0, 0}, FL_NADA, "ret"},
    {FX_PUSH64_R, FE_PUSHr, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "push %0"},
    {FX_POP64_R, FE_POPr, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "pop %0"},
    {FX_UD2, FE_UD2, 0, {MO_NADA, MO_NADA, MO_NADA}, {0, 0, 0}, FL_NADA, "ud2"},
    {FX_INT3, FE_INT3, 0, {MO_NADA, MO_NADA, MO_NADA}, {0, 0, 0}, FL_NADA, "int3"},
    {FX_MOVSX32_RM8, FE_MOVSXr32m8, 2, {MO_REG, MO_MEM, MO_NADA}, {32, 8, 0}, FL_NADA, "movsx %0, %1"},
    {FX_MOVSX32_RM16, FE_MOVSXr32m16, 2, {MO_REG, MO_MEM, MO_NADA}, {32, 16, 0}, FL_NADA, "movsx %0, %1"},
    {FX_MOVSX64_RM8, FE_MOVSXr64m8, 2, {MO_REG, MO_MEM, MO_NADA}, {64, 8, 0}, FL_NADA, "movsx %0, %1"},
    {FX_MOVSX64_RM16, FE_MOVSXr64m16, 2, {MO_REG, MO_MEM, MO_NADA}, {64, 16, 0}, FL_NADA, "movsx %0, %1"},
    {FX_MOVZX32_RM16, FE_MOVZXr32m16, 2, {MO_REG, MO_MEM, MO_NADA}, {32, 16, 0}, FL_NADA, "movzx %0, %1"},
    {FX_MOVZX32_RR16, FE_MOVZXr32r16, 2, {MO_REG, MO_REG, MO_NADA}, {32, 16, 0}, FL_NADA, "movzx %0, %1"},
    {FX_MOV16_MR, FE_MOV16mr, 2, {MO_MEM, MO_REG, MO_NADA}, {16, 16, 0}, FL_NADA, "mov %0, %1"},
    {FX_MOVSX32_RR8, FE_MOVSXr32r8, 2, {MO_REG, MO_REG, MO_NADA}, {32, 8, 0}, FL_NADA, "movsx %0, %1"},
    {FX_MOVSX32_RR16, FE_MOVSXr32r16, 2, {MO_REG, MO_REG, MO_NADA}, {32, 16, 0}, FL_NADA, "movsx %0, %1"},
    {FX_CDQ, FE_CDQ, 0, {MO_NADA, MO_NADA, MO_NADA}, {0, 0, 0}, FL_NADA, "cdq"},
    {FX_CQO, FE_CQO, 0, {MO_NADA, MO_NADA, MO_NADA}, {0, 0, 0}, FL_NADA, "cqo"},
    {FX_IDIV32_R, FE_IDIV32r, 1, {MO_REG, MO_NADA, MO_NADA}, {32, 0, 0}, FL_NADA, "idiv %0"},
    {FX_IDIV64_R, FE_IDIV64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "idiv %0"},
    {FX_DIV32_R, FE_DIV32r, 1, {MO_REG, MO_NADA, MO_NADA}, {32, 0, 0}, FL_NADA, "div %0"},
    {FX_DIV64_R, FE_DIV64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "div %0"},
    {FX_SHL32_RCL, FE_SHL32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 8, 0}, FL_NADA, "shl %0, %1"},
    {FX_SHL64_RCL, FE_SHL64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 8, 0}, FL_NADA, "shl %0, %1"},
    {FX_SHR32_RCL, FE_SHR32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 8, 0}, FL_NADA, "shr %0, %1"},
    {FX_SHR64_RCL, FE_SHR64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 8, 0}, FL_NADA, "shr %0, %1"},
    {FX_NOT32_R, FE_NOT32r, 1, {MO_REG, MO_NADA, MO_NADA}, {32, 0, 0}, FL_NADA, "not %0"},
    {FX_NOT64_R, FE_NOT64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "not %0"},
    {FX_SHL32_RI, FE_SHL32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "shl %0, %1"},
    {FX_SHL64_RI, FE_SHL64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "shl %0, %1"},
    {FX_SHR32_RI, FE_SHR32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "shr %0, %1"},
    {FX_SHR64_RI, FE_SHR64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "shr %0, %1"},
    /* M2 P10b: SSE escalar. Operando xmm: largura nominal 128 (nao conta); memoria: 32 (ss), 64 (sd) ou 128 (ps). */
    {FX_MOVSS_XM, FE_SSE_MOVSSrm, 2, {MO_XMM, MO_MEM, MO_NADA}, {128, 32, 0}, FL_NADA, "movss %0, %1"},
    {FX_MOVSD_XM, FE_SSE_MOVSDrm, 2, {MO_XMM, MO_MEM, MO_NADA}, {128, 64, 0}, FL_NADA, "movsd %0, %1"},
    {FX_MOVSS_MX, FE_SSE_MOVSSmr, 2, {MO_MEM, MO_XMM, MO_NADA}, {32, 128, 0}, FL_NADA, "movss %0, %1"},
    {FX_MOVSD_MX, FE_SSE_MOVSDmr, 2, {MO_MEM, MO_XMM, MO_NADA}, {64, 128, 0}, FL_NADA, "movsd %0, %1"},
    {FX_MOVSS_XX, FE_SSE_MOVSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "movss %0, %1"},
    {FX_MOVSD_XX, FE_SSE_MOVSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "movsd %0, %1"},
    {FX_ADDSS_XX, FE_SSE_ADDSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "addss %0, %1"},
    {FX_SUBSS_XX, FE_SSE_SUBSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "subss %0, %1"},
    {FX_MULSS_XX, FE_SSE_MULSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "mulss %0, %1"},
    {FX_DIVSS_XX, FE_SSE_DIVSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "divss %0, %1"},
    {FX_ADDSD_XX, FE_SSE_ADDSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "addsd %0, %1"},
    {FX_SUBSD_XX, FE_SSE_SUBSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "subsd %0, %1"},
    {FX_MULSD_XX, FE_SSE_MULSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "mulsd %0, %1"},
    {FX_DIVSD_XX, FE_SSE_DIVSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "divsd %0, %1"},
    {FX_UCOMISS_XX, FE_SSE_UCOMISSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "ucomiss %0, %1"},
    {FX_UCOMISD_XX, FE_SSE_UCOMISDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "ucomisd %0, %1"},
    {FX_CVTSI2SS_XR32, FE_SSE_CVTSI2SS32rr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 32, 0}, FL_NADA, "cvtsi2ss %0, %1"},
    {FX_CVTSI2SS_XR64, FE_SSE_CVTSI2SS64rr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 64, 0}, FL_NADA, "cvtsi2ss %0, %1"},
    {FX_CVTSI2SD_XR32, FE_SSE_CVTSI2SD32rr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 32, 0}, FL_NADA, "cvtsi2sd %0, %1"},
    {FX_CVTSI2SD_XR64, FE_SSE_CVTSI2SD64rr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 64, 0}, FL_NADA, "cvtsi2sd %0, %1"},
    {FX_CVTSS2SD_XX, FE_SSE_CVTSS2SDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "cvtss2sd %0, %1"},
    {FX_CVTSD2SS_XX, FE_SSE_CVTSD2SSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "cvtsd2ss %0, %1"},
    {FX_XORPS_XM, FE_SSE_XORPSrm, 2, {MO_XMM, MO_MEM, MO_NADA}, {128, 128, 0}, FL_NADA, "xorps %0, %1"},
    {FX_XORPS_XX, FE_SSE_XORPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "xorps %0, %1"},
    {FX_MOVD_XR, FE_SSE_MOVD_G2Xrr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 32, 0}, FL_NADA, "movd %0, %1"},
    {FX_MOVD_RX, FE_SSE_MOVD_X2Grr, 2, {MO_REG, MO_XMM, MO_NADA}, {32, 128, 0}, FL_NADA, "movd %0, %1"},
    {FX_MOVQ_XR, FE_SSE_MOVQ_G2Xrr, 2, {MO_XMM, MO_REG, MO_NADA}, {128, 64, 0}, FL_NADA, "movq %0, %1"},
    {FX_MOVQ_RX, FE_SSE_MOVQ_X2Grr, 2, {MO_REG, MO_XMM, MO_NADA}, {64, 128, 0}, FL_NADA, "movq %0, %1"},
    {FX_MOVUPS_XM, FE_SSE_MOVUPSrm, 2, {MO_XMM, MO_MEM, MO_NADA}, {128, 128, 0}, FL_NADA, "movups %0, %1"},
    {FX_MOVUPS_MX, FE_SSE_MOVUPSmr, 2, {MO_MEM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "movups %0, %1"},
    {FX_MOVAPS_XX, FE_SSE_MOVAPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "movaps %0, %1"},
    {FX_SQRTSS_XX, FE_SSE_SQRTSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "sqrtss %0, %1"},
    {FX_SQRTSD_XX, FE_SSE_SQRTSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "sqrtsd %0, %1"},
    /* M3 P19 (E41): a chamada a um foreign de DLL passa pelo slot da IAT (lea rax,[rip+__imp_x] ; call qword [rax]). */
    {FX_CALL_M64, FE_CALLm, 1, {MO_MEM, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "call %0"},
    /* M3 P21v-c (E42): a chamada pelo valor de um proc (CHAMA_IND) leva o endereco em rax: call rax. */
    {FX_CALL_R64, FE_CALLr, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "call %0"},
    /* M7a F5 (E44): imul r,r,imm32 (o imediato fora de 0x204..0x207, que o fadec confunde com AH..BH; o isel evita esses valores). */
    {FX_IMUL32_RRI, FE_IMUL32rri, 3, {MO_REG, MO_REG, MO_IMED}, {32, 32, 0}, FL_NADA, "imul %0, %1, %2"},
    {FX_IMUL64_RRI, FE_IMUL64rri, 3, {MO_REG, MO_REG, MO_IMED}, {64, 64, 0}, FL_NADA, "imul %0, %1, %2"},
    /* M7b F1 (E46): divisao por constante (sar r,imm8 e o imul de um operando, produto de 128 bits com sinal em rdx:rax). */
    {FX_SAR32_RI, FE_SAR32ri, 2, {MO_REG, MO_IMED, MO_NADA}, {32, 0, 0}, FL_NADA, "sar %0, %1"},
    {FX_SAR64_RI, FE_SAR64ri, 2, {MO_REG, MO_IMED, MO_NADA}, {64, 0, 0}, FL_NADA, "sar %0, %1"},
    {FX_IMUL64_R, FE_IMUL64r, 1, {MO_REG, MO_NADA, MO_NADA}, {64, 0, 0}, FL_NADA, "imul %0"},
    /* R5 (E47): selecao sem desvio (cmovcc) e min/max escalar de real (devolve o 2o operando com NaN ou +-0: ver bir_selecao.c). */
    {FX_CMOV32_RR, FE_CMOVcc32rr, 2, {MO_REG, MO_REG, MO_NADA}, {32, 32, 0}, X64_FL_CC, "cmov%k %0, %1"},
    {FX_CMOV64_RR, FE_CMOVcc64rr, 2, {MO_REG, MO_REG, MO_NADA}, {64, 64, 0}, X64_FL_CC, "cmov%k %0, %1"},
    {FX_MINSS_XX, FE_SSE_MINSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "minss %0, %1"},
    {FX_MAXSS_XX, FE_SSE_MAXSSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "maxss %0, %1"},
    {FX_MINSD_XX, FE_SSE_MINSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "minsd %0, %1"},
    {FX_MAXSD_XX, FE_SSE_MAXSDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "maxsd %0, %1"},
    /* S2 (E48): aritmetica empacotada (4 x f32 ou 2 x f64), shufps com imm8 e unpcklpd. */
    {FX_ADDPS_XX, FE_SSE_ADDPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "addps %0, %1"},
    {FX_SUBPS_XX, FE_SSE_SUBPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "subps %0, %1"},
    {FX_MULPS_XX, FE_SSE_MULPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "mulps %0, %1"},
    {FX_DIVPS_XX, FE_SSE_DIVPSrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "divps %0, %1"},
    {FX_ADDPD_XX, FE_SSE_ADDPDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "addpd %0, %1"},
    {FX_SUBPD_XX, FE_SSE_SUBPDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "subpd %0, %1"},
    {FX_MULPD_XX, FE_SSE_MULPDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "mulpd %0, %1"},
    {FX_DIVPD_XX, FE_SSE_DIVPDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "divpd %0, %1"},
    {FX_SHUFPS_XXI, FE_SSE_SHUFPSrri, 3, {MO_XMM, MO_XMM, MO_IMED}, {128, 128, 0}, FL_NADA, "shufps %0, %1, %2"},
    {FX_UNPCKLPD_XX, FE_SSE_UNPCKLPDrr, 2, {MO_XMM, MO_XMM, MO_NADA}, {128, 128, 0}, FL_NADA, "unpcklpd %0, %1"},
};

static bool RecusarX0902(Diag *erro, const MInstr *mi, const char *texto)
{
    return DiagRelatar(erro, X0902, mi->pos, "codificador x64 recusou %s", texto);
}

/* largura 0 na tabela = qualquer tamanho de acesso (LEA nao tem tamanho). */
static bool MemoriaAceita(const FormaX64 *f, const MOperando *o, uint8_t i)
{
    if (o->escala != 0 && (o->base == X64_RIP || o->indice == X64_RSP || o->indice > X64_R15))
        return false; /* rsp nao e indice; [rip+idx*s] nao existe */
    if (o->escala != 0 && o->desloc == 0 && o->base != X64_RIP && (o->base & 7) == 5)
        return false; /* [rbp/r13 + idx*s] sem deslocamento: o fadec 0.9 grava mod=00 com base 101 (= sem base); o isel troca base e indice ou desiste (IselOtimEncodavel) */
    if (o->base == X64_RIP)
        return (f->flags & X64_FL_RIP) != 0;
    return o->base <= X64_R15 && (f->largura[i] == 0 || o->largura == f->largura[i]);
}

static bool OperandoAceito(const FormaX64 *f, const MOperando *o, uint8_t i)
{
    if (o->tipo != f->tipo[i])
        return false;
    if (o->tipo == MO_XMM)
        return o->base <= X64_R15; /* xmm0..xmm15 */
    if (o->tipo == MO_REG)
        return o->base <= X64_R15 && o->largura == f->largura[i];
    if (o->tipo == MO_MEM)
        return MemoriaAceita(f, o, i);
    return true;
}

static bool FormaAceita(const FormaX64 *f, const MInstr *mi)
{
    if (mi->nops != f->nops)
        return false;
    if ((f->flags & X64_FL_CC) && mi->cond > X64_CC_G)
        return false;
    for (uint8_t i = 0; i < f->nops; i++) {
        if (!OperandoAceito(f, &mi->op[i], i))
            return false;
    }
    return true;
}

static uint64_t MnemonicoDaForma(const FormaX64 *f, const MInstr *mi)
{
    uint64_t mnemonico = f->mnemonico;
    if (f->flags & X64_FL_CC)
        mnemonico |= (uint64_t)mi->cond;
    if (f->flags & X64_FL_REL32)
        mnemonico |= FE_JMPL;
    return mnemonico;
}

/* Mapeia o operando da MInstr para o FeOp do fadec. O rel32 e calculado a partir do endereco de dst,
 * para que o fadec, que subtrai o fim da instrucao, devolva exatamente o desloc. */
static FeOp OperandoParaFe(const FormaX64 *f, const MOperando *o, const uint8_t *dst)
{
    FeOp base;
    switch (o->tipo) {
    case MO_REG:
        return FE_AX + (FeOp)o->base;
    case MO_XMM:
        return FE_XMM0 + (FeOp)o->base;
    case MO_MEM:
        base = o->base == X64_RIP ? FE_IP : FE_AX + (FeOp)o->base;
        if (o->escala != 0)
            return FE_MEM(base, o->escala, FE_AX + (FeOp)o->indice, o->desloc);
        return FE_MEM(base, 0, 0, o->desloc);
    case MO_IMED:
        return o->imed;
    case MO_BLOCO:
    case MO_SIMBOLO:
        return (FeOp)(intptr_t)dst + (FeOp)((f->flags & X64_FL_CC) ? X64_REL32_BASE_JCC : X64_REL32_BASE_JMP) + o->desloc;
    default:
        return 0;
    }
}

static bool TemRip(const MInstr *mi)
{
    for (uint8_t i = 0; i < mi->nops; i++) {
        if (mi->op[i].tipo == MO_MEM && mi->op[i].base == X64_RIP)
            return true;
    }
    return false;
}

/* O fadec guarda registrador e imediato no mesmo tipo (FeOp) e le como AH..BH (0x204..0x207) o imediato de op1 com esses valores,
 * recusando a instrucao. Corrige-se codificando com um substituto de mesma classe (positivo, fora de imm8, cabe em imm32) e regravando
 * o imm32, que nas formas RI e sempre o fim da instrucao. */
static bool ImediatoColideComRegistrador(const MInstr *mi)
{
    return mi->nops == 2 && mi->op[1].tipo == MO_IMED && mi->op[1].imed >= X64_IMED_COLIDE_MIN && mi->op[1].imed <= X64_IMED_COLIDE_MAX;
}

static void OperandosParaFe(const FormaX64 *f, const MInstr *mi, const uint8_t *dst, FeOp ops[3])
{
    for (uint8_t i = 0; i < mi->nops; i++)
        ops[i] = OperandoParaFe(f, &mi->op[i], dst);
    if (f->id == FX_SHUFPS_XXI) /* o fadec le o imm8 como inteiro com sinal: 0x80..0xFF entram como o byte negativo de mesmo padrao de bits */
        ops[2] = (FeOp)(int8_t)(uint8_t)mi->op[2].imed;
}

/* Depois de codificar: regrava o imm32 real quando houve substituto e zera o disp32 de RIP (o ligador o preenche). */
static void AjustarBytesCodificados(const MInstr *mi, uint8_t *dst, uint32_t tam, bool colide)
{
    if (colide) {
        int32_t real = (int32_t)mi->op[1].imed;
        memcpy(dst + tam - X64_DISP_RIP_BYTES, &real, sizeof real);
    }
    if (TemRip(mi))
        memset(dst + tam - X64_DISP_RIP_BYTES, 0, X64_DISP_RIP_BYTES);
}

bool X64CodificarInstr(const MInstr *mi, uint8_t *dst, uint32_t *tam, Diag *erro)
{
    if (mi->forma >= FX_QTD)
        return RecusarX0902(erro, mi, "uma forma fora da tabela FORMAS_X64");
    const FormaX64 *f = &FORMAS_X64[mi->forma];
    if (!FormaAceita(f, mi))
        return RecusarX0902(erro, mi, f->molde);
    FeOp ops[3] = {0, 0, 0};
    OperandosParaFe(f, mi, dst, ops);
    bool colide = ImediatoColideComRegistrador(mi);
    if (colide)
        ops[1] = X64_IMED_SUBSTITUTO;
    uint8_t *fim = dst;
    if (fe_enc64_impl(&fim, MnemonicoDaForma(f, mi), ops[0], ops[1], ops[2], 0) != 0)
        return RecusarX0902(erro, mi, f->molde);
    *tam = (uint32_t)(fim - dst);
    AjustarBytesCodificados(mi, dst, *tam, colide);
    return true;
}
