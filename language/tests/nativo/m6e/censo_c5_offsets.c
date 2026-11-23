/* censo_c5_offsets.c - censo c5 do M6e (P93a, D-M6e-8): offsets dos registros de excecao, medidos por programa C.
 * Windows (MinGW x64): EXCEPTION_RECORD e CONTEXT. musl x86-64 (alpine-build): siginfo_t e ucontext_t.
 * Compilar: gcc -O0 -o <exe> censo_c5_offsets.c (Windows); gcc -O0 -static -o <exe> censo_c5_offsets.c (WSL).
 * Saida: uma linha NOME=valor por offset; o CENSO.txt registra a saida e compara com o esperado do ABI.
 * Versao: 0.1.0 - M6e P93a: primeira versao. */
#include <stdio.h>
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#endif

int main(void) {
#ifdef _WIN32
    printf("SO=windows\n");
    printf("EXCEPTION_RECORD.ExceptionCode=%zu\n", offsetof(EXCEPTION_RECORD, ExceptionCode));
    printf("EXCEPTION_RECORD.ExceptionInformation[1]=%zu\n",
           offsetof(EXCEPTION_RECORD, ExceptionInformation) + sizeof(ULONG_PTR));
    printf("CONTEXT.Rip=%zu\n", offsetof(CONTEXT, Rip));
    printf("CONTEXT.Rsp=%zu\n", offsetof(CONTEXT, Rsp));
    printf("CONTEXT.Rbp=%zu\n", offsetof(CONTEXT, Rbp));
#else
    printf("SO=linux\n");
    printf("siginfo_t.si_addr=%zu\n", offsetof(siginfo_t, si_addr));
    printf("REG_RIP=%d\n", REG_RIP);
    printf("REG_RSP=%d\n", REG_RSP);
    printf("REG_RBP=%d\n", REG_RBP);
    printf("ucontext_t.uc_mcontext.gregs=%zu\n", offsetof(ucontext_t, uc_mcontext.gregs));
    printf("gregs[REG_RIP]=%zu\n", offsetof(ucontext_t, uc_mcontext.gregs) + REG_RIP * sizeof(greg_t));
    printf("gregs[REG_RSP]=%zu\n", offsetof(ucontext_t, uc_mcontext.gregs) + REG_RSP * sizeof(greg_t));
    printf("gregs[REG_RBP]=%zu\n", offsetof(ucontext_t, uc_mcontext.gregs) + REG_RBP * sizeof(greg_t));
#endif
    return 0;
}
