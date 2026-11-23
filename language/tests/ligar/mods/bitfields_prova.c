/* Gemeo em C puro de tests\ligar\usa_bitfields.fj: mesmo roteiro, mesma saida (um numero por linha). Caso 15 do verificar_ligar.py. */
#include <stdio.h>
#include <string.h>
#include "bitfields.h"

static void MostrarCampos(const BitsDemo *p)
{
    printf("%u\n%u\n%d\n%u\n%u\n%u\n", (unsigned)p->a, (unsigned)p->b, (int)p->c, (unsigned)p->d, (unsigned)p->e, (unsigned)p->f);
    printf("%d\n%d\n%d\n%d\n%d\n", (int)p->tag, (int)p->ival, (int)p->pos.px, (int)p->pos.py, (int)p->count);
    for (int i = 0; i < 3; i++) printf("%d\n", (int)p->itens[i]);
}

static void GravarPeloC(BitsDemo *p)
{
    p->a = 1;
    p->b = 5;
    p->c = -3;
    p->d = 3000;
    p->e = 2;
    p->f = 21;
    p->tag = 7;
    p->ival = -9;
    p->pos.px = 100;
    p->pos.py = -200;
    p->count = 3;
    p->itens[0] = 10;
    p->itens[1] = 20;
    p->itens[2] = 30;
}

int main(void)
{
    _Alignas(8) unsigned char raw[64] = {0};
    BitsDemo *p = (BitsDemo *)raw;
    bits_demo_preencher(p, 3);
    MostrarCampos(p);
    memset(raw, 0, sizeof raw);
    GravarPeloC(p);
    MostrarCampos(p);
    for (int i = 0; i < 36; i++) printf("%d\n", bits_demo_byte(p, i));
    for (int i = 0; i < 6; i++) printf("%d\n", bits_demo_ler(p, i));
    printf("%d\n", bits_demo_tamanho());
    return 0;
}
