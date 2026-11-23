/* est_texto.c - membro 2 de libest.a (P19i-b, caso c_estatica_a): texto (cstr entrando e saindo) e estado dentro da lib.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-b: primeira versao.
 * Verificacao: python -I construir_lib.py e python -I ..\..\verificar_interop.py --forja <copia> --backend c --pasta estatica --caso c_estatica_a
 */
#include <stdint.h>

#define EST_CAPACIDADE 64
#define EST_MAX_REGISTROS 16

/* Conta vogais ASCII (aeiou, maiusculas e minusculas). */
int32_t est_vogais(const char *texto)
{
    int32_t total = 0;
    for (const char *p = texto; *p; p++) {
        switch (*p | 0x20) {
        case 'a': case 'e': case 'i': case 'o': case 'u': total++;
        }
    }
    return total;
}

/* Maiusculas ASCII num buffer estatico da lib (valido ate a proxima chamada); NULL se o texto nao cabe em 63 caracteres. */
const char *est_maiuscula(const char *texto)
{
    static char saida[EST_CAPACIDADE];
    int32_t n = 0;
    for (const char *p = texto; *p; p++, n++) {
        if (n >= EST_CAPACIDADE - 1) return (const char *)0;
        saida[n] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
    }
    saida[n] = 0;
    return saida;
}

/* Estado dentro da lib: registra valores (ate 16) e devolve o maximo; vazio = 0. */
static int32_t g_valores[EST_MAX_REGISTROS];
static int32_t g_total = 0;

int32_t est_registrar(int32_t valor)
{
    if (g_total < EST_MAX_REGISTROS) g_valores[g_total++] = valor;
    return g_total;
}

int32_t est_maximo(void)
{
    if (g_total == 0) return 0;
    int32_t m = g_valores[0];
    for (int32_t i = 1; i < g_total; i++) if (g_valores[i] > m) m = g_valores[i];
    return m;
}

void est_limpar(void) { g_total = 0; }
