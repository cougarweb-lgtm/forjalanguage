/* x64_isel_conta.c - selecao das contas que o M2 acrescenta ao degrau 0: DIV, REM, SHL, SHR, NOT e os *_OV de I8 e I16 (as demais contas ficam em x64_isel.c).
 * Versao: 0.1.4
 * Changelog: 0.1.4 - S2 (D0703): DIV de V128 vai para IselContaVetorial.
 *            0.1.3 - S1a (D0703): comparacao de classe real trocada por BirClasseEhReal (mesmo codigo).
 *            0.1.2 - M3 F9b-1: decisoes extraidas de IselDivisao.
 *            0.1.1 - M2 P10b: DIV de real (BIR_DIV com classe F*) vai para IselContaReal.
 *            0.1.0 - M2 P9: DIV/REM sem sinal = xor edx,edx + div; com sinal = cdq (cqo em 64 bits) + idiv, com I8 e I16 alargados por movsx (a conta e de 32 bits e o
 *                    resultado sai truncado no slot). O divisor nunca e 0 nem -1 aqui (guarda dos blocos da BIR, conferida pelo BirValidar), entao nao ha #DE. SHL/SHR
 *                    por cl (contador do mesmo tipo do valor, ja menor que a largura) ou por imm8; U8 e U16 operam em 32 bits e o slot trunca. NOT em 32 ou 64.
 *                    *_OV de I8 e I16: operandos alargados por movsx (com sinal) ou movzx, conta em 32 bits, depois cmp com o MIN e o MAX do tipo e salto ao frio
 *                    (sem sinal: so "cmp eax, MAX; ja", a subtracao abaixo de zero vira um numero enorme).
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 */
#include "nativo/x64/x64_interno.h"

/* Limites do tipo curto, indexados por [classe == I16][com sinal]. */
static const int32_t MINIMOS_CURTOS[2][2] = {{0, INT8_MIN}, {0, INT16_MIN}};
static const int32_t MAXIMOS_CURTOS[2][2] = {{UINT8_MAX, INT8_MAX}, {UINT16_MAX, INT16_MAX}};

/* cmp eax, valor / jcc frio. */
static bool TestarFaixa(X64Ctx *c, int32_t valor, X64Cond cond, uint32_t frio)
{
    if (!X64Emitir(c, MI2(FX_CMP32_RI, MReg(X64_RAX, 32), MImed(valor))))
        return false;
    return X64Emitir(c, MICc(FX_JCC_REL32, cond, MBloco(frio)));
}

bool IselOvCurto(X64Ctx *c, const BirInstr *i, uint8_t base)
{
    bool com_sinal = i->aux == BIR_SINAL;
    uint8_t largo = i->classe == BC_I16;

    if (!IselOperacao(c, i, base, IselExtensaoDaConta(i->classe, com_sinal)))
        return false;
    if (com_sinal && !TestarFaixa(c, MINIMOS_CURTOS[largo][1], X64_CC_L, i->alvo[0]))
        return false;
    if (!TestarFaixa(c, MAXIMOS_CURTOS[largo][com_sinal], com_sinal ? X64_CC_G : X64_CC_A, i->alvo[0]))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

/* ---- divisao e resto ---- */

/* [sem sinal][largura]: idiv e div de 32 e 64 bits. */
static const uint16_t FORMAS_DIVISAO[2][2] = {{FX_IDIV32_R, FX_IDIV64_R}, {FX_DIV32_R, FX_DIV64_R}};

/* O dividendo de 2 palavras: edx:eax por cdq/cqo (com sinal) ou edx zerado (sem sinal). */
static bool PrepararDividendo(X64Ctx *c, bool sem_sinal, uint8_t larg)
{
    if (sem_sinal)
        return X64Emitir(c, MI2(FX_XOR32_RR, MReg(X64_RDX, 32), MReg(X64_RDX, 32)));
    return X64Emitir(c, MI0(larg ? FX_CQO : FX_CDQ));
}

/* rax = dividendo, rcx = divisor, alargados por ext. */
static bool CarregarDividendoEDivisor(X64Ctx *c, const BirInstr *i, uint8_t ext)
{
    return IselCarregar(c, X64_RAX, &i->a, ext) && IselCarregar(c, X64_RCX, &i->b, ext);
}

bool IselDivisao(X64Ctx *c, const BirInstr *i)
{
    bool sem_sinal = i->aux == BIR_SEM_SINAL;
    uint8_t larg = IselIndiceLargura(i->classe);
    uint8_t ext = IselExtensaoDaConta(i->classe, !sem_sinal);

    if (i->classe == BC_V128) /* S2: a / b de vetor: divps/divpd */
        return IselContaVetorial(c, i);
    if (BirClasseEhReal(i->classe)) /* a / b de real: divss/divsd */
        return IselContaReal(c, i);
    if (!CarregarDividendoEDivisor(c, i, ext))
        return false;
    if (!PrepararDividendo(c, sem_sinal, larg))
        return false;
    if (!X64Emitir(c, MI1(FORMAS_DIVISAO[sem_sinal][larg], MReg(X64_RCX, (uint8_t)(32u << larg)))))
        return false;
    return IselGuardar(c, i->dst, i->op == BIR_REM ? X64_RDX : X64_RAX);
}

/* ---- deslocamento e not ---- */

/* [shr][largura]: por cl e por imm8. */
static const uint16_t FORMAS_DESLOCA_CL[2][2] = {{FX_SHL32_RCL, FX_SHL64_RCL}, {FX_SHR32_RCL, FX_SHR64_RCL}};
static const uint16_t FORMAS_DESLOCA_IMM[2][2] = {{FX_SHL32_RI, FX_SHL64_RI}, {FX_SHR32_RI, FX_SHR64_RI}};

static bool DeslocarRax(X64Ctx *c, const BirInstr *i)
{
    uint8_t sentido = i->op == BIR_SHR;
    uint8_t larg = IselIndiceLargura(i->classe);
    MOperando rax = MReg(X64_RAX, (uint8_t)(32u << larg));

    if (i->b.tipo == BO_IMED)
        return X64Emitir(c, MI2(FORMAS_DESLOCA_IMM[sentido][larg], rax, MImed(i->b.imed)));
    if (!IselCarregar(c, X64_RCX, &i->b, BE_NENHUMA))
        return false;
    return X64Emitir(c, MI2(FORMAS_DESLOCA_CL[sentido][larg], rax, MReg(X64_RCX, 8)));
}

bool IselDeslocamento(X64Ctx *c, const BirInstr *i)
{
    if (!IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA) || !DeslocarRax(c, i))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}

bool IselNot(X64Ctx *c, const BirInstr *i)
{
    uint8_t larg = IselIndiceLargura(i->classe);

    if (!IselCarregar(c, X64_RAX, &i->a, BE_NENHUMA))
        return false;
    if (!X64Emitir(c, MI1(larg ? FX_NOT64_R : FX_NOT32_R, MReg(X64_RAX, (uint8_t)(32u << larg)))))
        return false;
    return IselGuardar(c, i->dst, X64_RAX);
}
