#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("erro: informe o arquivo\n");
        return 2;
    }
    FILE *arquivo = fopen(argv[1], "rb");
    if (arquivo == NULL) {
        printf("erro: arquivo nao encontrado: %s\n", argv[1]);
        return 2;
    }
    long linhas = 0;
    int ultimo = -1;
    int c;
    while ((c = fgetc(arquivo)) != EOF) {
        if (c == '\n') {
            linhas++;
        }
        ultimo = c;
    }
    fclose(arquivo);
    if (ultimo != -1 && ultimo != '\n') {
        linhas++;
    }
    printf("%ld\n", linhas);
    return 0;
}
