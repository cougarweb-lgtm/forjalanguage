/*
 * rt_thread_linux.c - threads do std:thread no Linux x86-64 (M5c-b P58): pthread_create e pthread_join do musl (D-R2, K4). Unidade propria, como rt_thread_win.c (D-R8).
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M5c-b P63 (fecho): pilha da filha de 1 MiB (pthread_attr_setstacksize; o padrao do musl, ~128 KiB, estourava em recursao que passa no Windows).
 *   0.1.0 - M5c-b P58: forja_thread_criar (pthread_create; liga rt_multithread ANTES; a filha herda o MXCSR da criadora por stmxcsr/ldmxcsr), forja_thread_esperar (pthread_join,
 *           sem limite) e forja_thread_fechar (nada: o musl libera a thread no join e no Linux nao ha handle a fechar). A entrada da filha e um start routine SysV que carrega
 *           o MXCSR e chama a rotina Forja (fjt_ SysV, P57). Panic na filha = relatorio unico e exit 101 (codigo comum, rt_fio.c / rt_fio_linux.c).
 *
 * Exige a variante libc (NativoUsaThreadsNoElf: o main do musl e o crt1.o da semente). Os prototipos do musl sao a mao (sysv_abi), como em rt_linux_entrada_libc.c.
 * Compilar: as flags do rt_linux_* (-DRT_ALVO_LINUX, -fno-asynchronous-unwind-tables -fno-unwind-tables): gcc -c rt_thread_linux.c -o rt_linux_thread.o
 * Verificacao: construir.bat e python -I tests\nativo\linux\m5c\verificar_m5c_linux.py --forja <forja.exe>.
 */
#include <stddef.h>
#include <stdint.h>

#include "rt_compat.h"
#include "rt_sys.h"

#ifndef RT_ALVO_LINUX
#error "rt_thread_linux.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

typedef uint32_t (RT_ABI_SYSV *RtRotinaFn)(void *);
typedef void *(RT_ABI_SYSV *RtInicioFn)(void *);

/* Simbolos do musl (libc.a): ABI System V. pthread_t e um ponteiro no musl: o handle vai como void *. */
extern RT_ABI_SYSV int pthread_create(void **thread, const void *atributos, RtInicioFn inicio, void *arg);
extern RT_ABI_SYSV int pthread_join(void *thread, void **retorno);

/* pthread_attr_t do musl x86_64 = union de 14 int = 56 bytes (alinhada em 8): reservado a mao, sem incluir pthread.h. A pilha da filha e de 1 MiB como no Windows (o padrao do musl, ~128 KiB, estoura
 * onde o Windows nao estoura). */
struct RtPthreadAtributos {
    uint64_t reservado[7];
};
#define RT_PILHA_THREAD_BYTES (1024UL * 1024UL)
extern RT_ABI_SYSV int pthread_attr_init(struct RtPthreadAtributos *atributos);
extern RT_ABI_SYSV int pthread_attr_setstacksize(struct RtPthreadAtributos *atributos, unsigned long bytes);
extern RT_ABI_SYSV int pthread_attr_destroy(struct RtPthreadAtributos *atributos);

/* Pacote da partida: vive no heap do runtime ate a filha le-lo (a filha o libera). */
struct RtThreadPacote {
    RtRotinaFn rotina;
    void *arg;
    uint32_t mxcsr;
};

/* Entrada da filha (SysV, chamada pelo pthread): carrega o MXCSR da criadora antes da rotina Forja (FTZ/DAZ da principal valem na filha). */
static RT_ABI_SYSV void *RtThreadEntrada(void *p)
{
    struct RtThreadPacote *k = (struct RtThreadPacote *)p;
    RtRotinaFn rotina = k->rotina;
    void *arg = k->arg;
    uint32_t mx = k->mxcsr;

    sys_heap_liberar(k);
    __builtin_ia32_ldmxcsr(mx);
    rotina(arg);
    return NULL;
}

/* Cria a thread que roda rotina(arg). Devolve o handle (0 = sem recursos). Liga rt_multithread ANTES do pthread_create (sem isso a trava do heap nao vale). */
uint64_t forja_thread_criar(RtRotinaFn rotina, void *arg)
{
    struct RtThreadPacote *k;
    struct RtPthreadAtributos atributos;
    void *thread = NULL;
    int falhou;

    sys_multithread_ligar();
    k = (struct RtThreadPacote *)sys_heap_alocar((int64_t)sizeof *k);
    if (k == NULL)
        return 0;
    k->rotina = rotina;
    k->arg = arg;
    k->mxcsr = __builtin_ia32_stmxcsr();
    falhou = pthread_attr_init(&atributos) != 0;
    if (!falhou) {
        falhou = pthread_attr_setstacksize(&atributos, RT_PILHA_THREAD_BYTES) != 0 || pthread_create(&thread, &atributos, RtThreadEntrada, k) != 0;
        pthread_attr_destroy(&atributos);
    }
    if (falhou) {
        sys_heap_liberar(k);
        return 0;
    }
    return (uint64_t)(uintptr_t)thread;
}

/* Espera a thread terminar. 0 = terminou; 1 = a espera falhou. */
int32_t forja_thread_esperar(uint64_t h)
{
    return pthread_join((void *)(uintptr_t)h, NULL) == 0 ? 0 : 1;
}

/* Sem efeito: o pthread_join ja liberou a thread; nao ha handle a fechar no Linux (o Windows precisa do CloseHandle). */
void forja_thread_fechar(uint64_t h)
{
    (void)h;
}
