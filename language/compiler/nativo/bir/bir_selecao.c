/* bir_selecao.c - BirSelecionaMinMax: quando um SELECIONA real e exatamente o minss/maxss do x64 (usada pelo validador, pelo passe `selecao` e pelo isel).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - R5 (SIMD): primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * minss p, s devolve p se p < s e s em qualquer outro caso (NaN em qualquer lado, +0 contra -0, iguais); maxss p, s devolve p se p > s e s nos demais. Por isso so duas formas de
 * SELECIONA real batem bit a bit: (x < y) ? x : y (= minss x, y) e (x > y) ? x : y (= maxss x, y), e as duas com os valores trocados, que sao o outro op com os operandos trocados.
 * FLE/FGE/FEQ/FNE nao entram: com x == y (+0 e -0) escolheriam o outro operando.
 */
#include "nativo/bir/bir.h"

static bool MesmoOperando(const BirOperando *p, const BirOperando *q)
{
    if (p->tipo != q->tipo || p->classe != q->classe)
        return false;
    return p->tipo == BO_VREG ? p->id == q->id : (p->tipo == BO_IMED && p->imed == q->imed);
}

bool BirSelecionaMinMax(const BirInstr *i, bool *maximo, const BirOperando **primeiro, const BirOperando **segundo)
{
    const BirOperando *x, *y;
    bool direta;

    if (i->op != BIR_SELECIONA || !BirClasseEhReal(i->classe) || i->nargs != 2 || (i->aux != BCOND_FLT && i->aux != BCOND_FGT))
        return false;
    x = &i->args[0];
    y = &i->args[1];
    if (MesmoOperando(&i->a, x) && MesmoOperando(&i->b, y))
        direta = true;
    else if (MesmoOperando(&i->a, y) && MesmoOperando(&i->b, x))
        direta = false;
    else
        return false;
    /* x<y?x:y = minss x,y ; x<y?y:x = y>x?y:x = maxss y,x ; x>y?x:y = maxss x,y ; x>y?y:x = y<x?y:x = minss y,x */
    *maximo = (i->aux == BCOND_FLT) != direta;
    *primeiro = direta ? x : y;
    *segundo = direta ? y : x;
    return true;
}
