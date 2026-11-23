/* vivos_cb.c - lib da fixture de callback SysV (M5b P38-0), build no alpine por construir_abi_sysv_cb.sh.
 * vvc_chama_cb(cb, a, b, mism): guarda rbx/rbp/r12-r15 do chamador, poe valores conhecidos neles, chama cb(a, b) com a ABI
 * SysV, confere os 6 registradores na volta (callee-saved: o callback tem de devolve-los iguais) e grava a quantidade de
 * divergencias em *mism. Devolve o que o callback devolveu (rax).
 * vvc_alinhamento(): devolve rsp & 15 na ENTRADA da propria funcao (ABI SysV exige 8: o call empurrou o endereco de retorno sobre
 * um rsp alinhado a 16). Escrita em asm de topo porque um prologo de C ja mexe no rsp antes do corpo.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P38-0: primeira versao (6 registradores callee-saved + alinhamento na entrada).
 */

long vvc_chama_cb(long (*cb)(long, long), long a, long b, long *mism);
long vvc_alinhamento(void);

__asm__(
    ".text\n"
    ".globl vvc_chama_cb\n"
    ".type vvc_chama_cb, @function\n"
    "vvc_chama_cb:\n"
    "    push %rbx\n"
    "    push %rbp\n"
    "    push %r12\n"
    "    push %r13\n"
    "    push %r14\n"
    "    push %r15\n"
    "    push %rcx\n"
    "    mov %rdi, %rax\n"
    "    mov %rsi, %rdi\n"
    "    mov %rdx, %rsi\n"
    "    mov $0x1111, %rbx\n"
    "    mov $0x2222, %rbp\n"
    "    mov $0x3333, %r12\n"
    "    mov $0x4444, %r13\n"
    "    mov $0x5555, %r14\n"
    "    mov $0x6666, %r15\n"
    "    call *%rax\n"
    "    pop %rcx\n"
    "    xor %r8d, %r8d\n"
    "    cmp $0x1111, %rbx\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    cmp $0x2222, %rbp\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    cmp $0x3333, %r12\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    cmp $0x4444, %r13\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    cmp $0x5555, %r14\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    cmp $0x6666, %r15\n"
    "    setne %dl\n"
    "    movzbl %dl, %edx\n"
    "    add %rdx, %r8\n"
    "    mov %r8, (%rcx)\n"
    "    pop %r15\n"
    "    pop %r14\n"
    "    pop %r13\n"
    "    pop %r12\n"
    "    pop %rbp\n"
    "    pop %rbx\n"
    "    ret\n"
    ".size vvc_chama_cb, .-vvc_chama_cb\n"
    ".globl vvc_alinhamento\n"
    ".type vvc_alinhamento, @function\n"
    "vvc_alinhamento:\n"
    "    mov %rsp, %rax\n"
    "    and $15, %rax\n"
    "    ret\n"
    ".size vvc_alinhamento, .-vvc_alinhamento\n"
);
