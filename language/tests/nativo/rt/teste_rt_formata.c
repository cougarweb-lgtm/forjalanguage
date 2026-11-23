/*
 * teste_rt_formata.c - casos 23 a 25 do runtime nativo da Forja: texto formatado forja_fmt_* (M2 P15b). Escolha o caso com -DCASO=23..25.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - primeira versao: 23 inteiros (largura, zeros, hexa, negativo com zeros, INT64_MIN, bool, str e cstr), 24 reais (f32 e f64 sem precisao, precisao
 *           0, 2 e 30, largura, nan, inf, -0, 1e300 em %.0f), 25 buffer de 10 KB (cresce 256 -> 16384), str_final no heap, cstr_final no frame e na Arena, imprimir
 *           e reuso do buffer. Todos em release (nao ha vazamento a relatar).
 *
 * -DORACULO: usa o forja_rt.h e o CRT do gcc (__USE_MINGW_ANSI_STDIO=1). Sem -DORACULO: liga com rt_win.o (entrada forja_entrada) e as declaracoes de forja_rt_nativo.h.
 * O fmt e montado aqui com constantes proprias (largura em 0..7, precisao em 8..12, zeros 13, hexa 14), os mesmos bits do forja_rt.h.
 * Uso: ver verificar_rt_nativo.py
 */
#ifndef CASO
#error "compile com -DCASO=23..25"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ORACULO
#include "../../../runtime/forja_rt.h"
#else
#include "../../../runtime/nativo/forja_rt_nativo.h"
#endif

#define ARQUIVO_TESTE "teste_formata.fj"
#define TF_ZERO (1u << 13)
#define TF_HEX (1u << 14)
#define TF_LARG(n) ((uint32_t)(n))
#define TF_PREC(n) ((uint32_t)(n) << 8)

static inline void Barra(void)
{
    forja_fmt_bytes("|", 1);
}

static inline forja_str Texto(const char *p, int64_t n)
{
    forja_str s;

    s.ptr = p;
    s.len = n;
    return s;
}

static inline int64_t Soma(const char *p, int64_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    uint64_t acc = 17;
    int64_t i;

    for (i = 0; i < n; i++)
        acc = acc * 1000003ull + b[i] + 1ull;
    return (int64_t)(acc >> 1);
}

#if CASO == 23
/* Inteiros: sem formato, largura (menor e maior que o numero), zeros (com e sem sinal), hexa (0, minusculas, com zeros e largura), INT64_MIN e UINT64_MAX,
 * bool, str (inclusive vazia) e cstr com largura; cada linha reinicia o buffer. */
int forja_programa(void)
{
    int64_t v[] = {0, 7, -7, 42, -42, 123456789012, -9223372036854775807ll - 1, 9223372036854775807ll};
    uint32_t f[] = {0, TF_LARG(5), TF_LARG(5) | TF_ZERO, TF_LARG(2), TF_LARG(30) | TF_ZERO, TF_LARG(255)};
    uint64_t u[] = {0, 9, 255, 4000000000ull, 18446744073709551615ull};
    unsigned i, j;

    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        forja_fmt_inicio();
        for (j = 0; j < sizeof f / sizeof f[0]; j++) {
            forja_fmt_i64(v[i], f[j]);
            Barra();
        }
        forja_fmt_imprimir();
    }
    for (i = 0; i < sizeof u / sizeof u[0]; i++) {
        forja_fmt_inicio();
        forja_fmt_u64(u[i], 0);
        Barra();
        forja_fmt_u64(u[i], TF_HEX);
        Barra();
        forja_fmt_u64(u[i], TF_HEX | TF_LARG(8));
        Barra();
        forja_fmt_u64(u[i], TF_HEX | TF_LARG(20) | TF_ZERO);
        Barra();
        forja_fmt_u64(u[i], TF_LARG(24) | TF_ZERO);
        forja_fmt_imprimir();
    }
    forja_fmt_inicio();
    forja_fmt_bool(true, 0);
    Barra();
    forja_fmt_bool(false, TF_LARG(8));
    Barra();
    forja_fmt_bool(true, TF_LARG(2));
    Barra();
    forja_fmt_str(Texto("grifo", 5), TF_LARG(8));
    Barra();
    forja_fmt_str(Texto("", 0), TF_LARG(3));
    Barra();
    forja_fmt_str(Texto("grifo", 5), TF_LARG(2));
    Barra();
    forja_fmt_cstr("ponte", TF_LARG(9));
    Barra();
    forja_fmt_cstr("", 0);
    forja_fmt_imprimir();
    forja_fmt_inicio();
    forja_fmt_imprimir();
    return 0;
}
#elif CASO == 24
/* Reais: f32 (passado como double) e f64 com precisao 0 (%.0f, empate para par), 2 e 30 (campo vale N + 1), largura e zeros,
 * nan, inf, -inf, -0.0 e um f64 de 309 digitos inteiros em %.0f. Sem precisao NAO entra aqui (Ryu d2fixed P4): o compilador emite o Display curto
 * (forja_fmt_f64_curto), que o forja_rt.h nao tem no mesmo header (tests\nativo\ratio_display\conferir_display.py e as fixtures 391/392 o travam). */
int forja_programa(void)
{
    volatile double zero = 0.0;
    double indefinido = zero / zero;
    double infinito = 1.0 / zero;
    double v[] = {0.0, -0.0, 0.5, 1.5, 2.5, 0.1, 123456789.123456789, -1e-7, 1e300, 1e19, indefinido, infinito, -infinito};
    uint32_t f[] = {TF_PREC(1), TF_PREC(3), TF_PREC(31), TF_PREC(3) | TF_LARG(12), TF_PREC(3) | TF_LARG(12) | TF_ZERO, TF_PREC(2) | TF_LARG(10) | TF_ZERO};
    unsigned i, j;

    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        forja_fmt_inicio();
        for (j = 0; j < sizeof f / sizeof f[0]; j++) {
            forja_fmt_f64(v[i], f[j]);
            Barra();
        }
        forja_fmt_imprimir();
        forja_fmt_inicio();
        for (j = 0; j < sizeof f / sizeof f[0]; j++) {
            forja_fmt_f32((double)(float)v[i], f[j]);
            Barra();
        }
        forja_fmt_imprimir();
    }
    return 0;
}
#elif CASO == 25
/* Buffer de 10 KB: 1000 partes de 10 bytes, copiadas para o heap (str_final), o frame e uma Arena (cstr_final) e impressas; depois o buffer e reusado
 * por um texto curto (inicio zera o len, o cap fica). */
int forja_programa(void)
{
    uint32_t h = forja_arena_criar(256);
    forja_str s;
    const char *c;
    const char *a;
    int64_t i;

    forja_fmt_inicio();
    for (i = 0; i < 1000; i++) {
        forja_fmt_i64(i, TF_LARG(5) | TF_ZERO);
        forja_fmt_bytes("-ab:\n", 5);
    }
    s = forja_fmt_str_final(FORJA_ALOC_HEAP, ARQUIVO_TESTE, 30);
    forja_print_i64(s.len);
    forja_print_i64(Soma(s.ptr, s.len));
    c = forja_fmt_cstr_final(FORJA_ALOC_FRAME, ARQUIVO_TESTE, 31);
    forja_print_bool(forja_no_frame(c));
    forja_print_i64(Soma(c, 10000));
    forja_print_i64((int64_t)(unsigned char)c[10000]);
    a = forja_fmt_cstr_final(h, ARQUIVO_TESTE, 32);
    forja_print_i64(Soma(a, 10000));
    forja_print_i64((int64_t)(unsigned char)a[10000]);
    forja_print_i64(forja_arena_usado(h) >= 10001);
    forja_fmt_imprimir();
    forja_mem_liberar((void *)s.ptr, ARQUIVO_TESTE, 33);
    forja_fmt_inicio();
    forja_fmt_bytes("fim", 3);
    forja_fmt_imprimir();
    forja_fmt_inicio();
    s = forja_fmt_str_final(FORJA_ALOC_FRAME, ARQUIVO_TESTE, 34);
    forja_print_i64(s.len);
    forja_print_i64((int64_t)(unsigned char)s.ptr[0]);
    forja_frame_reset();
    forja_arena_destruir(h);
    return 0;
}
#endif

#ifdef ORACULO
int main(void)
{
    return forja_programa();
}
#endif
