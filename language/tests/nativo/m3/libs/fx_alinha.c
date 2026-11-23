/* fx_alinha.c - DLL de teste do foreign do backend nativo x64 (M3 P20; bateria 10.2 d, alinhamento da pilha na chamada).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P20: primeira versao.
 * Verificacao: python -I construir_libs.py --so-abi e python -I ..\..\verificar_nativo.py --pasta m3/abi
 * O Win64 exige RSP multiplo de 16 ANTES do 'call': dentro do chamado, na 1a instrucao, RSP e 8 mod 16. fx_rsp_mod devolve esse resto (esperado 8).
 * fx_alinha guarda um local alinhado a 16 e o grava por MOVAPS (_mm_store_ps): se o chamador deixou a pilha torta, o gcc (que assume a pilha certa)
 * poe o local num endereco torto e o MOVAPS falha com violacao de acesso. Devolve resto_do_endereco * 1000000 + x (resto 0 = alinhado).
 */
#include <stdint.h>
#include <xmmintrin.h>

#define FX __declspec(dllexport)

/* A 1a instrucao e a leitura de RSP: nenhum prologo do compilador entra no caminho. */
__asm__(".section .drectve\n"
        "    .ascii \" -export:fx_rsp_mod\"\n"
        ".text\n"
        ".globl fx_rsp_mod\n"
        "fx_rsp_mod:\n"
        "    movq %rsp, %rax\n"
        "    andq $15, %rax\n"
        "    ret\n");

FX int64_t fx_alinha(int64_t x)
{
    float local[4] __attribute__((aligned(16)));
    __m128 v = _mm_set1_ps((float)x);

    _mm_store_ps(local, v);
    return (int64_t)((uintptr_t)local & 15) * 1000000 + (int64_t)local[2];
}

/* Mesmo teste com mais argumentos (a pilha carrega os de 5 em diante) e um agregado por valor: o alinhamento vale com qualquer assinatura. */
typedef struct { int64_t a, b, c; } Tres;

FX int64_t fx_alinha_cheio(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, Tres t)
{
    float local[8] __attribute__((aligned(16)));
    __m128 v = _mm_set1_ps((float)(a + b + c + d + e + f + t.a + t.b + t.c));

    _mm_store_ps(local, v);
    _mm_store_ps(local + 4, v);
    return (int64_t)((uintptr_t)local & 15) * 1000000 + (int64_t)local[5];
}
