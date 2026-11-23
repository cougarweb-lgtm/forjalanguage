/*
 * rt_linux_entrada_libc.c - objeto de entrada do ELF quando o programa tem foreign (M5b P39-B2, D-R3): o main que o __libc_start_main do musl chama.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - M5c-b P57 (D-R7): o RtFio por thread. LigarFioPorThread cria a pthread_key do musl (destrutor = forja_fio_liberar), liga os ganchos rt_fio_obter/rt_fio_gravar do runtime e grava o
 *           fio principal nela, antes de sys_entrada. A thread de fora ganha o fio em forja_fio_da_ponte e o musl o devolve no fim dela. Simbolos novos do musl: pthread_key_create, pthread_getspecific, pthread_setspecific.
 *   0.2.0 - M5b P39-B3: os atributos do gcc saem daqui para rt_compat.h (RT_ABI_SYSV, RT_NORETORNO, RT_USADO), como manda o conferir_regras.
 *   0.1.0 - M5b P39-B2 (D0654 itens 3..5): main sysv_abi chama sys_entrada((argv - 1)): o musl passa argv = p + 1 da pilha do kernel, entao argv[-1] e o argc e
 *           a pilha inicial segue igual ao _start cru. sys_sair = exit() do musl (descarrega o stdio das libs). stdout pelo FILE stdout do musl, pelos ganchos
 *           rt_saida_libc_* de rt_saida.c: print da Forja e printf do C saem na ordem.
 *
 * O crt1.o do musl (semente em third\musl\x86_64) traz o _start; este objeto NAO traz _start (o do cru nao entra junto). Compilar como os outros rt_linux_*:
 *   gcc -std=c11 -O2 -ffreestanding -nostdlib -DRT_ALVO_LINUX -fno-asynchronous-unwind-tables -fno-unwind-tables -c rt_linux_entrada_libc.c -o rt_linux_entrada_libc.o
 * Saida esperada: main e sys_sair (globais); referencias a exit, fwrite, fflush, stdout (musl), sys_entrada e rt_saida_libc_* (o runtime comum).
 */
#ifndef RT_ALVO_LINUX
#error "rt_linux_entrada_libc.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#include <stddef.h>
#include <stdint.h>

#include "rt_compat.h"

typedef void (RT_ABI_SYSV *RtSaidaEscreveFn)(const void *p, int64_t n);
typedef void (RT_ABI_SYSV *RtSaidaDescarregaFn)(void);

/* Ganchos de rt_saida.c (runtime comum, Win64 por dentro; os ponteiros sao SysV). */
extern RtSaidaEscreveFn rt_saida_libc_escrever;
extern RtSaidaDescarregaFn rt_saida_libc_descarregar;
extern _Noreturn void sys_entrada(const int64_t *pilha);

/* Simbolos do musl (libc.a): ABI System V. stdout e o FILE * do musl. */
extern RT_ABI_SYSV size_t fwrite(const void *p, size_t tam, size_t n, void *arquivo);
extern RT_ABI_SYSV int fflush(void *arquivo);
extern RT_ABI_SYSV RT_NORETORNO void exit(int codigo);
extern void *stdout;

static RT_ABI_SYSV void EscreverPeloLibc(const void *p, int64_t n)
{
    if (n > 0)
        fwrite(p, 1, (size_t)n, stdout);
}

static RT_ABI_SYSV void DescarregarPeloLibc(void)
{
    fflush(stdout);
}

/* ---- o RtFio por thread (M5c-b P57, D-R7): uma pthread_key do musl; o fio principal e gravado no main, o das threads de fora na ponte (forja_fio_da_ponte) ---- */
typedef struct RtFio RtFio;
typedef RtFio *(*RtFioObterFn)(void);
typedef void (*RtFioGravarFn)(RtFio *fio);

extern RtFioObterFn rt_fio_obter;
extern RtFioGravarFn rt_fio_gravar;
extern RtFio *forja_fio_principal(void);
extern void forja_fio_liberar(RtFio *fio);

extern RT_ABI_SYSV int pthread_key_create(unsigned *chave, void (RT_ABI_SYSV *destrutor)(void *));
extern RT_ABI_SYSV void *pthread_getspecific(unsigned chave);
extern RT_ABI_SYSV int pthread_setspecific(unsigned chave, const void *valor);

static unsigned rt_fio_chave; /* compartilhado: criada 1x no main, antes de qualquer thread; so lida depois */

static RtFio *ObterFioDaThread(void)
{
    return (RtFio *)pthread_getspecific(rt_fio_chave);
}

static void GravarFioDaThread(RtFio *fio)
{
    pthread_setspecific(rt_fio_chave, fio);
}

/* O musl chama o destrutor (System V) no fim da thread, com o valor da chave. */
static RT_ABI_SYSV void DestruirFioDaThread(void *fio)
{
    forja_fio_liberar((RtFio *)fio);
}

/* Cria a chave e grava o fio principal nela. Sem chave (musl sem espaco) nao ha como seguir: o fio fica o principal global e a ponte nao cria fio (obter NULL). */
static void LigarFioPorThread(void)
{
    if (pthread_key_create(&rt_fio_chave, DestruirFioDaThread) != 0)
        return;
    rt_fio_obter = ObterFioDaThread;
    rt_fio_gravar = GravarFioDaThread;
    pthread_setspecific(rt_fio_chave, forja_fio_principal());
}

/* O gcc do MinGW poe uma chamada a __main no inicio de qualquer funcao chamada main (o C do Windows): o ELF nao tem __main. Por isso o main
 * e um salto (asm abaixo) para esta funcao, que tem outro nome. */
RT_ABI_SYSV RT_USADO int forja_main_libc(int argc, char **argv, char **envp)
{
    (void)argc;
    (void)envp;
    rt_saida_libc_escrever = EscreverPeloLibc;
    rt_saida_libc_descarregar = DescarregarPeloLibc;
    LigarFioPorThread();
    sys_entrada((const int64_t *)argv - 1);
}

/* main: chamado pelo __libc_start_main do musl (System V: rdi argc, rsi argv, rdx envp). O salto mantem os registradores; argv[-1] e argc. */
__asm__(
    ".section .text$sys_main_libc,\"xr\"\n"
    ".globl main\n"
    ".def main; .scl 2; .type 32; .endef\n"
    "main:\n"
    "    jmp forja_main_libc\n"
    ".text\n");

/* Sai pelo exit() do musl: roda os atexit e o fflush do stdio das libs antes do exit_group. */
_Noreturn void sys_sair(int codigo)
{
    exit(codigo);
}
