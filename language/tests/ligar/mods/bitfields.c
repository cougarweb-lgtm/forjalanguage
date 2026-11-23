/* Lib de PROVA do caso 15 do verificar_ligar.py (compilada pelo gcc; a Forja acessa os bitfields so por get/set gerados). */
#include "bitfields.h"

int32_t bits_demo_tamanho(void)
{
    return (int32_t)sizeof(BitsDemo);
}

void bits_demo_preencher(BitsDemo *s, int32_t n)
{
    s->a = 1;
    s->b = 5;
    s->c = -3;
    s->d = 3000;
    s->e = 2;
    s->f = 21;
    s->tag = 7;
    s->ival = -9;
    s->pos.px = 100;
    s->pos.py = -200;
    s->count = n;
    for (int32_t i = 0; i < n; i++) s->itens[i] = 10 * (i + 1);
}

int32_t bits_demo_ler(const BitsDemo *s, int32_t i)
{
    switch (i) {
    case 0: return (int32_t)s->a;
    case 1: return (int32_t)s->b;
    case 2: return (int32_t)s->c;
    case 3: return (int32_t)s->d;
    case 4: return (int32_t)s->e;
    case 5: return (int32_t)s->f;
    default: return -1;
    }
}

int32_t bits_demo_byte(const BitsDemo *s, int32_t i)
{
    return (int32_t)((const unsigned char *)s)[i];
}
