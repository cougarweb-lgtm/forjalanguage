/*
 * rt_syscall_linux.h - syscalls diretas do Linux x86-64 por asm inline (M5 P26d).
 *
 * Versao: 0.10.0
 * Changelog:
 *   0.10.0 - D-G3-L2b: flock 73 (std:file.lock) e sysinfo 99 (std:os.mem_available), na lista fechada do D-M5-3 (emenda).
 *   0.9.0 - M8 P1: gettid 186 (o id da thread do perfil, rt_perfil_linux.c).
 *   0.8.0 - M8 K1 (D-M8-8): getpid 39 (std:os.pid) e sched_getaffinity 204 (std:os.cpu_count).
 *   0.7.0 - R9 (TCP do std:net): poll 7, connect 42, listen 50, getsockopt 55, fcntl 72 e accept4 288, na lista fechada do D-R9 (rt_net_linux.c).
 *   0.6.0 - M5c-b P59/P60: mprotect 10 (std:mem VirtualArena, commit) e as syscalls de UDP do std:net (socket 41, sendto 44, recvfrom 45, bind 49, getsockname 51, setsockopt 54), na lista fechada do D-R9.
 *   0.5.0 - M5c-b P57: futex 202 (a trava do estado compartilhado entre threads, rt_sys_linux.c; so FUTEX_WAIT_PRIVATE e FUTEX_WAKE_PRIVATE).
 *   0.4.0 - M5 P28b-2b: getrusage 98 (std:os peak_memory) e setrlimit 160 (RLIMIT_AS do filho, Command.mem_limit_bytes).
 *   0.3.0 - M5 P28b-2: dup2 33, fork 57, execve 59, wait4 61, kill 62, chdir 80, setpgid 109 e pipe2 293 (emenda do D-M5-3, std:process; sem vfork nem clone).
 *   0.2.0 - M5 P28b-1: lseek 8, fsync 74, getcwd 79, rename 82, mkdir 83, rmdir 84, unlink 87, getdents64 217, newfstatat 262 e getrandom 318 (emenda do D-M5-3; getrandom so com o fallback do /dev/urandom).
 *   0.1.1 - IA1-L6: SYSL_NANOSLEEP (35) para o sleep_ms do std:os (rt_os_linux.c).
 *   0.1.0 - M5 P26d: os numeros da lista fechada do D-M5-3 (read, write, close, fstat, mmap, munmap, rt_sigaction, rt_sigprocmask, rt_sigreturn, ioctl, mremap, sigaltstack,
 *           clock_gettime, exit_group, openat; o rt_sigreturn, 15, e so do restaurador em asm de rt_sys_linux.c) e sysl_chamar1/2/3/6 (1 a 3 argumentos em registradores; 4 a 6 por vetor {numero, a1..a6}). Saiu do rt_sys_linux.c para o heap
 *           (rt_heap_linux.c, que vive no objeto comum rt_linux_comum.o) usar as mesmas funcoes.
 *
 * Syscall devolve -errno em falha (valores de -4095 a -1). So funcoes 'static inline': nenhum simbolo no objeto. Sem libc; asm so aqui e no rt_sys_linux.c.
 */
#ifndef FORJA_RUNTIME_NATIVO_RT_SYSCALL_LINUX_H
#define FORJA_RUNTIME_NATIVO_RT_SYSCALL_LINUX_H

#include <stdbool.h>
#include <stdint.h>

#ifndef RT_ALVO_LINUX
#error "rt_syscall_linux.h exige -DRT_ALVO_LINUX"
#endif

/* Numeros das syscalls (x86-64). */
#define SYSL_READ 0LL
#define SYSL_WRITE 1LL
#define SYSL_CLOSE 3LL
#define SYSL_LSEEK 8LL
#define SYSL_FSTAT 5LL
#define SYSL_MMAP 9LL
#define SYSL_MPROTECT 10LL
#define SYSL_MUNMAP 11LL
#define SYSL_RT_SIGACTION 13LL
#define SYSL_RT_SIGPROCMASK 14LL
#define SYSL_IOCTL 16LL
#define SYSL_MREMAP 25LL
#define SYSL_DUP2 33LL
#define SYSL_NANOSLEEP 35LL
#define SYSL_POLL 7LL
#define SYSL_SOCKET 41LL
#define SYSL_CONNECT 42LL
#define SYSL_SENDTO 44LL
#define SYSL_RECVFROM 45LL
#define SYSL_BIND 49LL
#define SYSL_LISTEN 50LL
#define SYSL_GETSOCKNAME 51LL
#define SYSL_SETSOCKOPT 54LL
#define SYSL_GETSOCKOPT 55LL
#define SYSL_FORK 57LL
#define SYSL_FCNTL 72LL
#define SYSL_EXECVE 59LL
#define SYSL_WAIT4 61LL
#define SYSL_KILL 62LL
#define SYSL_FSYNC 74LL
#define SYSL_GETCWD 79LL
#define SYSL_CHDIR 80LL
#define SYSL_RENAME 82LL
#define SYSL_MKDIR 83LL
#define SYSL_RMDIR 84LL
#define SYSL_UNLINK 87LL
#define SYSL_SETPGID 109LL
#define SYSL_GETRUSAGE 98LL
#define SYSL_SYSINFO 99LL
#define SYSL_FLOCK 73LL
#define SYSL_GETPID 39LL
#define SYSL_SCHED_GETAFFINITY 204LL
#define SYSL_GETTID 186LL
#define SYSL_SETRLIMIT 160LL
#define SYSL_SIGALTSTACK 131LL
#define SYSL_FUTEX 202LL
#define SYSL_GETDENTS64 217LL
#define SYSL_CLOCK_GETTIME 228LL
#define SYSL_EXIT_GROUP 231LL
#define SYSL_OPENAT 257LL
#define SYSL_NEWFSTATAT 262LL
#define SYSL_ACCEPT4 288LL
#define SYSL_PIPE2 293LL
#define SYSL_GETRANDOM 318LL

#define SYSL_EINTR (-4LL)
#define SYSL_ERRO_MAXIMO 4095ULL /* retorno entre -4095 e -1 e erro */
#define SYSL_PROT_LER_ESCREVER 3LL /* PROT_READ | PROT_WRITE */
#define SYSL_MAP_ANONIMO_PRIVADO 0x22LL /* MAP_PRIVATE | MAP_ANONYMOUS */

static inline int64_t sysl_chamar1(int64_t n, int64_t a)
{
    int64_t r;

    __asm__ volatile("syscall" : "=a"(r) : "0"(n), "D"(a) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sysl_chamar2(int64_t n, int64_t a, int64_t b)
{
    int64_t r;

    __asm__ volatile("syscall" : "=a"(r) : "0"(n), "D"(a), "S"(b) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sysl_chamar3(int64_t n, int64_t a, int64_t b, int64_t c)
{
    int64_t r;

    __asm__ volatile("syscall" : "=a"(r) : "0"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sysl_chamar6(const int64_t *v)
{
    int64_t r;
    register int64_t r10 __asm__("r10") = v[4];
    register int64_t r8 __asm__("r8") = v[5];
    register int64_t r9 __asm__("r9") = v[6];

    __asm__ volatile("syscall" : "=a"(r) : "0"(v[0]), "D"(v[1]), "S"(v[2]), "d"(v[3]), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return r;
}

static inline bool sysl_falhou(int64_t r)
{
    return (uint64_t)r >= (uint64_t)0 - SYSL_ERRO_MAXIMO;
}

#endif
