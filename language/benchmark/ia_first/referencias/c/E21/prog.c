#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    int total = atoi(argv[1]);
    /* Lista de listas: um vetor de ponteiros, cada linha com k + 1 valores. */
    long long **triangulo = malloc((size_t)total * sizeof *triangulo);
    if (triangulo == NULL) {
        return 3;
    }
    for (int k = 0; k < total; k++) {
        triangulo[k] = malloc((size_t)(k + 1) * sizeof **triangulo);
        triangulo[k][0] = 1;
        triangulo[k][k] = 1;
        for (int j = 1; j < k; j++) {
            triangulo[k][j] = triangulo[k - 1][j - 1] + triangulo[k - 1][j];
        }
        for (int j = 0; j <= k; j++) {
            printf(j == 0 ? "%lld" : " %lld", triangulo[k][j]);
        }
        printf("\n");
    }
    for (int k = 0; k < total; k++) {
        free(triangulo[k]);
    }
    free(triangulo);
    return 0;
}
