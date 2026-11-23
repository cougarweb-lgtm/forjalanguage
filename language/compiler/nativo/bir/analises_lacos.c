/* analises_lacos.c - lacos naturais da BIR (back-edge cujo cabecalho domina a origem) e profundidade de laco por bloco. Ver analises.h.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - G3 G0b: quebra de ciclo x64->optimizer (movido de nativo/optimizer/ para nativo/bir/).
 *            0.1.0 - M7a F1: primeira versao.
 * Verificacao: construir_rapido.py && teste_nativo.exe
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/bir/analises_interno.h"

/* Area de trabalho da coleta do corpo: marca[b] = 1 se b ja entrou; pilha de blocos a expandir; lista = corpo em coleta. */
typedef struct { uint8_t *marca; uint32_t *pilha, *lista; uint32_t npilha, nlista; } ColetaCorpo;

static int CompararId(const void *x, const void *y)
{
    uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y;

    return a < b ? -1 : (a > b ? 1 : 0);
}

static void EntrarNoCorpo(ColetaCorpo *c, uint32_t b)
{
    if (c->marca[b])
        return;
    c->marca[b] = 1;
    c->lista[c->nlista++] = b;
    c->pilha[c->npilha++] = b;
}

/* Predecessores do cabecalho h que sao origem de back-edge (h domina a origem) entram na pilha. Devolve quantos. */
static uint32_t SemearOrigens(const AnalisesBir *a, uint32_t h, ColetaCorpo *c)
{
    uint32_t n = 0;

    for (uint32_t k = a->pred_ini[h]; k < a->pred_ini[h + 1]; k++) {
        if (AnalisesDomina(a, h, a->pred[k])) {
            EntrarNoCorpo(c, a->pred[k]);
            n++;
        }
    }
    return n;
}

/* Corpo do laco de cabecalho h: sobe pelos predecessores a partir das origens ate fechar no cabecalho. */
static void ExpandirCorpo(const AnalisesBir *a, uint32_t h, ColetaCorpo *c)
{
    while (c->npilha > 0) {
        uint32_t b = c->pilha[--c->npilha];

        if (b == h)
            continue;
        for (uint32_t k = a->pred_ini[b]; k < a->pred_ini[b + 1]; k++) {
            if (a->pos_rpo[a->pred[k]] != BIR_SEM_BLOCO)
                EntrarNoCorpo(c, a->pred[k]);
        }
    }
}

/* Registra o laco de h (corpo ordenado), soma a profundidade e limpa as marcas. */
static void GuardarLaco(AnalisesBir *a, uint32_t h, ColetaCorpo *c)
{
    LacoBir *laco = &a->lacos[a->nlacos++];

    qsort(c->lista, c->nlista, sizeof *c->lista, CompararId);
    laco->cabecalho = h;
    laco->ncorpo = c->nlista;
    laco->corpo = AnalisesVetor32(a, c->nlista);
    memcpy(laco->corpo, c->lista, (size_t)c->nlista * sizeof *c->lista);
    for (uint32_t k = 0; k < c->nlista; k++) {
        a->prof_laco[c->lista[k]]++;
        c->marca[c->lista[k]] = 0;
    }
    c->nlista = 0;
}

void AnalisesCalcularLacos(AnalisesBir *a)
{
    ColetaCorpo c = {.marca = (uint8_t *)AnalisesVetor32(a, ((size_t)a->nblocos + 3) / 4), .pilha = AnalisesVetor32(a, a->nblocos),
                     .lista = AnalisesVetor32(a, a->nblocos)};

    a->lacos = ArenaAlocar(&a->arena, ((size_t)a->nblocos + 1) * sizeof *a->lacos);
    a->prof_laco = AnalisesVetor32(a, a->nblocos);
    for (uint32_t h = 0; h < a->nblocos; h++) {
        if (a->pos_rpo[h] == BIR_SEM_BLOCO)
            continue;
        EntrarNoCorpo(&c, h);
        c.npilha = 0;
        if (SemearOrigens(a, h, &c) == 0) {
            c.marca[h] = 0;
            c.nlista = 0;
            continue;
        }
        ExpandirCorpo(a, h, &c);
        GuardarLaco(a, h, &c);
    }
}
