/*
 * rt_sync_win.c - Mutex e Cond do std:sync no Windows x64 (M5 P49). Unidade propria (D-R8): nao muda rt_win_dev.o, rt_win_rel.o, rt_win_ryu.o nem rt_win_es.o.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P49: forja_sync_* sobre SRWLOCK (AcquireSRWLockExclusive, ReleaseSRWLockExclusive, TryAcquireSRWLockExclusive) e
 *           CONDITION_VARIABLE (SleepConditionVariableSRW com flag 0, WakeConditionVariable, WakeAllConditionVariable). Sem funcao de criacao:
 *           o valor zero da palavra de 8 bytes e o SRWLOCK_INIT e o CONDITION_VARIABLE_INIT.
 *
 * Os prototipos sao a mao, sem windows.h (como rt_thread_win.c). Objeto so entra na ligacao quando o programa referencia forja_sync_* (RuntimeUsaRyu).
 * Verificacao: construir.bat e tests\nativo\m5c\sync (verificar_sync_p49.py).
 */
#include <stdint.h>
#include <stddef.h>

__declspec(dllimport) void AcquireSRWLockExclusive(void *srw);
__declspec(dllimport) void ReleaseSRWLockExclusive(void *srw);
__declspec(dllimport) uint8_t TryAcquireSRWLockExclusive(void *srw);
__declspec(dllimport) int SleepConditionVariableSRW(void *cv, void *srw, uint32_t ms, uint32_t flags);
__declspec(dllimport) void WakeConditionVariable(void *cv);
__declspec(dllimport) void WakeAllConditionVariable(void *cv);

/* Trava exclusiva do Mutex. */
void forja_sync_travar(void *m)
{
    AcquireSRWLockExclusive(m);
}

/* Solta a trava exclusiva do Mutex. */
void forja_sync_soltar(void *m)
{
    ReleaseSRWLockExclusive(m);
}

/* 1 = travou; 0 = outra thread tem o Mutex. */
int32_t forja_sync_tentar(void *m)
{
    return TryAcquireSRWLockExclusive(m) != 0;
}

/* Espera o sinal sem limite (INFINITE = 0xFFFFFFFF). Solta m enquanto espera e volta com m travado. */
void forja_sync_esperar(void *c, void *m)
{
    SleepConditionVariableSRW(c, m, 0xFFFFFFFFu, 0);
}

/* 1 = acordou com sinal; 0 = expirou (ou falha) sem sinal. */
int32_t forja_sync_esperar_ms(void *c, void *m, uint32_t ms)
{
    return SleepConditionVariableSRW(c, m, ms, 0) != 0;
}

/* Acorda uma thread esperando na Cond. */
void forja_sync_sinalizar(void *c)
{
    WakeConditionVariable(c);
}

/* Acorda todas as threads esperando na Cond. */
void forja_sync_difundir(void *c)
{
    WakeAllConditionVariable(c);
}
