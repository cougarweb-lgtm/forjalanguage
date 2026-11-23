/*
 * rt_fmt_fixo.c - double em texto de precisao fixa (o "%.Nf"), exato, sem tabela: porte do fmt_fp do musl 1.2.6 (src/stdio/vfprintf.c) para o runtime nativo (Ryu d2fixed P4).
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - revisao Ryu P5: o ajuste "if (!**a) (*a)++" so le o bloco quando *a < z (o slot z nunca foi escrito).
 *   0.1.0 - Ryu d2fixed P4: primeira versao. Substitui d2fixed_buffered_n (Ryu, tabela de 329 KB de fonte) atras de rt_fmt_f (rt_real.c).
 *
 * Origem e licenca (Regra #8): musl libc 1.2.6, https://musl.libc.org/releases/musl-1.2.6.tar.gz (SHA256 do tarball d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a),
 * arquivo src/stdio/vfprintf.c, funcao fmt_fp. Licenca MIT, "Copyright (c) 2005-2020 Rich Felker, et al." (texto completo em language\third\musl_math\COPYRIGHT; registro em third\LICENCAS_TERCEIROS.txt).
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without
 * restriction, subject to the inclusion of this copyright notice and the MIT permission notice in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
 * WITHOUT WARRANTY OF ANY KIND.
 *
 * Adaptacoes (so estas; o algoritmo e o do musl): so o ramo 'f' (sem 'e', 'g' nem 'a'); saida num buffer do chamador em vez de FILE*; sem largura, prefixo '+' nem ' ' (so '-');
 * entrada double em vez de long double: o frexpl vira a leitura dos bits, e o "round = 2/LDBL_EPSILON" (2^64, passo 2) vira 2/DBL_EPSILON (2^53, passo 2), que decide o empate
 * pelo mesmo truque (soma de round + small no double, arredondamento para par do valor binario exato). Os nao finitos NAO passam por aqui (rt_real.c os trata antes).
 * O 21 do "need" e o LDBL_MANT_DIG/3 do musl (64/3), mantido para o corte de casas ser o do original.
 *
 * Contrato: rt_fmt_fixo(v, precisao 0..30, dst) escreve sem NUL e devolve o tamanho; dst com 352 bytes ou mais (maior texto: sinal + 309 digitos + '.' + 30 = 341).
 * Incluido por rt_terceiros.c (nos objetos rt_win_ryu.o e rt_linux_comum.o); nao compila sozinho.
 */
#include <stdint.h>

#define RT_FIXO_SLOTS 128 /* max_mant_slots (5) + max_exp_slots (123) do musl para double */
#define RT_FIXO_BASE 1000000000u
#define RT_FIXO_MANT_LDBL 64 /* LDBL_MANT_DIG do musl no "need" */
#define RT_FIXO_MAX_EXP_LDBL 16384 /* LDBL_MAX_EXP do musl no piso de divisao com negativo */

/* Expoente binario e y em [1,2) de v >= 0 (v = y * 2^e2); y = 0 com v = 0. Subnormal normalizado pela mantissa. */
static double rt_fixo_normalizar(double v, int *e2)
{
    union {
        double d;
        uint64_t u;
    } x;
    uint64_t mant;
    int ex;

    x.d = v;
    ex = (int)((x.u >> 52) & 0x7FFu);
    mant = x.u & 0xFFFFFFFFFFFFFull;
    *e2 = 0;
    if (ex == 0 && mant == 0)
        return 0.0;
    if (ex != 0) {
        *e2 = ex - 1023;
    } else {
        *e2 = -1022;
        while ((mant >> 52) == 0) {
            mant <<= 1;
            (*e2)--;
        }
    }
    x.u = ((uint64_t)1023 << 52) | (mant & 0xFFFFFFFFFFFFFull);
    return x.d;
}

/* Os 9 digitos decimais de x (com zeros a esquerda) em s[0..9). */
static void rt_fixo_nove(uint32_t x, unsigned char *s)
{
    int i;

    for (i = 8; i >= 0; i--) {
        s[i] = (unsigned char)('0' + x % 10u);
        x /= 10u;
    }
}

/* Escreve x sem zeros a esquerda (ao menos um digito) em dst; devolve quantos. */
static int64_t rt_fixo_inteiro(uint32_t x, unsigned char *dst)
{
    unsigned char t[9];
    int k;
    int64_t n = 0;

    rt_fixo_nove(x, t);
    for (k = 0; k < 8 && t[k] == '0'; k++) {
    }
    for (; k < 9; k++)
        dst[n++] = t[k];
    return n;
}

/* Passo 1 do musl: y em [1,2)*2^e2 vira blocos base 1e9 em big[a..z); r e o 1o bloco depois da virgula. Devolve z e atualiza *a e *r.
 * LIMITE: parametros 6 a, r e z andam juntos pelos mesmos tres lacos do musl; um struct de uso unico so esconderia os ponteiros.
 * LIMITE: cognitiva 25 os tres lacos (multiplica por 2^29, divide por 2^9, avanca) sao o algoritmo do musl; separar duplicaria o estado dos ponteiros e afastaria o porte do original. */
static uint32_t *rt_fixo_blocos(double y, int e2, int p, uint32_t *big, uint32_t **a, uint32_t **r)
{
    uint32_t *z;
    uint32_t *d;

    if (y != 0.0) {
        y *= 0x1p28;
        e2 -= 28;
    }
    if (e2 < 0)
        *a = *r = z = big;
    else
        *a = *r = z = big + RT_FIXO_SLOTS - 5 - 1;
    do {
        *z = (uint32_t)y;
        y = (double)RT_FIXO_BASE * (y - (double)*z++);
    } while (y != 0.0);
    while (e2 > 0) {
        uint32_t carry = 0;
        int sh = e2 < 29 ? e2 : 29;

        for (d = z - 1; d >= *a; d--) {
            uint64_t x = ((uint64_t)*d << sh) + carry;

            *d = (uint32_t)(x % RT_FIXO_BASE);
            carry = (uint32_t)(x / RT_FIXO_BASE);
        }
        if (carry)
            *--*a = carry;
        while (z > *a && !z[-1])
            z--;
        e2 -= sh;
    }
    while (e2 < 0) {
        uint32_t carry = 0;
        int sh = -e2 < 9 ? -e2 : 9;
        int need = 1 + (p + RT_FIXO_MANT_LDBL / 3 + 8) / 9;

        for (d = *a; d < z; d++) {
            uint32_t rm = *d & ((1u << sh) - 1u);

            *d = (*d >> sh) + carry;
            carry = (RT_FIXO_BASE >> sh) * rm;
        }
        *a += (*a < z) ? !**a : 0; /* revisao Ryu P5: com a == z o slot nao foi escrito (o musl original le o lixo da pilha aqui); ternario para nao subir a cognitiva (teto 25) */
        if (carry)
            *z++ = carry;
        if (z - *r > need)
            z = *r + need;
        e2 += sh;
    }
    return z;
}

/* Arredondamento do musl: o empate decide pelo bit de paridade e pela soma round + small no double (round = 2^53, passo 2).
 * LIMITE: cognitiva 23 a decisao de empate do musl em um bloco so; dividir esconderia a ligacao entre o resto x, a paridade e o vai-um.
 * LIMITE: niveis 4 o vai-um dentro do teste de empate; mantido como no original para a paridade com o printf ser conferivel linha a linha. */
static void rt_fixo_arredondar(uint32_t **a, uint32_t *r, uint32_t **z, int p)
{
    int j = p;
    uint32_t *d;
    uint32_t x;
    uint32_t i;

    if (j >= 9 * (int)(*z - r - 1))
        return;
    d = r + 1 + ((j + 9 * RT_FIXO_MAX_EXP_LDBL) / 9 - RT_FIXO_MAX_EXP_LDBL);
    j += 9 * RT_FIXO_MAX_EXP_LDBL;
    j %= 9;
    for (i = 10, j++; j < 9; i *= 10u, j++) {
    }
    x = *d % i;
    if (x || d + 1 != *z) {
        double round = 0x1p53;
        double small;

        if ((*d / i & 1u) || (i == RT_FIXO_BASE && d > *a && (d[-1] & 1u)))
            round += 2;
        if (x < i / 2)
            small = 0x0.8p0;
        else if (x == i / 2 && d + 1 == *z)
            small = 0x1.0p0;
        else
            small = 0x1.8p0;
        *d -= x;
        if (round + small != round) {
            *d = *d + i;
            while (*d > 999999999u) {
                *d-- = 0;
                if (d < *a)
                    *--*a = 0;
                (*d)++;
            }
        }
    }
    if (*z > d + 1)
        *z = d + 1;
}

/* Escreve os blocos como "inteiro.fracao" com p casas; devolve o tamanho.
 * LIMITE: parametros 5 posicionais: 1 definicao e 1 chamador; os tres ponteiros dos blocos vem do mesmo passo do musl. */
static int64_t rt_fixo_escrever(const uint32_t *a, const uint32_t *r, const uint32_t *z, int p, unsigned char *dst)
{
    const uint32_t *d;
    unsigned char t[9];
    int64_t n = 0;
    int k;

    if (a > r)
        a = r;
    for (d = a; d <= r; d++) {
        if (d == a) {
            n += rt_fixo_inteiro(*d, dst + n);
        } else {
            rt_fixo_nove(*d, t);
            for (k = 0; k < 9; k++)
                dst[n++] = t[k];
        }
    }
    if (p)
        dst[n++] = '.';
    for (; d < z && p > 0; d++, p -= 9) {
        rt_fixo_nove(*d, t);
        for (k = 0; k < 9 && k < p; k++)
            dst[n++] = t[k];
    }
    for (; p > 0; p--)
        dst[n++] = '0';
    return n;
}

int64_t rt_fmt_fixo(double v, int precisao, unsigned char *dst)
{
    uint32_t big[RT_FIXO_SLOTS];
    uint32_t *a;
    uint32_t *r;
    uint32_t *z;
    union {
        double d;
        uint64_t u;
    } x;
    int64_t n = 0;
    int e2;
    double y;

    x.d = v;
    if ((x.u >> 63) != 0) {
        dst[n++] = '-';
        x.u &= 0x7FFFFFFFFFFFFFFFull;
    }
    y = rt_fixo_normalizar(x.d, &e2);
    z = rt_fixo_blocos(y, e2, precisao, big, &a, &r);
    rt_fixo_arredondar(&a, r, &z, precisao);
    while (z > a && !z[-1])
        z--;
    return n + rt_fixo_escrever(a, r, z, precisao, dst + n);
}
