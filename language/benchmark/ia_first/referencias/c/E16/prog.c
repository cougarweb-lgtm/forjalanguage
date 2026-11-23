#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) {
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
    long long alvo = strtoll(argv[2], NULL, 10);
    int baixo = 0;
    int alto = quantos - 1;
    int achado = -1;
    while (baixo <= alto && achado < 0) {
        int meio = baixo + (alto - baixo) / 2;
        if (valores[meio] == alvo) {
            achado = meio;
        } else if (valores[meio] < alvo) {
            baixo = meio + 1;
        } else {
            alto = meio - 1;
        }
    }
    printf("%d\n", achado);
    return 0;
}
