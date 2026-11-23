/* el.c - fonte de estlib.lib (P19i-b, caso c_estatica_lib): archive GNU (!<arch>) com a extensao .lib, como o ld aceita para -lestlib (Q28).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-b: primeira versao.
 * Verificacao: python -I construir_lib.py e python -I ..\..\verificar_interop.py --forja <copia> --backend c --pasta estatica --caso c_estatica_lib
 */
#include <stdint.h>

/* base elevada a exp (exp >= 0) em 64 bits, grava em *saida e devolve 1; exp < 0 devolve 0 e deixa *saida intacta. */
int32_t el_potencia(int64_t base, int32_t exp, int64_t *saida)
{
    if (exp < 0) return 0;
    int64_t r = 1;
    for (int32_t i = 0; i < exp; i++) r *= base;
    *saida = r;
    return 1;
}

/* XOR de todos os bytes de dados[0..n). */
uint8_t el_xor(uint8_t *dados, int32_t n)
{
    uint8_t r = 0;
    for (int32_t i = 0; i < n; i++) r ^= dados[i];
    return r;
}

/* Inverte dados[0..n) no lugar. */
void el_inverter(uint8_t *dados, int32_t n)
{
    for (int32_t i = 0, j = n - 1; i < j; i++, j--) {
        uint8_t t = dados[i];
        dados[i] = dados[j];
        dados[j] = t;
    }
}

/* Media aritmetica de n doubles; 0.0 se n <= 0. */
double el_media(double *valores, int32_t n)
{
    if (n <= 0) return 0.0;
    double soma = 0.0;
    for (int32_t i = 0; i < n; i++) soma += valores[i];
    return soma / n;
}
