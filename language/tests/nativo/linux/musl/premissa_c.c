/* premissa_c.c - P32(c) do contrato_m5_resto: crt1.o + main SysV + funcao ms_abi + pthread_create de 6 threads + libc.a.
 * Objetivo: provar que o musl e o UNICO dono do processo (TLS e errno por thread) mesmo com main SysV
 * chamando funcao ms_abi (D-R2, D-R5). Sem runtime da Forja: so gcc avulso e ld.
 * Uso: ver premissa_c.sh (monta e roda no WSL alpine-build).
 * Versao: 0.1.0
 * Changelog: 0.1.0 (2026-11-14) primeira versao.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

#define N_THREADS 6
#define N_ITER 20000

typedef struct {
    int id;
    long acc;
    int bad;
} Arg;

/* convencao Win64 (ms_abi), a mesma que a Forja usa internamente (D-M5-2) */
__attribute__((ms_abi)) static long somar_ms(long a, long b)
{
    return a + b;
}

static void *trabalhar(void *p)
{
    Arg *a = (Arg *)p;
    int esperado = 1000 + a->id;
    for (int i = 0; i < N_ITER; i++) {
        a->acc = somar_ms(a->acc, 1);
        errno = esperado;
        sched_yield();
        if (errno != esperado) {
            a->bad++;
        }
    }
    return NULL;
}

int main(void)
{
    pthread_t th[N_THREADS];
    Arg arg[N_THREADS];
    long total = 0;
    int bad = 0;

    for (int i = 0; i < N_THREADS; i++) {
        arg[i].id = i;
        arg[i].acc = 0;
        arg[i].bad = 0;
        if (pthread_create(&th[i], NULL, trabalhar, &arg[i]) != 0) {
            printf("FALHOU pthread_create %d\n", i);
            return 2;
        }
    }
    for (int i = 0; i < N_THREADS; i++) {
        pthread_join(th[i], NULL);
        total += arg[i].acc;
        bad += arg[i].bad;
    }
    if (total != (long)N_THREADS * N_ITER || bad != 0) {
        printf("FALHOU soma=%ld esperado=%ld errno_trocado=%d\n", total, (long)N_THREADS * N_ITER, bad);
        return 1;
    }
    printf("PASSOU premissa c: %d threads, soma=%ld, errno por thread sem troca\n", N_THREADS, total);
    return 0;
}
