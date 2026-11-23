/* cback.c - DLL de C de teste da bateria de interop (P19i-a, caso c_callback): a DLL chama de volta procs da Forja (callback) com user_data.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P19i-a: primeira versao.
 * Verificacao: python -I construir_lib.py e python -I ..\..\..\verificar_interop.py --forja <copia> --backend c --pasta dinamica --caso c_callback
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CB __declspec(dllexport)

typedef int32_t (*cb_comparar)(const void *a, const void *b, void *user_data);
typedef int32_t (*cb_visitar)(int32_t item, void *user_data);

/* Ordenacao por insercao estavel (a DLL escolhe o algoritmo; a Forja so decide a ordem pelo callback). */
CB void cb_ordenar(void *base, size_t n, size_t tamanho, cb_comparar comparar, void *user_data)
{
    unsigned char *v = (unsigned char *)base;
    unsigned char guarda[64];
    for (size_t i = 1; i < n; i++) {
        memcpy(guarda, v + i * tamanho, tamanho);
        size_t j = i;
        while (j > 0 && comparar(v + (j - 1) * tamanho, guarda, user_data) > 0) {
            memcpy(v + j * tamanho, v + (j - 1) * tamanho, tamanho);
            j--;
        }
        memcpy(v + j * tamanho, guarda, tamanho);
    }
}

/* Soma o retorno de visitar(1..n); o user_data atravessa intacto. */
CB int32_t cb_somar_visitas(int32_t n, cb_visitar visitar, void *user_data)
{
    int32_t soma = 0;
    for (int32_t i = 1; i <= n; i++) soma += visitar(i, user_data);
    return soma;
}

/* Callback guardado na DLL e disparado depois, em outra chamada (o trampolim precisa viver entre as chamadas). */
static cb_visitar g_registrado = 0;
static void *g_contexto = 0;
CB void cb_registrar(cb_visitar visitar, void *user_data)
{
    g_registrado = visitar;
    g_contexto = user_data;
}
CB int32_t cb_disparar(int32_t valor) { return g_registrado != 0 ? g_registrado(valor, g_contexto) : -1; }

/* Funcao comum da DLL que o callback da Forja chama de volta (Forja -> DLL -> Forja -> DLL). */
CB int32_t cb_peso(int32_t item) { return item * item + 1; }
