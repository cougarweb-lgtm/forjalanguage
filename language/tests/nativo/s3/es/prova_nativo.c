/*
 * prova_nativo.c - prova do modo nativo dos headers (S3 P5a): FORJA_ES_NATIVO, alocador e flush no lugar do runtime nativo, sem libc.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - S3 P5a: inclui forja_rt_es.h e forja_rt_stdin.h com as macros de plataforma definidas aqui (HeapAlloc e HeapFree de kernel32 no lugar de
 *           sys_heap_*; flush vazio no lugar de rt_saida_descarregar). -DPROVA_SEM_STDIN compila so o fs; -DPROVA_SEM_FS compila so o stdin.
 *           Quem roda: verificar_es.py, que confere o nm -u (so kernel32 e memcpy/memset).
 */
#include <stdint.h>

#define FORJA_ES_NATIVO

__declspec(dllimport) void *prova_k32_GetProcessHeap(void) __asm__("GetProcessHeap");
__declspec(dllimport) void *prova_k32_HeapAlloc(void *heap, unsigned long flags, unsigned long long bytes) __asm__("HeapAlloc");
__declspec(dllimport) int prova_k32_HeapFree(void *heap, unsigned long flags, void *p) __asm__("HeapFree");

static inline void *prova_alocar(int64_t n)
{
    return prova_k32_HeapAlloc(prova_k32_GetProcessHeap(), 0, (unsigned long long)n);
}

static inline void prova_liberar(void *p)
{
    prova_k32_HeapFree(prova_k32_GetProcessHeap(), 0, p);
}

static inline void prova_descarregar(void)
{
}

#define FORJA_ES_ALOCAR(n) prova_alocar(n)
#define FORJA_ES_LIBERAR(p) prova_liberar(p)
#define FORJA_ES_DESCARREGAR_SAIDA() prova_descarregar()

#if !defined(PROVA_SEM_FS)
#include "forja_rt_es.h"
#endif
#if !defined(PROVA_SEM_STDIN)
#include "forja_rt_stdin.h"
#endif
