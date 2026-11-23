#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    FILE *arquivo = fopen(argv[1], "r");
    if (arquivo == NULL) {
        return 2;
    }
    char linha[64];
    long long soma = 0;
    while (fgets(linha, sizeof linha, arquivo) != NULL) {
        char *fim = NULL;
        long long valor = strtoll(linha, &fim, 10);
        if (fim != linha) {
            soma += valor;
        }
    }
    fclose(arquivo);
    printf("%lld\n", soma);
    return 0;
}
