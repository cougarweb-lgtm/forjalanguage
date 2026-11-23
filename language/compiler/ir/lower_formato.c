/* lower_formato.c - lowering de f"..." (print, format e format_c) em IR_FORMATA.
 * Versao: 0.2.2
 * Changelog: 0.2.2 - Ryu P1: LwFormata marca IrPrograma.usa_real_curto quando alguma parte tem FMT_CURTO (o backend C inclui forja_rt_ryu.h so entao).
 *            0.2.1 - IA1-L5a: modo 3 (eprint: imprime em stderr) tambem sem destino.
 *            0.2.0 - Linguagem v1.11 L11: parte {expr:?} (FMT_DEPURAR) expande em varios pares por LwDepurar (lower_depurar.c); IR_FORMATA.nargs = pares gerados.
 *            0.1.0 -Linguagem v1.7 P8: LwFormata. Todas as {expr} saem ANTES da instrucao (D19): um format dentro de uma {expr}
 *                    termina antes de o de fora comecar.
 * Verificacao: construir.bat && python ..\tests\lowering\verificar_lowering.py
 */
#include "ir/lower_interno.h"

/* Parte de texto puro: o parser grava indice -1 (a semantica grava o formato empacotado, >= 0, nas {expr}). */
static bool ParteEhTexto(const Arg *parte)
{
    return parte->indice < 0;
}

/* Valor de cada parte, na ordem do texto: texto decodificado ou a expressao avaliada. */
static Valor *ValoresDasPartes(LowerCtx *c, const Expr *ftexto)
{
    Valor *valores = LwArgs(c, ftexto->nargs);
    for (uint32_t k = 0; k < ftexto->nargs; k++) {
        const Arg *parte = &ftexto->args[k];
        if (ParteEhTexto(parte)) valores[k] = LwValorTexto(LwTextoDecodificado(c, parte->valor), TIPO_STR);
        else valores[k] = LwExpr(c, parte->valor);
    }
    return valores;
}

/* {expr:?}: a semantica grava so FMT_DEPURAR em Arg.indice. */
static bool ParteEhDepurar(const Arg *parte)
{
    return parte->indice >= 0 && (parte->indice & FMT_DEPURAR) != 0;
}

/* args = pares (formato u32, valor); pedaco de texto leva formato 0; uma parte {x:?} vira varios pares (lower_depurar.c). */
static Valor *ParesDaInstrucao(LowerCtx *c, const Expr *ftexto, const Valor *valores, uint32_t *npares)
{
    Vec pares;
    VecIniciar(&pares, sizeof(Valor));
    for (uint32_t k = 0; k < ftexto->nargs; k++) {
        const Arg *parte = &ftexto->args[k];
        if (ParteEhDepurar(parte)) {
            LwDepurar(c, &pares, valores[k], ftexto->pos_op);
            continue;
        }
        *(Valor *)VecEmpurrar(&pares) = LwValorInt(ParteEhTexto(parte) ? 0 : parte->indice, TIPO_U32);
        *(Valor *)VecEmpurrar(&pares) = valores[k];
    }
    Valor *saida = LwArgs(c, (uint32_t)pares.n);
    if (pares.n != 0) memcpy(saida, pares.dados, pares.n * sizeof(Valor));
    *npares = (uint32_t)pares.n;
    VecLiberar(&pares);
    return saida;
}

static TipoId TipoDoModo(uint8_t modo)
{
    if (modo == 1) return TIPO_STR;
    return modo == 2 ? TIPO_CSTR : TIPO_VAZIO;
}

void LwFormata(LowerCtx *c, const Expr *ftexto, uint8_t modo, Valor alocador, IrInstr **saida)
{
    uint32_t npares;
    Valor *valores = ValoresDasPartes(c, ftexto);
    Valor *pares = ParesDaInstrucao(c, ftexto, valores, &npares);
    IrInstr *i = LwNovaInstr(c, IR_FORMATA, ftexto->pos_op);
    for (uint32_t k = 0; k < ftexto->nargs; k++)
        if (!ParteEhTexto(&ftexto->args[k]) && (ftexto->args[k].indice & FMT_CURTO) != 0) c->prog->usa_real_curto = true;
    i->op_aux = modo;
    i->tipo = TipoDoModo(modo);
    i->destino = modo == 1 || modo == 2 ? LwNovoTemp(c) : -1; /* 0 imprime e 3 imprime em stderr: sem destino */
    i->a = alocador;
    i->args = pares;
    i->nargs = npares;
    *saida = i;
}
