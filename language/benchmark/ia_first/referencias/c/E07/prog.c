#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sinal '-' opcional, depois 1 a 15 digitos e nada mais. */
static int valido(const char *texto) {
    const char *p = texto;
    if (*p == '-') {
        p++;
    }
    size_t digitos = strlen(p);
    if (digitos < 1 || digitos > 15) {
        return 0;
    }
    for (size_t i = 0; i < digitos; i++) {
        if (p[i] < '0' || p[i] > '9') {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("erro: informe um numero\n");
        return 2;
    }
    if (!valido(argv[1])) {
        printf("invalido: %s\n", argv[1]);
        return 1;
    }
    printf("%lld\n", strtoll(argv[1], NULL, 10) * 2);
    return 0;
}
