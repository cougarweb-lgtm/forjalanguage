#include <stdio.h>
#include <stdlib.h>

static int comparar(const void *a, const void *b) {
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    FILE *arquivo = fopen(argv[1], "r");
    if (arquivo == NULL) {
        return 2;
    }
    long long valores[1024];
    int quantos = 0;
    char linha[64];
    while (quantos < 1024 && fgets(linha, sizeof linha, arquivo) != NULL) {
        char *fim = NULL;
        long long valor = strtoll(linha, &fim, 10);
        if (fim != linha) {
            valores[quantos++] = valor;
        }
    }
    fclose(arquivo);
    qsort(valores, (size_t)quantos, sizeof valores[0], comparar);
    for (int i = 0; i < quantos; i++) {
        if (i == 0 || valores[i] != valores[i - 1]) {
            printf("%lld\n", valores[i]);
        }
    }
    return 0;
}
