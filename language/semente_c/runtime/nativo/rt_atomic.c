/*
 * rt_atomic.c - std:atomic (AtomicI32, AtomicI64, AtomicU64, AtomicPtr e fence) no backend nativo (M5 P50). Unidade propria (D-R8): nao muda rt_win_dev.o, rt_win_rel.o, rt_win_ryu.o nem rt_win_es.o.
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - M5 P50: sem asm (o conferir_regras proibe asm fora de rt_salto/rt_legado); a fence seq_cst fica no __atomic_thread_fence, que o gcc x86-64 traduz em lock orq (barreira cheia com prefixo lock) em vez de mfence.
 *   0.1.0 - M5 P50: forja_atomic_* com os builtins __atomic do gcc (lock xadd, lock cmpxchg, xchg, mov, mfence). Sem chamada ao SO:
 *           a mesma fonte serve ao Windows e, no M5c-b, ao Linux. Ordem (0 relaxed, 1 acquire, 2 release, 3 seq_cst): em x86-64 as
 *           ordens viram a mesma instrucao, salvo o store seq_cst (xchg, que da a barreira) e a fence seq_cst (mfence).
 *           Operacoes read-modify-write (add, sub, exchange, cas) sao sempre seq_cst (lock): mais forte que qualquer ordem pedida, mesmo custo em x86.
 *
 * Os prototipos sao a mao, sem stdatomic.h nem windows.h (como rt_sync_win.c). Objeto so entra na ligacao quando o programa referencia forja_atomic_*.
 * Verificacao: construir.bat e tests\nativo\m5c\atomic (verificar_atomic_p50.py).
 */
#include <stdint.h>
#include <stddef.h>

#define FORJA_ORDEM_RELAXED 0
#define FORJA_ORDEM_SEQ_CST 3

/* Load: mov no x86-64 (o load seq_cst e mov; a barreira fica no store). */
#define FORJA_ATOMIC_INTEIRO(Sufixo, Tipo) \
    Tipo forja_atomic_carrega_##Sufixo(void *a, int32_t ordem) \
    { \
        (void)ordem; \
        return __atomic_load_n((Tipo *)a, __ATOMIC_SEQ_CST); \
    } \
    void forja_atomic_grava_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        if (ordem == FORJA_ORDEM_SEQ_CST) \
            (void)__atomic_exchange_n((Tipo *)a, valor, __ATOMIC_SEQ_CST); \
        else \
            __atomic_store_n((Tipo *)a, valor, __ATOMIC_RELEASE); \
    } \
    Tipo forja_atomic_soma_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return __atomic_fetch_add((Tipo *)a, valor, __ATOMIC_SEQ_CST); \
    } \
    Tipo forja_atomic_subtrai_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return __atomic_fetch_sub((Tipo *)a, valor, __ATOMIC_SEQ_CST); \
    } \
    Tipo forja_atomic_troca_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return __atomic_exchange_n((Tipo *)a, valor, __ATOMIC_SEQ_CST); \
    } \
    int32_t forja_atomic_cas_##Sufixo(void *a, Tipo esperado, Tipo novo, int32_t ordem) \
    { \
        Tipo lido = esperado; \
        (void)ordem; \
        return __atomic_compare_exchange_n((Tipo *)a, &lido, novo, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 1 : 0; \
    }

FORJA_ATOMIC_INTEIRO(i32, int32_t)
FORJA_ATOMIC_INTEIRO(i64, int64_t)
FORJA_ATOMIC_INTEIRO(u64, uint64_t)

/* AtomicPtr: o mesmo padrao sobre void *. */
void *forja_atomic_carrega_ptr(void *a, int32_t ordem)
{
    (void)ordem;
    return __atomic_load_n((void **)a, __ATOMIC_SEQ_CST);
}

void forja_atomic_grava_ptr(void *a, void *valor, int32_t ordem)
{
    if (ordem == FORJA_ORDEM_SEQ_CST)
        (void)__atomic_exchange_n((void **)a, valor, __ATOMIC_SEQ_CST);
    else
        __atomic_store_n((void **)a, valor, __ATOMIC_RELEASE);
}

void *forja_atomic_troca_ptr(void *a, void *valor, int32_t ordem)
{
    (void)ordem;
    return __atomic_exchange_n((void **)a, valor, __ATOMIC_SEQ_CST);
}

int32_t forja_atomic_cas_ptr(void *a, void *esperado, void *novo, int32_t ordem)
{
    void *lido = esperado;

    (void)ordem;
    return __atomic_compare_exchange_n((void **)a, &lido, novo, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 1 : 0;
}

/* Fence global: seq_cst = mfence; acquire e release = so barreira de compilador (no x86-64 nao ha instrucao); relaxed = nada. */
void forja_atomic_barreira(int32_t ordem)
{
    if (ordem == FORJA_ORDEM_SEQ_CST)
        __atomic_thread_fence(__ATOMIC_SEQ_CST); /* o gcc x86-64 emite lock orq (barreira cheia, com prefixo lock), no lugar do mfence; asm fora de rt_salto/rt_legado nao entra (CROSS-R05) */
    else if (ordem != FORJA_ORDEM_RELAXED)
        __atomic_thread_fence(__ATOMIC_ACQ_REL);
}
