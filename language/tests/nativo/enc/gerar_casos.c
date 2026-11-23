/* gerar_casos.c - modo "enc" do teste_nativo: grava casos.asm (BITS 64), nossos.bin e casos.idx para o oraculo NASM (verificar_enc.py).
 * Versao: 0.10.0
 * Changelog: 0.10.0 - S2 (D0703): caso E48 (ADDPS..DIVPD e UNPCKLPD xmm,xmm; SHUFPS xmm,xmm,imm8; MOVUPS com [base+indice*escala+desloc] em xmm0..15).
 *            0.9.0 - M7b F1: caso E46 (SAR r,imm8 de 32 e 64 bits e IMUL r64 de um operando).
 *            0.8.0 - M7a F5: casos E44 (IMUL r,r,imm32) e E45 (LEA com [base+indice*escala+desloc]).
 *            0.7.0 - M7a F4: caso E43 (MOVAPS xmm,xmm; 33 formas SSE).
 *            0.6.0 - M3 P21v-c: caso E42 (CALL r64: 16 registradores).
 *            0.5.0 - M3 P19: caso E41 (CALL r/m64 pela memoria: 16 bases x 3 deslocamentos = 48 casos).
 *            0.4.0 -Linguagem v1.11 L13: caso E38 (SQRTSS e SQRTSD xmm,xmm; 32 formas SSE).
 *            0.3.0 - M2 P10b: casos E31..E37 e E40 (30 formas SSE escalares: xmm com xmm, memoria e registro de inteiro); memoria de 128 bits no texto ("oword").
 *            0.2.0 - M2 P9: casos E19..E27 (MOVSX/MOVZX de 8 e 16 bits, MOV m16,r16, CDQ, CQO, IDIV, DIV, NOT, SHL/SHR por cl e imm8); largura 16 no texto.
 *            0.1.1 - backend x64 P7a: imediatos 0x203..0x208 (o 0x204..0x207 colide com AH..BH no fadec).
 *            0.1.0 - backend x64 P4b: casos E01..E18 do contrato M1 (16 GPR em cada posicao, mem com base/d8/d32 e [rip+d32] no LEA,
 *                    imediatos do M1); cada caso sai com o texto NASM, o codigo do fadec e o estado ok/recusado.
 * Verificacao: python tests\nativo\enc\verificar_enc.py (sai 0 so com falhas=0 e recusados=0)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "common/buf.h"
#include "common/diag.h"
#include "nativo/x64/x64.h"
#include "operandos_enc.h"

#define ENC_QTD_REG 16
#define ENC_QTD_IMED 19
#define ENC_QTD_IMED64 18
#define ENC_QTD_DESLOC 3
#define ENC_QTD_REL 9
#define ENC_QTD_CONTADOR 10
#define ENC_TAM_LINHA 200
#define ENC_TAM_CAMINHO 1024
/* LEA r64,[rip+d32] tem 7 bytes: REX.W, 8D, ModRM e disp32. O NASM recebe [rel $+(d+7)]. */
#define ENC_TAM_LEA_RIP 7

/* Imediatos do M1: 0, 1, -1, 127, 128, -128, -129, 2^31-1 e -2^31, mais 0x203..0x208 (0x204..0x207 colidem com AH..BH no fadec;
 * o X64CodificarInstr os contorna, P7a). O MOV64ri ainda leva 2^32 e os limites de 64 bits
 * (o +2^63 nao cabe em int64: vai 2^63-1). */
static const int64_t ENC_IMEDS[ENC_QTD_IMED] = {0, 1, -1, 127, 128, -128, -129, INT32_MAX, INT32_MIN,
                                                0x203, 0x204, 0x205, 0x206, 0x207, 0x208,
                                                255, 65535, 32767, -32768}; /* M2 P9: MIN/MAX de 8 e 16 bits do CMP r32,imm do *_OV curto */
static const int64_t ENC_IMEDS64[ENC_QTD_IMED64] = {0, 1, -1, 127, 128, -128, -129, INT32_MAX, INT32_MIN,
                                                    0x100000000LL, INT64_MAX, INT64_MIN,
                                                    0x203, 0x204, 0x205, 0x206, 0x207, 0x208};
/* Memoria: sem deslocamento, d8 negativo e d32. */
static const int32_t ENC_DESLOCS[ENC_QTD_DESLOC] = {0, -8, 0x12345678};
static const int64_t ENC_RELS[ENC_QTD_REL] = {0, 1, -1, 127, 128, -128, -129, 0x12345678, -0x12345678};
static const char *const ENC_NOMES_ALU[6] = {"add", "sub", "and", "or", "xor", "cmp"};
static const char *const ENC_NOMES_COND[16] = {"o", "no", "b", "ae", "e", "ne", "be", "a",
                                               "s", "ns", "p", "np", "l", "ge", "le", "g"};

typedef struct {
    FILE *arq_asm;
    FILE *arq_idx;
    Buf bin;
    uint32_t linha;
    uint32_t recusados;
} EncGerador;

/* grupo = rotulo E01..E18 do contrato; larg_d e larg_s = largura de cada operando (0 = qualquer, como LEA). */
typedef struct {
    const char *grupo;
    uint16_t forma;
    const char *mnem;
    uint8_t larg_d;
    uint8_t larg_s;
} EncForma;

static uint16_t EncFormaAlu(uint8_t operacao, bool imediato, bool w64)
{
    /* Os FX_*_RR e FX_*_RI de cada operacao sao consecutivos (32 antes de 64), na mesma ordem de ENC_NOMES_ALU. */
    uint16_t base = imediato ? FX_ADD32_RI : FX_ADD32_RR;
    return (uint16_t)(base + 2 * operacao + (w64 ? 1 : 0));
}

static const char *EncTamanho(uint8_t larg)
{
    switch (larg) {
    case 8: return "byte ";
    case 16: return "word ";
    case 32: return "dword ";
    case 64: return "qword ";
    case 128: return "oword ";
    default: return "";
    }
}

/* NASM nao tem [rip+d]: o RIP-relativo sai como [rel $+N], com N = d + tamanho da instrucao (LEA r64 tem 7 bytes). */
static void EncMemTexto(char *out, size_t n, X64Reg base, int32_t desloc, uint8_t larg)
{
    const char *nome = X64NomeRegistro(base, 64);
    if (base == X64_RIP)
        snprintf(out, n, "%s[rel $%+lld]", EncTamanho(larg), (long long)desloc + ENC_TAM_LEA_RIP);
    else if (desloc == 0)
        snprintf(out, n, "%s[%s]", EncTamanho(larg), nome);
    else
        snprintf(out, n, "%s[%s%+d]", EncTamanho(larg), nome, (int)desloc);
}

static bool EncAbrir(EncGerador *g, const char *pasta)
{
    char caminho[ENC_TAM_CAMINHO];
    memset(g, 0, sizeof *g);
    g->linha = 2;
    snprintf(caminho, sizeof caminho, "%s/casos.asm", pasta);
    g->arq_asm = fopen(caminho, "w");
    snprintf(caminho, sizeof caminho, "%s/casos.idx", pasta);
    g->arq_idx = fopen(caminho, "w");
    if (g->arq_asm == NULL || g->arq_idx == NULL)
        return false;
    fprintf(g->arq_asm, "bits 64\n");
    return true;
}

static bool EncFechar(EncGerador *g, const char *pasta)
{
    char caminho[ENC_TAM_CAMINHO];
    snprintf(caminho, sizeof caminho, "%s/nossos.bin", pasta);
    FILE *bin = fopen(caminho, "wb");
    bool ok = bin != NULL && fwrite(g->bin.dados, 1, g->bin.tam, bin) == g->bin.tam;
    if (bin != NULL)
        ok = fclose(bin) == 0 && ok;
    ok = fclose(g->arq_asm) == 0 && ok;
    ok = fclose(g->arq_idx) == 0 && ok;
    return ok;
}

/* Codifica o caso e grava a linha NASM e a linha do idx. Caso recusado vai com tamanho 0 e estado "recusado". */
static void EncCaso(EncGerador *g, const char *grupo, const MInstr *mi, const char *txt)
{
    uint8_t cod[16];
    uint32_t tam = 0;
    Diag erro = {0};
    bool ok = X64CodificarInstr(mi, cod, &tam, &erro);
    uint32_t off = (uint32_t)g->bin.tam;
    fprintf(g->arq_asm, "%s\n", txt);
    if (ok) {
        BufAnexarN(&g->bin, (const char *)cod, tam);
    } else {
        tam = 0;
        g->recusados++;
    }
    fprintf(g->arq_idx, "%s %u %u %u %u %s\n", grupo, (unsigned)mi->forma, off, tam, g->linha, ok ? "ok" : "recusado");
    g->linha++;
}

static void EncRR(EncGerador *g, const EncForma *f, X64Reg d, X64Reg s)
{
    MInstr mi = MiNova(f->forma, 2);
    char txt[ENC_TAM_LINHA];
    mi.op[0] = OpReg(d, f->larg_d);
    mi.op[1] = OpReg(s, f->larg_s);
    snprintf(txt, sizeof txt, "%s %s, %s", f->mnem, X64NomeRegistro(d, f->larg_d), X64NomeRegistro(s, f->larg_s));
    EncCaso(g, f->grupo, &mi, txt);
}

/* Cada registro entra como destino e como origem, com parceiros dados por formula (3k+1 e 5k+2 mod 16). */
static void EncParesRR(EncGerador *g, const EncForma *f)
{
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        EncRR(g, f, (X64Reg)k, (X64Reg)((k * 3 + 1) % ENC_QTD_REG));
        EncRR(g, f, (X64Reg)((k * 5 + 2) % ENC_QTD_REG), (X64Reg)k);
    }
}

/* Registro (op0) e memoria (op1, base e deslocamento). RIP so passa aqui no LEA. */
static void EncRM(EncGerador *g, const EncForma *f, X64Reg reg, X64Reg base, int32_t desloc)
{
    MInstr mi = MiNova(f->forma, 2);
    char mem[64];
    char txt[ENC_TAM_LINHA];
    mi.op[0] = OpReg(reg, f->larg_d);
    mi.op[1] = OpMem(base, desloc, f->larg_s);
    EncMemTexto(mem, sizeof mem, base, desloc, f->larg_s);
    snprintf(txt, sizeof txt, "%s %s, %s", f->mnem, X64NomeRegistro(reg, f->larg_d), mem);
    EncCaso(g, f->grupo, &mi, txt);
}

static void EncRMTodos(EncGerador *g, const EncForma *f)
{
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
            for (uint32_t d = 0; d < ENC_QTD_DESLOC; d++)
                EncRM(g, f, (X64Reg)k, (X64Reg)b, ENC_DESLOCS[d]);
        }
    }
}

static void EncRipTodos(EncGerador *g, const EncForma *f)
{
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t d = 0; d < ENC_QTD_DESLOC; d++)
            EncRM(g, f, (X64Reg)k, X64_RIP, ENC_DESLOCS[d]);
    }
}

/* Memoria em op0 (largura larg_d) e registro em op1 (largura larg_s). */
static void EncMR(EncGerador *g, const EncForma *f, X64Reg base, int32_t desloc, X64Reg reg)
{
    MInstr mi = MiNova(f->forma, 2);
    char mem[64];
    char txt[ENC_TAM_LINHA];
    mi.op[0] = OpMem(base, desloc, f->larg_d);
    mi.op[1] = OpReg(reg, f->larg_s);
    EncMemTexto(mem, sizeof mem, base, desloc, f->larg_d);
    snprintf(txt, sizeof txt, "%s %s, %s", f->mnem, mem, X64NomeRegistro(reg, f->larg_s));
    EncCaso(g, f->grupo, &mi, txt);
}

static void EncMRTodos(EncGerador *g, const EncForma *f)
{
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
            for (uint32_t d = 0; d < ENC_QTD_DESLOC; d++)
                EncMR(g, f, (X64Reg)b, ENC_DESLOCS[d], (X64Reg)k);
        }
    }
}

/* MOV r64,imm que cabe em 32 bits sai como "strict dword": o NASM senao escolheria B8 (zero-extend) e nao o C7 sx32 do codificador. */
static void EncRI(EncGerador *g, const EncForma *f, X64Reg reg, int64_t imm)
{
    MInstr mi = MiNova(f->forma, 2);
    char txt[ENC_TAM_LINHA];
    bool cabe32 = imm >= INT32_MIN && imm <= INT32_MAX;
    const char *tam = (f->forma == FX_MOV64_RI && cabe32) ? "strict dword " : "";
    mi.op[0] = OpReg(reg, f->larg_d);
    mi.op[1] = OpImed(imm);
    snprintf(txt, sizeof txt, "%s %s, %s%lld", f->mnem, X64NomeRegistro(reg, f->larg_d), tam, (long long)imm);
    EncCaso(g, f->grupo, &mi, txt);
}

static void EncRU(EncGerador *g, const EncForma *f, X64Reg reg)
{
    MInstr mi = MiNova(f->forma, 1);
    char txt[ENC_TAM_LINHA];
    mi.op[0] = OpReg(reg, f->larg_d);
    snprintf(txt, sizeof txt, "%s %s", f->mnem, X64NomeRegistro(reg, f->larg_d));
    EncCaso(g, f->grupo, &mi, txt);
}

static void EncSet(EncGerador *g, X64Cond cond, X64Reg reg)
{
    MInstr mi = MiNova(FX_SETCC8_R, 1);
    char txt[ENC_TAM_LINHA];
    mi.cond = cond;
    mi.op[0] = OpReg(reg, 8);
    snprintf(txt, sizeof txt, "set%s %s", ENC_NOMES_COND[cond], X64NomeRegistro(reg, 8));
    EncCaso(g, "E15", &mi, txt);
}

/* Salto ou call: o NASM recebe "$+N" com N = rel + tamanho da instrucao, para que o disp saia igual a rel. */
static void EncRel(EncGerador *g, const char *grupo, const MInstr *mi, const char *prefixo, uint8_t tam_instr)
{
    char txt[ENC_TAM_LINHA];
    long long alvo = (long long)mi->op[0].desloc + tam_instr;
    snprintf(txt, sizeof txt, "%s $%+lld", prefixo, alvo);
    EncCaso(g, grupo, mi, txt);
}

static void GerarE01E07(EncGerador *g)
{
    EncForma mov32 = {"E01", FX_MOV32_RR, "mov", 32, 32};
    EncForma mov64 = {"E01", FX_MOV64_RR, "mov", 64, 64};
    EncForma movzx8 = {"E06", FX_MOVZX32_RR8, "movzx", 32, 8};
    EncForma movsxd = {"E07", FX_MOVSXD64_RR32, "movsxd", 64, 32};
    EncParesRR(g, &mov32);
    EncParesRR(g, &mov64);
    EncParesRR(g, &movzx8);
    EncParesRR(g, &movsxd);
}

static void GerarE02E03E08(EncGerador *g)
{
    EncForma mov32 = {"E02", FX_MOV32_RM, "mov", 32, 32};
    EncForma mov64 = {"E02", FX_MOV64_RM, "mov", 64, 64};
    EncForma movzx = {"E03", FX_MOVZX32_RM8, "movzx", 32, 8};
    EncForma lea = {"E08", FX_LEA64_RM, "lea", 64, 0};
    EncRMTodos(g, &mov32);
    EncRMTodos(g, &mov64);
    EncRMTodos(g, &movzx);
    EncRMTodos(g, &lea);
    EncRipTodos(g, &lea);
}

static void GerarE04E14Mem(EncGerador *g)
{
    EncForma m8 = {"E04", FX_MOV8_MR, "mov", 8, 8};
    EncForma m32 = {"E04", FX_MOV32_MR, "mov", 32, 32};
    EncForma m64 = {"E04", FX_MOV64_MR, "mov", 64, 64};
    EncForma test = {"E14", FX_TEST64_MR, "test", 64, 64};
    EncMRTodos(g, &m8);
    EncMRTodos(g, &m32);
    EncMRTodos(g, &m64);
    EncMRTodos(g, &test);
}

static void GerarE05(EncGerador *g)
{
    EncForma m32 = {"E05", FX_MOV32_RI, "mov", 32, 0};
    EncForma m64 = {"E05", FX_MOV64_RI, "mov", 64, 0};
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t i = 0; i < ENC_QTD_IMED; i++)
            EncRI(g, &m32, (X64Reg)k, ENC_IMEDS[i]);
        for (uint32_t i = 0; i < ENC_QTD_IMED64; i++)
            EncRI(g, &m64, (X64Reg)k, ENC_IMEDS64[i]);
    }
}

static void GerarE09E10(EncGerador *g)
{
    for (uint8_t op = 0; op < 6; op++) {
        for (uint8_t w = 0; w < 2; w++) {
            uint8_t larg = w ? 64 : 32;
            EncForma rr = {"E09", EncFormaAlu(op, false, w != 0), ENC_NOMES_ALU[op], larg, larg};
            EncForma ri = {"E10", EncFormaAlu(op, true, w != 0), ENC_NOMES_ALU[op], larg, 0};
            EncParesRR(g, &rr);
            for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
                for (uint32_t i = 0; i < ENC_QTD_IMED; i++)
                    EncRI(g, &ri, (X64Reg)k, ENC_IMEDS[i]);
            }
        }
    }
}

static void GerarE11E14Reg(EncGerador *g)
{
    EncForma imul32 = {"E11", FX_IMUL32_RR, "imul", 32, 32};
    EncForma imul64 = {"E11", FX_IMUL64_RR, "imul", 64, 64};
    EncForma test8 = {"E14", FX_TEST8_RR, "test", 8, 8};
    EncForma test32 = {"E14", FX_TEST32_RR, "test", 32, 32};
    EncForma test64 = {"E14", FX_TEST64_RR, "test", 64, 64};
    EncParesRR(g, &imul32);
    EncParesRR(g, &imul64);
    EncParesRR(g, &test8);
    EncParesRR(g, &test32);
    EncParesRR(g, &test64);
}

static void GerarE12E13(EncGerador *g)
{
    EncForma mul32 = {"E12", FX_MUL32_R, "mul", 32, 0};
    EncForma mul64 = {"E12", FX_MUL64_R, "mul", 64, 0};
    EncForma neg32 = {"E13", FX_NEG32_R, "neg", 32, 0};
    EncForma neg64 = {"E13", FX_NEG64_R, "neg", 64, 0};
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        EncRU(g, &mul32, (X64Reg)k);
        EncRU(g, &mul64, (X64Reg)k);
        EncRU(g, &neg32, (X64Reg)k);
        EncRU(g, &neg64, (X64Reg)k);
    }
}

static void GerarE15(EncGerador *g)
{
    for (uint32_t c = 0; c < 16; c++) {
        for (uint32_t k = 0; k < ENC_QTD_REG; k++)
            EncSet(g, (X64Cond)c, (X64Reg)k);
    }
}

static void GerarE16E17(EncGerador *g)
{
    char prefixo[32];
    for (uint32_t c = 0; c < 16; c++) {
        snprintf(prefixo, sizeof prefixo, "j%s near", ENC_NOMES_COND[c]);
        for (uint32_t i = 0; i < ENC_QTD_REL; i++) {
            MInstr mi = MiNova(FX_JCC_REL32, 1);
            mi.cond = (X64Cond)c;
            mi.op[0] = OpRel(MO_BLOCO, (int32_t)ENC_RELS[i]);
            EncRel(g, "E16", &mi, prefixo, 6);
        }
    }
    for (uint32_t i = 0; i < ENC_QTD_REL; i++) {
        MInstr jmp = MiNova(FX_JMP_REL32, 1);
        MInstr call = MiNova(FX_CALL_REL32, 1);
        jmp.op[0] = OpRel(MO_BLOCO, (int32_t)ENC_RELS[i]);
        call.op[0] = OpRel(MO_SIMBOLO, (int32_t)ENC_RELS[i]);
        EncRel(g, "E16", &jmp, "jmp near", 5);
        EncRel(g, "E17", &call, "call", 5);
    }
}

static void GerarE18(EncGerador *g)
{
    EncForma push = {"E18", FX_PUSH64_R, "push", 64, 0};
    EncForma pop = {"E18", FX_POP64_R, "pop", 64, 0};
    MInstr mi;
    mi = MiNova(FX_RET, 0);
    EncCaso(g, "E18", &mi, "ret");
    mi = MiNova(FX_UD2, 0);
    EncCaso(g, "E18", &mi, "ud2");
    mi = MiNova(FX_INT3, 0);
    EncCaso(g, "E18", &mi, "int3");
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        EncRU(g, &push, (X64Reg)k);
        EncRU(g, &pop, (X64Reg)k);
    }
}

/* ---- M2 P9: E19..E27 ---- */

/* shl/shr r,cl: o contador e sempre cl (rcx de 8 bits). */
static void EncCl(EncGerador *g, const EncForma *f, X64Reg reg)
{
    MInstr mi = MiNova(f->forma, 2);
    char txt[ENC_TAM_LINHA];
    mi.op[0] = OpReg(reg, f->larg_d);
    mi.op[1] = OpReg(X64_RCX, 8);
    snprintf(txt, sizeof txt, "%s %s, cl", f->mnem, X64NomeRegistro(reg, f->larg_d));
    EncCaso(g, f->grupo, &mi, txt);
}

static void GerarE19E20E22(EncGerador *g)
{
    EncForma sx32m8 = {"E19", FX_MOVSX32_RM8, "movsx", 32, 8};
    EncForma sx32m16 = {"E19", FX_MOVSX32_RM16, "movsx", 32, 16};
    EncForma sx64m8 = {"E19", FX_MOVSX64_RM8, "movsx", 64, 8};
    EncForma sx64m16 = {"E19", FX_MOVSX64_RM16, "movsx", 64, 16};
    EncForma zx32m16 = {"E20", FX_MOVZX32_RM16, "movzx", 32, 16};
    EncForma zx32r16 = {"E20", FX_MOVZX32_RR16, "movzx", 32, 16};
    EncForma sx32r8 = {"E22", FX_MOVSX32_RR8, "movsx", 32, 8};
    EncForma sx32r16 = {"E22", FX_MOVSX32_RR16, "movsx", 32, 16};
    EncRMTodos(g, &sx32m8);
    EncRMTodos(g, &sx32m16);
    EncRMTodos(g, &sx64m8);
    EncRMTodos(g, &sx64m16);
    EncRMTodos(g, &zx32m16);
    EncParesRR(g, &zx32r16);
    EncParesRR(g, &sx32r8);
    EncParesRR(g, &sx32r16);
}

static void GerarE21E23(EncGerador *g)
{
    EncForma mov16 = {"E21", FX_MOV16_MR, "mov", 16, 16};
    MInstr mi;
    EncMRTodos(g, &mov16);
    mi = MiNova(FX_CDQ, 0);
    EncCaso(g, "E23", &mi, "cdq");
    mi = MiNova(FX_CQO, 0);
    EncCaso(g, "E23", &mi, "cqo");
}

static void GerarE24E26(EncGerador *g)
{
    EncForma idiv32 = {"E24", FX_IDIV32_R, "idiv", 32, 0};
    EncForma idiv64 = {"E24", FX_IDIV64_R, "idiv", 64, 0};
    EncForma div32 = {"E24", FX_DIV32_R, "div", 32, 0};
    EncForma div64 = {"E24", FX_DIV64_R, "div", 64, 0};
    EncForma not32 = {"E26", FX_NOT32_R, "not", 32, 0};
    EncForma not64 = {"E26", FX_NOT64_R, "not", 64, 0};
    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        EncRU(g, &idiv32, (X64Reg)k);
        EncRU(g, &idiv64, (X64Reg)k);
        EncRU(g, &div32, (X64Reg)k);
        EncRU(g, &div64, (X64Reg)k);
        EncRU(g, &not32, (X64Reg)k);
        EncRU(g, &not64, (X64Reg)k);
    }
}

/* Contadores do imm8: 0, 1, 2, 7, 8, 15, 16, 31, 32, 63 (os limites de cada largura e a vizinhanca). */
static const int64_t ENC_CONTADORES[ENC_QTD_CONTADOR] = {0, 1, 2, 7, 8, 15, 16, 31, 32, 63};

static void GerarE25E27(EncGerador *g)
{
    EncForma cl[4] = {{"E25", FX_SHL32_RCL, "shl", 32, 8}, {"E25", FX_SHL64_RCL, "shl", 64, 8},
                      {"E25", FX_SHR32_RCL, "shr", 32, 8}, {"E25", FX_SHR64_RCL, "shr", 64, 8}};
    EncForma imm[4] = {{"E27", FX_SHL32_RI, "shl", 32, 0}, {"E27", FX_SHL64_RI, "shl", 64, 0},
                       {"E27", FX_SHR32_RI, "shr", 32, 0}, {"E27", FX_SHR64_RI, "shr", 64, 0}};
    for (uint32_t f = 0; f < 4; f++) {
        for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
            EncCl(g, &cl[f], (X64Reg)k);
            for (uint32_t i = 0; i < ENC_QTD_CONTADOR; i++)
                EncRI(g, &imm[f], (X64Reg)k, ENC_CONTADORES[i]);
        }
    }
}

/* ---- M2 P10b: E31..E37 e E40 (SSE escalar) ---- */

/* Forma SSE: tipo de cada operando ('x' xmm, 'r' registro de inteiro, 'm' memoria) e a largura dele (xmm conta 128). */
typedef struct {
    const char *grupo;
    uint16_t forma;
    const char *mnem;
    char tipo_d;
    char tipo_s;
    uint8_t larg_d;
    uint8_t larg_s;
} EncSse;

/* O operando n (numero do xmm, do registro) ou a memoria [base+desloc]; o texto NASM sai em txt. */
static MOperando EncSseOperando(char tipo, uint8_t larg, uint32_t n, X64Reg base, int32_t desloc, char *txt, size_t tam)
{
    MOperando o;
    if (tipo == 'm') {
        EncMemTexto(txt, tam, base, desloc, larg);
        return OpMem(base, desloc, larg);
    }
    o = OpReg((X64Reg)n, larg);
    if (tipo == 'x') {
        o.tipo = MO_XMM;
        snprintf(txt, tam, "xmm%u", (unsigned)n);
    } else {
        snprintf(txt, tam, "%s", X64NomeRegistro((X64Reg)n, larg));
    }
    return o;
}

/* Um caso: d e s sao os numeros dos registradores; base e desloc valem para o operando de memoria. */
static void EncSseCaso(EncGerador *g, const EncSse *f, uint32_t d, uint32_t s, X64Reg base, int32_t desloc)
{
    MInstr mi = MiNova(f->forma, 2);
    char txt_d[64];
    char txt_s[64];
    char txt[ENC_TAM_LINHA];
    mi.op[0] = EncSseOperando(f->tipo_d, f->larg_d, d, base, desloc, txt_d, sizeof txt_d);
    mi.op[1] = EncSseOperando(f->tipo_s, f->larg_s, s, base, desloc, txt_s, sizeof txt_s);
    snprintf(txt, sizeof txt, "%s %s, %s", f->mnem, txt_d, txt_s);
    EncCaso(g, f->grupo, &mi, txt);
}

/* Sem memoria: 32 pares por formula (como EncParesRR). Com memoria: o registro lado a lado com 16 bases e 3 deslocamentos. */
static void EncSseTodos(EncGerador *g, const EncSse *f)
{
    bool mem_d = f->tipo_d == 'm';

    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        if (f->tipo_d != 'm' && f->tipo_s != 'm') {
            EncSseCaso(g, f, k, (k * 3 + 1) % ENC_QTD_REG, X64_RAX, 0);
            EncSseCaso(g, f, (k * 5 + 2) % ENC_QTD_REG, k, X64_RAX, 0);
            continue;
        }
        for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
            for (uint32_t d = 0; d < ENC_QTD_DESLOC; d++)
                EncSseCaso(g, f, mem_d ? 0 : k, mem_d ? k : 0, (X64Reg)b, ENC_DESLOCS[d]);
        }
    }
}

#define ENC_QTD_SSE 42

static void GerarE31aE40(EncGerador *g)
{
    static const EncSse formas[ENC_QTD_SSE] = {
        {"E31", FX_MOVSS_XM, "movss", 'x', 'm', 128, 32}, {"E31", FX_MOVSD_XM, "movsd", 'x', 'm', 128, 64},
        {"E31", FX_MOVSS_MX, "movss", 'm', 'x', 32, 128}, {"E31", FX_MOVSD_MX, "movsd", 'm', 'x', 64, 128},
        {"E31", FX_MOVSS_XX, "movss", 'x', 'x', 128, 128}, {"E31", FX_MOVSD_XX, "movsd", 'x', 'x', 128, 128},
        {"E32", FX_ADDSS_XX, "addss", 'x', 'x', 128, 128}, {"E32", FX_SUBSS_XX, "subss", 'x', 'x', 128, 128},
        {"E32", FX_MULSS_XX, "mulss", 'x', 'x', 128, 128}, {"E32", FX_DIVSS_XX, "divss", 'x', 'x', 128, 128},
        {"E32", FX_ADDSD_XX, "addsd", 'x', 'x', 128, 128}, {"E32", FX_SUBSD_XX, "subsd", 'x', 'x', 128, 128},
        {"E32", FX_MULSD_XX, "mulsd", 'x', 'x', 128, 128}, {"E32", FX_DIVSD_XX, "divsd", 'x', 'x', 128, 128},
        {"E33", FX_UCOMISS_XX, "ucomiss", 'x', 'x', 128, 128}, {"E33", FX_UCOMISD_XX, "ucomisd", 'x', 'x', 128, 128},
        {"E34", FX_CVTSI2SS_XR32, "cvtsi2ss", 'x', 'r', 128, 32}, {"E34", FX_CVTSI2SS_XR64, "cvtsi2ss", 'x', 'r', 128, 64},
        {"E34", FX_CVTSI2SD_XR32, "cvtsi2sd", 'x', 'r', 128, 32}, {"E34", FX_CVTSI2SD_XR64, "cvtsi2sd", 'x', 'r', 128, 64},
        {"E35", FX_CVTSS2SD_XX, "cvtss2sd", 'x', 'x', 128, 128}, {"E35", FX_CVTSD2SS_XX, "cvtsd2ss", 'x', 'x', 128, 128},
        {"E36", FX_XORPS_XM, "xorps", 'x', 'm', 128, 128}, {"E36", FX_XORPS_XX, "xorps", 'x', 'x', 128, 128},
        {"E37", FX_MOVD_XR, "movd", 'x', 'r', 128, 32}, {"E37", FX_MOVD_RX, "movd", 'r', 'x', 32, 128},
        {"E37", FX_MOVQ_XR, "movq", 'x', 'r', 128, 64}, {"E37", FX_MOVQ_RX, "movq", 'r', 'x', 64, 128},
        {"E40", FX_MOVUPS_XM, "movups", 'x', 'm', 128, 128}, {"E40", FX_MOVUPS_MX, "movups", 'm', 'x', 128, 128},
        {"E38", FX_SQRTSS_XX, "sqrtss", 'x', 'x', 128, 128}, {"E38", FX_SQRTSD_XX, "sqrtsd", 'x', 'x', 128, 128},
        {"E43", FX_MOVAPS_XX, "movaps", 'x', 'x', 128, 128},
        {"E48", FX_ADDPS_XX, "addps", 'x', 'x', 128, 128}, {"E48", FX_SUBPS_XX, "subps", 'x', 'x', 128, 128},
        {"E48", FX_MULPS_XX, "mulps", 'x', 'x', 128, 128}, {"E48", FX_DIVPS_XX, "divps", 'x', 'x', 128, 128},
        {"E48", FX_ADDPD_XX, "addpd", 'x', 'x', 128, 128}, {"E48", FX_SUBPD_XX, "subpd", 'x', 'x', 128, 128},
        {"E48", FX_MULPD_XX, "mulpd", 'x', 'x', 128, 128}, {"E48", FX_DIVPD_XX, "divpd", 'x', 'x', 128, 128},
        {"E48", FX_UNPCKLPD_XX, "unpcklpd", 'x', 'x', 128, 128},
    };
    for (uint32_t k = 0; k < ENC_QTD_SSE; k++)
        EncSseTodos(g, &formas[k]);
}

/* ---- M3 P19: E41 (CALL r/m64, o call pelo slot da IAT: call qword [base+desloc]) ---- */

static void GerarE41(EncGerador *g)
{
    for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
        for (uint32_t d = 0; d < ENC_QTD_DESLOC; d++) {
            MInstr mi = MiNova(FX_CALL_M64, 1);
            char mem[64];
            char txt[ENC_TAM_LINHA];
            mi.op[0] = OpMem((X64Reg)b, ENC_DESLOCS[d], 64);
            EncMemTexto(mem, sizeof mem, (X64Reg)b, ENC_DESLOCS[d], 64);
            snprintf(txt, sizeof txt, "call %s", mem);
            EncCaso(g, "E41", &mi, txt);
        }
    }
}

/* ---- M3 P21v-c: E42 (CALL r64, a chamada indireta pelo valor de um proc: call rax e as outras 15) ---- */

static void GerarE42(EncGerador *g)
{
    EncForma call = {"E42", FX_CALL_R64, "call", 64, 0};

    for (uint32_t k = 0; k < ENC_QTD_REG; k++)
        EncRU(g, &call, (X64Reg)k);
}

/* ---- M7a F5: E44 (IMUL r,r,imm32; sem 0x204..0x207, que o isel nao usa: o fadec os le como AH..BH) e E45 ([base+indice*escala+desloc] no LEA) ---- */

#define ENC_QTD_IMED_IMUL 11
#define ENC_QTD_ESCALA 4
#define ENC_QTD_DESLOC_IDX 3

static void GerarE44(EncGerador *g)
{
    static const int64_t imeds[ENC_QTD_IMED_IMUL] = {0, 1, -1, 127, 128, -128, -129, INT32_MAX, INT32_MIN, 0x203, 0x208};
    static const EncForma formas[2] = {{"E44", FX_IMUL32_RRI, "imul", 32, 32}, {"E44", FX_IMUL64_RRI, "imul", 64, 64}};

    for (uint32_t f = 0; f < 2; f++) {
        for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
            for (uint32_t i = 0; i < ENC_QTD_IMED_IMUL; i++) {
                MInstr mi = MiNova(formas[f].forma, 3);
                X64Reg d = (X64Reg)k, s = (X64Reg)((k * 3 + i) % ENC_QTD_REG);
                char txt[ENC_TAM_LINHA];

                mi.op[0] = OpReg(d, formas[f].larg_d);
                mi.op[1] = OpReg(s, formas[f].larg_d);
                mi.op[2] = OpImed(imeds[i]);
                snprintf(txt, sizeof txt, "imul %s, %s, %lld", X64NomeRegistro(d, formas[f].larg_d), X64NomeRegistro(s, formas[f].larg_d), (long long)imeds[i]);
                EncCaso(g, "E44", &mi, txt);
            }
        }
    }
}

static void GerarE45(EncGerador *g)
{
    static const uint8_t escalas[ENC_QTD_ESCALA] = {1, 2, 4, 8};
    static const int32_t deslocs[ENC_QTD_DESLOC_IDX] = {0, 8, 4096};

    for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
        for (uint32_t x = 0; x < ENC_QTD_REG; x++) {
            for (uint32_t e = 0; e < ENC_QTD_ESCALA; e++) {
                for (uint32_t d = 0; d < ENC_QTD_DESLOC_IDX; d++) {
                    MInstr mi = MiNova(FX_LEA64_RM, 2);
                    X64Reg dst = (X64Reg)((b + x) % ENC_QTD_REG);
                    char txt[ENC_TAM_LINHA];

                    if (x == X64_RSP || (deslocs[d] == 0 && (b & 7) == 5))
                        continue; /* rsp nao e indice; [rbp|r13 + indice] sem deslocamento: o codificador recusa de proposito (fadec) */
                    mi.op[0] = OpReg(dst, 64);
                    mi.op[1] = OpMem((X64Reg)b, deslocs[d], 0);
                    mi.op[1].indice = (X64Reg)x;
                    mi.op[1].escala = escalas[e];
                    snprintf(txt, sizeof txt, "lea %s, [%s+%s*%u%+d]", X64NomeRegistro(dst, 64), X64NomeRegistro((X64Reg)b, 64), X64NomeRegistro((X64Reg)x, 64),
                             (unsigned)escalas[e], (int)deslocs[d]);
                    EncCaso(g, "E45", &mi, txt);
                }
            }
        }
    }
}

/* ---- M7b F1: E46 (SAR r,imm8 e IMUL r64 de um operando) ---- */

static void GerarE46(EncGerador *g)
{
    EncForma sar[2] = {{"E46", FX_SAR32_RI, "sar", 32, 0}, {"E46", FX_SAR64_RI, "sar", 64, 0}};
    EncForma imul1 = {"E46", FX_IMUL64_R, "imul", 64, 0};

    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t f = 0; f < 2; f++) {
            for (uint32_t i = 0; i < ENC_QTD_CONTADOR; i++)
                EncRI(g, &sar[f], (X64Reg)k, ENC_CONTADORES[i]);
        }
        EncRU(g, &imul1, (X64Reg)k);
    }
}

/* ---- S2: E48 (shufps xmm,xmm,imm8 e movups com [base+indice*escala+desloc], xmm0..15) ---- */

#define ENC_QTD_IMED_SHUF 6

static void EncShufps(EncGerador *g)
{
    static const uint8_t imeds[ENC_QTD_IMED_SHUF] = {0x00, 0x55, 0xAA, 0xFF, 0x1B, 0xE4};

    for (uint32_t k = 0; k < ENC_QTD_REG; k++) {
        for (uint32_t i = 0; i < ENC_QTD_IMED_SHUF; i++) {
            MInstr mi = MiNova(FX_SHUFPS_XXI, 3);
            uint32_t s = (k * 3 + i) % ENC_QTD_REG;
            char txt[ENC_TAM_LINHA];

            mi.op[0] = OpReg((X64Reg)k, 128);
            mi.op[0].tipo = MO_XMM;
            mi.op[1] = OpReg((X64Reg)s, 128);
            mi.op[1].tipo = MO_XMM;
            mi.op[2] = OpImed(imeds[i]);
            snprintf(txt, sizeof txt, "shufps xmm%u, xmm%u, %u", (unsigned)k, (unsigned)s, (unsigned)imeds[i]);
            EncCaso(g, "E48", &mi, txt);
        }
    }
}

static void EncMovupsIndexado(EncGerador *g, bool carga, uint32_t xmm, uint32_t b, uint32_t x, uint8_t escala, int32_t desloc)
{
    MInstr mi = MiNova(carga ? FX_MOVUPS_XM : FX_MOVUPS_MX, 2);
    MOperando reg = OpReg((X64Reg)xmm, 128), mem = OpMem((X64Reg)b, desloc, 128);
    char txt[ENC_TAM_LINHA];

    reg.tipo = MO_XMM;
    mem.indice = (X64Reg)x;
    mem.escala = escala;
    mi.op[carga ? 0 : 1] = reg;
    mi.op[carga ? 1 : 0] = mem;
    if (carga)
        snprintf(txt, sizeof txt, "movups xmm%u, oword [%s+%s*%u%+d]", (unsigned)xmm, X64NomeRegistro((X64Reg)b, 64), X64NomeRegistro((X64Reg)x, 64), (unsigned)escala, (int)desloc);
    else
        snprintf(txt, sizeof txt, "movups oword [%s+%s*%u%+d], xmm%u", X64NomeRegistro((X64Reg)b, 64), X64NomeRegistro((X64Reg)x, 64), (unsigned)escala, (int)desloc, (unsigned)xmm);
    EncCaso(g, "E48", &mi, txt);
}

static void GerarE48(EncGerador *g)
{
    static const uint8_t escalas[ENC_QTD_ESCALA] = {1, 2, 4, 8};
    static const int32_t deslocs[ENC_QTD_DESLOC_IDX] = {0, 8, 4096};

    EncShufps(g);
    for (uint32_t b = 0; b < ENC_QTD_REG; b++) {
        for (uint32_t x = 0; x < ENC_QTD_REG; x++) {
            for (uint32_t e = 0; e < ENC_QTD_ESCALA; e++) {
                for (uint32_t d = 0; d < ENC_QTD_DESLOC_IDX; d++) {
                    if (x == X64_RSP || (deslocs[d] == 0 && (b & 7) == 5))
                        continue; /* igual ao E45: rsp nao e indice; [rbp|r13 + indice] sem deslocamento o codificador recusa de proposito */
                    EncMovupsIndexado(g, true, (b + x) % ENC_QTD_REG, b, x, escalas[e], deslocs[d]);
                    EncMovupsIndexado(g, false, (b * 5 + x) % ENC_QTD_REG, b, x, escalas[e], deslocs[d]);
                }
            }
        }
    }
}

static void GerarTodosCasos(EncGerador *g)
{
    GerarE01E07(g);
    GerarE02E03E08(g);
    GerarE04E14Mem(g);
    GerarE05(g);
    GerarE09E10(g);
    GerarE11E14Reg(g);
    GerarE12E13(g);
    GerarE15(g);
    GerarE16E17(g);
    GerarE18(g);
    GerarE19E20E22(g);
    GerarE21E23(g);
    GerarE24E26(g);
    GerarE25E27(g);
    GerarE31aE40(g);
    GerarE41(g);
    GerarE42(g);
    GerarE44(g);
    GerarE45(g);
    GerarE46(g);
    GerarE48(g);
}

/* Modo "enc" do teste_nativo: grava os casos em <pasta> e devolve 0 quando os arquivos foram escritos. */
int ModoEnc(const char *pasta)
{
    EncGerador g;
    bool ok;
    if (!EncAbrir(&g, pasta)) {
        fprintf(stderr, "teste_nativo: nao consegui abrir os arquivos do enc em %s\n", pasta);
        return 1;
    }
    GerarTodosCasos(&g);
    ok = EncFechar(&g, pasta);
    printf("enc: %u casos, %u recusados em %s\n", g.linha - 2, g.recusados, pasta);
    BufLiberar(&g.bin);
    return ok ? 0 : 1;
}
