/* vivos.c - funcoes de teste do P37c (valores vivos): cada uma destroi rsi, rdi e xmm6-xmm15 com asm (o SysV deixa esses registradores para o chamado).
 * Compilada no WSL (alpine-build) por construir_abi_sysv.sh: vivos/vendor/linux/libvivos.a.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P37c: primeira versao (vv_clob_i, vv_clob_f: soma depois de sobrescrever os registradores).
 */
#define VV_DESTROI_REGISTRADORES \
    "movabsq $0x5a5a5a5a5a5a5a5a, %%rsi\n\t" \
    "movq %%rsi, %%rdi\n\t" \
    "movq %%rsi, %%xmm6\n\t" "movq %%rsi, %%xmm7\n\t" "movq %%rsi, %%xmm8\n\t" "movq %%rsi, %%xmm9\n\t" \
    "movq %%rsi, %%xmm10\n\t" "movq %%rsi, %%xmm11\n\t" "movq %%rsi, %%xmm12\n\t" "movq %%rsi, %%xmm13\n\t" \
    "movq %%rsi, %%xmm14\n\t" "movq %%rsi, %%xmm15\n\t"

#define VV_CLOBBER_LISTA "rsi", "rdi", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"

long vv_clob_i(long a, long b)
{
    long soma = a + b;
    __asm__ volatile(VV_DESTROI_REGISTRADORES : : : VV_CLOBBER_LISTA);
    return soma;
}

double vv_clob_f(double a, double b)
{
    double soma = a + b;
    __asm__ volatile(VV_DESTROI_REGISTRADORES : : : VV_CLOBBER_LISTA);
    return soma;
}
