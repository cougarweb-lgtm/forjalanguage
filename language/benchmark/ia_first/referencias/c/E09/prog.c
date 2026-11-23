#include <math.h>
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
    double valores[1024];
    int quantos = 0;
    char linha[64];
    while (quantos < 1024 && fgets(linha, sizeof linha, arquivo) != NULL) {
        char *fim = NULL;
        double valor = strtod(linha, &fim);
        if (fim != linha) {
            valores[quantos++] = valor;
        }
    }
    fclose(arquivo);
    double soma = 0.0;
    for (int i = 0; i < quantos; i++) {
        soma += valores[i];
    }
    double media = soma / quantos;
    double quadrados = 0.0;
    for (int i = 0; i < quantos; i++) {
        quadrados += (valores[i] - media) * (valores[i] - media);
    }
    printf("media=%.2f desvio=%.2f\n", media, sqrt(quadrados / quantos));
    return 0;
}
