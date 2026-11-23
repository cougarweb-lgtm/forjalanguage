/* bir_validar_mem.c - forma das operacoes de memoria da BIR (END_SLOT, CARREGA, GRAVA, COPIA_MEM, ZERA_MEM) e dos slots da funcao. Falha = X0904 (quem chama relata).
 * Versao: 0.3.0
 * Changelog: 0.3.0 - S1b (D0703): ClasseDeDado aceita BC_V128 (CARREGA e GRAVA de vetor; o validador exige vreg e forma em bir_validar.c).
 *            0.2.1 - M3 F9b-2: decisoes extraidas de BirAgregadosValidos, GravaValido e BirMemoriaValida.
 *            0.2.0 - M2 P11b: BirAgregadosValidos (param_tam e retorno_tam na classe BC_I64).
 *            0.1.0 - M2 P11a: slot com tamanho > 0 e alinhamento potencia de 2 ate 16; base sempre vreg BC_I64; CARREGA/GRAVA com classe valida (BC_NADA nao carrega);
 *                    COPIA_MEM e ZERA_MEM com 1..64 bytes (acima disso o construtor chama memcpy/memset).
 * Verificacao: construir.bat && teste_nativo.exe (casos de unidade da BIR de memoria) e python -I ..\tests\nativo\bir\verificar_bir.py
 */
#include "nativo/bir/bir_interno.h"

static bool BaseValida(const BirFuncao *f, const BirOperando *base)
{
    return base->tipo == BO_VREG && base->classe == BC_I64 && base->id < f->nvregs && f->classe_vreg[base->id] == BC_I64;
}

static bool PotenciaDeDois(uint32_t n)
{
    return n != 0 && (n & (n - 1u)) == 0;
}

bool BirSlotsValidos(const BirFuncao *f)
{
    for (uint32_t k = 0; k < f->nslots; k++) {
        const BirSlot *s = &f->slots[k];

        if (s->tam == 0 || !PotenciaDeDois(s->alinhamento) || s->alinhamento > BIR_SLOT_ALINHAMENTO_MAX)
            return false;
    }
    return true;
}

/* Parametro agregado (param_tam[k] > 0) e retorno agregado (retorno_tam > 0) viajam como endereco: classe BC_I64. */
static bool ParamAgregadoValido(const BirFuncao *f, uint32_t k)
{
    return f->param_tam[k] == 0 || f->classe_vreg[k] == BC_I64;
}

bool BirAgregadosValidos(const BirFuncao *f)
{
    if (f->retorno_tam != 0 && f->retorno != BC_I64)
        return false;
    if (f->param_tam == NULL)
        return true;
    for (uint32_t k = 0; k < f->nparams; k++) {
        if (!ParamAgregadoValido(f, k))
            return false;
    }
    return true;
}

static bool EndSlotValido(const BirFuncao *f, const BirInstr *i)
{
    bool dst_ok = i->dst < f->nvregs && f->classe_vreg[i->dst] == BC_I64 && i->classe == BC_I64;

    return dst_ok && i->a.tipo == BO_IMED && i->a.imed >= 0 && (uint64_t)i->a.imed < f->nslots;
}

static bool ClasseDeDado(uint8_t classe)
{
    return classe >= BC_I8 && classe <= BC_V128;
}

static bool CarregaValido(const BirFuncao *f, const BirInstr *i)
{
    bool dst_ok = i->dst < f->nvregs && f->classe_vreg[i->dst] == i->classe;

    return dst_ok && ClasseDeDado(i->classe) && BaseValida(f, &i->a) && i->desloc >= 0;
}

static bool ValorGravadoValido(const BirInstr *i)
{
    return (i->b.tipo == BO_VREG || i->b.tipo == BO_IMED) && ClasseDeDado(i->b.classe) && i->classe == i->b.classe;
}

static bool GravaValido(const BirFuncao *f, const BirInstr *i)
{
    return i->dst == BIR_SEM_VREG && ValorGravadoValido(i) && BaseValida(f, &i->a) && i->desloc >= 0;
}

static bool BytesValidos(int32_t n)
{
    return n >= 1 && (uint32_t)n <= BIR_COPIA_MEM_MAX;
}

static bool CopiaMemValida(const BirFuncao *f, const BirInstr *i)
{
    return i->dst == BIR_SEM_VREG && BaseValida(f, &i->a) && BaseValida(f, &i->b) && BytesValidos(i->desloc);
}

static bool ZeraMemValida(const BirFuncao *f, const BirInstr *i)
{
    return i->dst == BIR_SEM_VREG && BaseValida(f, &i->a) && i->b.tipo == BO_NADA && BytesValidos(i->desloc);
}

bool BirMemoriaValida(const BirFuncao *f, const BirInstr *i)
{
    switch (i->op) {
    case BIR_END_SLOT: return EndSlotValido(f, i);
    case BIR_CARREGA: return CarregaValido(f, i);
    case BIR_GRAVA: return GravaValido(f, i);
    case BIR_COPIA_MEM: return CopiaMemValida(f, i);
    case BIR_ZERA_MEM: return ZeraMemValida(f, i);
    default: return false;
    }
}
