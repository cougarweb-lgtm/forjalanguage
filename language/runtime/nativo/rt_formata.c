/*
 * rt_formata.c - texto formatado f"...", format e format_c do runtime nativo da Forja (M2 P15b): forja_fmt_*.
 *
 * Versao: 0.3.1
 * Changelog:
 *   0.3.1 - Ryu d2fixed P4: rt_fmt_g saiu; forja_fmt_f32/f64 so recebem precisao (sem ela o compilador emite o _curto) e seguem em rt_fmt_f (musl fmt_fp); precisao 0 sairia %.6f (nunca emitido).
 *   0.3.0 - M5 P43: rt_formata vira campo do RtFio (rt_fio.h): o buffer do f"..." e por thread; o RtFormata mudou de lugar.
 *   0.2.0 - IA1-L5a: forja_fmt_buffer (dados e tamanho do buffer, para o eprint de rt_saida_erro.c). O eprint em si NAO mora aqui: um chamador novo de forja_fmt_bytes muda a decisao de inline do gcc e o rt_win.o
 *           dos programas sem eprint (medido: +384 bytes no .text de um print(f"...")); forja_fmt_buffer nao tem chamador neste arquivo.
 *   0.1.0 - primeira versao: portado de forja_rt.h:1914-2040 com os mesmos textos e bits. Buffer unico do modulo (uma thread) que cresce x2 a partir de 256
 *           (sys_heap_*, fora do rastreio de dev: o forja_rt.h usa realloc), R0007 sem memoria; decimal por rt_fmt_i64/rt_fmt_u64, hexa minusculo proprio,
 *           real sem precisao pelo Display curto (rt_real_curto.c) ou rt_fmt_f (precisao - 1) de rt_real.c, que bate com o printf do MinGW (nan, inf, -0). (Ryu d2fixed P4: rt_fmt_g saiu.)
 *
 * Fmt (uint32_t): bits 0..7 largura, 8..12 precisao (0 = %g; N = %.(N-1)f), 13 zeros a esquerda, 14 hexa (so inteiro sem sinal).
 * forja_fmt_f32 recebe double (o codigo gerado alarga o float antes, como o (double)(v) do emit_c).
 * Incluido por rt_win.c depois de rt_real.c e rt_aloc.c (usa rt_print_linha, forja_mem_alocar e forja_falha_alocacao); nao compila sozinho.
 */
#include "forja_rt_nativo.h"

#define RT_FMT_LARGURA 0xFFu
#define RT_FMT_PRECISAO_DESLOC 8
#define RT_FMT_PRECISAO 0x1Fu
#define RT_FMT_ZERO (1u << 13)
#define RT_FMT_HEX (1u << 14)
#define RT_FMT_CAP_MIN 256
#define RT_FMT_DIGITOS_BYTES 24

/* O buffer do f"..." e do fio da thread (RtFormata em rt_fio.h): duas threads formatando ao mesmo tempo nao misturam o texto. */
#define rt_formata (rt_fio()->formata)

void forja_fmt_inicio(void)
{
    rt_formata.len = 0;
}

/* Garante n bytes livres depois do len: cap dobra a partir de 256; sem memoria e R0007 com o cap pedido. */
void forja_fmt_reservar(int64_t n)
{
    int64_t cap = rt_formata.cap;
    char *novo;

    if (rt_formata.len + n <= cap)
        return;
    while (rt_formata.len + n > cap)
        cap = cap * 2 > RT_FMT_CAP_MIN ? cap * 2 : RT_FMT_CAP_MIN;
    novo = (char *)(rt_formata.dados == NULL ? sys_heap_alocar(cap) : sys_heap_realocar(rt_formata.dados, cap));
    if (novo == NULL)
        forja_falha_alocacao("sem memoria para o texto formatado em bytes", cap, "forja_fmt", 0);
    rt_formata.dados = novo;
    rt_formata.cap = cap;
}

void forja_fmt_bytes(const char *p, int64_t n)
{
    if (n <= 0)
        return;
    forja_fmt_reservar(n);
    memcpy(rt_formata.dados + rt_formata.len, p, (size_t)n);
    rt_formata.len += n;
}

/* Preenche com espacos, ou com zeros depois do '-' (largura em bytes; n >= w nao preenche). */
void forja_fmt_alinhado(const char *p, int64_t n, uint32_t fmt)
{
    int64_t w = (int64_t)(fmt & RT_FMT_LARGURA);
    bool zero = (fmt & RT_FMT_ZERO) != 0;

    if (n >= w) {
        forja_fmt_bytes(p, n);
        return;
    }
    if (zero && p[0] == '-') {
        forja_fmt_bytes("-", 1);
        p++;
        n--;
        w--;
    }
    forja_fmt_reservar(w - n);
    memset(rt_formata.dados + rt_formata.len, zero ? '0' : ' ', (size_t)(w - n));
    rt_formata.len += w - n;
    forja_fmt_bytes(p, n);
}

void forja_fmt_str(forja_str s, uint32_t fmt)
{
    forja_fmt_alinhado(s.ptr, s.len, fmt);
}

void forja_fmt_cstr(const char *p, uint32_t fmt)
{
    forja_fmt_alinhado(p, (int64_t)strlen(p), fmt);
}

void forja_fmt_bool(bool v, uint32_t fmt)
{
    forja_fmt_alinhado(v ? "true" : "false", v ? 4 : 5, fmt);
}

void forja_fmt_i64(int64_t v, uint32_t fmt)
{
    unsigned char t[RT_FMT_DIGITOS_BYTES];

    forja_fmt_alinhado((const char *)t, rt_fmt_i64(t, v), fmt);
}

/* Hexa minusculo sem prefixo (o %llx): ao menos um digito. */
static int64_t rt_formata_hex(unsigned char *dst, uint64_t v)
{
    static const unsigned char digitos[] = "0123456789abcdef";
    unsigned char invertido[16];
    int64_t n = 0;
    int64_t i;

    do {
        invertido[n++] = digitos[v & 0xFu];
        v >>= 4;
    } while (v != 0);
    for (i = 0; i < n; i++)
        dst[i] = invertido[n - 1 - i];
    return n;
}

void forja_fmt_u64(uint64_t v, uint32_t fmt)
{
    unsigned char t[RT_FMT_DIGITOS_BYTES];
    int64_t n = (fmt & RT_FMT_HEX) != 0 ? rt_formata_hex(t, v) : rt_fmt_u64(t, v);

    forja_fmt_alinhado((const char *)t, n, fmt);
}

/* Real com precisao: N = %.(N-1)f. Sem precisao o compilador emite forja_fmt_f32_curto / forja_fmt_f64_curto (rt_real_curto.c, FMT_CURTO) e nunca chega aqui;
 * se chegar (precisao 0), sai %.6f, o padrao do printf. Nao chamar o curto daqui: puxaria a tabela do Ryu para todo programa que formata real com precisao. */
static void rt_formata_real(double v, uint32_t fmt)
{
    unsigned char t[RT_REAL_BUF_BYTES];
    uint32_t pr = (fmt >> RT_FMT_PRECISAO_DESLOC) & RT_FMT_PRECISAO;
    int64_t n = rt_fmt_f(v, pr != 0 ? (int)pr - 1 : 6, t);

    forja_fmt_alinhado((const char *)t, n, fmt);
}

void forja_fmt_f32(double v, uint32_t fmt)
{
    rt_formata_real(v, fmt);
}

void forja_fmt_f64(double v, uint32_t fmt)
{
    rt_formata_real(v, fmt);
}

/* Copia o buffer para um bloco novo de len + 1 bytes do alocador, com NUL no fim. */
const char *forja_fmt_cstr_final(uint32_t alocador, const char *arquivo, int linha)
{
    char *p = (char *)forja_mem_alocar(alocador, rt_formata.len + 1, arquivo, linha);

    if (rt_formata.len > 0)
        memcpy(p, rt_formata.dados, (size_t)rt_formata.len);
    p[rt_formata.len] = 0;
    return p;
}

forja_str forja_fmt_str_final(uint32_t alocador, const char *arquivo, int linha)
{
    forja_str s;

    s.ptr = forja_fmt_cstr_final(alocador, arquivo, linha);
    s.len = rt_formata.len;
    return s;
}

/* O buffer em stdout com '\n' (CRLF pela camada de saida). */
void forja_fmt_imprimir(void)
{
    rt_print_linha(rt_formata.dados, rt_formata.len);
}

/* O buffer do f"...": dados e tamanho, para o eprint (rt_saida_erro.c, outra unidade). Nenhuma funcao deste arquivo a chama: o codigo das outras fica como era (D-IA1-13). */
const char *forja_fmt_buffer(int64_t *len)
{
    *len = rt_formata.len;
    return rt_formata.dados;
}
