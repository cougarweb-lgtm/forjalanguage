/*
 * rt_salto.c - salvar e saltar do runtime nativo da Forja (M2 P12b): o setjmp/longjmp proprio, em asm, sem libc.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M5 P26d: as diretivas .seh_proc/.seh_endprologue/.seh_endproc saem no alvo Linux (RT_SALTO_SEH); no Windows o asm e o mesmo de antes (objeto igual, byte a byte).
 *   0.1.0 - M2 P12b: primeira versao. forja_rt_salvar guarda os registradores preservados do Win64 (rbx rbp rdi rsi r12..r15, xmm6..xmm15, mxcsr),
 *           o rsp e o rip de DEPOIS do retorno; forja_rt_saltar os devolve e continua no ponto da chamada de salvar, que volta de novo com o valor dado.
 *           O __builtin_setjmp do gcc 16.1 falha em -O2 neste runtime (pendencia do M1): daqui vem o salto do executor de testes (rt_teste.c).
 *
 * Unico lugar do runtime (e do backend) com asm: um __asm__ de topo de arquivo, listado no conferir_regras.py. O .seh_proc/.seh_endproc
 * deixa o .pdata como o de qualquer funcao folha; a retomada pelo salto nao usa o desenrolar da pilha.
 * ForjaSalto e o layout que o asm conhece (gpr em 0..79, xmm em 80..239, mxcsr em 240); sizeof 256 por causa do alinhamento 16.
 * Incluido por rt_win.c; nao compila sozinho.
 */
#include <stdint.h>

#include "rt_compat.h"

/* As diretivas .seh_* (desenrolar de pilha do Windows) geram .pdata/.xdata; o objeto Linux nao as tem (D-M5-7). No Windows o texto e o mesmo de antes. */
#ifdef RT_ALVO_LINUX
#define RT_SALTO_SEH(diretiva) ""
#else
#define RT_SALTO_SEH(diretiva) diretiva "\n"
#endif

typedef struct {
    _Alignas(16) uint64_t gpr[10]; /* rbx rbp rdi rsi r12 r13 r14 r15 rsp rip */
    uint8_t xmm[160];              /* xmm6..xmm15, 16 bytes cada */
    uint32_t mxcsr;
} ForjaSalto;

/* Guarda o contexto em *s e devolve 0; depois de forja_rt_saltar(s, v) volta daqui de novo, com v (0 vira 1, como o longjmp). */
int forja_rt_salvar(ForjaSalto *s) RT_RETORNA_DUAS_VEZES;
/* Restaura o contexto de *s (guardado por forja_rt_salvar, ainda ativo) e continua depois da chamada a salvar. Nao volta. */
_Noreturn void forja_rt_saltar(ForjaSalto *s, int v);

__asm__(
    ".text\n"
    ".globl forja_rt_salvar\n"
    ".def forja_rt_salvar; .scl 2; .type 32; .endef\n"
    RT_SALTO_SEH(".seh_proc forja_rt_salvar")
    "forja_rt_salvar:\n"
    RT_SALTO_SEH(".seh_endprologue")
    "    movq %rbx, 0(%rcx)\n"
    "    movq %rbp, 8(%rcx)\n"
    "    movq %rdi, 16(%rcx)\n"
    "    movq %rsi, 24(%rcx)\n"
    "    movq %r12, 32(%rcx)\n"
    "    movq %r13, 40(%rcx)\n"
    "    movq %r14, 48(%rcx)\n"
    "    movq %r15, 56(%rcx)\n"
    "    leaq 8(%rsp), %rax\n"
    "    movq %rax, 64(%rcx)\n"
    "    movq (%rsp), %rax\n"
    "    movq %rax, 72(%rcx)\n"
    "    movups %xmm6, 80(%rcx)\n"
    "    movups %xmm7, 96(%rcx)\n"
    "    movups %xmm8, 112(%rcx)\n"
    "    movups %xmm9, 128(%rcx)\n"
    "    movups %xmm10, 144(%rcx)\n"
    "    movups %xmm11, 160(%rcx)\n"
    "    movups %xmm12, 176(%rcx)\n"
    "    movups %xmm13, 192(%rcx)\n"
    "    movups %xmm14, 208(%rcx)\n"
    "    movups %xmm15, 224(%rcx)\n"
    "    stmxcsr 240(%rcx)\n"
    "    xorl %eax, %eax\n"
    "    ret\n"
    RT_SALTO_SEH(".seh_endproc")
    ".globl forja_rt_saltar\n"
    ".def forja_rt_saltar; .scl 2; .type 32; .endef\n"
    RT_SALTO_SEH(".seh_proc forja_rt_saltar")
    "forja_rt_saltar:\n"
    RT_SALTO_SEH(".seh_endprologue")
    "    movl %edx, %eax\n"
    "    testl %eax, %eax\n"
    "    jnz 1f\n"
    "    movl $1, %eax\n"
    "1:\n"
    "    movq 0(%rcx), %rbx\n"
    "    movq 8(%rcx), %rbp\n"
    "    movq 16(%rcx), %rdi\n"
    "    movq 24(%rcx), %rsi\n"
    "    movq 32(%rcx), %r12\n"
    "    movq 40(%rcx), %r13\n"
    "    movq 48(%rcx), %r14\n"
    "    movq 56(%rcx), %r15\n"
    "    movups 80(%rcx), %xmm6\n"
    "    movups 96(%rcx), %xmm7\n"
    "    movups 112(%rcx), %xmm8\n"
    "    movups 128(%rcx), %xmm9\n"
    "    movups 144(%rcx), %xmm10\n"
    "    movups 160(%rcx), %xmm11\n"
    "    movups 176(%rcx), %xmm12\n"
    "    movups 192(%rcx), %xmm13\n"
    "    movups 208(%rcx), %xmm14\n"
    "    movups 224(%rcx), %xmm15\n"
    "    ldmxcsr 240(%rcx)\n"
    "    movq 72(%rcx), %rdx\n"
    "    movq 64(%rcx), %rsp\n"
    "    jmp *%rdx\n"
    RT_SALTO_SEH(".seh_endproc")
);
