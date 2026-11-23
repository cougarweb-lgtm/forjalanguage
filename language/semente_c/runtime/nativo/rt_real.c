/*
 * rt_real.c - formatacao de real do runtime nativo da Forja (P10a): rt_fmt_f (precisao fixa, porte do fmt_fp do musl),
 * forja_falha_conversao_f e forja_as_<S>_de_f64.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - Ryu d2fixed P4: rt_fmt_f vai para o rt_fmt_fixo.c (musl fmt_fp, sem tabela); somem rt_fmt_e, rt_fmt_g e forja_print_f32 (codigo morto: print de real vira f"{x}" na semantica);
 *           o R0002 escreve o valor pelo Display curto (forja_curto_texto_f64, rt_real_curto.c) em vez de %.17g.
 *   0.1.2 - M2 P11a: forja_falha_conversao_f inicia a RtCausa por rt_causa_iniciar (a RtCausa ganhou o limite); mesmo texto.
 *   0.1.1 - M2 P10b: passa a ser incluido por rt_win.c (a unidade de teste rt_win_m2.c sumiu); sem mudanca de codigo.
 *   0.1.0 - M2 P10a: rt_fmt_e, rt_fmt_f e rt_fmt_g (regra do %g do C); forja_print_f32 (%.9g + \n);
 *           forja_falha_conversao_f (R0002 com %.17g); forja_as_<S>_de_f64 com a faixa do forja_rt.h 0.9.1
 *           (FORJA_DEV) ou a saturacao do forja_rt.h 0.9.1 (sem FORJA_DEV).
 *
 * Nao finitos, como o printf do MinGW com __USE_MINGW_ANSI_STDIO=1 (medido em 08/11/2026): NaN sai como "nan"
 * com qualquer sinal (o printf do MinGW nao mostra "-nan"); +inf como "inf"; -inf como "-inf". O -0.0 sai como
 * "-0" (e "-0.000" etc. no %f), o sinal vem do proprio rt_fmt_fixo.
 *
 * rt_fmt_f: dst tem de ter RT_REAL_BUF_BYTES bytes (o maior texto e %.30f de 1.8e308); precisao 0..30.
 * Devolvem o tamanho do texto. Sem NUL.
 *
 * Com -DRT_REAL_SO_FORMATO so o formatador entra (rt_real.o liga com rt_terceiros.o sem o runtime; e o que
 * conferir_fmt_real.c usa). Sem a macro, a fachada forja_* usa rt_print_linha, RtCausa e forja_falha de
 * rt_nativo.c: entao rt_real.c tem de vir depois de rt_nativo.c na mesma unidade (rt_win.c).
 * Dev/release: FORJA_DEV escolhe a variante de forja_as_<S>_de_f64, como no forja_rt.h.
 * Incluido por rt_win.c; nao compila sozinho sem a macro.
 */
#include <float.h>
#include <stdint.h>

#define RT_REAL_BUF_BYTES 352

/* Texto de um nao finito em dst (devolve o tamanho); finito devolve 0. */
static int64_t rt_real_copia(unsigned char *dst, const char *s)
{
    int64_t n = 0;

    while (s[n] != '\0') {
        dst[n] = (unsigned char)s[n];
        n++;
    }
    return n;
}

static int64_t rt_real_especial(double v, unsigned char *dst)
{
    if (v != v)
        return rt_real_copia(dst, "nan");
    if (v > DBL_MAX)
        return rt_real_copia(dst, "inf");
    if (v < -DBL_MAX)
        return rt_real_copia(dst, "-inf");
    return 0;
}

/* Texto de precisao fixa (%.Nf), exato: o porte do fmt_fp do musl em rt_fmt_fixo.c (rt_terceiros.c o inclui; em rt_win_ryu.o / rt_linux_comum.o nas variantes -DFORJA_RT_SEM_RYU). */
int64_t rt_fmt_fixo(double v, int precisao, unsigned char *dst);

int64_t rt_fmt_f(double v, int precisao, unsigned char *dst)
{
    int64_t n = rt_real_especial(v, dst);

    if (n != 0)
        return n;
    return rt_fmt_fixo(v, precisao, dst);
}

#ifndef RT_REAL_SO_FORMATO

/* Texto Display (o mais curto que volta ao mesmo valor, posicional) de um f64: vive em rt_real_curto.c (objeto rt_win_curto.o / rt_linux_curto.o). */
int64_t forja_curto_texto_f64(double v, unsigned char *dst);

/* Conversao 'as' de real para inteiro fora da faixa (R0002); o valor sai pelo Display curto (a forma %.17g do 0.19.6 saiu). */
_Noreturn void forja_falha_conversao_f(double valor, const char *alvo, const char *arquivo, int linha)
{
    unsigned char texto[RT_REAL_BUF_BYTES];
    int64_t n = forja_curto_texto_f64(valor, texto);
    RtCausa causa;
    int64_t i;

    rt_causa_iniciar(&causa, RT_CAUSA_BYTES);
    rt_causa_texto(&causa, "conversao 'as' de ");
    for (i = 0; i < n; i++)
        rt_causa_byte(&causa, (char)texto[i]);
    rt_causa_texto(&causa, " para ");
    rt_causa_texto(&causa, alvo);
    rt_causa_texto(&causa, " fora da faixa");
    forja_falha(FORJA_R0002, rt_causa_fim(&causa), arquivo, linha);
}

#ifdef FORJA_DEV

/* Dev: faixa aberta (MIN-1, MAX+1) e trunca para zero; NaN falha porque nenhuma comparacao com NaN e verdadeira. */
#define RT_REAL_DEV_AS_F64(S, T, MIN, MAX) \
T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    if (!(v > (double)(MIN) - 1.0 && v < (double)(MAX) + 1.0)) \
        forja_falha_conversao_f(v, #S, arquivo, linha); \
    return (T)v; \
}

RT_REAL_DEV_AS_F64(i8, int8_t, INT8_MIN, INT8_MAX)
RT_REAL_DEV_AS_F64(i16, int16_t, INT16_MIN, INT16_MAX)
RT_REAL_DEV_AS_F64(i32, int32_t, INT32_MIN, INT32_MAX)
RT_REAL_DEV_AS_F64(u8, uint8_t, 0, UINT8_MAX)
RT_REAL_DEV_AS_F64(u16, uint16_t, 0, UINT16_MAX)
RT_REAL_DEV_AS_F64(u32, uint32_t, 0, UINT32_MAX)

/* real -> i64: faixa [-2^63, 2^63). NaN cai fora. */
int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha)
{
    if (!(v >= -0x1p63 && v < 0x1p63))
        forja_falha_conversao_f(v, "i64", arquivo, linha);
    return (int64_t)v;
}

/* real -> u64: faixa (-1, 2^64); (-1, 0) trunca para 0 e cabe. NaN cai fora. */
uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha)
{
    if (!(v > -1.0 && v < 0x1p64))
        forja_falha_conversao_f(v, "u64", arquivo, linha);
    return (uint64_t)v;
}

#else

/* Release: satura em vez de falhar. Ordem das comparacoes: NaN falsifica toda comparacao e devolve 0. */
#define RT_REAL_REL_AS_F64_SINAL(S, T, MIN, MAX) \
T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (v > (double)(MAX)) \
        return (MAX); \
    if (v >= (double)(MIN)) \
        return (T)v; \
    if (v < (double)(MIN)) \
        return (MIN); \
    return 0; \
}

/* Release sem sinal: abaixo de 0 (e NaN) vira 0. */
#define RT_REAL_REL_AS_F64_SEM_SINAL(S, T, MAX) \
T forja_as_##S##_de_f64(double v, const char *arquivo, int linha) \
{ \
    (void)arquivo; \
    (void)linha; \
    if (v > (double)(MAX)) \
        return (MAX); \
    if (v >= 0.0) \
        return (T)v; \
    return 0; \
}

RT_REAL_REL_AS_F64_SINAL(i8, int8_t, INT8_MIN, INT8_MAX)
RT_REAL_REL_AS_F64_SINAL(i16, int16_t, INT16_MIN, INT16_MAX)
RT_REAL_REL_AS_F64_SINAL(i32, int32_t, INT32_MIN, INT32_MAX)
RT_REAL_REL_AS_F64_SEM_SINAL(u8, uint8_t, UINT8_MAX)
RT_REAL_REL_AS_F64_SEM_SINAL(u16, uint16_t, UINT16_MAX)
RT_REAL_REL_AS_F64_SEM_SINAL(u32, uint32_t, UINT32_MAX)

/* Release: real -> i64 satura nas faixas [-2^63, 2^63); NaN devolve 0. */
int64_t forja_as_i64_de_f64(double v, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (v >= 0x1p63)
        return INT64_MAX;
    if (v >= -0x1p63)
        return (int64_t)v;
    if (v < -0x1p63)
        return INT64_MIN;
    return 0;
}

/* Release: real -> u64 satura: acima de 2^64 vira UINT64_MAX, abaixo de 0 vira 0 e NaN devolve 0. */
uint64_t forja_as_u64_de_f64(double v, const char *arquivo, int linha)
{
    (void)arquivo;
    (void)linha;
    if (v >= 0x1p64)
        return UINT64_MAX;
    if (v >= 0.0)
        return (uint64_t)v;
    return 0;
}

#endif

#endif
