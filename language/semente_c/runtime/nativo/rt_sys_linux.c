/*
 * rt_sys_linux.c - camada sys do runtime nativo no Linux x86-64: syscalls diretas, _start e sinais (M5 P26d).
 *
 * Versao: 0.8.0
 * Changelog:
 *   0.8.0 - M6e P93f: guarda de memoria de verdade: sys_guarda_reservar/commit/decommit (mmap PROT_NONE MAP_NORESERVE, mprotect, mmap MAP_FIXED por cima), sys_guarda_limite_mapas (ENOMEM = vm.max_map_count),
 *           sys_falha_ligar (siginfo/ucontext pelos offsets do censo c5) e instalacao unica dos sinais (sysl_sinais_garantir); o tratador aceita sys_excecao_ligar ausente.
 *   0.7.0 - M6e P93e: sys_falha_ligar e sys_guarda_* como stubs (a guarda de memoria no Linux e o P93f; a reserva devolve NULL e o alocador de guarda falha com R0007).
 *   0.6.0 - M5c-b P63 (D0657): sys_trava_pegar_srw e sys_trava_soltar_srw ganham RT_NOIPA (o gcc, na mesma unidade, adiantava a leitura de statics protegidos para antes do call da trava).
 *   0.5.0 - M5c-b P57: rt_multithread, sys_multithread_ligar e a trava por futex (sys_trava_pegar_srw/soltar_srw, syscall 202); o RtFio por thread mora em rt_fio.h e rt_fio_linux.c. Continuam proibidos: /proc, clone, arch_prctl e TLS por fs (a thread e do musl).
 *   0.4.0 - M5b P39-B2 (D0654 item 3): sys_sair sai daqui; cada objeto de entrada define o seu (cru: exit_group; libc: exit() do musl).
 *   0.3.0 - M5b P39: o _start sai para rt_linux_entrada_cru.c (objeto de entrada, D-R3); aqui fica so o restaurador do sinal.
 *   0.2.0 - M5 P28b-1: sysl_envp_vivo (ambiente mutavel de set_env/unset_env). Proibido agora: /proc, vDSO, statx, TLS, clone (getrandom e o auxv so com fallback, ver rt_os_linux.c).
 *   0.1.0 - M5 P26d: primeira versao. As 15 syscalls da lista fechada do D-M5-3 (rt_syscall_linux.h; aqui o rt_sigreturn, 15, no restaurador) por asm inline; _start (le argc/argv/envp da pilha inicial, alinha a pilha e entra em forja_entrada pela convencao Win64);
 *           escrita, console (ioctl TCGETS), saida, paginas (mmap/munmap), relogio (clock_gettime sem vDSO) e as excecoes de hardware por sinal (D-M5-5). Os argumentos/ambiente e o
 *           arquivo moram em rt_proc_linux.c e rt_arquivo_linux.c (mesma unidade, rt_linux.c); o heap (dlmalloc) vive no objeto comum rt_linux_comum.o (rt_comum_linux.c).
 *
 * O runtime inteiro e compilado pelo MESMO gcc MinGW do Windows (convencao Win64 por padrao, D-M5-2): so tres pontos falam System V ou o kernel e estao aqui: o _start (asm), o
 * tratador de sinal (RT_ABI_SYSV) e o restaurador do sinal (asm, rt_sigreturn). Proibido neste runtime: /proc, vDSO, statx, TLS, clone (getrandom e auxv so opcionais, com fallback).
 * Syscall devolve -errno em caso de falha (valores de -1 a -4095). Incluido por rt_linux.c; nao compila sozinho.
 */
#include <stddef.h>

#include "rt_compat.h"
#include "rt_sys.h"
#include "rt_syscall_linux.h"

#ifndef RT_ALVO_LINUX
#error "rt_sys_linux.c exige -DRT_ALVO_LINUX"
#endif

#define SYSL_TCGETS 0x5401LL
#define SYSL_CLOCK_MONOTONIC 1LL
#define SYSL_NS_POR_S 1000000000LL

/* Sinais e a forma da acao do kernel (struct kernel_sigaction do x86-64: tratador, flags, restaurador, mascara de 8 bytes). */
#define SYSL_SIGILL 4
#define SYSL_SIGBUS 7
#define SYSL_SIGFPE 8
#define SYSL_SIGSEGV 11
#define SYSL_SA_SIGINFO 0x00000004ULL
#define SYSL_SA_ONSTACK 0x08000000ULL
#define SYSL_SA_RESTORER 0x04000000ULL
#define SYSL_SA_NODEFER 0x40000000ULL
#define SYSL_SIG_UNBLOCK 1LL
#define SYSL_TAMANHO_MASCARA 8LL
#define SYSL_PILHA_ALTERNATIVA_BYTES (64 * 1024)
/* Codigos que o rt_teste.c entende (os do Windows): o sinal do Linux e traduzido para eles. */
#define SYSL_CODIGO_ACESSO 0xC0000005UL
#define SYSL_CODIGO_ILEGAL 0xC000001DUL
#define SYSL_CODIGO_DIVISAO 0xC0000094UL

typedef struct {
    void *tratador;
    uint64_t flags;
    void *restaurador;
    uint64_t mascara;
} SysAcaoSinal;

typedef struct {
    void *base;
    int32_t flags;
    int32_t reservado;
    uint64_t tamanho;
} SysPilhaAlternativa;

/* Pilha inicial do processo (argc, argv, envp), guardada por sys_entrada. */
static int64_t sysl_argc;
static char **sysl_argv;
static char **sysl_envp;
/* Ambiente VIVO (P28b-1): igual ao sysl_envp ate a 1a mutacao (set_env/unset_env), quando rt_proc_linux.c o troca por uma copia no heap; o sysl_envp nunca e sobrescrito. */
static char **sysl_envp_vivo;
static int (*sys_tratador_excecao)(unsigned long);
static int (*sysl_pre_falha)(const SysFalha *); /* compartilhado: gravado 1x pela 1a alocacao com guarda, sob rt_rastreio_trava (M6e P93f) */
static void sysl_falha_do_sinal(SysFalha *f, int sinal, const void *info, const void *contexto);

void sys_restaurador(void);
_Noreturn void forja_entrada(void);

/* ---- entrada e restaurador (as duas coisas que so existem em asm) ---- */

/* O restaurador e o que o kernel executa ao fim do tratador de sinal: rt_sigreturn (15). Na propria secao, para o ligador descartar o que ninguem alcanca.
 * O _start (que chama sys_entrada) saiu para rt_linux_entrada_cru.c (M5b P39, D-R3): o objeto de entrada do ELF sem libc. */
__asm__(
    ".section .text$sys_restaurador,\"xr\"\n"
    ".globl sys_restaurador\n"
    ".def sys_restaurador; .scl 2; .type 32; .endef\n"
    "sys_restaurador:\n"
    "    mov $15, %eax\n"
    "    syscall\n"
    "    ud2\n"
    ".text\n");

/* Chamada pelo _start com o ponteiro da pilha inicial. */
_Noreturn void sys_entrada(const int64_t *pilha)
{
    sysl_argc = pilha[0];
    sysl_argv = (char **)(pilha + 1);
    sysl_envp = sysl_argv + pilha[0] + 1;
    sysl_envp_vivo = sysl_envp;
    forja_entrada();
}

/* ---- escrita, console e saida ---- */

void sys_escrever(int fd, const void *buf, int64_t n)
{
    const unsigned char *p = (const unsigned char *)buf;

    /* Falha de write (fd fechado, pipe quebrado) para sem erro, como no Windows. EINTR repete. */
    while (n > 0) {
        int64_t r = sysl_chamar3(SYSL_WRITE, fd, (int64_t)p, n);

        if (r == SYSL_EINTR)
            continue;
        if (sysl_falhou(r) || r == 0)
            return;
        p += r;
        n -= r;
    }
}

bool sys_e_console(int fd)
{
    unsigned char termios[64]; /* struct termios do kernel: 36 bytes */

    return sysl_chamar3(SYSL_IOCTL, fd, SYSL_TCGETS, (int64_t)termios) == 0;
}

void sys_console_ligar_utf8(void)
{
}

void sys_console_restaurar(void)
{
}

/* sys_sair nao mora aqui (M5b P39-B2, D0654 item 3): o objeto de entrada define o seu (exit_group cru em rt_linux_entrada_cru.c; exit() do musl em rt_linux_entrada_libc.c). */

/* ---- paginas e relogio ---- */

void *sys_paginas_reservar(int64_t n)
{
    int64_t v[7] = {SYSL_MMAP, 0, n, SYSL_PROT_LER_ESCREVER, SYSL_MAP_ANONIMO_PRIVADO, -1, 0};
    int64_t r = sysl_chamar6(v);

    return sysl_falhou(r) ? NULL : (void *)r;
}

void sys_paginas_liberar(void *p, int64_t n)
{
    sysl_chamar2(SYSL_MUNMAP, (int64_t)p, n);
}

int64_t sys_tempo_ns(void)
{
    int64_t ts[2] = {0, 0}; /* struct timespec: segundos e nanossegundos */

    sysl_chamar2(SYSL_CLOCK_GETTIME, SYSL_CLOCK_MONOTONIC, (int64_t)ts);
    return ts[0] * SYSL_NS_POR_S + ts[1];
}

/* ---- trava do estado compartilhado entre threads (M5c-b P57, D-R3/D-R7) ----
 * Futex proprio de 3 estados (0 livre, 1 travado, 2 travado com espera), FUTEX_PRIVATE: so quem passa por aqui e o flag rt_multithread ligado; o programa de uma thread nao paga trava.
 * A palavra de 4 bytes e o inicio do SysTrava (zero = livre, igual ao SRWLOCK_INIT do Windows). */
#define SYSL_FUTEX_WAIT_PRIVADO 128LL /* FUTEX_WAIT | FUTEX_PRIVATE_FLAG */
#define SYSL_FUTEX_WAKE_PRIVADO 129LL /* FUTEX_WAKE | FUTEX_PRIVATE_FLAG */

volatile uint8_t rt_multithread; /* compartilhado: flag pegajoso, so vai de 0 para 1 (sys_multithread_ligar) */

void sys_multithread_ligar(void)
{
    rt_multithread = 1;
}

static void sysl_futex_esperar(volatile uint32_t *palavra, uint32_t visto)
{
    int64_t v[7] = {SYSL_FUTEX, (int64_t)palavra, SYSL_FUTEX_WAIT_PRIVADO, (int64_t)visto, 0, 0, 0}; /* r10 = timeout NULL: espera sem limite */

    sysl_chamar6(v);
}

static void sysl_futex_acordar(volatile uint32_t *palavra)
{
    sysl_chamar3(SYSL_FUTEX, (int64_t)palavra, SYSL_FUTEX_WAKE_PRIVADO, 1);
}

RT_NOIPA void sys_trava_pegar_srw(SysTrava *t)
{
    volatile uint32_t *palavra = (volatile uint32_t *)t;
    uint32_t visto = __sync_val_compare_and_swap(palavra, 0u, 1u);

    if (visto == 0)
        return;
    if (visto != 2)
        visto = __atomic_exchange_n(palavra, 2u, __ATOMIC_ACQUIRE);
    while (visto != 0) {
        sysl_futex_esperar(palavra, 2u);
        visto = __atomic_exchange_n(palavra, 2u, __ATOMIC_ACQUIRE);
    }
}

/* Soltar uma trava livre nao faz nada (a flag pode ter virado entre o pegar e o soltar de quem ja estava numa secao: ressalva do P44). */
RT_NOIPA void sys_trava_soltar_srw(SysTrava *t)
{
    volatile uint32_t *palavra = (volatile uint32_t *)t;

    if (__atomic_exchange_n(palavra, 0u, __ATOMIC_RELEASE) == 2)
        sysl_futex_acordar(palavra);
}

/* ---- excecoes de hardware por sinal (D-M5-5) ---- */

static int64_t sysl_acao_sinal(int sinal, const SysAcaoSinal *acao)
{
    int64_t v[7] = {SYSL_RT_SIGACTION, sinal, (int64_t)acao, 0, SYSL_TAMANHO_MASCARA, 0, 0};

    return sysl_chamar6(v);
}

static void sysl_desbloquear(int sinal)
{
    uint64_t mascara = 1ULL << (sinal - 1);
    int64_t v[7] = {SYSL_RT_SIGPROCMASK, SYSL_SIG_UNBLOCK, (int64_t)&mascara, 0, SYSL_TAMANHO_MASCARA, 0, 0};

    sysl_chamar6(v);
}

static unsigned long sysl_codigo_do_sinal(int sinal)
{
    if (sinal == SYSL_SIGSEGV || sinal == SYSL_SIGBUS)
        return SYSL_CODIGO_ACESSO;
    if (sinal == SYSL_SIGILL)
        return SYSL_CODIGO_ILEGAL;
    return sinal == SYSL_SIGFPE ? SYSL_CODIGO_DIVISAO : 0UL;
}

/* O kernel chama o tratador pela convencao System V (rdi = sinal). O tratador do runtime (Win64) trata e salta para o teste, ou devolve 0: nesse caso a acao volta ao padrao e
 * o retorno refaz a instrucao que falhou, que agora derruba o processo com o sinal (igual ao programa C do oraculo). */
static RT_ABI_SYSV void sysl_sinal_tratar(int sinal, void *info, void *contexto)
{
    sysl_desbloquear(sinal);
    if (sysl_pre_falha != NULL) {
        SysFalha f;

        sysl_falha_do_sinal(&f, sinal, info, contexto);
        sysl_pre_falha(&f); /* a guarda que reconhece a falha relata e sai; devolveu: nao e dela */
    }
    if (sys_tratador_excecao == NULL || sys_tratador_excecao(sysl_codigo_do_sinal(sinal)) == 0) {
        SysAcaoSinal padrao = {NULL, 0, NULL, 0};

        sysl_acao_sinal(sinal, &padrao);
    }
}

static void sysl_instalar_sinal(int sinal)
{
    SysAcaoSinal acao = {(void *)sysl_sinal_tratar, SYSL_SA_SIGINFO | SYSL_SA_ONSTACK | SYSL_SA_RESTORER | SYSL_SA_NODEFER, (void *)sys_restaurador, 0};

    sysl_acao_sinal(sinal, &acao);
}

/* Pilha alternativa de 64 KiB: o estouro da pilha principal tambem chega ao tratador. Sem memoria, segue sem ela. */
static void sysl_pilha_alternativa(void)
{
    SysPilhaAlternativa alt = {sys_paginas_reservar(SYSL_PILHA_ALTERNATIVA_BYTES), 0, 0, SYSL_PILHA_ALTERNATIVA_BYTES};

    if (alt.base != NULL)
        sysl_chamar2(SYSL_SIGALTSTACK, (int64_t)&alt, 0);
}

/* M6e P93f: a guarda de memoria no Linux. Primitivas sobre mmap/mprotect; o commit que falha com ENOMEM acusa o limite de mapeamentos (vm.max_map_count) em sys_guarda_limite_mapas. */
#define SYSL_PROT_NENHUM 0LL
#define SYSL_MAP_GUARDA_RESERVA 0x4022LL /* MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE */
#define SYSL_MAP_GUARDA_FIXO 0x4032LL    /* idem + MAP_FIXED (0x10): troca o mapeamento no lugar, o endereco continua nosso */
#define SYSL_ENOMEM (-12LL)

static volatile uint8_t sysl_limite_mapas; /* compartilhado: so vai de 0 para 1 (ENOMEM num commit da guarda) */

void *sys_guarda_reservar(int64_t n)
{
    int64_t v[7] = {SYSL_MMAP, 0, n, SYSL_PROT_NENHUM, SYSL_MAP_GUARDA_RESERVA, -1, 0};
    int64_t r = sysl_chamar6(v);

    return sysl_falhou(r) ? NULL : (void *)r;
}

bool sys_guarda_commit(void *p, int64_t n)
{
    int64_t r = sysl_chamar3(SYSL_MPROTECT, (int64_t)p, n, SYSL_PROT_LER_ESCREVER);

    if (r == SYSL_ENOMEM)
        sysl_limite_mapas = 1;
    return !sysl_falhou(r);
}

/* O mmap fixo por cima descarta as paginas e as deixa sem acesso; se o kernel recusar (limite de mapeamentos), o mprotect sem acesso ainda tira o direito de uso. */
void sys_guarda_decommit(void *p, int64_t n)
{
    int64_t v[7] = {SYSL_MMAP, (int64_t)p, n, SYSL_PROT_NENHUM, SYSL_MAP_GUARDA_FIXO, -1, 0};

    if (sysl_falhou(sysl_chamar6(v)))
        sysl_chamar3(SYSL_MPROTECT, (int64_t)p, n, SYSL_PROT_NENHUM);
}

bool sys_guarda_limite_mapas(void)
{
    return sysl_limite_mapas != 0;
}

/* Sinais de falha de memoria: o kernel entrega siginfo_t (si_addr em +16) e ucontext_t (gregs em +40; REG_RBP 10, REG_RSP 15, REG_RIP 16), offsets do censo c5 (musl x86-64, offsetof por
 * programa C). Os registradores sao os do CONTEXTO da thread que falhou, mesmo com o tratador rodando na pilha alternativa. */
#define SYSL_INFO_ENDERECO 16
#define SYSL_CTX_GREGS 40
#define SYSL_REG_RBP 10
#define SYSL_REG_RSP 15
#define SYSL_REG_RIP 16

static bool sysl_sinais_ligados; /* compartilhado: gravado sob rt_rastreio_trava; sys_excecao_ligar (forja test, 1 thread) pode chegar antes ou depois */

static uint64_t sysl_ler64(const void *base, int desloc)
{
    return *(const uint64_t *)((const unsigned char *)base + desloc);
}

static void sysl_falha_do_sinal(SysFalha *f, int sinal, const void *info, const void *contexto)
{
    const void *gregs = (const unsigned char *)contexto + SYSL_CTX_GREGS;

    f->codigo = sysl_codigo_do_sinal(sinal);
    f->endereco = sysl_ler64(info, SYSL_INFO_ENDERECO);
    f->rip = sysl_ler64(gregs, SYSL_REG_RIP * 8);
    f->rsp = sysl_ler64(gregs, SYSL_REG_RSP * 8);
    f->rbp = sysl_ler64(gregs, SYSL_REG_RBP * 8);
}

static void sysl_sinais_garantir(void)
{
    static const int sinais[4] = {SYSL_SIGSEGV, SYSL_SIGBUS, SYSL_SIGILL, SYSL_SIGFPE};

    if (sysl_sinais_ligados)
        return;
    sysl_sinais_ligados = true;
    sysl_pilha_alternativa();
    for (int i = 0; i < 4; i++)
        sysl_instalar_sinal(sinais[i]);
}

void sys_falha_ligar(int (*pre)(const SysFalha *f))
{
    sysl_pre_falha = pre;
    sysl_sinais_garantir();
}

void sys_excecao_ligar(int (*tratador)(unsigned long codigo))
{
    sys_tratador_excecao = tratador;
    sysl_sinais_garantir();
}
