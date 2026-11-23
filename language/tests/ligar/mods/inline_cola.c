/* Lib de PROVA do caso 16 do verificar_ligar.py: define as globais e a funcao comum de inline_cola.h. */
#include "inline_cola.h"

int32_t ic_contador = 100;
const int32_t ic_limite = 77;
const char ic_nome[] = "cola";

int32_t ic_incrementa(int32_t passo)
{
    ic_contador += passo;
    return ic_contador;
}
