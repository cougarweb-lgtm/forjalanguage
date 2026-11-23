/*
 * rt_heap_linux.c - heap do runtime nativo no Linux: dlmalloc 2.8.6 so-mmap (M5 P26d, D-M5-4).
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M5c-b P57 (D-R3): USE_LOCKS 2 (trava do usuario) com a trava por futex de rt_sys_linux.c, so com rt_multithread ligado: programa de 1 thread troca a trava por uma gravacao de palavra (sem syscall, sem atomico). O estado fica na propria palavra (1 = travado), entao a flag virar no meio de uma secao nao desequilibra o soltar.
 *   0.1.0 - M5 P26d: primeira versao. ENVELOPE do dlmalloc vendorizado em language\third\dlmalloc (MIT-0, Doug Lea; LEIAME.txt la): sem brk (HAVE_MORECORE 0), sem trava (USE_LOCKS 0), sem
 *           libc (todos os LACKS_*), MMAP/MUNMAP/MREMAP pelas syscalls de rt_sys_linux.c e ABORT por forja_falha (R0003). As funcoes do dlmalloc ficam 'static' (DLMALLOC_EXPORT): so as tres
 *           que o runtime usa entram no objeto. Nenhuma linha do malloc.c foi alterada.
 *
 * Motivo do dlmalloc: alocador maduro, sem threads nem TLS (o plano 4.6 recusou rpmalloc/mimalloc por TLS). Plano B se um dia falhar: alocador proprio do plano 4.6.
 * O dlmalloc pede memset e memcpy: sao os do rt_mem.c, resolvidos pelo ligador (o objeto comum nao os define). Mora no objeto comum rt_linux_comum.o (rt_comum_linux.c, que
 * inclui este arquivo): o heap e igual nas variantes dev e release, entao um objeto so poupa ~20 KB no forja.exe. Incluido por rt_comum_linux.c; nao compila sozinho.
 */
#include <stddef.h>

#include "forja_rt_nativo.h"
#include "rt_sys.h"
#include "rt_syscall_linux.h"

/* O dlmalloc chama memset e memcpy (LACKS_STRING_H tira o string.h); as definicoes sao as do rt_mem.c, na unidade da variante. */
void *memcpy(void *destino, const void *origem, size_t n);
void *memset(void *destino, int valor, size_t n);

#define SYSL_MREMAP_MAY_MOVE 1LL
#define SYSL_MAPA_FALHOU ((void *)~(size_t)0) /* MFAIL do dlmalloc */

static const char sysl_heap_r0003[] = "R0003";
static const char sysl_heap_causa[] = "dlmalloc: estrutura do heap corrompida";

static _Noreturn void sysl_heap_abortar(void)
{
    forja_falha(sysl_heap_r0003, sysl_heap_causa, __FILE__, __LINE__);
}

static void *sysl_heap_mapear(size_t n)
{
    int64_t v[7] = {SYSL_MMAP, 0, (int64_t)n, SYSL_PROT_LER_ESCREVER, SYSL_MAP_ANONIMO_PRIVADO, -1, 0};
    int64_t r = sysl_chamar6(v);

    return sysl_falhou(r) ? SYSL_MAPA_FALHOU : (void *)r;
}

static int sysl_heap_desmapear(void *p, size_t n)
{
    return (int)sysl_chamar2(SYSL_MUNMAP, (int64_t)p, (int64_t)n);
}

static void *sysl_heap_remapear(void *p, size_t antigo, size_t novo, int mover)
{
    int64_t v[7] = {SYSL_MREMAP, (int64_t)p, (int64_t)antigo, (int64_t)novo, mover ? SYSL_MREMAP_MAY_MOVE : 0, 0, 0};
    int64_t r = sysl_chamar6(v);

    return sysl_falhou(r) ? SYSL_MAPA_FALHOU : (void *)r;
}

/* A trava do dlmalloc (USE_LOCKS 2). Com rt_multithread desligado so grava a palavra: o programa de uma thread nao paga futex nem atomico. */
static SysTrava malloc_global_mutex; /* compartilhado: so trava com rt_multithread ligado (sysl_heap_travar) */

/* Devolve 0 (o ACQUIRE_LOCK do dlmalloc e uma expressao de valor 0 = ok). */
static inline int sysl_heap_travar(SysTrava *t)
{
    if (rt_multithread)
        sys_trava_pegar_srw(t);
    else
        *(volatile uint32_t *)t = 1u;
    return 0;
}

static inline void sysl_heap_soltar(SysTrava *t)
{
    if (rt_multithread)
        sys_trava_soltar_srw(t);
    else
        *(volatile uint32_t *)t = 0u;
}

/* O dlmalloc trata _WIN32/WIN32 como o ambiente do windows.h; aqui e so mmap por syscall. As macros voltam ao que eram depois do include. */
#pragma push_macro("_WIN32")
#pragma push_macro("_WIN64")
#pragma push_macro("WIN32")
#pragma push_macro("WIN64")
#undef _WIN32
#undef _WIN64
#undef WIN32
#undef WIN64

#define MAP_ANONYMOUS 0x20 /* so para o dlmalloc nao declarar dev_zero_fd; o MMAP() abaixo e o nosso */
#define EINVAL 22
#define ENOMEM 12
#define HAVE_MMAP 1
#define HAVE_MORECORE 0
#define HAVE_MREMAP 1
#define USE_LOCKS 2
#define MLOCK_T SysTrava
#define INITIAL_LOCK(lk) ((lk)->srw = NULL, 0)
#define DESTROY_LOCK(lk) (0)
#define ACQUIRE_LOCK(lk) sysl_heap_travar(lk)
#define RELEASE_LOCK(lk) sysl_heap_soltar(lk)
#define TRY_LOCK(lk) (sysl_heap_travar(lk) == 0)
#define USE_DL_PREFIX 1
#define NO_MALLOC_STATS 1
#define NO_MALLINFO 1
#define MMAP_CLEARS 1
#define FOOTERS 0
#define INSECURE 0
#define ONLY_MSPACES 0
#define DLMALLOC_EXPORT static
#define LACKS_SYS_TYPES_H
#define LACKS_ERRNO_H
#define LACKS_TIME_H
#define LACKS_STDLIB_H
#define LACKS_STRING_H
#define LACKS_STRINGS_H
#define LACKS_UNISTD_H
#define LACKS_SYS_MMAN_H
#define LACKS_FCNTL_H
#define LACKS_SYS_PARAM_H
#define LACKS_SCHED_H
#define MALLOC_FAILURE_ACTION
#define ABORT sysl_heap_abortar()
#define malloc_getpagesize ((size_t)4096U)
#define MMAP(s) sysl_heap_mapear(s)
#define DIRECT_MMAP(s) sysl_heap_mapear(s)
#define MUNMAP(a, s) sysl_heap_desmapear((a), (s))
#define MREMAP(a, o, n, mv) sysl_heap_remapear((a), (o), (n), (mv))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../../third/dlmalloc/malloc.c"
/* NO_MALLOC_STATS tira o corpo de dlmalloc_stats mas o malloc.c ainda o declara; sendo 'static' sem definicao o gcc recusa. */
static void dlmalloc_stats(void)
{
}
#pragma GCC diagnostic pop

#pragma pop_macro("WIN64")
#pragma pop_macro("WIN32")
#pragma pop_macro("_WIN64")
#pragma pop_macro("_WIN32")

void *sys_heap_alocar(int64_t n)
{
    return dlmalloc((size_t)n);
}

void *sys_heap_realocar(void *p, int64_t n)
{
    return dlrealloc(p, (size_t)n);
}

void sys_heap_liberar(void *p)
{
    dlfree(p);
}
