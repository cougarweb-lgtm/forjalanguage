/*
 * forja_rt_ordenar_estavel.h - sort_by(s, less = p) da Forja: ordenacao ESTAVEL, no lugar, com o comparador do usuario (K08a).
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - K08a: primeira versao. Blocos de 20 por insercao + SymMerge com rotacao (o sort.Stable do Go); sem malloc e sem buffer (so trocas de bytes).
 *
 * UM codigo para os dois backends: o backend C escreve este texto no .c gerado (depois de "#define FORJA_ORDENAR_API static") so quando o programa usa sort_by
 * (IrPrograma.usa_ordenar_estavel); o runtime nativo o inclui em rt_ordenar.c (objeto rt_win_curto.o) sem definir FORJA_ORDENAR_API (funcao externa).
 * Elemento i = base + i * tam; menor(x, y) recebe ponteiros para dois elementos e vale verdadeiro quando x vem antes de y (igual ao less da Forja).
 * Pre-requisito: <stdint.h> ja incluido. Verificacao: tests\lowering\323_* a 327_* (sort_by) nos dois backends.
 */
#ifndef FORJA_ORDENAR_API
#define FORJA_ORDENAR_API
#endif

typedef _Bool (*ForjaOrdMenorFn)(void *x, void *y);
typedef struct { unsigned char *base; int64_t tam; ForjaOrdMenorFn menor; } ForjaOrdEst;

#define FORJA_ORD_EL(o, i) ((o)->base + (i) * (o)->tam)

/* Troca os tam bytes dos elementos i e j. */
FORJA_ORDENAR_API void forja_ord_troca_el(const ForjaOrdEst *o, int64_t i, int64_t j)
{
    unsigned char *x = FORJA_ORD_EL(o, i), *y = FORJA_ORD_EL(o, j);
    for (int64_t k = 0; k < o->tam; k++) {
        unsigned char t = x[k];
        x[k] = y[k];
        y[k] = t;
    }
}

/* O comparador do usuario sobre os elementos i e j. */
FORJA_ORDENAR_API _Bool forja_ord_menor_el(const ForjaOrdEst *o, int64_t i, int64_t j)
{
    return o->menor(FORJA_ORD_EL(o, i), FORJA_ORD_EL(o, j));
}

/* Troca n elementos a partir de a com n elementos a partir de b (as faixas nao se cruzam). */
FORJA_ORDENAR_API void forja_ord_troca_faixa(const ForjaOrdEst *o, int64_t a, int64_t b, int64_t n)
{
    for (int64_t k = 0; k < n; k++)
        forja_ord_troca_el(o, a + k, b + k);
}

/* Insercao estavel de [a, b): so troca com o vizinho que e estritamente menor, por isso os iguais nao cruzam. */
FORJA_ORDENAR_API void forja_ord_insere_bloco(const ForjaOrdEst *o, int64_t a, int64_t b)
{
    for (int64_t i = a + 1; i < b; i++) {
        for (int64_t j = i; j > a && forja_ord_menor_el(o, j, j - 1); j--)
            forja_ord_troca_el(o, j, j - 1);
    }
}

/* Gira [a, m) e [m, b) (troca de faixas, sem buffer): no fim, [m, b) vem antes de [a, m). */
FORJA_ORDENAR_API void forja_ord_rotaciona(const ForjaOrdEst *o, int64_t a, int64_t m, int64_t b)
{
    int64_t i = m - a, j = b - m;
    while (i != j) {
        if (i > j) {
            forja_ord_troca_faixa(o, m - i, m, j);
            i -= j;
        } else {
            forja_ord_troca_faixa(o, m - i, m + j - i, i);
            j -= i;
        }
    }
    forja_ord_troca_faixa(o, m - i, m, i);
}

/* [a, m) tem um so elemento (m = a + 1): ele desce por trocas ate a posicao dele dentro de [m, b). */
FORJA_ORDENAR_API void forja_ord_funde_um(const ForjaOrdEst *o, int64_t a, int64_t m, int64_t b)
{
    int64_t i = m, j = b;
    while (i < j) {
        int64_t h = (i + j) / 2;
        if (forja_ord_menor_el(o, h, a)) i = h + 1;
        else j = h;
    }
    for (int64_t k = a; k < i - 1; k++)
        forja_ord_troca_el(o, k, k + 1);
}

/* [m, b) tem um so elemento (b = m + 1): ele sobe por trocas ate a posicao dele dentro de [a, m). Estavel: so passa os maiores estritos. */
FORJA_ORDENAR_API void forja_ord_funde_ultimo(const ForjaOrdEst *o, int64_t a, int64_t m)
{
    int64_t i = a, j = m;
    while (i < j) {
        int64_t h = (i + j) / 2;
        if (!forja_ord_menor_el(o, m, h)) i = h + 1;
        else j = h;
    }
    for (int64_t k = m; k > i; k--)
        forja_ord_troca_el(o, k, k - 1);
}

/* Funde [a, m) e [m, b), ja ordenados (SymMerge do Go): divide no meio, acha o corte por busca binaria, gira o miolo e funde as duas partes. */
FORJA_ORDENAR_API void forja_ord_funde(const ForjaOrdEst *o, int64_t a, int64_t m, int64_t b)
{
    if (m - a == 1) {
        forja_ord_funde_um(o, a, m, b);
        return;
    }
    if (b - m == 1) {
        forja_ord_funde_ultimo(o, a, m);
        return;
    }
    int64_t mid = (a + b) / 2, n = mid + m, inicio, r, fim;
    if (m > mid) {
        inicio = n - b;
        r = mid;
    } else {
        inicio = a;
        r = m;
    }
    while (inicio < r) {
        int64_t c = (inicio + r) / 2;
        if (!forja_ord_menor_el(o, n - 1 - c, c)) inicio = c + 1;
        else r = c;
    }
    fim = n - inicio;
    if (inicio < m && m < fim)
        forja_ord_rotaciona(o, inicio, m, fim);
    if (a < inicio && inicio < mid)
        forja_ord_funde(o, a, inicio, mid);
    if (mid < fim && fim < b)
        forja_ord_funde(o, mid, fim, b);
}

/* Ordena estavel os n elementos de base (tam bytes cada) com menor. Blocos de 20 por insercao; depois funde pares de blocos, dobrando o bloco. */
FORJA_ORDENAR_API void forja_ordenar_estavel(void *base, int64_t n, int64_t tam, ForjaOrdMenorFn menor)
{
    ForjaOrdEst o;
    int64_t bloco = 20, a = 0, b = 20;
    o.base = (unsigned char *)base;
    o.tam = tam;
    o.menor = menor;
    while (b <= n) {
        forja_ord_insere_bloco(&o, a, b);
        a = b;
        b += bloco;
    }
    forja_ord_insere_bloco(&o, a, n);
    for (; bloco < n; bloco *= 2) {
        a = 0;
        b = 2 * bloco;
        while (b <= n) {
            forja_ord_funde(&o, a, a + bloco, b);
            a = b;
            b += 2 * bloco;
        }
        if (a + bloco < n)
            forja_ord_funde(&o, a, a + bloco, n);
    }
}
