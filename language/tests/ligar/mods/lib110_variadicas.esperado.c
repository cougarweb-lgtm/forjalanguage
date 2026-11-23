// gerado por 'forja ligar' 0.10.0 a partir de lib110.h; nao edite a mao (gere de novo).
// Wrapper de parametros fixos das funcoes variadicas (--wrapper-variadicas): o texto ja formatado entra por "%s".
// gcc -O2 -c -I <pasta do header> lib110_variadicas.c -o lib110_variadicas.o && ar rcs liblib110_variadicas.a lib110_variadicas.o
#include "lib110.h"

int32_t l110_log_texto(const char *texto)
{
    return l110_log("%s", texto);
}
