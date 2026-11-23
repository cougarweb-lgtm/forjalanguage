/* operandos_enc.h - construtores de MInstr e operandos usados pelos testes do codificador x64 (unidade e gerador de casos).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - backend x64 P4b: MiNova, OpReg, OpMem, OpImed e OpRel, tirados de teste_nativo.c para o gerador caber em 800 linhas.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_TESTE_NATIVO_ENC_OPERANDOS_ENC_H
#define FORJA_TESTE_NATIVO_ENC_OPERANDOS_ENC_H

#include <stdint.h>
#include <string.h>

#include "nativo/x64/x64.h"

/* Instrucao zerada com a forma e o numero de operandos dados. */
static inline MInstr MiNova(uint16_t forma, uint8_t nops)
{
    MInstr mi;
    memset(&mi, 0, sizeof mi);
    mi.forma = forma;
    mi.nops = nops;
    return mi;
}

static inline MOperando OpReg(X64Reg reg, uint8_t largura)
{
    MOperando o;
    memset(&o, 0, sizeof o);
    o.tipo = MO_REG;
    o.base = reg;
    o.largura = largura;
    return o;
}

static inline MOperando OpMem(X64Reg base, int32_t desloc, uint8_t largura)
{
    MOperando o = OpReg(base, largura);
    o.tipo = MO_MEM;
    o.desloc = desloc;
    return o;
}

static inline MOperando OpImed(int64_t valor)
{
    MOperando o = OpReg(X64_RAX, 0);
    o.tipo = MO_IMED;
    o.imed = valor;
    return o;
}

static inline MOperando OpRel(MOperandoTipo tipo, int32_t rel)
{
    MOperando o = OpReg(X64_RAX, 0);
    o.tipo = tipo;
    o.desloc = rel;
    return o;
}

#endif
