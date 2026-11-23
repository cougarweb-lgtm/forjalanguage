/*
 * rt_soa.c - soa dinamica do runtime nativo da Forja: o bloco unico das colunas (M2 P17a).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - primeira versao: portado de forja_rt.h:1594-1680 (forja_soa_total, criar, crescer, liberar) com os mesmos textos de R0007, a mesma
 *           epoca do frame e o mesmo crescimento (cap 0..3 vira 8, depois dobra). Internas recebem RtSoaForma {tamanhos, ncol, arquivo, linha}
 *           para ficar em 4 parametros; as publicas mantem as assinaturas do forja_rt.h.
 *
 * As K colunas moram num bloco unico; a coluna k comeca em off_k = soma de RT_ALINHA(cap * tams[j]) para j < k.
 * Vem DEPOIS de rt_aloc.c na unidade (usa rt_frame_epoca, rt_aloc_lista e RT_ALINHA, que sao dele); nao compila sozinho.
 */
#include "forja_rt_nativo.h"

#ifndef FORJA_ALOCS_MAX
#error "rt_soa.c precisa de rt_aloc.c incluido antes"
#endif

typedef struct {
    const int64_t *tams;
    uint32_t ncol;
    int64_t linhas; /* linhas (cap) do bloco que se monta */
    const char *arquivo;
    int linha;
} RtSoaForma;

/* Total de bytes do bloco de cap linhas, ou -1 quando alguma conta passaria do limite (cap grande demais). */
int64_t forja_soa_total(const int64_t *tams, uint32_t ncol, int64_t cap)
{
    int64_t total = 0;
    uint32_t k;

    for (k = 0; k < ncol; k++) {
        if (cap > (INT64_MAX / 4) / tams[k])
            return -1;
        total += RT_ALINHA(cap * tams[k]);
        if (total > INT64_MAX / 2)
            return -1;
    }
    return total;
}

static void rt_soa_distribuir(void **col, const RtSoaForma *f, unsigned char *bloco)
{
    int64_t off = 0;
    uint32_t k;

    for (k = 0; k < f->ncol; k++) {
        col[k] = bloco + off;
        off += RT_ALINHA(f->linhas * f->tams[k]);
    }
}

/* Copia as len linhas das colunas atuais para as posicoes do bloco novo (de f->linhas linhas). */
static void rt_soa_copiar(void **col, const RtSoaForma *f, unsigned char *bloco, int64_t len)
{
    int64_t off = 0;
    uint32_t k;

    for (k = 0; k < f->ncol; k++) {
        if (len > 0)
            memcpy(bloco + off, col[k], (size_t)(len * f->tams[k]));
        off += RT_ALINHA(f->linhas * f->tams[k]);
    }
}

static unsigned char *rt_soa_bloco(const RtSoaForma *f, uint32_t alocador, int64_t total, uint32_t *epoca)
{
    if (alocador == FORJA_ALOC_FRAME)
        *epoca = rt_frame_epoca();
    if (alocador == FORJA_ALOC_HEAP || alocador == FORJA_ALOC_FRAME)
        return (unsigned char *)forja_mem_alocar(alocador, total, f->arquivo, f->linha);
    return (unsigned char *)rt_aloc_lista(alocador, total, f->arquivo, f->linha);
}

/* Novo cap (f->linhas) e total de bytes do bloco crescido; R0007 sem alocador ou passando do limite. */
static int64_t rt_soa_dimensionar(RtSoaForma *f, uint32_t alocador, int64_t cap)
{
    int64_t total = -1;

    if (alocador == 0)
        forja_falha_alocacao("append numa soa sem alocador (valor zero): crie com make(soa[dynamic]S, em = heap)", 0, f->arquivo, f->linha);
    f->linhas = 0;
    if (cap <= INT64_MAX / 2) {
        f->linhas = cap < 4 ? 8 : cap * 2;
        total = forja_soa_total(f->tams, f->ncol, f->linhas);
    }
    if (total < 0)
        forja_falha_alocacao("append: a soa passou do limite de memoria com cap", cap, f->arquivo, f->linha);
    return total;
}

void forja_soa_criar(void **col, const int64_t *tams, uint32_t ncol, int64_t cap, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha)
{
    RtSoaForma f = {tams, ncol, cap, arquivo, linha};
    int64_t total;
    uint32_t k;

    *epoca = 0;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0) {
        for (k = 0; k < ncol; k++)
            col[k] = NULL;
        return;
    }
    total = forja_soa_total(tams, ncol, cap);
    if (total < 0)
        forja_falha_alocacao("make com cap grande demais para a memoria", cap, arquivo, linha);
    rt_soa_distribuir(col, &f, rt_soa_bloco(&f, alocador, total, epoca));
}

void forja_soa_crescer(void **col, const int64_t *tams, uint32_t ncol, int64_t len, int64_t *cap, uint32_t alocador, uint32_t *epoca, const char *arquivo, int linha)
{
    RtSoaForma f = {tams, ncol, 0, arquivo, linha};
    int64_t total = rt_soa_dimensionar(&f, alocador, *cap);
    unsigned char *bloco = rt_soa_bloco(&f, alocador, total, epoca);

    rt_soa_copiar(col, &f, bloco, len);
    if (alocador == FORJA_ALOC_HEAP && col[0] != NULL)
        forja_mem_liberar(col[0], arquivo, linha);
    rt_soa_distribuir(col, &f, bloco);
    *cap = f.linhas;
}

void forja_soa_liberar(void **col, uint32_t alocador, const char *arquivo, int linha)
{
    if (alocador == FORJA_ALOC_HEAP && col[0] != NULL)
        forja_mem_liberar(col[0], arquivo, linha);
}
