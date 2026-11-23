/*
 * rt_real_curto.c - print de f32/f64 pela representacao MAIS CURTA que volta igual (Ryu d2s e f2s), numa unidade propria do runtime nativo (IA1-L5a, L-03).
 *
 * Versao: 0.4.0
 * Changelog:
 *   0.4.0 - Ryu d2fixed P4: forja_curto_texto_f64 (texto do f64 em buffer, sem alinhar) alimenta o forja_fmt_f64_curto e o texto do R0002 (rt_real.c); mesmo texto de antes. O gerador a deixa static.
 *   0.3.0 - Ryu d2fixed P1: o texto vira o Display (Rust): SEMPRE posicional, sem expoente (1e300 sai com 301 caracteres); some a regra [-4,16) do IA1-L03 e o cientifico. runtime\gerar_rt_ryu.py
 *           gera forja_rt_ryu.h (backend C) a partir DESTE arquivo: os dois backends imprimem igual por construcao (nao ponha #include novo nem mude as 3 linhas de #include/#define do topo sem ajustar o gerador).
 *   0.2.0 - IA1-L5a: mora no objeto proprio rt_win_curto.o (rt_curto.c), nao mais no rt_win_ryu.o: um literal ou uma funcao nova no Ryu muda a ordem do .rdata que o ligador leva inteiro e os exes
 *           de quem so formata real (rt_fmt_e/f/g) deixavam de ser byte a byte iguais (D-IA1-13; medido). O expoente do texto do Ryu sai de rt_curto_ler para rt_curto_expoente (cognitiva).
 *   0.1.0 - IA1-L5a: primeira versao. forja_fmt_f32_curto e forja_fmt_f64_curto.
 *
 * Regra do texto (a mesma no backend C, que a recebe por runtime\gerar_rt_ryu.py): digitos mais curtos que voltam ao mesmo valor, sempre posicional e sem ".0" sobrando
 * ("1", "0.5", "12345.678", "0.0001", "1000000000000000", "0.000...05").
 * Nao finitos como o resto do runtime: "nan", "inf", "-inf"; zero negativo "-0".
 * f32 chega alargado a double (como forja_fmt_f32) e volta a float aqui.
 *
 * Incluido por rt_curto.c DEPOIS do envelope (NDEBUG e malloc trocado); nao compila sozinho. Usa forja_fmt_alinhado de rt_formata.c (rt_win.o).
 * Verificacao: as fixtures tests\lowering\202_* (print de real) com os dois backends, iguais entre si.
 */
#include "forja_rt_nativo.h"

#include <stdbool.h>

/* d2s.c e f2s.c definem cada um um to_chars estatico com tipos diferentes: um nome por arquivo. */
#define to_chars to_chars_d2s
#include "../../third/ryu/d2s.c"
#undef to_chars
#define to_chars to_chars_f2s
#include "../../third/ryu/f2s.c"
#undef to_chars

#define RT_CURTO_DIGITOS_MAX 17
#define RT_CURTO_SAIDA_MAX 352 /* pior caso: -5e-324 = "-0." + 323 zeros + "5" = 327 caracteres; maior f64 = 309 */

typedef struct {
    char dig[RT_CURTO_DIGITOS_MAX + 1];
    int nd;
    int e10;
    bool neg;
} RtCurto;

/* O expoente decimal que vem depois do 'E' (sinal opcional); s[i] ja e o 1o caractere depois dele. */
static int rt_curto_expoente(const char *s, int n, int i)
{
    bool negativo = i < n && s[i] == '-';
    int e10 = 0;

    if (i < n && (s[i] == '-' || s[i] == '+'))
        i++;
    for (; i < n; i++)
        e10 = e10 * 10 + (s[i] - '0');
    return negativo ? -e10 : e10;
}

/* "d.dddE-x" do Ryu -> digitos (sem zeros finais), expoente decimal do 1o digito e sinal. */
static RtCurto rt_curto_ler(const char *s, int n)
{
    RtCurto r = {{0}, 0, 0, false};
    int i = 0;

    if (n > 0 && s[0] == '-') {
        r.neg = true;
        i = 1;
    }
    for (; i < n && s[i] != 'E' && s[i] != 'e'; i++)
        if (s[i] != '.' && r.nd < RT_CURTO_DIGITOS_MAX)
            r.dig[r.nd++] = s[i];
    if (i < n)
        r.e10 = rt_curto_expoente(s, n, i + 1);
    while (r.nd > 1 && r.dig[r.nd - 1] == '0')
        r.nd--;
    return r;
}

/* Posicional, para qualquer e10 (Display do Rust: sem expoente). */
static int rt_curto_posicional(const RtCurto *r, char *out)
{
    int n = 0;
    int k;

    if (r->e10 >= 0) {
        for (k = 0; k <= r->e10; k++)
            out[n++] = k < r->nd ? r->dig[k] : '0';
        if (r->nd > r->e10 + 1) {
            out[n++] = '.';
            for (k = r->e10 + 1; k < r->nd; k++)
                out[n++] = r->dig[k];
        }
        return n;
    }
    out[n++] = '0';
    out[n++] = '.';
    for (k = 0; k < -r->e10 - 1; k++)
        out[n++] = '0';
    for (k = 0; k < r->nd; k++)
        out[n++] = r->dig[k];
    return n;
}

/* Texto Display do que o Ryu escreveu (ryu, n bytes) em out; devolve o tamanho. */
static int rt_curto_escrever(const char *ryu, int n, char *out)
{
    RtCurto r = rt_curto_ler(ryu, n);
    int len = 0;

    if (r.neg)
        out[len++] = '-';
    len += rt_curto_posicional(&r, out + len);
    return len;
}

/* nan, inf, -inf em t (4 bytes ou mais); devolve o tamanho, ou 0 se finito (o zero passa pelo Ryu: "0E0" e "-0E0"). */
static int rt_curto_nao_finito(double v, char *t)
{
    /* Texto montado no quadro: o nao finito sai com os mesmos tres/quatro bytes do rt_real.c ("nan", "inf", "-inf"). */
    const char menos_inf[4] = {'-', 'i', 'n', 'f'};
    uint64_t bits;
    int neg;
    int i;

    __builtin_memcpy(&bits, &v, sizeof bits); /* expoente todo 1: mantissa 0 = inf, senao nan (comparar por bits evita constante double no .rdata) */
    if (((bits >> 52) & 0x7FFu) != 0x7FFu)
        return 0;
    if ((bits & 0xFFFFFFFFFFFFFull) != 0) {
        t[0] = 'n';
        t[1] = 'a';
        t[2] = 'n';
        return 3;
    }
    neg = (bits >> 63) != 0;
    for (i = 0; i < 4; i++)
        t[i] = menos_inf[i];
    if (!neg) {
        t[0] = 'i';
        t[1] = 'n';
        t[2] = 'f';
    }
    return neg ? 4 : 3;
}

/* Texto Display de v em out (RT_CURTO_SAIDA_MAX bytes); devolve o tamanho. Exportada para o R0002 (rt_real.c); no forja_rt_ryu.h o gerador a deixa static. */
int64_t forja_curto_texto_f64(double v, unsigned char *out)
{
    char buf[32];
    int n = rt_curto_nao_finito(v, (char *)out);

    if (n != 0)
        return n;
    return rt_curto_escrever(buf, d2s_buffered_n(v, buf), (char *)out);
}

void forja_fmt_f64_curto(double v, uint32_t fmt)
{
    unsigned char t[RT_CURTO_SAIDA_MAX];

    forja_fmt_alinhado((char *)t, (int64_t)forja_curto_texto_f64(v, t), fmt);
}

void forja_fmt_f32_curto(double v, uint32_t fmt)
{
    char buf[32];
    char t[RT_CURTO_SAIDA_MAX];
    float f = (float)v;
    int n = rt_curto_nao_finito((double)f, t);

    if (n == 0)
        n = rt_curto_escrever(buf, f2s_buffered_n(f, buf), t);
    forja_fmt_alinhado(t, n, fmt);
}
