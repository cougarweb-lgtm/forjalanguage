/* numeracao_morto.c - limpeza do passe `numeracao`: tira a instrucao pura (copia, constante, conta, extensao, conversao) cujo dst nunca e lido em lugar nenhum da funcao; refaz em cascata.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - N1 do M7b: CARREGA sem uso sai (CargaRemovivel): base END_SLOT de definicao unica ou outro acesso vivo a mesma base no bloco.
 *            0.3.1 - R5 (SIMD): SELECIONA e pura e removivel.
 *            0.3.0 - M7b R3 rodada 2: usa a memoria de trabalho do contexto (NumTemp) em vez de calloc por funcao.
 *            0.2.0 - M7b R3 rodada 2: MarcarPassada percorre de tras para frente (a cascata fecha em 1-2 passadas).
 *            0.1.0 - M7b R3: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 *
 * Nunca tira: gravacao, chamada, trap, terminador, CMP (a guarda de DIV/REM/SHL/SHR da BIR e o par `cmp vN, #imed` + `desvia`), *_OV, END_SLOT e END_SIMBOLO (o simbolo entra na
 * ordem do 1o uso). Carga (BIR_CARREGA) so sai pela prova de CargaRemovivel. Instrucoes de uma funcao sao removidas por compactacao do vetor do bloco, na mesma ordem.
 */
#include <stdlib.h>

#include "common/base.h"
#include "nativo/optimizer/numeracao_interno.h"

#define PASSADAS_MAX 4u

static bool EhPuraRemovivel(uint8_t op)
{
    switch (op) {
    case BIR_CONST: case BIR_COPIA: case BIR_ADD: case BIR_SUB: case BIR_MUL: case BIR_AND: case BIR_OR: case BIR_XOR: case BIR_NEG: case BIR_NOT:
    case BIR_EXT_S: case BIR_EXT_Z: case BIR_TRUNC: case BIR_DIV: case BIR_REM: case BIR_SHL: case BIR_SHR: case BIR_I_PARA_F: case BIR_F_PARA_F: case BIR_RAIZ: case BIR_SELECIONA:
        return true;
    default:
        return false;
    }
}

/* Acesso de memoria com base vreg que conta como "outro acesso a mesma base" (carga ou gravacao). */
static bool EhAcessoDeBase(const BirInstr *i)
{
    return (i->op == BIR_CARREGA || i->op == BIR_GRAVA) && i->a.tipo == BO_VREG;
}

/* Ha, no bloco, outro acesso (carga ou gravacao) com a mesma base da instrucao j que continua vivo (nao tirado nesta passada ou antes)? */
static bool OutroAcessoVivo(const BirBloco *bloco, const uint8_t *tirada, uint32_t j)
{
    uint32_t base = bloco->instrs[j].a.id;

    for (uint32_t k = 0; k < bloco->n; k++) {
        const BirInstr *outro = &bloco->instrs[k];

        if (k != j && !tirada[k] && EhAcessoDeBase(outro) && outro->a.id == base)
            return true;
    }
    return false;
}

/* Carga sem uso sai so por uma prova: a base e END_SLOT de definicao unica (endereco do quadro da funcao: nao falha, NumBaseDe) OU ha outro acesso vivo a mesma base no
 * bloco. A ultima carga/gravacao de um grupo nunca sai, pois o acesso que a justificou ainda esta vivo. Base de ponteiro de runtime, parametro ou simbolo fica. */
static bool CargaRemovivel(const NumCtx *c, const BirBloco *bloco, const uint8_t *tirada, uint32_t j)
{
    const BirInstr *i = &bloco->instrs[j];

    if (i->a.tipo != BO_VREG)
        return false;
    return NumBaseDe(c, i->a.id).slot == 1 || OutroAcessoVivo(bloco, tirada, j);
}

/* Sem uso no dst: tira a pura, e a carga so pela prova de CargaRemovivel. */
static bool RemovivelSemUso(const NumCtx *c, const BirBloco *bloco, const uint8_t *tirada, uint32_t j)
{
    uint8_t op = bloco->instrs[j].op;

    if (op == BIR_CARREGA)
        return CargaRemovivel(c, bloco, tirada, j);
    return EhPuraRemovivel(op);
}

static void Usar(uint32_t *usos, uint32_t nvregs, const BirOperando *o, int passo)
{
    if (o->tipo == BO_VREG && o->id < nvregs)
        usos[o->id] += (uint32_t)passo;
}

static void ContarUsos(const BirFuncao *f, uint32_t *usos, const uint8_t *tirada, const uint32_t *inicio)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        for (uint32_t j = 0; j < f->blocos[b].n; j++) {
            const BirInstr *i = &f->blocos[b].instrs[j];

            if (tirada[inicio[b] + j])
                continue;
            Usar(usos, f->nvregs, &i->a, 1);
            Usar(usos, f->nvregs, &i->b, 1);
            for (uint32_t k = 0; k < i->nargs; k++)
                Usar(usos, f->nvregs, &i->args[k], 1);
        }
    }
}

/* Uma passada: marca em tirada as instrucoes sem uso e desconta os operandos delas. Devolve quantas marcou. */
static uint32_t MarcarPassada(const NumCtx *c, uint32_t *usos, uint8_t *tirada, const uint32_t *inicio)
{
    const BirFuncao *f = c->f;
    uint32_t marcadas = 0;

    for (uint32_t b = f->nblocos; b-- > 0;) { /* de tras para frente: o consumidor sai antes do produtor e a cascata fecha numa passada na maioria dos casos */
        for (uint32_t j = f->blocos[b].n; j-- > 0;) {
            const BirInstr *i = &f->blocos[b].instrs[j];

            if (tirada[inicio[b] + j] || i->dst == BIR_SEM_VREG || i->dst >= f->nvregs || usos[i->dst] != 0 || !RemovivelSemUso(c, &f->blocos[b], &tirada[inicio[b]], j))
                continue;
            tirada[inicio[b] + j] = 1;
            marcadas++;
            Usar(usos, f->nvregs, &i->a, -1);
            Usar(usos, f->nvregs, &i->b, -1);
        }
    }
    return marcadas;
}

static void Compactar(BirFuncao *f, const uint8_t *tirada, const uint32_t *inicio)
{
    for (uint32_t b = 0; b < f->nblocos; b++) {
        BirBloco *bloco = &f->blocos[b];
        uint32_t m = 0;

        for (uint32_t j = 0; j < bloco->n; j++) {
            if (!tirada[inicio[b] + j])
                bloco->instrs[m++] = bloco->instrs[j];
        }
        bloco->n = m;
    }
}

uint32_t NumRemoverMortos(BirFuncao *f, NumCtx *c)
{
    uint32_t total = 0, tirados = 0, nova;
    uint32_t *inicio = NumTemp(c, (size_t)f->nblocos + 1u, 0);
    uint32_t *usos = NumTemp(c, (size_t)f->nvregs + 1u, 0);
    uint8_t *tirada;

    for (uint32_t b = 0; b < f->nblocos; b++) {
        inicio[b] = total;
        total += f->blocos[b].n;
    }
    tirada = (uint8_t *)NumTemp(c, ((size_t)total + 1u + 3u) / 4u, 0);
    ContarUsos(f, usos, tirada, inicio);
    for (uint32_t passada = 0; passada < PASSADAS_MAX; passada++) {
        nova = MarcarPassada(c, usos, tirada, inicio);
        tirados += nova;
        if (nova == 0)
            break;
    }
    if (tirados > 0)
        Compactar(f, tirada, inicio);
    return tirados;
}
