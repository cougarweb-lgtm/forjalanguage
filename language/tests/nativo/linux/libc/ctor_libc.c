/* ctor_libc.c - M5b P39-B3 (d): construtor em .init_array (__attribute__((constructor))) que o musl roda antes do main da Forja.
 * O main le ctor_valor(): 4242 so se o construtor rodou; 0 se o init_array foi pulado. Lib: vendor/linux/libctor_libc.a (construir_libc.sh).
 * Versao: 0.1.0 - M5b P39-B3: primeira versao. */
#include <stdint.h>

static int64_t valor_construido = 0;

__attribute__((constructor)) static void ConstrutorDaLib(void)
{
    valor_construido = 4242;
}

int64_t ctor_valor(void)
{
    return valor_construido;
}
