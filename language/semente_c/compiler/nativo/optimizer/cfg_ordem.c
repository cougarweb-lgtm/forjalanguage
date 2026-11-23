/* cfg_ordem.c - utilitarios dos passes `cfg` e `layout`: elegibilidade e aplicacao de uma ordem de blocos (ver cfg.h).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M7b R4b: primeira versao.
 * Verificacao: construir_rapido.py && python tests\nativo\otim\verificar_alocador.py --forja <forja.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "nativo/optimizer/cfg.h"

bool CfgElegivel(const BirPrograma *programa, const BirFuncao *funcao)
{
    if (programa->simbolos_dbg || funcao->nblocos < 2)
        return false;
    for (uint32_t b = 0; b < funcao->nblocos; b++) {
        if (funcao->blocos[b].n == 0)
            return false;
    }
    return true;
}

static bool EhContaOv(uint8_t op)
{
    return op == BIR_ADD_OV || op == BIR_SUB_OV || op == BIR_MUL_OV || op == BIR_NEG_OV;
}

/* Troca os alvos da instrucao pelos ids novos (nada aponta para bloco removido: o chamador so tira bloco que ninguem alcanca). */
static void ReescreverAlvos(BirInstr *i, const uint32_t *novo_id)
{
    if (i->op == BIR_SALTA || i->op == BIR_DESVIA || EhContaOv(i->op))
        i->alvo[0] = novo_id[i->alvo[0]];
    if (i->op == BIR_DESVIA)
        i->alvo[1] = novo_id[i->alvo[1]];
}

void CfgAplicarOrdem(BirFuncao *funcao, const uint32_t *ordem, uint32_t n)
{
    uint32_t velho = funcao->nblocos;
    uint32_t *novo_id = malloc((size_t)velho * sizeof *novo_id);
    BirBloco *novos = malloc((size_t)n * sizeof *novos);

    if (novo_id == NULL || novos == NULL)
        FalhaInterna("otim: memoria esgotada");
    for (uint32_t b = 0; b < velho; b++)
        novo_id[b] = BIR_SEM_BLOCO;
    for (uint32_t k = 0; k < n; k++) {
        novo_id[ordem[k]] = k;
        novos[k] = funcao->blocos[ordem[k]];
    }
    for (uint32_t b = 0; b < velho; b++) {
        if (novo_id[b] == BIR_SEM_BLOCO)
            free(funcao->blocos[b].instrs);
    }
    memcpy(funcao->blocos, novos, (size_t)n * sizeof *novos);
    funcao->nblocos = n;
    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t j = 0; j < funcao->blocos[k].n; j++)
            ReescreverAlvos(&funcao->blocos[k].instrs[j], novo_id);
    }
    free(novos);
    free(novo_id);
}
