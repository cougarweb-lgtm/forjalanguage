/*
 * forja_rt_sync.h - std:sync no backend C (M5 P49): Mutex (SRWLOCK) e Cond (CONDITION_VARIABLE) sobre a palavra de 8 bytes de cada struct.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P49: forja_sync_travar, _soltar, _tentar, _esperar, _esperar_ms, _sinalizar e _difundir por FORJA_ROTULO_C (sem windows.h),
 *           como o forja_rt_thread.h. POSIX = stub (sem thread nesta etapa, como o Q2 do P28b): trava e espera nao fazem nada.
 *
 * Texto ASCII puro. Entra no .c gerado so quando o programa tem foreign interno forja_sync_* (emit_c_runtime.c).
 */
#include <stdint.h>

#if defined(_WIN32)
void forja_sync_trava_win(void *srw) FORJA_ROTULO_C("AcquireSRWLockExclusive");
void forja_sync_solta_win(void *srw) FORJA_ROTULO_C("ReleaseSRWLockExclusive");
uint8_t forja_sync_tenta_win(void *srw) FORJA_ROTULO_C("TryAcquireSRWLockExclusive");
int forja_sync_dorme_win(void *cv, void *srw, uint32_t ms, uint32_t flags) FORJA_ROTULO_C("SleepConditionVariableSRW");
void forja_sync_acorda_win(void *cv) FORJA_ROTULO_C("WakeConditionVariable");
void forja_sync_acorda_todos_win(void *cv) FORJA_ROTULO_C("WakeAllConditionVariable");

static inline void forja_sync_travar(void *m)
{
    forja_sync_trava_win(m);
}

static inline void forja_sync_soltar(void *m)
{
    forja_sync_solta_win(m);
}

static inline int32_t forja_sync_tentar(void *m)
{
    return forja_sync_tenta_win(m) != 0;
}

static inline void forja_sync_esperar(void *c, void *m)
{
    forja_sync_dorme_win(c, m, 0xFFFFFFFFu, 0);
}

static inline int32_t forja_sync_esperar_ms(void *c, void *m, uint32_t ms)
{
    return forja_sync_dorme_win(c, m, ms, 0) != 0;
}

static inline void forja_sync_sinalizar(void *c)
{
    forja_sync_acorda_win(c);
}

static inline void forja_sync_difundir(void *c)
{
    forja_sync_acorda_todos_win(c);
}
#else
/* POSIX: stub (sem thread nesta etapa, como o Q2 do P28b). */
static inline void forja_sync_travar(void *m)
{
    (void)m;
}

static inline void forja_sync_soltar(void *m)
{
    (void)m;
}

static inline int32_t forja_sync_tentar(void *m)
{
    (void)m;
    return 1;
}

static inline void forja_sync_esperar(void *c, void *m)
{
    (void)c;
    (void)m;
}

static inline int32_t forja_sync_esperar_ms(void *c, void *m, uint32_t ms)
{
    (void)c;
    (void)m;
    (void)ms;
    return 0;
}

static inline void forja_sync_sinalizar(void *c)
{
    (void)c;
}

static inline void forja_sync_difundir(void *c)
{
    (void)c;
}
#endif
