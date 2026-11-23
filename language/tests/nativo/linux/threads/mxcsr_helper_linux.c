/* mxcsr_helper_linux.c - M5c-b P58: gemea Linux de mxcsr_ftz.fj. Duas funcoes para ligar FTZ/DAZ na thread atual e ler o MXCSR dela (o _controlfp do msvcrt nao existe no Linux).
 * Lib: vendor/linux/libmxcsr_linux.a (construir_mxcsr.sh, no alpine-build). Nada disto entra no forja.exe. Simbolos sem o prefixo forja_ (reservado pelo compilador, E0905).
 * Versao: 0.2.0 - M5c-b P58: simbolos renomeados para mxcsr_t_ler e mxcsr_t_ligar_ftz (E0905: forja_ e prefixo do compilador).
 *          0.1.0 - M5c-b P58: primeira versao (__builtin_ia32_stmxcsr e ldmxcsr; bits FTZ 0x8000 e DAZ 0x0040).
 */
#include <stdint.h>

#define MXCSR_FTZ_DAZ 0x8040u

uint32_t mxcsr_t_ler(void)
{
    return (uint32_t)__builtin_ia32_stmxcsr();
}

void mxcsr_t_ligar_ftz(void)
{
    __builtin_ia32_ldmxcsr((uint32_t)__builtin_ia32_stmxcsr() | MXCSR_FTZ_DAZ);
}
