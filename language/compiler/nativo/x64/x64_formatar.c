/* x64_formatar.c - texto Intel de uma MInstr (molde da FORMAS_X64) e nomes de registros e condicoes.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M7a F5: memoria com indice ("[rax+rcx*4+8]").
 *            0.3.0 - M2 P10b: operando xmm ("xmm3") e acesso de 128 bits ("xmmword ").
 *            0.2.0 - M2 P9: registradores de 16 bits (ax..r15w) e "word " no acesso a memoria.
 *            0.1.0 - backend x64 P4b: X64Formatar (molde com %0..%2 e %k), X64NomeRegistro (64, 32 e 8 bits) e tamanho do acesso
 *                    na memoria ("qword [rbp-8]"); largura 0 na tabela = sem palavra de tamanho (LEA).
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <stdint.h>

#include "nativo/x64/x64.h"

static const char *const NOMES_REG_64[16] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char *const NOMES_REG_32[16] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
};

static const char *const NOMES_REG_16[16] = {
    "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
};

/* Com REX, os codigos 4..7 de 8 bits sao spl/bpl/sil/dil (e nao ah..bh); o codificador ja trata isso. */
static const char *const NOMES_REG_8[16] = {
    "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
};

/* Sufixos Intel de Jcc e SETcc, na ordem de X64Cond. */
static const char *const NOMES_COND[16] = {
    "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g"
};

const char *X64NomeRegistro(X64Reg reg, uint8_t largura)
{
    if (reg == X64_RIP)
        return "rip";
    if (reg > X64_R15)
        return "?";
    switch (largura) {
    case 64: return NOMES_REG_64[reg];
    case 32: return NOMES_REG_32[reg];
    case 16: return NOMES_REG_16[reg];
    case 8: return NOMES_REG_8[reg];
    default: return "?";
    }
}

static const char *TamanhoTexto(uint8_t largura)
{
    switch (largura) {
    case 8: return "byte ";
    case 16: return "word ";
    case 32: return "dword ";
    case 64: return "qword ";
    case 128: return "xmmword ";
    default: return "";
    }
}

static void AnexarMemoria(Buf *saida, const MOperando *o, uint8_t largura)
{
    BufAnexar(saida, TamanhoTexto(largura));
    BufAnexar(saida, "[");
    BufAnexar(saida, X64NomeRegistro(o->base, 64));
    if (o->escala != 0)
        BufFormatar(saida, "+%s*%u", X64NomeRegistro(o->indice, 64), (unsigned)o->escala);
    if (o->desloc != 0)
        BufFormatar(saida, "%+d", (int)o->desloc);
    BufAnexar(saida, "]");
}

static void AnexarOperando(Buf *saida, const MInstr *mi, uint8_t indice)
{
    const MOperando *o = &mi->op[indice];
    switch (o->tipo) {
    case MO_REG:
        BufAnexar(saida, X64NomeRegistro(o->base, o->largura));
        break;
    case MO_XMM:
        BufFormatar(saida, "xmm%u", (unsigned)o->base);
        break;
    case MO_MEM:
        AnexarMemoria(saida, o, FORMAS_X64[mi->forma].largura[indice]);
        break;
    case MO_IMED:
        BufAnexarI64(saida, o->imed);
        break;
    case MO_BLOCO:
        BufFormatar(saida, "b%llu", (unsigned long long)o->imed);
        break;
    case MO_SIMBOLO:
        BufFormatar(saida, "s%u", (unsigned int)o->simbolo);
        break;
    default:
        BufAnexar(saida, "?");
        break;
    }
}

void X64Formatar(const MInstr *mi, Buf *saida)
{
    if (mi->forma >= FX_QTD) {
        BufAnexar(saida, "?");
        return;
    }
    const char *molde = FORMAS_X64[mi->forma].molde;
    for (const char *p = molde; *p != '\0'; p++) {
        if (*p != '%' || p[1] == '\0') {
            BufAnexarN(saida, p, 1);
            continue;
        }
        p++;
        if (*p == 'k')
            BufAnexar(saida, NOMES_COND[mi->cond & 0xF]);
        else
            AnexarOperando(saida, mi, (uint8_t)(*p - '0'));
    }
}
