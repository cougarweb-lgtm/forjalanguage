/* bir_locais.c - casa dos locais nomeados do usuario na BIR (D-M6-13) e o dump dela (item "locais" do --debug-code).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M6 P65a: BirRetratarLocais (retrato tirado no fim de cada funcao, antes de vreg_local/slot_local serem reaproveitados) e BirImprimirLocais.
 * Verificacao: construir.bat && python ..\tests\nativo\m6\verificar_nomes.py --forja .\forja.exe
 *
 * Os locais do usuario sao os ids 0..IrFuncao.nnomes_locais-1 (parametros primeiro); id acima disso e temporario do lowering e nao entra.
 * Formato do dump:
 *   funcao fj_main
 *     local 0 x tipo=i32 tid=7 casa=vreg3 indireto=0
 *     local 1 p tipo=i64 tid=12 casa=slot0 indireto=0
 *     sem_casa=0
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

/* Casa do local: slot vence vreg; param agregado (param_tam > 0) guarda so um ponteiro no home. false = nunca definido. */
static bool CasaDoLocal(const BirCtx *c, const IrFuncao *irf, uint32_t id, BirLocalCasa *casa)
{
    int32_t slot = id < c->n_local ? c->slot_local[id] : -1;
    int32_t vreg = id < c->n_local ? c->vreg_local[id] : -1;

    if (slot < 0 && vreg < 0)
        return false;
    casa->vreg = vreg;
    casa->slot = slot;
    casa->indireto = slot < 0 && id < irf->nparams && c->f->param_tam != NULL && c->f->param_tam[id] != 0;
    return true;
}

void BirRetratarLocais(BirCtx *c, const IrFuncao *irf)
{
    BirFuncao *f = c->f;
    Arena *arena = &c->p->arena;

    if (irf->nnomes_locais == 0)
        return;
    f->casas = ArenaAlocar(arena, irf->nnomes_locais * sizeof *f->casas);
    for (uint32_t id = 0; id < irf->nnomes_locais; id++) {
        const LocalNome *n = &irf->nomes_locais[id];
        BirLocalCasa casa = {.local_id = id, .tipo = n->tipo, .tipo_nome = BirNomeDoTipo(c->e->tipos, n->tipo)};

        if (!CasaDoLocal(c, irf, id, &casa)) {
            f->nsem_casa++;
            continue;
        }
        casa.nome = ArenaCopiarTexto(arena, InternerTexto(c->e->nomes, n->nome), InternerTam(c->e->nomes, n->nome));
        f->casas[f->ncasas++] = casa;
    }
}

static void ImprimirCasa(const BirLocalCasa *k, Buf *saida)
{
    BufFormatar(saida, "  local %u %s tipo=%s tid=%u casa=", (unsigned)k->local_id, k->nome, k->tipo_nome, (unsigned)k->tipo);
    if (k->slot >= 0)
        BufFormatar(saida, "slot%d", (int)k->slot);
    else
        BufFormatar(saida, "vreg%d", (int)k->vreg);
    BufFormatar(saida, " indireto=%d\n", k->indireto ? 1 : 0);
}

void BirImprimirLocais(const BirPrograma *programa, Buf *saida)
{
    for (uint32_t k = 0; k < programa->nfuncoes; k++) {
        const BirFuncao *f = &programa->funcoes[k];

        BufFormatar(saida, "funcao %s\n", programa->simbolos[f->simbolo].nome);
        for (uint32_t j = 0; j < f->ncasas; j++)
            ImprimirCasa(&f->casas[j], saida);
        BufFormatar(saida, "  sem_casa=%u\n", (unsigned)f->nsem_casa);
    }
}
