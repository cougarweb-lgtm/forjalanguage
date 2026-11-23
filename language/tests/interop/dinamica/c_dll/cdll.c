/* cdll.c - DLL de C de teste da bateria de interop (P19i-a, caso c_dll): escalares, ponteiro de saida, buffer, texto, estado e enum.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-a: primeira versao.
 * Verificacao: construir_lib.bat e python -I ..\..\..\verificar_interop.py --forja <copia> --backend c --pasta dinamica --caso c_dll
 */
#include <stdint.h>
#include <string.h>

#define CD __declspec(dllexport)

typedef enum { COR_VERMELHO = 0, COR_VERDE = 1, COR_AZUL = 5 } cd_cor;

CD int32_t cd_soma(int32_t a, int32_t b) { return a + b; }
CD int64_t cd_mul64(int64_t a, int64_t b) { return a * b; }
CD double cd_media(double a, double b) { return (a + b) / 2.0; }
CD uint8_t cd_inverte_bits(uint8_t v) { return (uint8_t)~v; }

/* Ponteiro de saida: devolve 1 e grava quociente e resto; 0 se o divisor e zero (saidas intactas). */
CD int32_t cd_divide(int32_t a, int32_t b, int32_t *quociente, int32_t *resto)
{
    if (b == 0) return 0;
    *quociente = a / b;
    *resto = a % b;
    return 1;
}

/* Buffer do chamador: preenche n inteiros com 10, 20, 30... e devolve a soma. */
CD int64_t cd_preenche(int32_t *destino, int32_t n)
{
    int64_t soma = 0;
    for (int32_t i = 0; i < n; i++) {
        destino[i] = (i + 1) * 10;
        soma += destino[i];
    }
    return soma;
}

/* Texto: cstr de entrada (tamanho) e de saida (constante da DLL; NULL quando o codigo e negativo). */
CD int32_t cd_tamanho(const char *texto) { return (int32_t)strlen(texto); }
CD const char *cd_nome_da_cor(cd_cor cor)
{
    switch (cor) {
    case COR_VERMELHO: return "vermelho";
    case COR_VERDE: return "verde";
    case COR_AZUL: return "azul";
    }
    return (const char *)0;
}

/* Estado dentro da DLL: contador que sobrevive entre chamadas. */
static int32_t g_contador = 0;
CD int32_t cd_contar(int32_t passo) { g_contador += passo; return g_contador; }
CD void cd_zerar(void) { g_contador = 0; }
