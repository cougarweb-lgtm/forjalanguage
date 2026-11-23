/*
 * lt_a.c - biblioteca C de teste da contabilidade de memoria por lib estatica (M6d, P92a).
 * Nao usa a Forja: so C, compilado pelo gcc (MinGW UCRT no Windows, Alpine no WSL) por construir_libteste.py.
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - M6d P92d: lt_trabalho_thread guarda o bloco em variavel volatile (o gcc eliminava os malloc da thread; lib_thread_sobra nao tinha o que vazar).
 *   0.1.0 - M6d P92a: primeira versao. Familia malloc/calloc/realloc/free/strdup de proposito, cada funcao com um caso do censo:
 *           lt_vaza (sem free), lt_cria/lt_libera (par), lt_texto (strdup), lt_calloc, lt_cresce (realloc), lt_global_init/fim (o `ends` de teste),
 *           lt_thread_aloca (thread do SO dentro da lib), lt_livre_crt (ponteiro do CRT, desconhecido = silencio), lt_bench (n pares).
 *
 * Mapa de simbolos (Windows, nm -u): malloc, calloc, realloc, free e __imp__strdup (_strdup com __declspec(dllimport)).
 * Linux (musl): malloc, calloc, realloc, free e strdup.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
/* dllimport de proposito: o gcc emite a relocacao para __imp__strdup (a forma que o libcurl usa). */
__declspec(dllimport) char *_strdup(const char *s);
__declspec(dllimport) wchar_t *_wcsdup(const wchar_t *s);
#define LT_STRDUP(s) _strdup(s)
#else
#define LT_STRDUP(s) strdup(s)
#endif

static void *lt_bloco_global = NULL;

void *lt_vaza(size_t n)
{
    return malloc(n);
}

void *lt_cria(size_t n)
{
    return malloc(n);
}

void lt_libera(void *p)
{
    free(p);
}

char *lt_texto(const char *s)
{
    return LT_STRDUP(s);
}

void *lt_calloc(size_t n, size_t m)
{
    return calloc(n, m);
}

void *lt_cresce(void *p, size_t n)
{
    return realloc(p, n);
}

void lt_global_init(void)
{
    lt_bloco_global = malloc(64);
}

void lt_global_fim(void)
{
    free(lt_bloco_global);
    lt_bloco_global = NULL;
}

/* Thread do SO criada DENTRO da lib: faz k pares malloc/free e `sobra` malloc sem free, e a principal junta. */
typedef struct {
    size_t k;
    size_t sobra;
} LtPedidoThread;

static void lt_trabalho_thread(LtPedidoThread *pedido)
{
    size_t i;
    void *volatile bloco; /* volatile: o gcc apagava os malloc sem uso (free(malloc(n)) e (void)malloc(n)) e a thread nao alocava nada */

    for (i = 0; i < pedido->k; i++) {
        bloco = malloc(32);
        free(bloco);
    }
    for (i = 0; i < pedido->sobra; i++)
        bloco = malloc(48);
}

#ifdef _WIN32
static DWORD WINAPI LtEntradaThread(LPVOID arg)
{
    lt_trabalho_thread((LtPedidoThread *)arg);
    return 0;
}

void lt_thread_aloca(size_t k, size_t sobra)
{
    LtPedidoThread pedido = {k, sobra};
    HANDLE h = CreateThread(NULL, 0, LtEntradaThread, &pedido, 0, NULL);
    if (h != NULL) {
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    }
}
#else
#include <pthread.h>

static void *LtEntradaThread(void *arg)
{
    lt_trabalho_thread((LtPedidoThread *)arg);
    return NULL;
}

void lt_thread_aloca(size_t k, size_t sobra)
{
    LtPedidoThread pedido = {k, sobra};
    pthread_t t;
    if (pthread_create(&t, NULL, LtEntradaThread, &pedido) == 0)
        pthread_join(t, NULL);
}
#endif

/* Ponteiro alocado pelo CRT por dentro (fora da lib): o free desta lib nao o conhece e o relatorio fica em silencio. */
void lt_livre_crt(void)
{
#ifdef _WIN32
    wchar_t *w = _wcsdup(L"crt");
    free(w);
#else
    static char texto[] = "linha\n";
    char *linha = NULL;
    size_t cap = 0;
    FILE *f = fmemopen(texto, sizeof texto - 1, "r");
    if (f != NULL) {
        (void)getline(&linha, &cap, f);
        fclose(f);
    }
    free(linha);
#endif
}

/* n pares malloc/free de 64 bytes (medicao de custo: lt_bench(200000)). */
void lt_bench(size_t n)
{
    size_t i;
    void *volatile bloco; /* volatile: sem ele o gcc apaga o par malloc/free */

    for (i = 0; i < n; i++) {
        bloco = malloc(64);
        if (bloco != NULL)
            free(bloco);
    }
}
