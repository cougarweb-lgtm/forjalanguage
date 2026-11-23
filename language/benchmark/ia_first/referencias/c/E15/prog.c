#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    /* "wb": sem traducao de fim de linha, para o arquivo sair com '\n' puro. */
    FILE *arquivo = fopen(argv[1], "wb");
    if (arquivo == NULL) {
        return 2;
    }
    fprintf(arquivo, "n,quadrado,cubo\n");
    for (int n = 1; n <= 5; n++) {
        fprintf(arquivo, "%d,%d,%d\n", n, n * n, n * n * n);
    }
    fclose(arquivo);
    printf("gravado: %s (5 linhas)\n", argv[1]);
    return 0;
}
