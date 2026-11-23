/*
 * rt_varena_win.c - VirtualArena do std:mem no Windows x64 (M5 P52). Unidade propria (D-R8): nao muda rt_win_dev.o, rt_win_rel.o, rt_win_ryu.o nem rt_win_es.o.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5 P52: forja_varena_reservar (VirtualAlloc MEM_RESERVE, PAGE_NOACCESS: espaco de enderecos sem commit), forja_varena_commit (VirtualAlloc
 *           MEM_COMMIT, PAGE_READWRITE), forja_varena_liberar (VirtualFree MEM_RELEASE, tamanho 0 = regiao inteira) e forja_varena_ponteiro (endereco
 *           inteiro para rawptr). So as primitivas: o bump pointer e o controle de commit moram em std\mem.fj (Forja pura, como o P51).
 *
 * Os prototipos sao a mao, sem windows.h (como rt_sync_win.c). Objeto so entra na ligacao quando o programa referencia forja_varena_* (RuntimeUsaRyu).
 * Verificacao: construir.bat e python -I tests\nativo\m5c\varena\verificar_varena_p52.py <forja.exe>.
 */
#include <stdint.h>

#define VARENA_MEM_RESERVE 0x00002000u
#define VARENA_MEM_COMMIT 0x00001000u
#define VARENA_MEM_RELEASE 0x00008000u
#define VARENA_PAGINA_SEM_ACESSO 0x00000001u
#define VARENA_PAGINA_LEITURA_ESCRITA 0x00000004u

__declspec(dllimport) void *VirtualAlloc(void *endereco, uint64_t n, unsigned long tipo, unsigned long protecao);
__declspec(dllimport) int VirtualFree(void *endereco, uint64_t n, unsigned long tipo);

/* Reserva n bytes de espaco de enderecos, sem commit. 0 = o sistema recusou (o chamador devolve sem_reserva). */
int64_t forja_varena_reservar(int64_t n)
{
    return (int64_t)(uintptr_t)VirtualAlloc(0, (uint64_t)n, VARENA_MEM_RESERVE, VARENA_PAGINA_SEM_ACESSO);
}

/* Commit de [endereco, endereco + n) em leitura e escrita. 1 = ok; 0 = o sistema recusou (sem memoria). */
int32_t forja_varena_commit(int64_t endereco, int64_t n)
{
    return VirtualAlloc((void *)(uintptr_t)endereco, (uint64_t)n, VARENA_MEM_COMMIT, VARENA_PAGINA_LEITURA_ESCRITA) != 0;
}

/* Devolve a regiao reservada que comeca em endereco. MEM_RELEASE exige tamanho 0: libera a regiao inteira. */
void forja_varena_liberar(int64_t endereco)
{
    (void)VirtualFree((void *)(uintptr_t)endereco, 0, VARENA_MEM_RELEASE);
}

/* O rawptr da Forja nao faz aritmetica: o bump pointer soma o deslocamento em inteiro e converte aqui. */
void *forja_varena_ponteiro(int64_t endereco)
{
    return (void *)(uintptr_t)endereco;
}
