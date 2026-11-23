/*
 * forja_rt_ordenar.h - sort(fatia) da Forja: ordena no lugar, crescente, uma fatia de inteiro, real (f32, f64) ou str (IA1-L5b, L-06).
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L5b: primeira versao. Quicksort (mediana de 3, Hoare) com insercao nas pontas pequenas e heapsort quando a recursao passa de 2*log2(n); sem libc, sem alocacao.
 *
 * UM codigo para os dois backends: o backend C escreve este texto no .c gerado (EmitRuntimeOrdenarC, depois de "#define FORJA_ORDENAR_API static __attribute__((unused))") so quando o
 * programa usa sort; o runtime nativo o inclui em rt_ordenar.c (objeto rt_win_curto.o) sem definir FORJA_ORDENAR_API (funcao externa forja_ordenar). Mesmo algoritmo nos dois = mesma ordem
 * dos elementos iguais (nao e estavel: -0 e +0, ou dois str iguais com ponteiros diferentes, podem sair em ordem diferente da entrada, igual nos dois backends).
 * Ordem: inteiros pelo valor; f32/f64 pelo valor com NaN depois de tudo; str byte a byte (sem sinal) e, em empate, o menor tamanho primeiro.
 * Pre-requisito: <stdint.h> ja incluido. Verificacao: tests\lowering\208_* a 21x_* (ordenar) nos dois backends.
 */
#ifndef FORJA_ORDENAR_API
#define FORJA_ORDENAR_API
#endif

/* str = {ptr, len} (mesmo layout do forja_str do C e do str do nativo). */
typedef struct { const unsigned char *ptr; int64_t len; } ForjaOrdStr;

/* x < y por bytes; empate nos bytes comuns: o mais curto vem antes. */
FORJA_ORDENAR_API int forja_ord_menor_str(ForjaOrdStr x, ForjaOrdStr y)
{
    int64_t comum = x.len < y.len ? x.len : y.len;
    for (int64_t k = 0; k < comum; k++) {
        if (x.ptr[k] != y.ptr[k])
            return x.ptr[k] < y.ptr[k];
    }
    return x.len < y.len;
}

#define FORJA_ORD_MENOR_NUM(x, y) ((x) < (y))
#define FORJA_ORD_MENOR_REAL(x, y) (((x) < (y)) || (((x) == (x)) && ((y) != (y)))) /* NaN e maior que todo numero */
#define FORJA_ORD_MENOR_STR(x, y) forja_ord_menor_str((x), (y))

#define FORJA_ORD_TROCA(T, a, b) do { T fj_t = (a); (a) = (b); (b) = fj_t; } while (0)

/* Gera forja_ord_<NOME>(T *a, int64_t n). */
#define FORJA_ORD_GERAR(NOME, T, MENOR) \
FORJA_ORDENAR_API void forja_ord_desce_##NOME(T *a, int64_t raiz, int64_t fim) \
{ \
    for (;;) { \
        int64_t filho = 2 * raiz + 1; \
        if (filho >= fim) \
            return; \
        if (filho + 1 < fim && MENOR(a[filho], a[filho + 1])) \
            filho++; \
        if (!MENOR(a[raiz], a[filho])) \
            return; \
        FORJA_ORD_TROCA(T, a[raiz], a[filho]); \
        raiz = filho; \
    } \
} \
FORJA_ORDENAR_API void forja_ord_heap_##NOME(T *a, int64_t n) \
{ \
    for (int64_t k = n / 2 - 1; k >= 0; k--) \
        forja_ord_desce_##NOME(a, k, n); \
    for (int64_t k = n - 1; k > 0; k--) { \
        FORJA_ORD_TROCA(T, a[0], a[k]); \
        forja_ord_desce_##NOME(a, 0, k); \
    } \
} \
FORJA_ORDENAR_API void forja_ord_insere_##NOME(T *a, int64_t n) \
{ \
    for (int64_t k = 1; k < n; k++) { \
        T v = a[k]; \
        int64_t j = k; \
        while (j > 0 && MENOR(v, a[j - 1])) { \
            a[j] = a[j - 1]; \
            j--; \
        } \
        a[j] = v; \
    } \
} \
FORJA_ORDENAR_API int64_t forja_ord_parte_##NOME(T *a, int64_t n) \
{ \
    int64_t m = (n - 1) / 2; \
    if (MENOR(a[m], a[0])) FORJA_ORD_TROCA(T, a[m], a[0]); \
    if (MENOR(a[n - 1], a[0])) FORJA_ORD_TROCA(T, a[n - 1], a[0]); \
    if (MENOR(a[n - 1], a[m])) FORJA_ORD_TROCA(T, a[n - 1], a[m]); \
    T p = a[m]; \
    int64_t i = -1, j = n; \
    for (;;) { \
        do { i++; } while (MENOR(a[i], p)); \
        do { j--; } while (MENOR(p, a[j])); \
        if (i >= j) \
            return j; \
        FORJA_ORD_TROCA(T, a[i], a[j]); \
    } \
} \
FORJA_ORDENAR_API void forja_ord_rapido_##NOME(T *a, int64_t n, int fundo) \
{ \
    while (n > 16) { \
        if (fundo-- == 0) { \
            forja_ord_heap_##NOME(a, n); \
            return; \
        } \
        int64_t j = forja_ord_parte_##NOME(a, n); \
        if (j + 1 < n - j - 1) { \
            forja_ord_rapido_##NOME(a, j + 1, fundo); \
            a += j + 1; \
            n -= j + 1; \
        } else { \
            forja_ord_rapido_##NOME(a + j + 1, n - j - 1, fundo); \
            n = j + 1; \
        } \
    } \
    forja_ord_insere_##NOME(a, n); \
} \
FORJA_ORDENAR_API void forja_ord_##NOME(T *a, int64_t n) \
{ \
    int fundo = 0; \
    for (int64_t m = n; m > 1; m >>= 1) \
        fundo += 2; \
    forja_ord_rapido_##NOME(a, n, fundo); \
}

FORJA_ORD_GERAR(i8, int8_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(i16, int16_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(i32, int32_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(i64, int64_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(u8, uint8_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(u16, uint16_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(u32, uint32_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(u64, uint64_t, FORJA_ORD_MENOR_NUM)
FORJA_ORD_GERAR(f32, float, FORJA_ORD_MENOR_REAL)
FORJA_ORD_GERAR(f64, double, FORJA_ORD_MENOR_REAL)
FORJA_ORD_GERAR(str, ForjaOrdStr, FORJA_ORD_MENOR_STR)

/* tipo = ORDENA_* de ir.h (I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, STR). */
FORJA_ORDENAR_API void forja_ordenar(void *p, int64_t n, unsigned tipo)
{
    switch (tipo) {
    case 0: forja_ord_i8((int8_t *)p, n); break;
    case 1: forja_ord_i16((int16_t *)p, n); break;
    case 2: forja_ord_i32((int32_t *)p, n); break;
    case 3: forja_ord_i64((int64_t *)p, n); break;
    case 4: forja_ord_u8((uint8_t *)p, n); break;
    case 5: forja_ord_u16((uint16_t *)p, n); break;
    case 6: forja_ord_u32((uint32_t *)p, n); break;
    case 7: forja_ord_u64((uint64_t *)p, n); break;
    case 8: forja_ord_f32((float *)p, n); break;
    case 9: forja_ord_f64((double *)p, n); break;
    default: forja_ord_str((ForjaOrdStr *)p, n); break;
    }
}
