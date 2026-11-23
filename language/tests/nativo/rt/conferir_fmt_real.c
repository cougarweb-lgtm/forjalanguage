/*
 * conferir_fmt_real.c - formatador de precisao fixa do runtime nativo (rt_fmt_f do rt_real.c, porte do fmt_fp do musl em rt_fmt_fixo.c) contra o
 * printf do MinGW (oraculo), byte a byte, em %.0f, %.1f, %.2f, %.6f, %.17f e %.30f; mais o tempo de 10^6 de %.3f (nativo x snprintf).
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - Ryu d2fixed P4: o formatador e o rt_fmt_fixo (sem Ryu, sem rt_fmt_g/rt_fmt_e, que sairam); semente 20261116 (splitmix64); 10^6 doubles de bits aleatorios (finitos), 10^6 doubles de
 *           expoente decimal moderado, 10^6 decimais "redondos" (n / 10^k, onde mora o empate .5), as bordas por expoente binario (2048 x 5 x 2) e uma lista fixa de bordas (0, -0, 0.5, 2.5, 1e22,
 *           5e-324, DBL_MAX, vizinhos de .5); o tempo de 10^6 de %.3f no mesmo harness.
 *   0.1.0 - P10a: 2 * 10^6 valores de semente fixa 20261108; %.9g, %.17g e os %f.
 *
 * Compilar (a partir de tests\nativo\rt; <lang> = a pasta language):
 *   gcc -std=c11 -O2 -ffreestanding -fno-builtin -Wall -Wextra -Werror -DRT_REAL_SO_FORMATO -c ..\..\..\runtime\nativo\rt_real.c -o rt_real.o
 *   gcc -std=c11 -O2 -ffreestanding -fno-builtin -Wall -Wextra -Werror -c ..\..\..\runtime\nativo\rt_terceiros.c -o rt_terceiros.o
 *   gcc -std=c11 -O2 -Wall -Wextra -Werror -D__USE_MINGW_ANSI_STDIO=1 conferir_fmt_real.c rt_real.o rt_terceiros.o -o conferir_fmt_real.exe
 * Saida esperada: 6 linhas "formato=... casos=N iguais=N", 1 linha "tempo ..." e exit 0.
 */
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEMENTE 20261116ULL
#define QTD_ALEATORIOS 1000000
#define QTD_EXPOENTES 2048
#define QTD_MANTISSAS_BORDA 5
#define QTD_BORDA (QTD_EXPOENTES * QTD_MANTISSAS_BORDA * 2)
#define QTD_FIXAS 64
#define QTD_TOTAL (QTD_BORDA + QTD_FIXAS + 3 * QTD_ALEATORIOS)
#define QTD_FORMATOS 6
#define TAM_TEXTO 400
#define MAX_DIVERGENCIAS 5

/* Mesmo contrato do rt_real.c 0.2.0; dst com 352 bytes ou mais. */
int64_t rt_fmt_f(double v, int precisao, unsigned char *dst);

static const int Precisoes[QTD_FORMATOS] = {0, 1, 2, 6, 17, 30};

static double Valores[QTD_TOTAL];

static uint64_t Proximo(uint64_t *estado)
{
    uint64_t z;

    *estado += 0x9E3779B97F4A7C15ULL;
    z = *estado;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double DoublePorBits(uint64_t bits)
{
    union {
        uint64_t u;
        double d;
    } x;

    x.u = bits;
    return x.d;
}

static uint64_t BitsDoDouble(double v)
{
    union {
        uint64_t u;
        double d;
    } x;

    x.d = v;
    return x.u;
}

static int Finito(double v)
{
    return ((BitsDoDouble(v) >> 52) & 0x7FFu) != 0x7FFu;
}

/* Bordas: para cada expoente binario, 5 mantissas (0, 1, metade, tudo 1, um padrao misto), com sinal + e -. */
static int64_t GerarBordas(double *dst)
{
    static const uint64_t Mantissas[QTD_MANTISSAS_BORDA] = {
        0, 1, 1ULL << 51, (1ULL << 52) - 1, 0x123456789ABCDULL,
    };
    uint64_t e;
    uint64_t k;
    uint64_t sinal;
    int64_t i = 0;

    for (e = 0; e < QTD_EXPOENTES - 1; e++) {
        for (k = 0; k < QTD_MANTISSAS_BORDA; k++) {
            for (sinal = 0; sinal < 2; sinal++)
                dst[i++] = DoublePorBits((sinal << 63) | (e << 52) | Mantissas[k]);
        }
    }
    return i;
}

/* Lista fixa: zeros, empates (.5, 2.5, 0.125), 1e22, o menor subnormal, o maior double e vizinhos de .5 em duas grandezas. */
static int64_t GerarFixas(double *dst)
{
    static const double Base[] = {0.0, -0.0, 0.5, -0.5, 1.5, 2.5, 3.5, 0.125, 0.375, 1e22, 1e23, 5e-324, -5e-324, DBL_MAX, -DBL_MAX, DBL_MIN, 4503599627370496.5,
                                  0.05, 0.15, 0.25, 0.35, 1.005, 2.675, 1e15, 123456789.5, 999999999.5, 0.9999999995};
    int64_t n = 0;
    size_t i;

    for (i = 0; i < sizeof Base / sizeof Base[0]; i++) {
        dst[n++] = Base[i];
        dst[n++] = DoublePorBits(BitsDoDouble(Base[i]) + (BitsDoDouble(Base[i]) != 0 ? 1u : 0u));
    }
    return n;
}

static void GerarDuplos(double *dst, uint64_t *estado)
{
    int64_t i = 0;

    while (i < QTD_ALEATORIOS) {
        double v = DoublePorBits(Proximo(estado));

        if (Finito(v))
            dst[i++] = v;
    }
}

/* Expoente decimal moderado: mantissa aleatoria em [1,2), expoente binario em [-100, 100]. */
static void GerarModerados(double *dst, uint64_t *estado)
{
    int64_t i;

    for (i = 0; i < QTD_ALEATORIOS; i++) {
        uint64_t r = Proximo(estado);
        uint64_t e = 1023 + (r % 201) - 100;

        dst[i] = DoublePorBits(((r >> 8) & 1 ? 1ULL << 63 : 0) | (e << 52) | (Proximo(estado) & ((1ULL << 52) - 1)));
    }
}

/* Decimais redondos n / 10^k (k = 0..8) e n/10^k + 0.5/10^k: onde o empate decide o arredondamento. */
static void GerarRedondos(double *dst, uint64_t *estado)
{
    static const double Potencias[9] = {1.0, 10.0, 100.0, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8};
    int64_t i;

    for (i = 0; i < QTD_ALEATORIOS; i++) {
        uint64_t r = Proximo(estado);
        double n = (double)(r % 100000000ULL);
        double p = Potencias[(r >> 40) % 9];

        dst[i] = (((r >> 50) & 1) ? n + 0.5 : n) / p;
    }
}

static int IgualAoOraculo(double v, int precisao)
{
    char oraculo[TAM_TEXTO];
    unsigned char nativo[TAM_TEXTO];
    int64_t n = rt_fmt_f(v, precisao, nativo);
    int m = snprintf(oraculo, TAM_TEXTO, "%.*f", precisao, v);

    return n == m && memcmp(oraculo, nativo, (size_t)n) == 0;
}

static void RelatarDivergencia(double v, int precisao)
{
    char oraculo[TAM_TEXTO];
    unsigned char nativo[TAM_TEXTO];
    int64_t n = rt_fmt_f(v, precisao, nativo);

    snprintf(oraculo, TAM_TEXTO, "%.*f", precisao, v);
    printf("DIVERGENCIA %%.%df bits=%016llx oraculo=[%s] nativo=[%.*s]\n", precisao, (unsigned long long)BitsDoDouble(v), oraculo, (int)n, (char *)nativo);
}

static int64_t Conferir(int precisao, int64_t total, int64_t *divergencias)
{
    int64_t i;
    int64_t iguais = 0;

    for (i = 0; i < total; i++) {
        if (IgualAoOraculo(Valores[i], precisao))
            iguais++;
        else if (*divergencias < MAX_DIVERGENCIAS) {
            RelatarDivergencia(Valores[i], precisao);
            (*divergencias)++;
        }
    }
    return iguais;
}

static double SegundosDesde(clock_t inicio)
{
    return (double)(clock() - inicio) / (double)CLOCKS_PER_SEC;
}

/* 10^6 de %.3f: soma os tamanhos para o gcc nao descartar o trabalho. */
static void Cronometrar(int64_t total)
{
    unsigned char nativo[TAM_TEXTO];
    char oraculo[TAM_TEXTO];
    int64_t i;
    int64_t soma = 0;
    clock_t inicio = clock();
    double s_nativo;
    double s_oraculo;

    for (i = 0; i < total && i < 1000000; i++)
        soma += rt_fmt_f(Valores[QTD_BORDA + QTD_FIXAS + QTD_ALEATORIOS + i], 3, nativo);
    s_nativo = SegundosDesde(inicio);
    inicio = clock();
    for (i = 0; i < total && i < 1000000; i++)
        soma += snprintf(oraculo, TAM_TEXTO, "%.3f", Valores[QTD_BORDA + QTD_FIXAS + QTD_ALEATORIOS + i]);
    s_oraculo = SegundosDesde(inicio);
    printf("tempo 1e6 x %%.3f (valores moderados): nativo=%.3fs snprintf=%.3fs (soma %lld)\n", s_nativo, s_oraculo, (long long)soma);
}

int main(void)
{
    uint64_t estado = SEMENTE;
    int64_t divergencias = 0;
    int64_t n = 0;
    int falhas = 0;
    int k;

    n += GerarBordas(Valores + n);
    n += GerarFixas(Valores + n);
    while (n < QTD_BORDA + QTD_FIXAS)
        Valores[n++] = 0.0;
    GerarDuplos(Valores + n, &estado);
    n += QTD_ALEATORIOS;
    GerarModerados(Valores + n, &estado);
    n += QTD_ALEATORIOS;
    GerarRedondos(Valores + n, &estado);
    n += QTD_ALEATORIOS;
    for (k = 0; k < QTD_FORMATOS; k++) {
        int64_t iguais = Conferir(Precisoes[k], n, &divergencias);

        printf("formato=%%.%df casos=%lld iguais=%lld\n", Precisoes[k], (long long)n, (long long)iguais);
        if (iguais != n)
            falhas++;
    }
    Cronometrar(n);
    return falhas == 0 ? 0 : 1;
}
