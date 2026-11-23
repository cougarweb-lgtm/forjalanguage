/*
 * rt_linux_entrada_cru.c - o _start do runtime Linux sem libc (M5b P39, D-R3): o objeto de entrada do ELF quando o programa nao tem foreign com libc.
 *
 * Versao: 0.3.0
 * Changelog:
 *   0.3.0 - M5b P41 (--pie): o _start chama forja_autorrelocar_cru(__ehdr_start) antes de sys_entrada. Ele acha o PT_DYNAMIC pelos cabecalhos de programa
 *           do ELF (vaddr 0 + base) e aplica os R_X86_64_RELATIVE de DT_RELA/DT_RELASZ. Sem PT_DYNAMIC (ET_EXEC, o caso de sempre) nao faz nada.
 *           So aritmetica sobre a base: nenhum endereco absoluto (o objeto e COFF do MinGW, sem TEXTREL).
 *   0.2.0 - M5b P39-B2 (D0654 item 3): define sys_sair (exit_group cru, o unico jeito de sair sem libc); o sys_sair saiu de rt_sys_linux.c.
 *   0.1.0 - M5b P39: o _start sai de rt_sys_linux.c (rt_linux_dev.o e rt_linux_rel.o ficam sem ele) e vira objeto proprio, igual nas duas variantes.
 *           A variante libc (rt_linux_entrada_libc.c) traz o main no lugar: o musl chama o main e e o _start dele que roda antes.
 *
 * O kernel entrega rsp -> argc, argv..., NULL, envp..., NULL. O _start passa o ponteiro da pilha em rcx (Win64), alinha a 16, reserva os 32 B de shadow space e
 * chama sys_entrada (rt_sys_linux.c, no objeto comum do runtime), que nunca volta. Compilar: gcc -std=c11 -O2 -ffreestanding -nostdlib -DRT_ALVO_LINUX
 *   -fno-asynchronous-unwind-tables -fno-unwind-tables -c rt_linux_entrada_cru.c -o rt_linux_entrada_cru.o
 * Saida esperada: .text$sys_start com _start (global) e a referencia a sys_entrada; nenhum outro simbolo, sem .pdata/.xdata.
 */
#ifndef RT_ALVO_LINUX
#error "rt_linux_entrada_cru.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#include <stddef.h>
#include <stdint.h>
#include "rt_syscall_linux.h"

/* Sai pelo kernel: o programa sem foreign nao tem libc para chamar exit(). */
_Noreturn void sys_sair(int codigo)
{
    for (;;)
        sysl_chamar1(SYSL_EXIT_GROUP, codigo);
}

#ifdef RT_ENTRADA_PIE /* variante pie (rt_linux_entrada_pie.o): a auto-relocacao existe so nela, para o ELF sem --pie ficar byte a byte igual */
#define RT_ENT_TAM_QUAD 8u
#define RT_ENT_PT_DYNAMIC 2u
#define RT_ENT_OFF_PHOFF 32u   /* Elf64_Ehdr.e_phoff */
#define RT_ENT_OFF_PHNUM 56u   /* Elf64_Ehdr.e_phnum */
#define RT_ENT_TAM_PHDR 56u
#define RT_ENT_TAM_DYN 16u     /* Elf64_Dyn: tag e valor */
#define RT_ENT_TAM_RELA 24u    /* Elf64_Rela: offset, info, adendo */
#define RT_ENT_DT_NULL 0u
#define RT_ENT_DT_RELA 7u
#define RT_ENT_DT_RELASZ 8u
#define RT_ENT_R_RELATIVE 8u

/* Le n bytes little-endian (n <= 8). Sem memcpy: o objeto nao liga libc. */
static uint64_t RtEntLe(const uint8_t *p, uint32_t n)
{
    uint64_t v = 0;

    for (uint32_t k = n; k > 0; k--)
        v = (v << 8) | p[k - 1];
    return v;
}

/* O PT_DYNAMIC do ELF carregado (vaddr + base), ou NULL no ET_EXEC. */
static const uint8_t *RtEntDinamico(const uint8_t *base)
{
    uint64_t phoff = RtEntLe(base + RT_ENT_OFF_PHOFF, RT_ENT_TAM_QUAD);
    uint64_t phnum = RtEntLe(base + RT_ENT_OFF_PHNUM, 2);

    for (uint64_t k = 0; k < phnum; k++) {
        const uint8_t *ph = base + phoff + k * RT_ENT_TAM_PHDR;

        if (RtEntLe(ph, 4) == RT_ENT_PT_DYNAMIC)
            return base + RtEntLe(ph + 16, RT_ENT_TAM_QUAD);
    }
    return NULL;
}

/* Auto-relocacao do static-pie: base = endereco de carga (o cabecalho ELF esta no vaddr 0). So aritmetica sobre base. */
void forja_autorrelocar_cru(const uint8_t *base)
{
    const uint8_t *dyn = RtEntDinamico(base);
    const uint8_t *rela = NULL;
    uint64_t tam = 0;

    if (dyn == NULL)
        return;
    for (const uint8_t *d = dyn; RtEntLe(d, RT_ENT_TAM_QUAD) != RT_ENT_DT_NULL; d += RT_ENT_TAM_DYN) {
        uint64_t tag = RtEntLe(d, RT_ENT_TAM_QUAD), valor = RtEntLe(d + 8, RT_ENT_TAM_QUAD);

        if (tag == RT_ENT_DT_RELA)
            rela = base + valor;
        if (tag == RT_ENT_DT_RELASZ)
            tam = valor;
    }
    for (uint64_t k = 0; rela != NULL && k < tam; k += RT_ENT_TAM_RELA) {
        const uint8_t *r = rela + k;
        uint8_t *campo = (uint8_t *)base + RtEntLe(r, RT_ENT_TAM_QUAD);
        uint64_t valor = (uint64_t)(uintptr_t)base + RtEntLe(r + 16, RT_ENT_TAM_QUAD);

        if (RtEntLe(r + 8, 4) != RT_ENT_R_RELATIVE)
            continue;
        for (uint32_t b = 0; b < RT_ENT_TAM_QUAD; b++)
            campo[b] = (uint8_t)(valor >> (b * 8));
    }
}

__asm__(
    ".section .text$sys_start,\"xr\"\n"
    ".globl _start\n"
    ".def _start; .scl 2; .type 32; .endef\n"
    "_start:\n"
    "    xor %ebp, %ebp\n"
    "    mov %rsp, %rbx\n"
    "    and $-16, %rsp\n"
    "    sub $32, %rsp\n"
    "    lea __ehdr_start(%rip), %rcx\n"
    "    call forja_autorrelocar_cru\n"
    "    mov %rbx, %rcx\n"
    "    call sys_entrada\n"
    "    ud2\n"
    ".text\n");
#else
__asm__(
    ".section .text$sys_start,\"xr\"\n"
    ".globl _start\n"
    ".def _start; .scl 2; .type 32; .endef\n"
    "_start:\n"
    "    xor %ebp, %ebp\n"
    "    mov %rsp, %rcx\n"
    "    and $-16, %rsp\n"
    "    sub $32, %rsp\n"
    "    call sys_entrada\n"
    "    ud2\n"
    ".text\n");
#endif
