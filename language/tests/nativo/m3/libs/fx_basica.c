/* fx_basica.c - DLL de teste do foreign do backend nativo x64 (M3 P19): funcoes de todos os tamanhos de inteiro, reais misturados, 6 e 12 argumentos e retorno NULL.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M3 P19c: fx_escreve (printf no stdout do UCRT; a fixture dll_escreve confere a ordem com o print do programa).
 *            0.1.0 - M3 P19: primeira versao.
 * Verificacao: construir_libs.bat e python -I ..\..\verificar_nativo.py --pasta m3/foreign
 */
#include <stdint.h>
#include <stdio.h>

#define FX __declspec(dllexport)

FX int8_t fx_i8(int8_t a, int8_t b) { return (int8_t)(a + b); }
FX uint8_t fx_u8(uint8_t a, uint8_t b) { return (uint8_t)(a + b); }
FX int16_t fx_i16(int16_t a, int16_t b) { return (int16_t)(a * b); }
FX uint16_t fx_u16(uint16_t a, uint16_t b) { return (uint16_t)(a * b); }
FX int32_t fx_i32(int32_t a, int32_t b) { return a - b; }
FX uint32_t fx_u32(uint32_t a, uint32_t b) { return a + b; }
FX int64_t fx_i64(int64_t a, int64_t b) { return a * b; }
FX uint64_t fx_u64(uint64_t a, uint64_t b) { return a + b; }
FX float fx_f32(float a, float b) { return a * b + 0.5f; }
FX double fx_f64(double a, double b) { return a / b; }

/* inteiros e reais alternados: RCX/RDX/R8/R9 e XMM0..3 dividem as posicoes (o Win64 conta a posicao, nao a classe). */
FX double fx_mistura(int32_t a, double b, int64_t c, float d) { return a + b + (double)c + d; }
FX double fx_mistura6(int32_t a, double b, int64_t c, float d, uint8_t e, int16_t f)
{
    return a + b + (double)c + d + e + f;
}

/* 6 e 12 argumentos: do 5o em diante vao pela pilha, depois dos 32 bytes de sombra. */
FX int64_t fx_seis(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f;
}
FX int64_t fx_doze(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, int64_t g, int64_t h, int64_t i, int64_t j,
                   int64_t k, int64_t l)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i + 10 * j + 11 * k + 12 * l;
}
FX double fx_doze_mist(int32_t a, double b, int32_t c, double d, int32_t e, double f, int32_t g, double h, int32_t i, double j, int32_t k,
                       double l)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i + 10 * j + 11 * k + 12 * l;
}

FX const char *fx_texto(int32_t quer)
{
    return quer != 0 ? "forja" : (const char *)0;
}

FX int32_t fx_quem(void) { return 1; }

/* Escreve no stdout do UCRT (o FILE que o print do oraculo C usa) e devolve os caracteres escritos. */
FX int32_t fx_escreve(int32_t n) { return printf("fx %d\n", n); }
