/*
 * rt_thread_win.c - threads do std:thread no Windows x64 (M5 P47). Unidade propria (D-R8): nao muda rt_win_dev.o, rt_win_rel.o, rt_win_ryu.o nem rt_win_es.o.
 *
 * Versao: 0.1.1
 * Changelog:
 *   0.1.1 - M5 P47: MXCSR por __builtin_ia32_stmxcsr/ldmxcsr em vez de asm (conferir_regras: asm so em rt_salto.c e rt_legado.c).
 *   0.1.0 - M5 P47: forja_thread_criar (CreateThread; liga rt_multithread antes; a filha herda o MXCSR da criadora por stmxcsr/ldmxcsr),
 *           forja_thread_esperar (WaitForSingleObject, sem limite) e forja_thread_fechar (CloseHandle). A entrada da filha e o trampolim fjt_
 *           do proc da Forja (V_PROC), que cria o fio por forja_fio_da_ponte. Panic na filha = relatorio unico e exit 101 (rt_fio.c, P45).
 *
 * Os prototipos sao a mao, sem windows.h (como rt_sys_win.c). Objeto so entra na ligacao quando o programa referencia forja_thread_* (RuntimeUsaRyu).
 * Verificacao: construir.bat e tests\nativo\m5c\thread (verificar_thread_p47.py).
 */
#include <stdint.h>
#include <stddef.h>

void sys_multithread_ligar(void);

__declspec(dllimport) void *CreateThread(void *atributos, size_t pilha, uint32_t (*rotina)(void *), void *parametro, uint32_t flags, uint32_t *id);
__declspec(dllimport) uint32_t WaitForSingleObject(void *alvo, uint32_t ms);
__declspec(dllimport) int CloseHandle(void *alvo);
__declspec(dllimport) void *GetProcessHeap(void);
__declspec(dllimport) void *HeapAlloc(void *heap, uint32_t flags, size_t tamanho);
__declspec(dllimport) int HeapFree(void *heap, uint32_t flags, void *bloco);

/* Pacote da partida: vive no heap do processo ate a filha le-lo (a filha o libera). */
struct RtThreadPacote {
    uint32_t (*rotina)(void *);
    void *arg;
    uint32_t mxcsr;
};

/* Entrada da filha: carrega o MXCSR da criadora antes do proc da Forja (FTZ/DAZ da principal valem na filha). */
static uint32_t rt_thread_entrada(void *p)
{
    struct RtThreadPacote *k = (struct RtThreadPacote *)p;
    uint32_t (*rotina)(void *) = k->rotina;
    void *arg = k->arg;
    uint32_t mx = k->mxcsr;

    HeapFree(GetProcessHeap(), 0, k);
    __builtin_ia32_ldmxcsr(mx);
    return rotina(arg);
}

/* Cria a thread que roda rotina(arg). Devolve o handle (0 = sem recursos). Liga rt_multithread ANTES do CreateThread. */
uint64_t forja_thread_criar(uint32_t (*rotina)(void *), void *arg)
{
    struct RtThreadPacote *k;
    void *h;
    uint32_t mx;

    mx = __builtin_ia32_stmxcsr();
    sys_multithread_ligar();
    k = (struct RtThreadPacote *)HeapAlloc(GetProcessHeap(), 0, sizeof *k);
    if (k == NULL)
        return 0;
    k->rotina = rotina;
    k->arg = arg;
    k->mxcsr = mx;
    h = CreateThread(NULL, 0, rt_thread_entrada, k, 0, NULL);
    if (h == NULL) {
        HeapFree(GetProcessHeap(), 0, k);
        return 0;
    }
    return (uint64_t)(uintptr_t)h;
}

/* Espera a thread terminar. 0 = terminou; 1 = a espera falhou. */
int32_t forja_thread_esperar(uint64_t h)
{
    return WaitForSingleObject((void *)(uintptr_t)h, 0xFFFFFFFFu) == 0 ? 0 : 1;
}

/* Fecha o handle da thread (depois do esperar). */
void forja_thread_fechar(uint64_t h)
{
    CloseHandle((void *)(uintptr_t)h);
}
