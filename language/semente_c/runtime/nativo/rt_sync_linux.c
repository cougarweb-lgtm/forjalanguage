/*
 * rt_sync_linux.c - Mutex e Cond do std:sync no Linux x86-64 (M5c-b P58): futex proprio (syscall 202, sempre FUTEX_PRIVATE), sem musl (D-R9). Unidade propria (D-R8).
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5c-b P58: Mutex = futex de 3 estados (0 livre, 1 tomado, 2 tomado com espera) na 1a palavra de 4 B do registro de 8 B (valor zero valido, como o SRWLOCK_INIT).
 *           Cond = contador de sequencia na 1a palavra (zero valido, como o CONDITION_VARIABLE_INIT): wait le a sequencia, solta o Mutex, espera o futex enquanto a sequencia
 *           for a mesma e volta com o Mutex travado; signal e broadcast somam 1 e acordam 1 ou todas. wait_ms repete a espera ate o prazo (espera falsa nao conta como expirou)
 *           e devolve 1 se a sequencia mudou (sinal), 0 se expirou. Tempo relativo do futex; o prazo vem de sys_tempo_ns (relogio monotonico do runtime).
 *
 * Sem libc: so syscall 202 por rt_syscall_linux.h. Objeto so entra na ligacao quando o programa referencia forja_sync_* (RuntimeUsaRyu).
 * Compilar: as flags do rt_linux_* (-DRT_ALVO_LINUX, -fno-asynchronous-unwind-tables -fno-unwind-tables): gcc -c rt_sync_linux.c -o rt_linux_sync.o
 * Verificacao: construir.bat e python -I tests\nativo\linux\m5c\verificar_m5c_linux.py --forja <forja.exe>.
 */
#include <stddef.h>
#include <stdint.h>

#include "rt_sys.h"
#include "rt_syscall_linux.h"

#ifndef RT_ALVO_LINUX
#error "rt_sync_linux.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#define SYNC_FUTEX_ESPERAR_PRIVADO 128LL /* FUTEX_WAIT | FUTEX_PRIVATE_FLAG */
#define SYNC_FUTEX_ACORDAR_PRIVADO 129LL /* FUTEX_WAKE | FUTEX_PRIVATE_FLAG */
#define SYNC_ACORDAR_TODAS 0x7fffffffLL
#define SYNC_NS_POR_S 1000000000LL
#define SYNC_NS_POR_MS 1000000LL

/* Estados do Mutex (a palavra de 4 B do registro). */
#define SYNC_LIVRE 0u
#define SYNC_TOMADO 1u
#define SYNC_TOMADO_COM_ESPERA 2u

/* Espera o futex enquanto a palavra for 'visto'. tempo_ns < 0 = sem limite (timeout NULL); senao, tempo relativo em ns. */
static void SyncEsperarFutex(volatile uint32_t *palavra, uint32_t visto, int64_t tempo_ns)
{
    int64_t ts[2] = {tempo_ns / SYNC_NS_POR_S, tempo_ns % SYNC_NS_POR_S};
    int64_t v[7] = {SYSL_FUTEX, (int64_t)palavra, SYNC_FUTEX_ESPERAR_PRIVADO, (int64_t)visto, 0, 0, 0};

    if (tempo_ns >= 0)
        v[4] = (int64_t)ts;
    sysl_chamar6(v);
}

static void SyncAcordarFutex(volatile uint32_t *palavra, int64_t quantas)
{
    sysl_chamar3(SYSL_FUTEX, (int64_t)palavra, SYNC_FUTEX_ACORDAR_PRIVADO, quantas);
}

static void SyncTravar(volatile uint32_t *palavra)
{
    uint32_t visto = __sync_val_compare_and_swap(palavra, SYNC_LIVRE, SYNC_TOMADO);

    if (visto == SYNC_LIVRE)
        return;
    if (visto != SYNC_TOMADO_COM_ESPERA)
        visto = __atomic_exchange_n(palavra, SYNC_TOMADO_COM_ESPERA, __ATOMIC_ACQUIRE);
    while (visto != SYNC_LIVRE) {
        SyncEsperarFutex(palavra, SYNC_TOMADO_COM_ESPERA, -1);
        visto = __atomic_exchange_n(palavra, SYNC_TOMADO_COM_ESPERA, __ATOMIC_ACQUIRE);
    }
}

static void SyncSoltar(volatile uint32_t *palavra)
{
    if (__atomic_exchange_n(palavra, SYNC_LIVRE, __ATOMIC_RELEASE) == SYNC_TOMADO_COM_ESPERA)
        SyncAcordarFutex(palavra, 1);
}

/* Trava exclusiva do Mutex. */
void forja_sync_travar(void *m)
{
    SyncTravar((volatile uint32_t *)m);
}

/* Solta a trava exclusiva do Mutex (acorda 1 espera se houver). */
void forja_sync_soltar(void *m)
{
    SyncSoltar((volatile uint32_t *)m);
}

/* 1 = travou; 0 = outra thread tem o Mutex. */
int32_t forja_sync_tentar(void *m)
{
    return __sync_val_compare_and_swap((volatile uint32_t *)m, SYNC_LIVRE, SYNC_TOMADO) == SYNC_LIVRE;
}

/* Espera o sinal sem limite. Solta m enquanto espera e volta com m travado. */
void forja_sync_esperar(void *c, void *m)
{
    volatile uint32_t *seq = (volatile uint32_t *)c;
    uint32_t visto = __atomic_load_n(seq, __ATOMIC_ACQUIRE);

    SyncSoltar((volatile uint32_t *)m);
    SyncEsperarFutex(seq, visto, -1);
    SyncTravar((volatile uint32_t *)m);
}

/* Espera o sinal ate ms milissegundos. 1 = acordou com sinal; 0 = expirou. Solta m enquanto espera e volta com m travado. */
int32_t forja_sync_esperar_ms(void *c, void *m, uint32_t ms)
{
    volatile uint32_t *seq = (volatile uint32_t *)c;
    uint32_t visto = __atomic_load_n(seq, __ATOMIC_ACQUIRE);
    int64_t fim = sys_tempo_ns() + (int64_t)ms * SYNC_NS_POR_MS;
    int64_t restante;

    SyncSoltar((volatile uint32_t *)m);
    while (__atomic_load_n(seq, __ATOMIC_ACQUIRE) == visto) {
        restante = fim - sys_tempo_ns();
        if (restante <= 0)
            break;
        SyncEsperarFutex(seq, visto, restante);
    }
    SyncTravar((volatile uint32_t *)m);
    return __atomic_load_n(seq, __ATOMIC_ACQUIRE) != visto;
}

/* Acorda uma thread esperando na Cond (a sequencia sobe 1: quem esperava com a sequencia antiga ve a mudanca). */
void forja_sync_sinalizar(void *c)
{
    volatile uint32_t *seq = (volatile uint32_t *)c;

    __atomic_fetch_add(seq, 1u, __ATOMIC_SEQ_CST);
    SyncAcordarFutex(seq, 1);
}

/* Acorda todas as threads esperando na Cond. */
void forja_sync_difundir(void *c)
{
    volatile uint32_t *seq = (volatile uint32_t *)c;

    __atomic_fetch_add(seq, 1u, __ATOMIC_SEQ_CST);
    SyncAcordarFutex(seq, SYNC_ACORDAR_TODAS);
}
