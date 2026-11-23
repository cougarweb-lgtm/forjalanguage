#include <stdio.h>

typedef enum { VALIDO, NEGATIVO } Erro;

__attribute__((warn_unused_result)) static Erro validar(int n) {
    if (n < 0) {
        return NEGATIVO;
    }
    return VALIDO;
}

int main(void) {
    validar(0 - 5);
    printf("ok\n");
    return 0;
}
