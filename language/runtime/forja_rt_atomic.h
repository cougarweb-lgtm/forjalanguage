/*
 * forja_rt_atomic.h - std:atomic no backend C (M5 P50), o oraculo: C11 stdatomic no lugar dos builtins __atomic do rt_atomic.c.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P50: forja_atomic_* com os mesmos nomes e assinaturas de runtime\nativo\rt_atomic.c (AtomicI32, AtomicI64, AtomicU64, AtomicPtr e a barreira).
 *           Oraculo: operacoes em memory_order_seq_cst (o mais forte; em x86 e a mesma instrucao); a barreira usa a ordem pedida.
 *
 * Texto ASCII puro. Entra no .c gerado so quando o programa tem foreign interno forja_atomic_* (emit_c_runtime.c).
 */
#include <stdint.h>
#include <stdatomic.h>

#define FORJA_ORDEM_RELAXED 0
#define FORJA_ORDEM_ACQUIRE 1
#define FORJA_ORDEM_RELEASE 2
#define FORJA_ORDEM_SEQ_CST 3

static inline memory_order forja_atomic_ordem_c(int32_t ordem)
{
    if (ordem == FORJA_ORDEM_RELAXED)
        return memory_order_relaxed;
    if (ordem == FORJA_ORDEM_ACQUIRE)
        return memory_order_acquire;
    if (ordem == FORJA_ORDEM_RELEASE)
        return memory_order_release;
    return memory_order_seq_cst;
}

#define FORJA_ATOMIC_INTEIRO_C(Sufixo, Tipo) \
    static inline Tipo forja_atomic_carrega_##Sufixo(void *a, int32_t ordem) \
    { \
        (void)ordem; \
        return atomic_load_explicit((_Atomic Tipo *)a, memory_order_seq_cst); \
    } \
    static inline void forja_atomic_grava_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        atomic_store_explicit((_Atomic Tipo *)a, valor, memory_order_seq_cst); \
    } \
    static inline Tipo forja_atomic_soma_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return atomic_fetch_add_explicit((_Atomic Tipo *)a, valor, memory_order_seq_cst); \
    } \
    static inline Tipo forja_atomic_subtrai_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return atomic_fetch_sub_explicit((_Atomic Tipo *)a, valor, memory_order_seq_cst); \
    } \
    static inline Tipo forja_atomic_troca_##Sufixo(void *a, Tipo valor, int32_t ordem) \
    { \
        (void)ordem; \
        return atomic_exchange_explicit((_Atomic Tipo *)a, valor, memory_order_seq_cst); \
    } \
    static inline int32_t forja_atomic_cas_##Sufixo(void *a, Tipo esperado, Tipo novo, int32_t ordem) \
    { \
        Tipo lido = esperado; \
        (void)ordem; \
        return atomic_compare_exchange_strong_explicit((_Atomic Tipo *)a, &lido, novo, memory_order_seq_cst, memory_order_seq_cst) ? 1 : 0; \
    }

FORJA_ATOMIC_INTEIRO_C(i32, int32_t)
FORJA_ATOMIC_INTEIRO_C(i64, int64_t)
FORJA_ATOMIC_INTEIRO_C(u64, uint64_t)

static inline void *forja_atomic_carrega_ptr(void *a, int32_t ordem)
{
    (void)ordem;
    return atomic_load_explicit((void *_Atomic *)a, memory_order_seq_cst);
}

static inline void forja_atomic_grava_ptr(void *a, void *valor, int32_t ordem)
{
    (void)ordem;
    atomic_store_explicit((void *_Atomic *)a, valor, memory_order_seq_cst);
}

static inline void *forja_atomic_troca_ptr(void *a, void *valor, int32_t ordem)
{
    (void)ordem;
    return atomic_exchange_explicit((void *_Atomic *)a, valor, memory_order_seq_cst);
}

static inline int32_t forja_atomic_cas_ptr(void *a, void *esperado, void *novo, int32_t ordem)
{
    void *lido = esperado;

    (void)ordem;
    return atomic_compare_exchange_strong_explicit((void *_Atomic *)a, &lido, novo, memory_order_seq_cst, memory_order_seq_cst) ? 1 : 0;
}

static inline void forja_atomic_barreira(int32_t ordem)
{
    atomic_thread_fence(forja_atomic_ordem_c(ordem));
}
