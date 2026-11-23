/*
 * rt_linha.c - divisao da linha de comando do Windows em argumentos (std:proc, P26b).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P26b: primeira versao. Mesmas regras do CommandLineToArgvW (shell32 e proibida no runtime: so kernel32): o 1o argumento (o programa) vai
 *           ate a proxima aspa, sem escapes, ou ate o 1o branco; nos demais, 2n barras antes de aspa dao n barras e a aspa alterna, 2n+1 dao n barras e uma
 *           aspa literal, duas aspas seguidas dentro de aspas dao uma aspa literal e seguem dentro, barras sem aspa depois valem como estao. O resultado
 *           e comparado com o CommandLineToArgvW real (tests\nativo\m5\proc\comparar_linha.py).
 *
 * Funcao pura sobre bytes UTF-8 (os separadores sao ASCII, entao UTF-8 passa inteiro): sem sistema, sem libc, sem estado de arquivo.
 * Incluido por rt_win.c antes de rt_sys_win.c; tambem compila sozinho no teste de unidade do M5.
 */
#include <stddef.h>
#include <stdint.h>

/* Cursor da divisao: p le a linha, saida escreve no buffer dos argumentos. */
typedef struct {
    const char *p;
    char *saida;
} RtLinha;

static int rt_linha_branco(char c)
{
    return c == ' ' || c == '\t';
}

static void rt_linha_pular_brancos(RtLinha *l)
{
    while (rt_linha_branco(*l->p))
        l->p++;
}

/* O programa entre aspas vai ate a proxima aspa (que fecha; sem a aspa final, ate o fim da linha). */
static void rt_linha_programa_entre_aspas(RtLinha *l)
{
    l->p++;
    while (*l->p != '\0' && *l->p != '"')
        *l->saida++ = *l->p++;
    if (*l->p == '"')
        l->p++;
}

static void rt_linha_programa(RtLinha *l)
{
    if (*l->p == '"') {
        rt_linha_programa_entre_aspas(l);
    } else {
        while (*l->p != '\0' && !rt_linha_branco(*l->p))
            *l->saida++ = *l->p++;
    }
    *l->saida++ = '\0';
}

static void rt_linha_barras(RtLinha *l, int64_t n)
{
    for (int64_t k = 0; k < n; k++)
        *l->saida++ = '\\';
}

/* Aspas coladas depois da primeira: a 3a conta de uma serie vira uma aspa literal e zera a conta (regra do shell32, a mesma do Wine). */
static int rt_linha_aspas_coladas(RtLinha *l, int q)
{
    while (*l->p == '"') {
        if (++q == 3) {
            *l->saida++ = '"';
            q = 0;
        }
        l->p++;
    }
    return q == 2 ? 0 : q;
}

/* Barras seguidas de aspa: n barras contam n/2; n impar faz a aspa literal; par alterna o estado (q: 0 fora, 1 dentro de aspas). */
static int rt_linha_aspa(RtLinha *l, int64_t barras, int q)
{
    rt_linha_barras(l, barras / 2);
    if (barras % 2 == 1)
        *l->saida++ = '"';
    else
        q++;
    l->p++;
    return rt_linha_aspas_coladas(l, q);
}

/* Um passo do argumento: um grupo de barras com a aspa que as segue, ou um caractere comum. Devolve o estado de aspas. */
static int rt_linha_passo(RtLinha *l, int dentro)
{
    int64_t barras = 0;

    while (*l->p == '\\') {
        barras++;
        l->p++;
    }
    if (*l->p == '"')
        return rt_linha_aspa(l, barras, dentro);
    rt_linha_barras(l, barras);
    if (barras == 0)
        *l->saida++ = *l->p++;
    return dentro;
}

static void rt_linha_argumento(RtLinha *l)
{
    int dentro = 0;

    while (*l->p != '\0' && (dentro || !rt_linha_branco(*l->p)))
        dentro = rt_linha_passo(l, dentro);
    *l->saida++ = '\0';
}

/* Divide a linha (UTF-8, terminada em NUL). Os argumentos saem em buf, cada um com NUL; argv recebe os ponteiros e um NULL final. Devolve argc.
 * Capacidades: buf >= 2 * tamanho da linha + 2 bytes; argv >= tamanho da linha + 2 ponteiros. */
int64_t rt_linha_dividir(const char *linha, char *buf, char **argv)
{
    RtLinha l = {linha, buf};
    int64_t argc = 0;

    argv[argc++] = l.saida;
    rt_linha_programa(&l);
    rt_linha_pular_brancos(&l);
    while (*l.p != '\0') {
        argv[argc++] = l.saida;
        rt_linha_argumento(&l);
        rt_linha_pular_brancos(&l);
    }
    argv[argc] = NULL;
    return argc;
}
