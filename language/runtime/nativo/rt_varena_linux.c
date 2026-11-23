/*
 * rt_varena_linux.c - VirtualArena do std:mem no Linux x86-64 (M5c-b P59): mmap PROT_NONE, mprotect RW e munmap por syscall crua, sem libc. Unidade propria (D-R3): nao muda rt_linux_*.o.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M5c-b P59: forja_varena_reservar (mmap PROT_NONE + MAP_NORESERVE), forja_varena_commit (mprotect RW das paginas que cobrem [endereco, endereco + n)),
 *           forja_varena_liberar (munmap) e forja_varena_ponteiro (cast), com os mesmos nomes, assinaturas e retornos de rt_varena_win.c 0.1.0.
 *           Diferenca de SO: munmap pede o tamanho e o Windows (MEM_RELEASE) nao. Por isso reservar grava o tamanho total (u64) numa pagina de cabecalho ANTES da regiao
 *           devolvida, e liberar le dali. A pagina de cabecalho fica RW; a regiao comeca PROT_NONE, como no Windows.
 *
 * Exige RT_ALVO_LINUX (rt_syscall_linux.h). Sem libc: o ELF continua cru. Objeto so entra na ligacao quando o programa referencia forja_varena_* (LerVarena, nativo.c).
 * Verificacao: construir.bat e python -I tests\nativo\m5c\varena\verificar_varena_p52.py <forja.exe> (o mesmo verificador roda no WSL com --target x86_64-linux pelo verificar_m5c_linux.py).
 */
#include <stdint.h>

#include "rt_syscall_linux.h"

#define VARENA_PAGINA 4096LL
#define VARENA_CABECALHO 4096LL /* uma pagina antes da regiao: guarda o tamanho total do mmap */
#define VARENA_PROT_NENHUM 0LL
#define VARENA_MAP_RESERVA 0x4022LL /* MAP_PRIVATE (0x02) | MAP_ANONYMOUS (0x20) | MAP_NORESERVE (0x4000): sem commit no kernel */

/* Reserva n bytes de espaco de enderecos, sem acesso. 0 = o sistema recusou (o chamador devolve sem_reserva). */
int64_t forja_varena_reservar(int64_t n)
{
    int64_t total;
    int64_t base;
    int64_t v[7];

    if (n <= 0 || n > INT64_MAX - VARENA_CABECALHO - VARENA_PAGINA)
        return 0;
    total = n + VARENA_CABECALHO;
    v[0] = SYSL_MMAP;
    v[1] = 0;
    v[2] = total;
    v[3] = VARENA_PROT_NENHUM;
    v[4] = VARENA_MAP_RESERVA;
    v[5] = -1;
    v[6] = 0;
    base = sysl_chamar6(v);
    if (sysl_falhou(base))
        return 0;
    if (sysl_chamar3(SYSL_MPROTECT, base, VARENA_CABECALHO, SYSL_PROT_LER_ESCREVER) != 0) {
        sysl_chamar2(SYSL_MUNMAP, base, total);
        return 0;
    }
    *(int64_t *)base = total;
    return base + VARENA_CABECALHO;
}

/* Commit de [endereco, endereco + n) em leitura e escrita. As paginas que cobrem o intervalo (VirtualAlloc faz o mesmo). 1 = ok; 0 = o sistema recusou ou n invalido. */
int32_t forja_varena_commit(int64_t endereco, int64_t n)
{
    int64_t inicio;
    int64_t fim;

    if (n <= 0)
        return 0;
    inicio = endereco & ~(VARENA_PAGINA - 1);
    fim = (endereco + n + VARENA_PAGINA - 1) & ~(VARENA_PAGINA - 1);
    return sysl_chamar3(SYSL_MPROTECT, inicio, fim - inicio, SYSL_PROT_LER_ESCREVER) == 0 ? 1 : 0;
}

/* Devolve a regiao reservada que comeca em endereco (inteira, como MEM_RELEASE). 0 = nada a fazer. */
void forja_varena_liberar(int64_t endereco)
{
    int64_t base;
    int64_t total;

    if (endereco == 0)
        return;
    base = endereco - VARENA_CABECALHO;
    total = *(int64_t *)base;
    (void)sysl_chamar2(SYSL_MUNMAP, base, total);
}

/* O rawptr da Forja nao faz aritmetica: o bump pointer soma o deslocamento em inteiro e converte aqui. */
void *forja_varena_ponteiro(int64_t endereco)
{
    return (void *)(uintptr_t)endereco;
}
