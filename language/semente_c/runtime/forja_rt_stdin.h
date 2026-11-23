/*
 * forja_rt_stdin.h - entrada padrao do std:os no Windows, nos dois backends (S3 P5a, decisoes D7 e D9). Texto ASCII puro.
 *
 * Versao: 0.1.2
 * Changelog:
 *   0.1.2 - M5 P44: forja_stdin_ler e forja_stdin_ler_linha seguram forja_stdin_trava (o buffer, a posicao e o modo sao um so; as funcoes viram cascas de _dentro). Nativo: SysTrava, so trava com rt_multithread; backend C: nulo.
 *   0.1.1 - M5 P28b-1: guarda RT_ALVO_LINUX (so as constantes no runtime Linux; a entrada vive em rt_es_linux.c). O Windows e o backend C nao mudam.
 *   0.1.0 - S3 P5a: forja_stdin_ler (bytes crus) e forja_stdin_ler_linha (uma parte da linha por chamada), com buffer estatico de 64 KiB.
 *
 * Contrato:
 *   forja_stdin_ler(destino, n) -> i64: ate n bytes crus. 0 = fim da entrada; -1 = erro (tambem para n <= 0).
 *   forja_stdin_ler_linha(destino, n, ^achou_lf) -> i64: a linha SEM o LF e SEM um CR logo antes dele; bytes crus (sem validar UTF-8).
 *     *achou_lf = 1: esta parte fecha a linha (o LF foi consumido). Ultima linha sem LF vale: vem com *achou_lf = 0 e a proxima chamada devolve 0.
 *     Linha maior que n: vem em partes; cada parte com *achou_lf = 0 e continuada pela chamada seguinte, ate uma parte com *achou_lf = 1 ou o fim.
 *     Devolve 0 com *achou_lf = 0 = fim (nada mais). 0 com *achou_lf = 1 = linha vazia. -1 = erro (tambem para n < 2).
 *   Antes de bloquear esperando entrada, descarrega o stdout (FORJA_ES_DESCARREGAR_SAIDA): o prompt sem LF aparece antes da leitura.
 *   Console (GetConsoleMode ok): ReadConsoleW + WideCharToMultiByte; o high surrogate do fim de uma leitura espera a proxima. Linha que comeca com
 *   ^Z (0x1A) e o fim da entrada (e continua fim nas chamadas seguintes). Cano ou arquivo: ReadFile; ERROR_BROKEN_PIPE = fim.
 *   GetStdHandle nulo ou invalido = fim.
 *
 * Nativo: defina FORJA_ES_NATIVO antes do include (sem libc). As funcoes saem externas (rt_es_win.c). C: static inline, so entra no .c gerado quando o
 * programa importa std:os. Imports de kernel32 com __declspec(dllimport) + rotulo asm, como em forja_rt_es.h.
 */
#ifndef FORJA_RT_STDIN_H
#define FORJA_RT_STDIN_H

#include <stdint.h>

#if defined(FORJA_ES_NATIVO)
#define FORJA_ES_API
#ifndef FORJA_ES_DESCARREGAR_SAIDA
void rt_saida_descarregar(void);
#define FORJA_ES_DESCARREGAR_SAIDA() rt_saida_descarregar()
#endif
#ifndef FORJA_ES_TRAVA_T
#define FORJA_ES_TRAVA_T SysTrava
#define FORJA_ES_PEGAR(t) sys_trava_pegar(&(t))
#define FORJA_ES_SOLTAR(t, tomou) sys_trava_soltar(&(t), (tomou))
#endif
#else
#include <stdio.h>
#define FORJA_ES_API static inline
#ifndef FORJA_ES_DESCARREGAR_SAIDA
#define FORJA_ES_DESCARREGAR_SAIDA() fflush(stdout)
#endif
#ifndef FORJA_ES_TRAVA_T
#define FORJA_ES_TRAVA_T char
#define FORJA_ES_PEGAR(t) ((void)(t), 0)
#define FORJA_ES_SOLTAR(t, tomou) ((void)(t), (void)(tomou))
#endif
#endif

#define FORJA_STDIN_BUF 65536
#define FORJA_STDIN_LOTE_W 4096 /* unidades UTF-16 por ReadConsoleW */
#define FORJA_STDIN_LF 0x0AU
#define FORJA_STDIN_CR 0x0DU
#define FORJA_STDIN_CTRL_Z 0x1AU
#define FORJA_STDIN_UTF8 65001u
#define FORJA_STDIN_MODO_NAO_ABERTO 0
#define FORJA_STDIN_MODO_CANO 1
#define FORJA_STDIN_MODO_CONSOLE 2
#define FORJA_STDIN_MODO_FIM 3

/* P28b-1: com -DRT_ALVO_LINUX o gcc MinGW tambem define _WIN32; ali so valem as constantes acima (as funcoes vivem em rt_es_linux.c). */
#if defined(_WIN32) && !defined(RT_ALVO_LINUX)
#ifndef FORJA_ES_ROTULO
#define FORJA_ES_ROTULO_TXT_(x) #x
#define FORJA_ES_ROTULO_XTXT_(x) FORJA_ES_ROTULO_TXT_(x)
#define FORJA_ES_ROTULO(nome) __asm__(FORJA_ES_ROTULO_XTXT_(__USER_LABEL_PREFIX__) nome)
#endif

#define FORJA_STDIN_STD_INPUT 0xFFFFFFF6UL /* STD_INPUT_HANDLE (-10) */
#define FORJA_STDIN_INVALIDO ((void *)(intptr_t)-1) /* INVALID_HANDLE_VALUE */
#define FORJA_STDIN_ERRO_CANO_FECHADO 109UL /* ERROR_BROKEN_PIPE */

static uint8_t forja_stdin_buf[FORJA_STDIN_BUF];
static int64_t forja_stdin_pos;
static int64_t forja_stdin_fim;
static int32_t forja_stdin_modo;
static void *forja_stdin_h;
static unsigned short forja_stdin_wbuf[FORJA_STDIN_LOTE_W];
static unsigned short forja_stdin_pendente;
static int32_t forja_stdin_tem_pendente;
static int32_t forja_stdin_inicio_linha = 1;
static FORJA_ES_TRAVA_T forja_stdin_trava; /* compartilhado: as duas funcoes publicas seguram a trava inteira (buffer e modo da entrada sao um so); so trava com rt_multithread */

/* kernel32 sem windows.h. Assinaturas iguais em forja_rt_es.h: redeclarar igual e permitido. */
__declspec(dllimport) void *forja_stdin_k32_GetStdHandle(unsigned long id) FORJA_ES_ROTULO("GetStdHandle");
__declspec(dllimport) int forja_stdin_k32_GetConsoleMode(void *h, unsigned long *modo) FORJA_ES_ROTULO("GetConsoleMode");
/* LIMITE: params: assinatura do ReadConsoleW (5 argumentos) */
__declspec(dllimport) int forja_stdin_k32_ReadConsoleW(void *h, void *buf, unsigned long n, unsigned long *lidos, void *controle)
    FORJA_ES_ROTULO("ReadConsoleW");
/* LIMITE: params: assinatura do ReadFile (5 argumentos) */
__declspec(dllimport) int forja_es_k32_ReadFile(void *h, void *buf, unsigned long n, unsigned long *lidos, void *sobreposto) FORJA_ES_ROTULO("ReadFile");
__declspec(dllimport) unsigned long forja_es_k32_GetLastError(void) FORJA_ES_ROTULO("GetLastError");
/* LIMITE: params: assinatura do WideCharToMultiByte (8 argumentos) */
__declspec(dllimport) int forja_es_k32_WideCharToMultiByte(unsigned int pagina, unsigned long flags, const unsigned short *origem, int norigem,
                                                           char *destino, int ndestino, const char *padrao, int *usou_padrao)
    FORJA_ES_ROTULO("WideCharToMultiByte");

static inline void forja_stdin_iniciar(void)
{
    void *h = forja_stdin_k32_GetStdHandle(FORJA_STDIN_STD_INPUT);
    unsigned long modo = 0;

    forja_stdin_modo = FORJA_STDIN_MODO_FIM;
    if (h == 0 || h == FORJA_STDIN_INVALIDO)
        return;
    forja_stdin_h = h;
    forja_stdin_modo = forja_stdin_k32_GetConsoleMode(h, &modo) ? FORJA_STDIN_MODO_CONSOLE : FORJA_STDIN_MODO_CANO;
}

/* ReadFile do cano ou arquivo: bytes lidos (0 = fim, ERROR_BROKEN_PIPE tambem) ou -1. */
static inline int64_t forja_stdin_ler_cano(void)
{
    unsigned long lidos = 0;

    if (!forja_es_k32_ReadFile(forja_stdin_h, forja_stdin_buf, FORJA_STDIN_BUF, &lidos, 0))
        return forja_es_k32_GetLastError() == FORJA_STDIN_ERRO_CANO_FECHADO ? 0 : -1;
    return (int64_t)lidos;
}

static inline int32_t forja_stdin_e_alta(unsigned short unidade)
{
    return unidade >= 0xD800 && unidade <= 0xDBFF;
}

/* Um high surrogate no fim da leitura fica para a proxima (o par nao e partido). So vale quando ha mais de uma unidade. */
static inline void forja_stdin_guardar_alta(int64_t *total)
{
    forja_stdin_pendente = forja_stdin_wbuf[*total - 1];
    forja_stdin_tem_pendente = 1;
    (*total)--;
}

/* ReadConsoleW + WideCharToMultiByte para forja_stdin_buf. Devolve bytes (0 = fim) ou -1. */
static inline int64_t forja_stdin_ler_console(void)
{
    unsigned long antes = forja_stdin_tem_pendente ? 1UL : 0UL;
    unsigned long lidos = 0;
    int64_t total;
    int convertidos;

    if (antes)
        forja_stdin_wbuf[0] = forja_stdin_pendente;
    if (!forja_stdin_k32_ReadConsoleW(forja_stdin_h, forja_stdin_wbuf + antes, FORJA_STDIN_LOTE_W - antes, &lidos, 0))
        return -1;
    total = (int64_t)(antes + lidos);
    forja_stdin_tem_pendente = 0;
    if (lidos > 0 && total > 1 && forja_stdin_e_alta(forja_stdin_wbuf[total - 1]))
        forja_stdin_guardar_alta(&total);
    if (total == 0)
        return 0;
    convertidos = forja_es_k32_WideCharToMultiByte(FORJA_STDIN_UTF8, 0, forja_stdin_wbuf, (int)total, (char *)forja_stdin_buf, FORJA_STDIN_BUF, 0, 0);
    return convertidos > 0 ? convertidos : -1;
}

/* Le a proxima parte da entrada para o buffer (o stdout sai antes de bloquear). Devolve bytes (0 = fim) ou -1. */
static inline int64_t forja_stdin_ler_fonte(void)
{
    FORJA_ES_DESCARREGAR_SAIDA();
    return forja_stdin_modo == FORJA_STDIN_MODO_CONSOLE ? forja_stdin_ler_console() : forja_stdin_ler_cano();
}

/* Garante dado no buffer: 1 = ha dado; 0 = fim; -1 = erro. */
static inline int64_t forja_stdin_garantir(void)
{
    int64_t lidos;

    if (forja_stdin_pos < forja_stdin_fim)
        return 1;
    if (forja_stdin_modo == FORJA_STDIN_MODO_NAO_ABERTO)
        forja_stdin_iniciar();
    if (forja_stdin_modo == FORJA_STDIN_MODO_FIM)
        return 0;
    lidos = forja_stdin_ler_fonte();
    forja_stdin_pos = 0;
    forja_stdin_fim = lidos > 0 ? lidos : 0;
    return lidos > 0 ? 1 : lidos;
}

static inline void forja_stdin_copiar(uint8_t *destino, const uint8_t *origem, int64_t n)
{
    int64_t i;

    for (i = 0; i < n; i++)
        destino[i] = origem[i];
}

static inline int64_t forja_stdin_ler_dentro(uint8_t *destino, int64_t n)
{
    int64_t r;
    int64_t k;

    if (n <= 0)
        return -1;
    r = forja_stdin_garantir();
    if (r <= 0)
        return r;
    k = forja_stdin_fim - forja_stdin_pos;
    if (k > n)
        k = n;
    forja_stdin_copiar(destino, forja_stdin_buf + forja_stdin_pos, k);
    forja_stdin_pos += k;
    return k;
}

FORJA_ES_API int64_t forja_stdin_ler(uint8_t *destino, int64_t n)
{
    int tomou = FORJA_ES_PEGAR(forja_stdin_trava);
    int64_t r = forja_stdin_ler_dentro(destino, n);

    FORJA_ES_SOLTAR(forja_stdin_trava, tomou);
    return r;
}

static inline int64_t forja_stdin_inicio_console(void)
{
    int64_t r = forja_stdin_garantir();

    if (r <= 0 || forja_stdin_modo != FORJA_STDIN_MODO_CONSOLE)
        return r;
    if (forja_stdin_buf[forja_stdin_pos] == FORJA_STDIN_CTRL_Z) {
        forja_stdin_modo = FORJA_STDIN_MODO_FIM;
        return 0;
    }
    return 1;
}

/* Copia do buffer ate LF (consumido, nao copiado), ate encher n ou esgotar o buffer. 1 = achou LF. */
static inline int32_t forja_stdin_varrer(uint8_t *destino, int64_t n, int64_t *k)
{
    uint8_t c;

    while (forja_stdin_pos < forja_stdin_fim && *k < n) {
        c = forja_stdin_buf[forja_stdin_pos++];
        if (c == FORJA_STDIN_LF)
            return 1;
        destino[(*k)++] = c;
    }
    return 0;
}

/* Fecha a linha com LF: tira um CR logo antes dele (copiado nesta chamada). */
static inline int64_t forja_stdin_fechar_linha(const uint8_t *destino, int64_t k, int32_t *achou_lf)
{
    if (k > 0 && destino[k - 1] == FORJA_STDIN_CR)
        k--;
    *achou_lf = 1;
    forja_stdin_inicio_linha = 1;
    return k;
}

/* Parte cheia (k == n) sem LF: um CR no fim volta ao buffer, para o seu LF vir na proxima parte e o CR sair junto. n >= 2 garante k >= 1. */
static inline int64_t forja_stdin_cortar_parte(const uint8_t *destino, int64_t k)
{
    if (destino[k - 1] == FORJA_STDIN_CR) {
        forja_stdin_pos--;
        k--;
    }
    forja_stdin_inicio_linha = 0;
    return k;
}

static inline int64_t forja_stdin_ler_linha_dentro(uint8_t *destino, int64_t n, int32_t *achou_lf)
{
    int64_t k = 0;
    int64_t r;

    *achou_lf = 0;
    if (n < 2)
        return -1;
    if (forja_stdin_inicio_linha) {
        r = forja_stdin_inicio_console();
        if (r <= 0)
            return r;
    }
    for (;;) {
        r = forja_stdin_garantir();
        if (r < 0)
            return -1;
        if (r == 0) {
            forja_stdin_inicio_linha = 1;
            return k;
        }
        if (forja_stdin_varrer(destino, n, &k))
            return forja_stdin_fechar_linha(destino, k, achou_lf);
        if (k == n)
            return forja_stdin_cortar_parte(destino, k);
    }
}

FORJA_ES_API int64_t forja_stdin_ler_linha(uint8_t *destino, int64_t n, int32_t *achou_lf)
{
    int tomou = FORJA_ES_PEGAR(forja_stdin_trava);
    int64_t r = forja_stdin_ler_linha_dentro(destino, n, achou_lf);

    FORJA_ES_SOLTAR(forja_stdin_trava, tomou);
    return r;
}

#elif !defined(RT_ALVO_LINUX)
/* Fora do Windows a entrada ainda nao existe (o POSIX e o M5): erro em todo caminho. */
FORJA_ES_API int64_t forja_stdin_ler(uint8_t *destino, int64_t n) { (void)destino; (void)n; return -1; }
FORJA_ES_API int64_t forja_stdin_ler_linha(uint8_t *destino, int64_t n, int32_t *achou_lf) { (void)destino; (void)n; *achou_lf = 0; return -1; }
#endif

#endif
