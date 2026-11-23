/* Gemeo em C puro de usa_inline_cola.fj (caso 16 do verificar_ligar.py): a saida tem que ser identica linha a linha. */
#include <stdio.h>
#include "inline_cola.h"

int main(void)
{
    printf("%d\n", ic_soma(40, 2));
    IcPar t = ic_troca((IcPar){ 1, 2 });
    printf("%d\n%d\n", t.a, t.b);
    printf("%d\n", ic_contador);
    ic_contador = 10;
    printf("%d\n", ic_incrementa(5));
    printf("%d\n", ic_contador_mais(1));
    printf("%d\n", ic_limite);
    printf("%s\n", ic_nome != NULL ? "true" : "false");
    printf("%d\n", IC_TRIPLO(14));
    printf("%d\n", IC_MAIOR(3, 9));
    return 0;
}
