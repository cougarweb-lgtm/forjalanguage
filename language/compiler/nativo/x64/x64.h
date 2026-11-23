/* x64.h - instrucoes x86-64 do backend nativo: registradores, condicoes, operandos, formas e a fachada do codificador sobre o fadec.
 * Versao: 0.11.0
 * Changelog: 0.11.0 - S2 (D0703): FX_ADDPS..DIVPD_XX, FX_SHUFPS_XXI e FX_UNPCKLPD_XX (caso E48 do oraculo); FX_QTD 127 -> 137.
 *            0.10.0 - R5 (SIMD): FX_CMOV32_RR, FX_CMOV64_RR, FX_MINSS_XX, FX_MAXSS_XX, FX_MINSD_XX e FX_MAXSD_XX (caso E47 do oraculo); FX_QTD 121 -> 127.
 *            0.9.0 - M7b F1: FX_SAR32_RI, FX_SAR64_RI e FX_IMUL64_R (caso E46 do oraculo); FX_QTD 118 -> 121.
 *            0.8.0 - M7a F5: MOperando.indice/escala (memoria [base+indice*escala+desloc]) e FX_IMUL32_RRI/FX_IMUL64_RRI (imul r,r,imm32; caso E44); FX_QTD 116 -> 118.
 *            0.7.0 - M7a F4: FX_MOVAPS_XX (caso E43 do oraculo); FX_QTD 115 -> 116.
 *            0.6.0 - M3 P21v-c: FX_CALL_R64 (call r64, caso E42 do oraculo); FX_QTD 114 -> 115.
 *            0.5.0 - M3 P19: FX_CALL_M64 (call qword [reg], caso E41 do oraculo); FX_QTD 113 -> 114.
 *            0.4.0 - Linguagem v1.11 L13: FX_SQRTSS_XX e FX_SQRTSD_XX (caso E38 do oraculo); FX_QTD 111 -> 113 (FX_QTD real conferido pelo teste_nativo).
 *            0.3.0 - M2 P10b: MO_XMM (xmm0..xmm15, o numero em base) e as formas E31..E37 e E40 (SSE escalar: MOVSS/MOVSD, ADDSS..DIVSD, UCOMISS/UCOMISD, CVTSI2SS/SD, CVTSS2SD/
 *                    CVTSD2SS, XORPS, MOVD/MOVQ entre xmm e inteiro, MOVUPS); 30 formas novas; FX_QTD 81 -> 111. Memoria SSE: largura 32 (ss), 64 (sd) ou 128 (ps).
 *            0.2.0 - M2 P9: largura 16 (MO_REG e MO_MEM) e as formas E19..E27: MOVSX/MOVZX de 8 e 16 bits, MOV m16,r16, CDQ, CQO, IDIV, DIV, SHL/SHR por cl e
 *                    por imm8 e NOT (25 formas novas; FX_QTD 56 -> 81).
 *            0.1.0 - backend x64 P4b: X64Reg, X64Cond, MOperando, MInstr, FormaX64/FORMAS_X64, X64CodificarInstr (X0902) e X64Formatar.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes) e python -I ..\tests\nativo\enc\verificar_enc.py
 */
#ifndef FORJA_NATIVO_X64_X64_H
#define FORJA_NATIVO_X64_X64_H

#include "common/buf.h"
#include "common/diag.h"

/* Numeros de hardware: RAX..R15 valem o campo ModRM/REX. X64_RIP so existe como base de [rip+d32]. */
typedef enum {
    X64_RAX, X64_RCX, X64_RDX, X64_RBX, X64_RSP, X64_RBP, X64_RSI, X64_RDI,
    X64_R8, X64_R9, X64_R10, X64_R11, X64_R12, X64_R13, X64_R14, X64_R15,
    X64_RIP = 16
} X64Reg;

/* Os 16 codigos de condicao (o nibble baixo de Jcc 0x70+cc, SETcc 0F 90+cc e Jcc 0F 80+cc). */
typedef enum {
    X64_CC_O, X64_CC_NO, X64_CC_B, X64_CC_AE, X64_CC_E, X64_CC_NE, X64_CC_BE, X64_CC_A,
    X64_CC_S, X64_CC_NS, X64_CC_P, X64_CC_NP, X64_CC_L, X64_CC_GE, X64_CC_LE, X64_CC_G
} X64Cond;

typedef enum { MO_NADA, MO_REG, MO_IMED, MO_MEM, MO_BLOCO, MO_SIMBOLO, MO_XMM } MOperandoTipo;

/* Operando da MInstr. Campos usados conforme o tipo:
 *   MO_REG: base = registro; largura 8, 16, 32 ou 64.
 *   MO_XMM: base = numero do registrador xmm (0..15); a largura nao conta (a instrucao diz se e ss, sd ou ps).
 *   MO_MEM: base = registro ou X64_RIP; desloc = deslocamento; escala 1, 2, 4 ou 8 soma indice*escala (escala 0 = sem indice; nunca com X64_RIP); largura 8, 16, 32, 64 ou 128 (tamanho do acesso; 128 so nas formas SSE).
 *   MO_IMED: imed = valor (a largura do imediato e escolhida pelo codificador).
 *   MO_BLOCO: imed = indice do bloco-alvo do salto (o desloc vale o rel32 gravado).
 *   MO_SIMBOLO: simbolo = indice do simbolo alvo da chamada (o desloc vale o rel32 gravado).
 * desloc, em JMP/Jcc/CALL, e o rel32 relativo ao fim da instrucao; quem corrige saltos locais e o P7. */
typedef struct {
    MOperandoTipo tipo;
    uint8_t largura;
    X64Reg base;
    int32_t desloc;
    int64_t imed;
    uint32_t simbolo;
    X64Reg indice;
    uint8_t escala;
} MOperando;

/* Instrucao de maquina: forma (indice de FORMAS_X64), condicao (so SETcc e Jcc), operandos e origem no .fj. */
typedef struct {
    uint16_t forma;
    X64Cond cond;
    uint8_t nops;
    MOperando op[3];
    uint32_t pos;
    uint32_t bir;
} MInstr;

/* Formas do M1 e do M2. A ordem tem de bater com FORMAS_X64[] (x64_codificar.c). */
typedef enum {
    FX_MOV32_RR, FX_MOV64_RR,
    FX_MOV32_RM, FX_MOV64_RM,
    FX_MOVZX32_RM8,
    FX_MOV8_MR, FX_MOV32_MR, FX_MOV64_MR,
    FX_MOV32_RI, FX_MOV64_RI,
    FX_MOVZX32_RR8,
    FX_MOVSXD64_RR32,
    FX_LEA64_RM,
    FX_ADD32_RR, FX_ADD64_RR, FX_SUB32_RR, FX_SUB64_RR, FX_AND32_RR, FX_AND64_RR,
    FX_OR32_RR, FX_OR64_RR, FX_XOR32_RR, FX_XOR64_RR, FX_CMP32_RR, FX_CMP64_RR,
    FX_ADD32_RI, FX_ADD64_RI, FX_SUB32_RI, FX_SUB64_RI, FX_AND32_RI, FX_AND64_RI,
    FX_OR32_RI, FX_OR64_RI, FX_XOR32_RI, FX_XOR64_RI, FX_CMP32_RI, FX_CMP64_RI,
    FX_IMUL32_RR, FX_IMUL64_RR,
    FX_MUL32_R, FX_MUL64_R,
    FX_NEG32_R, FX_NEG64_R,
    FX_TEST8_RR, FX_TEST32_RR, FX_TEST64_RR, FX_TEST64_MR,
    FX_SETCC8_R,
    FX_JCC_REL32, FX_JMP_REL32,
    FX_CALL_REL32,
    FX_RET, FX_PUSH64_R, FX_POP64_R, FX_UD2, FX_INT3,
    /* M2 P9 (E19..E27) */
    FX_MOVSX32_RM8, FX_MOVSX32_RM16, FX_MOVSX64_RM8, FX_MOVSX64_RM16,
    FX_MOVZX32_RM16, FX_MOVZX32_RR16,
    FX_MOV16_MR,
    FX_MOVSX32_RR8, FX_MOVSX32_RR16,
    FX_CDQ, FX_CQO,
    FX_IDIV32_R, FX_IDIV64_R, FX_DIV32_R, FX_DIV64_R,
    FX_SHL32_RCL, FX_SHL64_RCL, FX_SHR32_RCL, FX_SHR64_RCL,
    FX_NOT32_R, FX_NOT64_R,
    FX_SHL32_RI, FX_SHL64_RI, FX_SHR32_RI, FX_SHR64_RI,
    /* M2 P10b (E31..E37, E40); X = xmm, M = memoria, R = registrador inteiro */
    FX_MOVSS_XM, FX_MOVSD_XM, FX_MOVSS_MX, FX_MOVSD_MX, FX_MOVSS_XX, FX_MOVSD_XX,
    FX_ADDSS_XX, FX_SUBSS_XX, FX_MULSS_XX, FX_DIVSS_XX, FX_ADDSD_XX, FX_SUBSD_XX, FX_MULSD_XX, FX_DIVSD_XX,
    FX_UCOMISS_XX, FX_UCOMISD_XX,
    FX_CVTSI2SS_XR32, FX_CVTSI2SS_XR64, FX_CVTSI2SD_XR32, FX_CVTSI2SD_XR64,
    FX_CVTSS2SD_XX, FX_CVTSD2SS_XX,
    FX_XORPS_XM, FX_XORPS_XX,
    FX_MOVD_XR, FX_MOVD_RX, FX_MOVQ_XR, FX_MOVQ_RX,
    FX_MOVUPS_XM, FX_MOVUPS_MX,
    FX_MOVAPS_XX, /* M7a F4 (E43): copia xmm -> xmm inteira (sem a dependencia do movss/movsd, que preservam o alto) */
    FX_SQRTSS_XX, FX_SQRTSD_XX, /* Linguagem v1.11 L13 (E38): raiz quadrada escalar */
    FX_CALL_M64, /* M3 P19 (E41): call qword [reg], a chamada pelo slot da IAT */
    FX_CALL_R64, /* M3 P21v-c (E42): call r64, a chamada indireta pelo valor de um proc */
    FX_IMUL32_RRI, FX_IMUL64_RRI, /* M7a F5 (E44): imul r,r,imm32 */
    FX_SAR32_RI, FX_SAR64_RI, FX_IMUL64_R, /* M7b F1 (E46): sar r,imm8 e imul r64 de um operando (rdx:rax = rax * r) */
    FX_CMOV32_RR, FX_CMOV64_RR, /* R5 (E47): cmovcc r,r (a condicao em MInstr.cond) */
    FX_MINSS_XX, FX_MAXSS_XX, FX_MINSD_XX, FX_MAXSD_XX, /* R5 (E47): minss/maxss/minsd/maxsd xmm,xmm */
    FX_ADDPS_XX, FX_SUBPS_XX, FX_MULPS_XX, FX_DIVPS_XX, FX_ADDPD_XX, FX_SUBPD_XX, FX_MULPD_XX, FX_DIVPD_XX, /* S2 (E48): aritmetica empacotada xmm,xmm */
    FX_SHUFPS_XXI, FX_UNPCKLPD_XX, /* S2 (E48): shufps xmm,xmm,imm8 (splat f32) e unpcklpd xmm,xmm (splat f64) */
    FX_QTD
} X64Forma;

/* Flags da forma: X64_FL_CC usa mi->cond; X64_FL_REL32 e salto ou call (rel32 com FE_JMPL); X64_FL_RIP aceita [rip+d32]. */
#define X64_FL_CC 0x01
#define X64_FL_REL32 0x02
#define X64_FL_RIP 0x04

/* Uma linha da tabela: mnemonico fadec (sem o cc), operandos com tipo e largura, flags e molde Intel.
 * No molde, %0..%2 sao os operandos e %k o nome da condicao. */
typedef struct {
    X64Forma id;
    uint64_t mnemonico;
    uint8_t nops;
    MOperandoTipo tipo[3];
    uint8_t largura[3];
    uint8_t flags;
    const char *molde;
} FormaX64;

extern const FormaX64 FORMAS_X64[FX_QTD];

/* Codifica a instrucao em dst (15 bytes no minimo) e devolve o tamanho em tam. Falha = false com X0902 em diag.
 * Em [rip+d32] os 4 ultimos bytes saem zerados: quem chama registra a relocacao OR_REL32 em tam-4. */
bool X64CodificarInstr(const MInstr *mi, uint8_t *dst, uint32_t *tam, Diag *erro);

/* Anexa o texto Intel da instrucao (ex.: "mov eax, ecx") ao buffer de saida. */
void X64Formatar(const MInstr *mi, Buf *saida);

/* Nome do registro para a largura dada (64, 32, 16 ou 8), ex.: X64NomeRegistro(X64_RSI, 8) = "sil". */
const char *X64NomeRegistro(X64Reg reg, uint8_t largura);

#endif
