/*
 * rt_saida.c - stdout e stderr do runtime nativo da Forja (P2a).
 *
 * Versao: 0.7.0
 * Changelog:
 *   0.7.0 - D-G3-L2a (std:os.write_stdout): rt_saida_stdout_cru, bytes crus no stdout (sem CRLF) pelo mesmo buffer e pela mesma trava do print; no CRT o fwrite ainda traduz 0x0A (limite da variante FORJA_RT_CRT).
 *   0.6.0 - M6e P93k: rt_saida_descarregar_falha (descarrega sem rt_saida_trava) para o relato da guarda.
 *   0.5.1 - M5b P39-B3: os typedef dos ganchos usam RT_ABI_SYSV (rt_compat.h); as duas variaveis ganham a marca "compartilhado:" (gravadas 1 vez no inicio).
 *   0.5.0 - M5b P39-B2 (D0654 item 5): no Linux, os ganchos rt_saida_libc_escrever e rt_saida_libc_descarregar (NULL = buffer proprio, como antes). O objeto de
 *           entrada libc liga o FILE stdout do musl por eles: print da Forja e printf do C saem na mesma ordem. Fora do RT_ALVO_LINUX o arquivo nao muda.
 *   0.4.0 - M5 P44: rt_saida_trava (SRW, so com rt_multithread): rt_print_linha segura a linha inteira; rt_saida_descarregar (publica) trava sozinha e o corpo passa a rt_saida_esvaziar (interna, sem trava: o stdout e o fim_print ja estao sob a da linha).
 *           rt_stdout ganha a marca "compartilhado:".
 *   0.3.0 - M5 P26d: no alvo Linux (-DRT_ALVO_LINUX) 0x0A sai como 0x0A (sem CRLF): RT_SAIDA_QUEBRA_VIRA_CRLF. No Windows a expressao e a mesma de antes (objeto igual, byte a byte).
 *   0.2.0 - M3 P19c: variante -DFORJA_RT_CRT (so dos programas com foreign de fora): o stdout do programa passa a ser o FILE stdout do UCRT (fwrite; o buffer de 4096 B e
 *           a traducao de 0x0A ficam com o CRT) e rt_saida_descarregar vira fflush. Um buffer so: a ordem entre print e o que o foreign escreve no stdout (puts, printf, DLL)
 *           e a do programa, como no oraculo C, sem custo nenhum na chamada ao foreign. Sem o -D o arquivo compila para os mesmos bytes de antes.
 *   0.1.0 - primeira versao: stdout com buffer de 4096 B (descarga no fim de cada print quando e console),
 *           stderr direto, 0x0A saindo como 0x0D 0x0A e os formatadores i64, u64 e hex4.
 *
 * Regras (espelham o UCRT em modo texto, que e o que o oraculo usa):
 *   - stdout sem console: guarda ate 4096 B e descarrega ao encher ou no fim do programa.
 *   - stdout com console (GetFileType == FILE_TYPE_CHAR, NUL incluido): descarrega no fim de cada print.
 *   - stderr: nao guarda nada entre chamadas; cada chamada sai direto.
 *   - todo 0x0A em fd 1 e 2 sai como 0x0D 0x0A.
 * Incluido por rt_win.c; nao compila sozinho.
 */
#include <stdbool.h>
#include "rt_sys.h"
#include "rt_compat.h"

#define RT_SAIDA_BUF_BYTES 4096
#define RT_SAIDA_LOTE_BYTES 256
#define RT_LF 0x0AU
#define RT_CR 0x0DU

/* Verdadeiro quando o byte e uma quebra de linha que deve sair como CRLF. O Linux nao traduz nada (D-M5-3). */
#ifdef RT_ALVO_LINUX
#define RT_SAIDA_QUEBRA_VIRA_CRLF(c) 0
#else
#define RT_SAIDA_QUEBRA_VIRA_CRLF(c) ((c) == RT_LF)
#endif

typedef struct {
    bool console;
    int64_t usado;
    unsigned char dados[RT_SAIDA_BUF_BYTES];
} RtSaidaStdout;

/* Trava do stdout: rt_print_linha (rt_nativo.c) a segura a linha inteira; rt_saida_descarregar a segura sozinha. */
static SysTrava rt_saida_trava; /* compartilhado: so com rt_multithread ligado */

#ifdef FORJA_RT_CRT
/* O foreign de fora escreve no stdout do UCRT (puts, printf, uma DLL com printf). Dois buffers (este e o do CRT) saem fora de ordem; por isso aqui o stdout do programa
 * e o do CRT: o mesmo FILE que o print do oraculo C usa. 3 imports da ucrtbase.dll (o compilador os acrescenta ao pedido quando o programa tem foreign de fora). */
#define RT_CRT_STDOUT 1U
__declspec(dllimport) void *__acrt_iob_func(unsigned int indice);
__declspec(dllimport) uint64_t fwrite(const void *dados, uint64_t tamanho, uint64_t quantidade, void *arquivo);
__declspec(dllimport) int fflush(void *arquivo);

void rt_saida_iniciar(void)
{
}

void rt_saida_descarregar(void)
{
    fflush(__acrt_iob_func(RT_CRT_STDOUT));
}

void rt_saida_descarregar_falha(void)
{
    fflush(__acrt_iob_func(RT_CRT_STDOUT)); /* o lock do FILE do CRT e recursivo */
}

void rt_saida_stdout(const void *p, int64_t n)
{
    if (n > 0)
        fwrite(p, 1, (uint64_t)n, __acrt_iob_func(RT_CRT_STDOUT));
}

static void rt_saida_stdout_bruto(const void *p, int64_t n)
{
    if (n > 0)
        fwrite(p, 1, (uint64_t)n, __acrt_iob_func(RT_CRT_STDOUT));
}

void rt_saida_fim_print(void)
{
}
#else
/* Estatico zerado pelo C: usado = 0 e console = false ate rt_saida_iniciar. */
static RtSaidaStdout rt_stdout; /* compartilhado: sob rt_saida_trava */

#ifdef RT_ALVO_LINUX
/* M5b P39-B2 (D-R3): a variante libc liga aqui o FILE stdout do musl (rt_linux_entrada_libc.c). Sem ele (NULL, a variante cru) o stdout segue no buffer proprio. */
typedef void (RT_ABI_SYSV *RtSaidaEscreveFn)(const void *p, int64_t n);
typedef void (RT_ABI_SYSV *RtSaidaDescarregaFn)(void);
RtSaidaEscreveFn rt_saida_libc_escrever = NULL; /* compartilhado: gravado uma vez por forja_main_libc, antes de qualquer print */
RtSaidaDescarregaFn rt_saida_libc_descarregar = NULL; /* compartilhado: gravado uma vez por forja_main_libc, antes de qualquer print */
#endif

void rt_saida_iniciar(void)
{
    rt_stdout.console = sys_e_console(1);
    rt_stdout.usado = 0;
}

/* Esvazia o buffer; quem chama ja esta com a trava (rt_print_linha) ou e single-thread. */
static void rt_saida_esvaziar(void)
{
    sys_escrever(1, rt_stdout.dados, rt_stdout.usado);
    rt_stdout.usado = 0;
}

void rt_saida_descarregar(void)
{
    bool tomou;

#ifdef RT_ALVO_LINUX
    if (rt_saida_libc_descarregar != NULL) {
        rt_saida_libc_descarregar();
        return;
    }
#endif
    tomou = sys_trava_pegar(&rt_saida_trava);
    rt_saida_esvaziar();
    sys_trava_soltar(&rt_saida_trava, tomou);
}

/* M6e P93k: descarrega SEM pegar rt_saida_trava. Para o relato da guarda, que pode rodar dentro de rt_print_linha (SRW nao e recursivo) e vai sair do processo logo depois. */
void rt_saida_descarregar_falha(void)
{
#ifdef RT_ALVO_LINUX
    if (rt_saida_libc_descarregar != NULL) {
        rt_saida_libc_descarregar();
        return;
    }
#endif
    rt_saida_esvaziar();
}

void rt_saida_stdout(const void *p, int64_t n)
{
    const unsigned char *origem = (const unsigned char *)p;
    int64_t i;

#ifdef RT_ALVO_LINUX
    if (rt_saida_libc_escrever != NULL) {
        rt_saida_libc_escrever(p, n);
        return;
    }
#endif
    for (i = 0; i < n; i++) {
        /* Reserva 2 bytes: um 0x0A vira 0x0D 0x0A e precisa de espaco mesmo com o buffer quase cheio. */
        if (rt_stdout.usado + 2 > RT_SAIDA_BUF_BYTES)
            rt_saida_esvaziar();
        if (RT_SAIDA_QUEBRA_VIRA_CRLF(origem[i]))
            rt_stdout.dados[rt_stdout.usado++] = RT_CR;
        rt_stdout.dados[rt_stdout.usado++] = origem[i];
    }
}

/* Bytes crus no buffer do stdout: o laco do print sem trocar 0x0A por CRLF. Quem chama segura rt_saida_trava. */
static void rt_saida_stdout_bruto(const void *p, int64_t n)
{
    const unsigned char *origem = (const unsigned char *)p;
    int64_t i;

#ifdef RT_ALVO_LINUX
    if (rt_saida_libc_escrever != NULL) {
        rt_saida_libc_escrever(p, n);
        return;
    }
#endif
    for (i = 0; i < n; i++) {
        if (rt_stdout.usado + 1 > RT_SAIDA_BUF_BYTES)
            rt_saida_esvaziar();
        rt_stdout.dados[rt_stdout.usado++] = origem[i];
    }
}

void rt_saida_fim_print(void)
{
    if (rt_stdout.console)
        rt_saida_esvaziar();
}
#endif

/* std:os.write_stdout (D-G3-L2a): bytes crus no stdout, sem CRLF; o mesmo buffer e a mesma trava do print. No console descarrega como o fim do print. */
void rt_saida_stdout_cru(const void *p, int64_t n)
{
    bool tomou = sys_trava_pegar(&rt_saida_trava);

    rt_saida_stdout_bruto(p, n);
    rt_saida_fim_print();
    sys_trava_soltar(&rt_saida_trava, tomou);
}

void rt_saida_stderr(const void *p, int64_t n)
{
    const unsigned char *origem = (const unsigned char *)p;
    unsigned char lote[RT_SAIDA_LOTE_BYTES];
    int64_t k = 0;
    int64_t i;

    /* Traduz por lote na pilha: sem buffer persistente, a chamada inteira sai antes de voltar. */
    for (i = 0; i < n; i++) {
        if (k + 2 > RT_SAIDA_LOTE_BYTES) {
            sys_escrever(2, lote, k);
            k = 0;
        }
        if (RT_SAIDA_QUEBRA_VIRA_CRLF(origem[i]))
            lote[k++] = RT_CR;
        lote[k++] = origem[i];
    }
    sys_escrever(2, lote, k);
}

/* Decimal sem sinal em dst (ate 20 bytes, sem NUL). Devolve o tamanho. */
int64_t rt_fmt_u64(unsigned char *dst, uint64_t v)
{
    unsigned char invertido[20]; /* 2^64 - 1 tem 20 digitos */
    int64_t n = 0;
    int64_t i;

    do {
        invertido[n++] = (unsigned char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    for (i = 0; i < n; i++)
        dst[i] = invertido[n - 1 - i];
    return n;
}

/* Decimal com sinal em dst (ate 20 bytes, o sinal conta). Devolve o tamanho. */
int64_t rt_fmt_i64(unsigned char *dst, int64_t v)
{
    if (v < 0) {
        /* Negar INT64_MIN estoura i64: a negacao e feita em u64, onde 2^63 cabe. */
        dst[0] = '-';
        return 1 + rt_fmt_u64(dst + 1, (uint64_t)0 - (uint64_t)v);
    }
    return rt_fmt_u64(dst, (uint64_t)v);
}

/* Quatro digitos hex minusculos de v (o mesmo que %04x), para o \u00XX do JSON. Devolve 4. */
int64_t rt_fmt_hex4(unsigned char *dst, unsigned int v)
{
    static const unsigned char digitos[] = "0123456789abcdef";
    int64_t i;

    for (i = 0; i < 4; i++)
        dst[i] = digitos[(v >> (12 - 4 * i)) & 0xFU];
    return 4;
}
