/* analises.c - orquestra as analises da BIR (analises_cfg.c, analises_lacos.c, analises_vivo.c) e confere as invariantes. Ver analises.h.
 * Versao: 0.2.1
 * Changelog: 0.2.1 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.2.0 - M7b F2: AnalisesCalcularLacosApenas (arestas, RPO, idom e lacos, sem vivacidade); ConferirVivo usa AnalisesVivoConfere (conjuntos esparsos); sem o campo palavras.
 *            0.1.0 - M7a F1: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#include <stdio.h>
#include <string.h>

#include "nativo/bir/analises_interno.h"

uint32_t *AnalisesVetor32(AnalisesBir *a, size_t n)
{
    return ArenaAlocar(&a->arena, (n + 1) * sizeof(uint32_t));
}

uint64_t *AnalisesVetor64(AnalisesBir *a, size_t n)
{
    return ArenaAlocar(&a->arena, (n + 1) * sizeof(uint64_t));
}

void AnalisesCalcularLacosApenas(const BirFuncao *funcao, AnalisesBir *saida)
{
    memset(saida, 0, sizeof *saida);
    saida->nblocos = funcao->nblocos;
    saida->nvregs = funcao->nvregs;
    AnalisesMontarArestas(saida, funcao);
    AnalisesCalcularRpo(saida);
    AnalisesCalcularIdom(saida);
    AnalisesCalcularLacos(saida);
}

void AnalisesCompletarVivo(const BirFuncao *funcao, AnalisesBir *saida)
{
    AnalisesCalcularVivo(saida, funcao);
}

void AnalisesCalcular(const BirFuncao *funcao, AnalisesBir *saida)
{
    AnalisesCalcularLacosApenas(funcao, saida);
    AnalisesCompletarVivo(funcao, saida);
}

void AnalisesLiberar(AnalisesBir *a)
{
    ArenaLiberar(&a->arena);
    memset(a, 0, sizeof *a);
}

#define FALHA_ANALISE(causa, n, ...) (snprintf((causa), (n), __VA_ARGS__), false)

/* O RPO tem a entrada na frente, posicoes coerentes e exatamente os blocos alcancaveis (conferidos por uma busca independente em largura). */
static bool ConferirRpo(AnalisesBir *a, char *causa, size_t n)
{
    uint32_t *fila = AnalisesVetor32(a, a->nblocos);
    uint8_t *visto = ArenaAlocar(&a->arena, (size_t)a->nblocos + 1);
    uint32_t inicio = 0, fim = 0;

    if (a->nblocos == 0)
        return true;
    visto[0] = 1;
    fila[fim++] = 0;
    while (inicio < fim) {
        uint32_t b = fila[inicio++];

        for (uint32_t k = a->suc_ini[b]; k < a->suc_ini[b + 1]; k++) {
            if (!visto[a->suc[k]]) {
                visto[a->suc[k]] = 1;
                fila[fim++] = a->suc[k];
            }
        }
    }
    if (a->nrpo != fim || a->rpo[0] != 0)
        return FALHA_ANALISE(causa, n, "RPO tem %u blocos, alcancaveis %u", (unsigned)a->nrpo, (unsigned)fim);
    for (uint32_t i = 0; i < a->nrpo; i++) {
        if (!visto[a->rpo[i]] || a->pos_rpo[a->rpo[i]] != i)
            return FALHA_ANALISE(causa, n, "RPO[%u]=%u incoerente", (unsigned)i, (unsigned)a->rpo[i]);
    }
    return true;
}

/* idom(b) vem antes de b no RPO e domina todo predecessor alcancavel de b. */
static bool ConferirIdom(const AnalisesBir *a, char *causa, size_t n)
{
    for (uint32_t i = 1; i < a->nrpo; i++) {
        uint32_t b = a->rpo[i], d = a->idom[b];

        if (d >= a->nblocos || d == b || a->pos_rpo[d] >= i)
            return FALHA_ANALISE(causa, n, "idom do bloco %u invalido (%u)", (unsigned)b, (unsigned)d);
        for (uint32_t k = a->pred_ini[b]; k < a->pred_ini[b + 1]; k++) {
            if (a->pos_rpo[a->pred[k]] != BIR_SEM_BLOCO && !AnalisesDomina(a, d, a->pred[k]))
                return FALHA_ANALISE(causa, n, "idom %u do bloco %u nao domina o predecessor %u", (unsigned)d, (unsigned)b, (unsigned)a->pred[k]);
        }
    }
    return true;
}

/* O cabecalho domina todo bloco do corpo e a profundidade conta o bloco. */
static bool ConferirLacos(const AnalisesBir *a, char *causa, size_t n)
{
    for (uint32_t l = 0; l < a->nlacos; l++) {
        const LacoBir *laco = &a->lacos[l];

        for (uint32_t k = 0; k < laco->ncorpo; k++) {
            if (!AnalisesDomina(a, laco->cabecalho, laco->corpo[k]) || a->prof_laco[laco->corpo[k]] == 0)
                return FALHA_ANALISE(causa, n, "laco %u: bloco %u fora da regra", (unsigned)laco->cabecalho, (unsigned)laco->corpo[k]);
        }
    }
    return true;
}

/* Cada bloco alcancavel e ponto fixo da transferencia e a entrada da funcao so tem parametros vivos. */
static bool ConferirVivo(const BirFuncao *f, AnalisesBir *a, char *causa, size_t n)
{
    for (uint32_t i = 0; i < a->nrpo; i++) {
        if (!AnalisesVivoConfere(a, f, a->rpo[i]))
            return FALHA_ANALISE(causa, n, "vivacidade do bloco %u nao e ponto fixo", (unsigned)a->rpo[i]);
    }
    for (uint32_t v = f->nparams; v < a->nvregs && a->nblocos > 0; v++) {
        if (AnalisesVivoEntra(a, 0, v))
            return FALHA_ANALISE(causa, n, "vreg %u vivo na entrada sem ser parametro", (unsigned)v);
    }
    return true;
}

bool AnalisesConferir(const BirFuncao *funcao, AnalisesBir *a, char *causa, size_t n)
{
    return ConferirRpo(a, causa, n) && ConferirIdom(a, causa, n) && ConferirLacos(a, causa, n) && ConferirVivo(funcao, a, causa, n);
}
