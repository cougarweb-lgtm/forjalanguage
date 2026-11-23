/*
 * forja_rt_varena.h - std:mem VirtualArena no backend C (M5 P52), o oraculo: as mesmas 4 primitivas de runtime\nativo\rt_varena_win.c.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P52: forja_varena_reservar, forja_varena_commit, forja_varena_liberar e forja_varena_ponteiro com os mesmos nomes e tipos do rt_varena_win.c.
 *           Windows: VirtualAlloc e VirtualFree por FORJA_ROTULO_C (sem windows.h). POSIX: stub (reserva falha = sem_reserva); o Linux do VirtualArena e o P59.
 *
 * Texto ASCII puro. Entra no .c gerado so quando o programa tem foreign interno forja_varena_* (emit_c_runtime.c).
 */
#include <stdint.h>

#if defined(_WIN32)
void *forja_varena_va_win(void *endereco, uint64_t n, uint32_t tipo, uint32_t protecao) FORJA_ROTULO_C("VirtualAlloc");
int forja_varena_vl_win(void *endereco, uint64_t n, uint32_t tipo) FORJA_ROTULO_C("VirtualFree");

static inline int64_t forja_varena_reservar(int64_t n)
{
    return (int64_t)(uintptr_t)forja_varena_va_win(0, (uint64_t)n, 0x00002000u, 0x00000001u);
}

static inline int32_t forja_varena_commit(int64_t endereco, int64_t n)
{
    return forja_varena_va_win((void *)(uintptr_t)endereco, (uint64_t)n, 0x00001000u, 0x00000004u) != 0;
}

static inline void forja_varena_liberar(int64_t endereco)
{
    (void)forja_varena_vl_win((void *)(uintptr_t)endereco, 0, 0x00008000u);
}

static inline void *forja_varena_ponteiro(int64_t endereco)
{
    return (void *)(uintptr_t)endereco;
}
#else
/* POSIX: stub. O Linux do VirtualArena (mmap PROT_NONE + mprotect) e o P59. */
static inline int64_t forja_varena_reservar(int64_t n)
{
    (void)n;
    return 0;
}

static inline int32_t forja_varena_commit(int64_t endereco, int64_t n)
{
    (void)endereco;
    (void)n;
    return 0;
}

static inline void forja_varena_liberar(int64_t endereco)
{
    (void)endereco;
}

static inline void *forja_varena_ponteiro(int64_t endereco)
{
    return (void *)(uintptr_t)endereco;
}
#endif
