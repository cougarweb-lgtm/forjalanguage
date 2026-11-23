/* base.c - falha interna e relogio.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M4 P24d: RelogioNs no Windows usa QueryPerformanceCounter direto. O clock_gettime do mingw custava ~190 ns por leitura
 *                    (__clock_gettime = 3,2% do tempo amostrado no perfil do corpus_200000: 3 leituras por funcao do X64Gerar + etapas).
 *            0.1.0 - Fase 5a.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

_Noreturn void FalhaInterna(const char *fmt, ...)
{
    va_list ap;
    fputs("forja: erro interno: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(3);
}

#ifdef _WIN32
/* Segundos inteiros e resto separados: ticks * 1e9 direto estouraria 64 bits depois de ~30 min de uptime a 10 MHz. */
uint64_t RelogioNs(void)
{
    LARGE_INTEGER ticks, freq;

    QueryPerformanceCounter(&ticks);
    QueryPerformanceFrequency(&freq);
    uint64_t t = (uint64_t)ticks.QuadPart, f = (uint64_t)freq.QuadPart;
    return (t / f) * 1000000000ull + (t % f) * 1000000000ull / f;
}
#else
uint64_t RelogioNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
#endif
