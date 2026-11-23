/*
 * forja_rt_thread.h - std:thread no backend C (M5 P47): spawn, esperar e fechar uma thread; MXCSR da criadora na filha.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P47: forja_thread_criar (CreateThread por FORJA_ROTULO_C, sem windows.h; liga forja_rt_multithread antes; a filha recebe o MXCSR da criadora),
 *           forja_thread_esperar (WaitForSingleObject) e forja_thread_fechar (CloseHandle). POSIX = stub: criar devolve 0 (sem recursos), como o Q2 do P28b.
 *
 * Texto ASCII puro. Entra no .c gerado so quando o programa tem foreign interno forja_thread_* (emit_c_runtime.c).
 */
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
void *forja_thread_cria_win(void *atributos, size_t pilha, uint32_t (*rotina)(void *), void *parametro, uint32_t flags, uint32_t *id) FORJA_ROTULO_C("CreateThread");
uint32_t forja_thread_espera_win(void *alvo, uint32_t ms) FORJA_ROTULO_C("WaitForSingleObject");
int forja_thread_fecha_win(void *alvo) FORJA_ROTULO_C("CloseHandle");

struct ForjaThreadPacote {
    uint32_t (*rotina)(void *);
    void *arg;
    uint32_t mxcsr;
};

static uint32_t forja_thread_entrada(void *p)
{
    struct ForjaThreadPacote *k = (struct ForjaThreadPacote *)p;
    uint32_t (*rotina)(void *) = k->rotina;
    void *arg = k->arg;
    uint32_t mx = k->mxcsr;

    free(k);
    __asm__ volatile("ldmxcsr %0" : : "m"(mx));
    return rotina(arg);
}

static inline uint64_t forja_thread_criar(uint32_t (*rotina)(void *), void *arg)
{
    struct ForjaThreadPacote *k;
    void *h;
    uint32_t mx = 0;

    __asm__ volatile("stmxcsr %0" : "=m"(mx));
    forja_rt_multithread = 1;
    k = (struct ForjaThreadPacote *)malloc(sizeof *k);
    if (k == NULL)
        return 0;
    k->rotina = rotina;
    k->arg = arg;
    k->mxcsr = mx;
    h = forja_thread_cria_win(NULL, 0, forja_thread_entrada, k, 0, NULL);
    if (h == NULL) {
        free(k);
        return 0;
    }
    return (uint64_t)(uintptr_t)h;
}

static inline int32_t forja_thread_esperar(uint64_t h)
{
    return forja_thread_espera_win((void *)(uintptr_t)h, 0xFFFFFFFFu) == 0 ? 0 : 1;
}

static inline void forja_thread_fechar(uint64_t h)
{
    forja_thread_fecha_win((void *)(uintptr_t)h);
}
#else
/* POSIX: stub (sem thread nesta etapa, como o Q2 do P28b). */
static inline uint64_t forja_thread_criar(uint32_t (*rotina)(void *), void *arg)
{
    (void)rotina;
    (void)arg;
    return 0;
}

static inline int32_t forja_thread_esperar(uint64_t h)
{
    (void)h;
    return 1;
}

static inline void forja_thread_fechar(uint64_t h)
{
    (void)h;
}
#endif
