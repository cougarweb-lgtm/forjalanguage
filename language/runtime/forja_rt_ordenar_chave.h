/*
 * forja_rt_ordenar_chave.h - sort_by_key(s, key = Struct.campo) e binary_search(s, value = v) da Forja (K08b).
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - K08b: primeira versao. sort_by_key ordena ESTAVEL no lugar (mesmo algoritmo de forja_rt_ordenar_estavel.h: blocos de 20 + SymMerge, sem buffer) com
 *            o campo como chave (deslocamento + tipo ORDENA_*); binary_search devolve o LOWER BOUND e se achou (igualdade, NaN nunca e achado).
 *            Copia do algoritmo com prefixo forja_ordc_ de proposito: forja_rt_ordenar_estavel.h nao muda (o C dos programas de sort_by segue igual).
 *
 * UM codigo para os dois backends: o backend C escreve este texto no .c gerado (depois de "#define FORJA_ORDENAR_API static __attribute__((unused))") so quando
 * o programa usa sort_by_key ou binary_search (IrPrograma.usa_ordenar_chave); o runtime nativo o inclui em rt_ordenar.c (objeto rt_win_curto.o) sem definir
 * FORJA_ORDENAR_API (funcoes externas). Tipo = ORDENA_* de ir.h: 0 i8, 1 i16, 2 i32, 3 i64, 4 u8, 5 u16, 6 u32, 7 u64, 8 f32, 9 f64, 10 str.
 * Ordem: inteiros pelo valor; f32/f64 com NaN depois de tudo; str byte a byte e, em empate, o menor tamanho primeiro.
 * Pre-requisito: <stdint.h> ja incluido. Verificacao: tests\lowering\328_* a 34x_* (sort_by_key e binary_search) nos dois backends.
 */
#ifndef FORJA_ORDENAR_API
#define FORJA_ORDENAR_API
#endif

/* str = {ptr, len} (mesmo layout do forja_str do C e do str do nativo); nome proprio para nao colidir com forja_rt_ordenar.h. */
typedef struct { const unsigned char *ptr; int64_t len; } ForjaOrdcStr;

/* Forma de um slice para o algoritmo: elemento i = base + i * tam; a chave e o campo em desloc, do tipo tipo. */
typedef struct { unsigned char *base; int64_t tam; int64_t desloc; int64_t tipo; } ForjaOrdcSlice;

#define FORJA_ORDC_EL(o, i) ((o)->base + (i) * (o)->tam)

/* Le um str de p (16 bytes, sem exigir alinhamento). */
FORJA_ORDENAR_API ForjaOrdcStr forja_ordc_le_str(const unsigned char *p)
{
    ForjaOrdcStr s;
    __builtin_memcpy(&s, p, sizeof s);
    return s;
}

/* x < y por bytes; empate nos bytes comuns: o mais curto vem antes. */
FORJA_ORDENAR_API _Bool forja_ordc_menor_str(ForjaOrdcStr x, ForjaOrdcStr y)
{
    int64_t comum = x.len < y.len ? x.len : y.len;
    for (int64_t k = 0; k < comum; k++) {
        if (x.ptr[k] != y.ptr[k])
            return x.ptr[k] < y.ptr[k];
    }
    return x.len < y.len;
}

/* Iguais: mesmo tamanho e mesmos bytes. */
FORJA_ORDENAR_API _Bool forja_ordc_igual_str(ForjaOrdcStr x, ForjaOrdcStr y)
{
    if (x.len != y.len)
        return 0;
    for (int64_t k = 0; k < x.len; k++) {
        if (x.ptr[k] != y.ptr[k])
            return 0;
    }
    return 1;
}

/* Le T de x e de y (memcpy: o campo pode nao estar alinhado no ponteiro de base) e devolve a comparacao pedida. */
#define FORJA_ORDC_NUM_MENOR(T, x, y) do { T fj_a, fj_b; __builtin_memcpy(&fj_a, (x), sizeof fj_a); __builtin_memcpy(&fj_b, (y), sizeof fj_b); return fj_a < fj_b; } while (0)
#define FORJA_ORDC_REAL_MENOR(T, x, y) do { T fj_a, fj_b; __builtin_memcpy(&fj_a, (x), sizeof fj_a); __builtin_memcpy(&fj_b, (y), sizeof fj_b); return (fj_a < fj_b) || (fj_a == fj_a && fj_b != fj_b); } while (0) /* NaN e maior que todo numero */
#define FORJA_ORDC_NUM_IGUAL(T, x, y) do { T fj_a, fj_b; __builtin_memcpy(&fj_a, (x), sizeof fj_a); __builtin_memcpy(&fj_b, (y), sizeof fj_b); return fj_a == fj_b; } while (0)

/* x < y na ordem do sort: x e y apontam para valores do tipo ORDENA_* tipo. */
FORJA_ORDENAR_API _Bool forja_ordc_menor(const unsigned char *x, const unsigned char *y, int64_t tipo)
{
    switch (tipo) {
    case 0: FORJA_ORDC_NUM_MENOR(int8_t, x, y);
    case 1: FORJA_ORDC_NUM_MENOR(int16_t, x, y);
    case 2: FORJA_ORDC_NUM_MENOR(int32_t, x, y);
    case 3: FORJA_ORDC_NUM_MENOR(int64_t, x, y);
    case 4: FORJA_ORDC_NUM_MENOR(uint8_t, x, y);
    case 5: FORJA_ORDC_NUM_MENOR(uint16_t, x, y);
    case 6: FORJA_ORDC_NUM_MENOR(uint32_t, x, y);
    case 7: FORJA_ORDC_NUM_MENOR(uint64_t, x, y);
    case 8: FORJA_ORDC_REAL_MENOR(float, x, y);
    case 9: FORJA_ORDC_REAL_MENOR(double, x, y);
    default: return forja_ordc_menor_str(forja_ordc_le_str(x), forja_ordc_le_str(y));
    }
}

/* x == y na ordem do sort (NaN nunca e igual a nada: == de C). */
FORJA_ORDENAR_API _Bool forja_ordc_igual(const unsigned char *x, const unsigned char *y, int64_t tipo)
{
    switch (tipo) {
    case 0: FORJA_ORDC_NUM_IGUAL(int8_t, x, y);
    case 1: FORJA_ORDC_NUM_IGUAL(int16_t, x, y);
    case 2: FORJA_ORDC_NUM_IGUAL(int32_t, x, y);
    case 3: FORJA_ORDC_NUM_IGUAL(int64_t, x, y);
    case 4: FORJA_ORDC_NUM_IGUAL(uint8_t, x, y);
    case 5: FORJA_ORDC_NUM_IGUAL(uint16_t, x, y);
    case 6: FORJA_ORDC_NUM_IGUAL(uint32_t, x, y);
    case 7: FORJA_ORDC_NUM_IGUAL(uint64_t, x, y);
    case 8: FORJA_ORDC_NUM_IGUAL(float, x, y);
    case 9: FORJA_ORDC_NUM_IGUAL(double, x, y);
    default: return forja_ordc_igual_str(forja_ordc_le_str(x), forja_ordc_le_str(y));
    }
}

/* O comparador da chave sobre os elementos i e j (a chave e o campo em desloc). */
FORJA_ORDENAR_API _Bool forja_ordc_menor_el(const ForjaOrdcSlice *o, int64_t i, int64_t j)
{
    return forja_ordc_menor(FORJA_ORDC_EL(o, i) + o->desloc, FORJA_ORDC_EL(o, j) + o->desloc, o->tipo);
}

/* Troca os tam bytes dos elementos i e j. */
FORJA_ORDENAR_API void forja_ordc_troca_el(const ForjaOrdcSlice *o, int64_t i, int64_t j)
{
    unsigned char *x = FORJA_ORDC_EL(o, i), *y = FORJA_ORDC_EL(o, j);
    for (int64_t k = 0; k < o->tam; k++) {
        unsigned char t = x[k];
        x[k] = y[k];
        y[k] = t;
    }
}

/* Troca n elementos a partir de a com n elementos a partir de b (as faixas nao se cruzam). */
FORJA_ORDENAR_API void forja_ordc_troca_faixa(const ForjaOrdcSlice *o, int64_t a, int64_t b, int64_t n)
{
    for (int64_t k = 0; k < n; k++)
        forja_ordc_troca_el(o, a + k, b + k);
}

/* Insercao estavel de [a, b): so troca com o vizinho que e estritamente menor, por isso os iguais nao cruzam. */
FORJA_ORDENAR_API void forja_ordc_insere_bloco(const ForjaOrdcSlice *o, int64_t a, int64_t b)
{
    for (int64_t i = a + 1; i < b; i++) {
        for (int64_t j = i; j > a && forja_ordc_menor_el(o, j, j - 1); j--)
            forja_ordc_troca_el(o, j, j - 1);
    }
}

/* Gira [a, m) e [m, b) (troca de faixas, sem buffer): no fim, [m, b) vem antes de [a, m). */
FORJA_ORDENAR_API void forja_ordc_rotaciona(const ForjaOrdcSlice *o, int64_t a, int64_t m, int64_t b)
{
    int64_t i = m - a, j = b - m;
    while (i != j) {
        if (i > j) {
            forja_ordc_troca_faixa(o, m - i, m, j);
            i -= j;
        } else {
            forja_ordc_troca_faixa(o, m - i, m + j - i, i);
            j -= i;
        }
    }
    forja_ordc_troca_faixa(o, m - i, m, i);
}

/* [a, m) tem um so elemento: ele desce por trocas ate a posicao dele dentro de [m, b). */
FORJA_ORDENAR_API void forja_ordc_funde_um(const ForjaOrdcSlice *o, int64_t a, int64_t m, int64_t b)
{
    int64_t i = m, j = b;
    while (i < j) {
        int64_t h = (i + j) / 2;
        if (forja_ordc_menor_el(o, h, a)) i = h + 1;
        else j = h;
    }
    for (int64_t k = a; k < i - 1; k++)
        forja_ordc_troca_el(o, k, k + 1);
}

/* [m, b) tem um so elemento: ele sobe por trocas ate a posicao dele dentro de [a, m). Estavel: so passa os maiores estritos. */
FORJA_ORDENAR_API void forja_ordc_funde_ultimo(const ForjaOrdcSlice *o, int64_t a, int64_t m)
{
    int64_t i = a, j = m;
    while (i < j) {
        int64_t h = (i + j) / 2;
        if (!forja_ordc_menor_el(o, m, h)) i = h + 1;
        else j = h;
    }
    for (int64_t k = m; k > i; k--)
        forja_ordc_troca_el(o, k, k - 1);
}

/* Funde [a, m) e [m, b), ja ordenados (SymMerge do Go). */
FORJA_ORDENAR_API void forja_ordc_funde(const ForjaOrdcSlice *o, int64_t a, int64_t m, int64_t b)
{
    if (m - a == 1) {
        forja_ordc_funde_um(o, a, m, b);
        return;
    }
    if (b - m == 1) {
        forja_ordc_funde_ultimo(o, a, m);
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
        if (!forja_ordc_menor_el(o, n - 1 - c, c)) inicio = c + 1;
        else r = c;
    }
    fim = n - inicio;
    if (inicio < m && m < fim)
        forja_ordc_rotaciona(o, inicio, m, fim);
    if (a < inicio && inicio < mid)
        forja_ordc_funde(o, a, inicio, mid);
    if (mid < fim && fim < b)
        forja_ordc_funde(o, mid, fim, b);
}

/* sort_by_key: ordena estavel os n elementos de base (tam bytes cada) pelo campo em desloc, do tipo tipo. Blocos de 20 por insercao; depois funde pares. */
FORJA_ORDENAR_API void forja_ordc_ordenar(void *base, int64_t n, int64_t tam, int64_t desloc, int64_t tipo)
{
    ForjaOrdcSlice o;
    int64_t bloco = 20, a = 0, b = 20;
    o.base = (unsigned char *)base;
    o.tam = tam;
    o.desloc = desloc;
    o.tipo = tipo;
    while (b <= n) {
        forja_ordc_insere_bloco(&o, a, b);
        a = b;
        b += bloco;
    }
    forja_ordc_insere_bloco(&o, a, n);
    for (; bloco < n; bloco *= 2) {
        a = 0;
        b = 2 * bloco;
        while (b <= n) {
            forja_ordc_funde(&o, a, a + bloco, b);
            a = b;
            b += 2 * bloco;
        }
        if (a + bloco < n)
            forja_ordc_funde(&o, a, a + bloco, n);
    }
}

/* binary_search: LOWER BOUND de value em base[0..n) (fatia ordenada pelo mesmo comparador): o 1o indice cujo elemento nao e menor que value. */
FORJA_ORDENAR_API int64_t forja_ordc_posicao(const void *base, int64_t n, int64_t tam, const void *valor, int64_t tipo)
{
    const unsigned char *b = (const unsigned char *)base;
    int64_t lo = 0, hi = n;
    while (lo < hi) {
        int64_t meio = lo + (hi - lo) / 2;
        if (forja_ordc_menor(b + meio * tam, (const unsigned char *)valor, tipo)) lo = meio + 1;
        else hi = meio;
    }
    return lo;
}

/* binary_search: achou = o elemento na posicao do lower bound e igual a value (NaN nunca). */
FORJA_ORDENAR_API _Bool forja_ordc_achou(const void *base, int64_t n, int64_t tam, const void *valor, int64_t tipo)
{
    int64_t p = forja_ordc_posicao(base, n, tam, valor, tipo);
    return p < n && forja_ordc_igual((const unsigned char *)base + p * tam, (const unsigned char *)valor, tipo);
}
