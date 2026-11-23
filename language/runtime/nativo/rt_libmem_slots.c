/*
 * rt_libmem_slots.c - involucros de memoria por biblioteca estatica em DEV (M6d P92c, D-M6d-5): forja_libmem_<fn>_<k> para k = 0..15 e <fn> = malloc, calloc, realloc, free, strdup, _strdup.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M6d P92d: cada involucro passa a contar de verdade (rt_libmem_malloc/calloc/realloc/free/strdup de rt_libmem.c: registra e baixa na tabela lateral do slot k). As declaracoes dos
 *           reais mudaram para rt_libmem.c (que os chama para guardar a tabela).
 *   0.1.0 - M6d P92c: primeira versao. So REPASSA ao malloc/calloc/realloc/free/strdup real: o ligador (libmem.c) redireciona para eles as chamadas do codigo de cada lib estatica contada.
 *
 * ABI: o chamador e o C da lib no ABI do alvo. Windows: Win64 (o padrao do runtime), os reais vem da ucrtbase por dllimport (so na variante CRT, como o fwrite de rt_saida.c; strdup chama o
 * _strdup real). Linux: System V (RT_ABI_SYSV) nas duas pontas, os reais sao os do musl que a uniao de terceiros traz. Cada involucro mora na secao propria (-ffunction-sections): so entra no
 * exe quando o ligador o cita. Fora da variante CRT (Windows) e fora do Linux o arquivo nao gera nada.
 *
 * Incluido no FIM de rt_rastreio.c (so FORJA_DEV), depois de rt_libmem.c; nao compila sozinho.
 */
#ifndef RT_RASTREIO_C
#error "rt_libmem_slots.c so existe incluido por rt_rastreio.c"
#endif

#if defined(RT_ALVO_LINUX) || defined(FORJA_RT_CRT)

#ifdef RT_ALVO_LINUX
#define RT_LIBMEM_ABI RT_ABI_SYSV
#else
#define RT_LIBMEM_ABI
#endif

#define RT_LIBMEM_FAMILIA(k)                                                                                            \
    RT_LIBMEM_ABI void *forja_libmem_malloc_##k(size_t n) { return rt_libmem_malloc(k, n); }                            \
    RT_LIBMEM_ABI void *forja_libmem_calloc_##k(size_t n, size_t m) { return rt_libmem_calloc(k, n, m); }               \
    RT_LIBMEM_ABI void *forja_libmem_realloc_##k(void *p, size_t n) { return rt_libmem_realloc(k, p, n); }              \
    RT_LIBMEM_ABI void forja_libmem_free_##k(void *p) { rt_libmem_free(p); }                                            \
    RT_LIBMEM_ABI char *forja_libmem_strdup_##k(const char *s) { return rt_libmem_strdup(k, s); }                       \
    RT_LIBMEM_ABI char *forja_libmem__strdup_##k(const char *s) { return rt_libmem_strdup(k, s); }

RT_LIBMEM_FAMILIA(0)
RT_LIBMEM_FAMILIA(1)
RT_LIBMEM_FAMILIA(2)
RT_LIBMEM_FAMILIA(3)
RT_LIBMEM_FAMILIA(4)
RT_LIBMEM_FAMILIA(5)
RT_LIBMEM_FAMILIA(6)
RT_LIBMEM_FAMILIA(7)
RT_LIBMEM_FAMILIA(8)
RT_LIBMEM_FAMILIA(9)
RT_LIBMEM_FAMILIA(10)
RT_LIBMEM_FAMILIA(11)
RT_LIBMEM_FAMILIA(12)
RT_LIBMEM_FAMILIA(13)
RT_LIBMEM_FAMILIA(14)
RT_LIBMEM_FAMILIA(15)

#endif
