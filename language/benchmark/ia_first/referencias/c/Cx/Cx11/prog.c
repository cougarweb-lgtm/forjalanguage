#include <stdio.h>

typedef enum { VALIDO, NEGATIVO } Erro;

__attribute__((warn_unused_result)) static Erro validar(int n) {
    if (n < 0) {
        return NEGATIVO;
    }
    return VALIDO;
}

int main(void) {
    if (validar(0 - 5) != VALIDO) {
        printf("invalido\n");
    } else {
        printf("ok\n");
    }
    return 0;
}
