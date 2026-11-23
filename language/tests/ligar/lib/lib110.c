/* lib110.c - implementacao de lib110.h (teste do 'forja ligar' 0.3.0). */
#include "lib110.h"

#include <stdarg.h>
#include <stdio.h>

int32_t get(int32_t indice)
{
    return indice * 3;
}

int32_t insert(int32_t a, int32_t b)
{
    return a + b;
}

int32_t has(int32_t valor)
{
    return valor > 0;
}

int32_t l110_valor_inteiro(l110_valor v)
{
    return v.inteiro + 1000;
}

int32_t l110_log(const char *formato, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, formato);
    int escritos = vsnprintf(buffer, sizeof buffer, formato, args);
    va_end(args);
    return escritos;
}
