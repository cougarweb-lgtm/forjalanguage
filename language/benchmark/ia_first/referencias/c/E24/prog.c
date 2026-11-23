#include <stdio.h>
#include <stdlib.h>

#include "matematica.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }
    long long a = strtoll(argv[1], NULL, 10);
    long long b = strtoll(argv[2], NULL, 10);
    printf("soma_quadrados(%lld, %lld) = %lld\n", a, b, soma_quadrados(a, b));
    return 0;
}
