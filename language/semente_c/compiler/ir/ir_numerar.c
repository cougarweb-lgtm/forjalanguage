/* ir_numerar.c - numeracao fixa das instrucoes de uma funcao da IR (pre-ordem) e a regra de quem entra no traco do diferencial.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - Linguagem v1.12 M3 P21v-b: IR_CHAMADA_IND entra no limite de SEM_DESTINO e de IrTracoDestino (ir.h 0.16.0).
 *            0.1.0 -P18a: primeira versao (ver ir_numerar.h).
 * Verificacao: construir.bat && teste_nativo.exe
 */
#include <stdlib.h>

#include "ir/ir_numerar.h"

/* Passo de contagem (pares == NULL) e de preenchimento (pares != NULL): a mesma caminhada, para os dois nao divergirem. */
typedef struct { IrNumeroPar *pares; uint32_t n; } Colhedor;

static void ColherBloco(Colhedor *c, const IrBloco *b);

static void ColherInstr(Colhedor *c, const IrInstr *i)
{
    if (c->pares != NULL)
        c->pares[c->n] = (IrNumeroPar){.instr = i, .indice = c->n};
    c->n++;
    ColherBloco(c, &i->cond);
    ColherBloco(c, &i->corpo);
    ColherBloco(c, &i->senao);
    for (uint32_t k = 0; k < i->ncasos; k++)
        ColherBloco(c, &i->casos[k].corpo);
}

static void ColherBloco(Colhedor *c, const IrBloco *b)
{
    for (const IrInstr *i = b->primeira; i != NULL; i = i->prox)
        ColherInstr(c, i);
}

static int ComparaPorEndereco(const void *x, const void *y)
{
    const IrInstr *a = ((const IrNumeroPar *)x)->instr, *b = ((const IrNumeroPar *)y)->instr;

    return a < b ? -1 : (a > b ? 1 : 0);
}

void IrNumerarFuncao(const IrFuncao *f, Arena *arena, IrNumeracao *saida)
{
    Colhedor conta = {0}, preenche = {0};

    ColherBloco(&conta, &f->corpo);
    *saida = (IrNumeracao){.pares = ArenaAlocar(arena, (conta.n + 1) * sizeof *saida->pares), .n = conta.n};
    preenche.pares = saida->pares;
    ColherBloco(&preenche, &f->corpo);
    qsort(saida->pares, saida->n, sizeof *saida->pares, ComparaPorEndereco);
}

uint32_t IrIndiceDe(const IrNumeracao *num, const IrInstr *instr)
{
    uint32_t lo = 0, hi = num->n;

    while (lo < hi) {
        uint32_t meio = lo + (hi - lo) / 2;

        if (num->pares[meio].instr == instr)
            return num->pares[meio].indice;
        if (num->pares[meio].instr < instr)
            lo = meio + 1;
        else
            hi = meio;
    }
    return IR_SEM_INDICE;
}

IrTracoForma IrTracoFormaDe(const Tipos *tipos, TipoId tipo)
{
    switch (tipo) {
    case TIPO_BOOL: case TIPO_I8: case TIPO_U8: return (IrTracoForma){8, false};
    case TIPO_I16: case TIPO_U16: return (IrTracoForma){16, false};
    case TIPO_I32: case TIPO_U32: return (IrTracoForma){32, false};
    case TIPO_INT: case TIPO_UINT: return (IrTracoForma){64, false};
    case TIPO_F64: return (IrTracoForma){64, true};
    default: break;
    }
    if (TipoClasseDe(tipos, tipo) == TC_REAL)
        return (IrTracoForma){32, true}; /* f32 e unidade: float de 32 bits */
    return (IrTracoForma){0, false};
}

/* Ops que nao gravam local nem temporario (controle, saida, checagem, escrita em memoria): indexada pelo IrOp. */
static const bool SEM_DESTINO[IR_CHAMADA_IND + 1] = {
    [IR_VERIFICA_FAIXA] = true, [IR_IMPRIME] = true, [IR_SE] = true, [IR_ENQUANTO] = true, [IR_PARA] = true,
    [IR_CHAVEIA] = true, [IR_RETORNA] = true, [IR_PARE] = true, [IR_CONTINUA] = true, [IR_EXPECT] = true,
    [IR_ASSERT] = true, [IR_PANIC] = true, [IR_GRAVA_PONTEIRO] = true, [IR_ANEXA] = true, [IR_LIBERA] = true,
    [IR_FRAME_RESET] = true, [IR_SOA_GRAVA] = true, [IR_SOA_ANEXA] = true, [IR_MAPA_INSERE] = true, [IR_MAPA_REMOVE] = true,
};

bool IrTracoDestino(const IrInstr *i, bool *e_local)
{
    *e_local = i->op == IR_LOCAL || i->op == IR_ATRIBUICAO;
    if (i->op == IR_LOCAL)
        return true;
    if (i->op == IR_ATRIBUICAO)
        return i->nome < 0; /* nome >= 0 grava um campo: agregado, fora do traco */
    return i->op <= IR_CHAMADA_IND && !SEM_DESTINO[i->op] && i->destino >= 0;
}
